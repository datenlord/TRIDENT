#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include "dma_utils.c"

// Only support U2, U8 and U11
static char domain_tag_u11[65] = "23edaba93b53fd378ab1b215ee71e1f173a82b081cf7b1a8000011a7ffffee58";
static char domain_tag_u8[65]  = "0c59041b7aa57a3757c9e652d111ec48d5f04d67039bae3300000232fffffdcd";
static char domain_tag_u2[65]  = "486e140d064f104ecca4efcfc634efe0098e27ee0009d80600000005fffffffa";


typedef struct read_thr_args{
	uint8_t *bytes;
	int batch_size;
} read_thr_args_t;


//helper function
static uint8_t fromAscii(uint8_t c)
{
  if (c >= '0' && c <= '9')
    return (c - '0');
  if (c >= 'a' && c <= 'f')
    return (c - 'a' + 10);
  if (c >= 'A' && c <= 'F')
    return (c - 'A' + 10);

  assert(0);
  return 0xff;
}


//helper function
static uint8_t ascii_r(uint8_t a, uint8_t b)
{
  return (fromAscii(a) << 4) + fromAscii(b);
}



void write_to_fpga(uint8_t *bytes, int batch_size, int arity) {

    char devname[] = "/dev/xdma0_h2c_0";
    int fd = open(devname, O_RDWR);
    if (fd < 0) {
		fprintf(stderr, "Unable to open device %s\n", devname);
        return;
    }

    int num_bytes = 32*arity*batch_size;
    int len = 32*arity;

#if 0
	for (int i = 31; i < num_bytes; i+=32) {
		uint8_t *last = &bytes[i];
        if (*last > 0x7f) {
			printf("i = %d, %2.2x", i, *last);
			assert(0);
		}
	}
#endif

    for (int i = len - 1; i < num_bytes; i+= len) {
        uint8_t *last = &bytes[i];
        assert(*last <= 0x7f);
        *last |= 0x80;
    }

	//assert(bytes[num_bytes-1] > 0x7f);

    size_t ret = write_from_buffer(devname, fd, (char*)bytes, 32*arity*batch_size/*size*/, 0/*offset*/);

	printf("Wrote %ld bytes to FPGA\n", ret);

	close(fd);

	return;
}




void *read_from_fpga(void *thr_args) {

	uint8_t *bytes = ((read_thr_args_t*)thr_args)->bytes;
	int batch_size = ((read_thr_args_t*)thr_args)->batch_size;

    char devname[] = "/dev/xdma0_c2h_0";
    int fd = open(devname, O_RDWR);
    if (fd < 0) {
		fprintf(stderr, "Unable to open device %s\n", devname);
        return 0;
    }

	size_t read_buffer_size = 32*batch_size;
	char *fpga_read_buffer;
	// posix_mealign + "4096" a must have, read error otherwise
	size_t ret = posix_memalign((void **)&fpga_read_buffer, 4096 /*alignment */ , read_buffer_size+4096);

	if (ret != 0) {
        	fprintf(stderr, "Failed to allocate read memory\n");
	}


	ret = read_to_buffer(devname, fd, fpga_read_buffer, read_buffer_size/*size*/, 0/*offset*/);
	memcpy(bytes, fpga_read_buffer, read_buffer_size);
	printf("Read %ld bytes from FPGA\n", ret);
	free(fpga_read_buffer);
	close(fd);

	return 0;
}



extern "C" {

	int hash_on_fpga(const char *preimages, char *results, int batch_size, int arity) {

#if 0
        printf("From rust batch size = %d arity =%d\n", batch_size, arity);
		int num_bytes = batch_size * arity * 32;
		for (int i = num_bytes-1, j = 1; i >= 0; i-=4, j++) {
			uint32_t preimage;	
			preimage = 0xff000000 & (uint8_t(preimages[i])<<24) | 0x00ff0000 & (uint8_t(preimages[i-1])<<16)
						| 0x0000ff00 & (uint8_t(preimages[i-2])<<8) | 0x000000ff & uint8_t(preimages[i-3]);
			printf("%8.8x", preimage);
			if (j % 8 == 0) { 
				printf(" %d\n", j);
			}
		}
#endif
        uint8_t domain_tag[32];

        char *domain_tag_str=0;

        if (arity == 2) {
            domain_tag_str = domain_tag_u2;
        } else if (arity == 8) {
            domain_tag_str = domain_tag_u8;
        } else if (arity == 11) {
            domain_tag_str = domain_tag_u11;
        }

        assert(domain_tag_str);
        int real_arity = arity + 1;

        for (int i = 63, j = 0; i>=0 ; i-=2, j++) {
            domain_tag[j] =  ascii_r((uint8_t)domain_tag_str[i-1], (uint8_t)domain_tag_str[i]);
        }

		char *real_preimages;
	
		posix_memalign((void **)&real_preimages, 4096 /*alignment */ , 32*real_arity*batch_size*sizeof(uint8_t) + 4096);
		assert(real_preimages);

        for (int i = 0; i < batch_size; i++) {
            int loc = i*32*real_arity;
            memcpy(real_preimages+loc, domain_tag, 32);
            memcpy(real_preimages+loc+32, preimages+i*32*arity, arity*32);
        }


		read_thr_args_t thr_args;
		thr_args.bytes = (uint8_t*)results;
		thr_args.batch_size = batch_size;

		pthread_t thr_id;
		pthread_create(&thr_id, NULL, read_from_fpga, (void*)&thr_args);

		write_to_fpga((uint8_t*)real_preimages, batch_size, real_arity);

		pthread_join(thr_id, NULL);

		free(real_preimages);

		return 1;
	}
}
