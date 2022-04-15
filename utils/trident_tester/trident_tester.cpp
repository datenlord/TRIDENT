#include "trident_tester.h"

/*

README:
*_inputs.txt: inputs that contain arity = 3, 9, and 12 preimages
*_ouputs.txt: poseidon hashing results(for comparison)

Both inputs/outputs are in Montgomery domain, R=256

By default we use /dev/xdma0_h2c_0 and /dev/xdma0_c2h_0 to communicate with FPGA

To compile, use:
g++ -o trident_tester trident_tester.cpp -lpthread

An example run: (arity=3 and num_of_transfers=10)

./trident_tester 10 3 arity_3_inputs.txt arity_3_outputs.txt

To skip comparison with "outputs", use:

./trident_tester 10 3 arity_3_inputs.txt

*/



std::mutex LogStream::_ls_mux;

//helper function
static uint8_t fromAscii(uint8_t c)
{
  if (c >= '0' && c <= '9')
    return (c - '0');
  if (c >= 'a' && c <= 'f')
    return (c - 'a' + 10);
  if (c >= 'A' && c <= 'F')
    return (c - 'A' + 10);

#if defined(__EXCEPTIONS) || defined(DEBUG)
  throw std::runtime_error("invalid character");
#else
  return 0xff;
#endif
}

//helper function
static uint8_t ascii_r(uint8_t a, uint8_t b)
{
  return (fromAscii(a) << 4) + fromAscii(b);
}


// convert string to bytes
void process_preimages(const std::vector<std::string> & data, 
	std::vector<std::string> & processed)
{
	size_t len = data.size();	

	for (int i = 0;i < len; i++) {
		auto & str = data[i];

		std::string new_str;
		for (int j = 0; j < str.size(); j+=2) {
			new_str += char(ascii_r(str[j], str[j + 1]));
			
		}
		processed.emplace_back(new_str);
	}
}



size_t batch_write_data(const std::vector<std::string> & data, int arity)
{
	size_t len = data.size();
	std::cout<<"# of inputs : "<< len << " arity = "<< arity<<"\n";

	if (len == 0) return 0;


	if (len % arity != 0) return 0;

	char devname[] = "/dev/xdma0_h2c_0";
    int fpga_fd = open(devname, O_RDWR);

    if (fpga_fd < 0) {
		log() << "Unable to open device "<< devname <<"\n";
		return 0;
	}

	char *fpga_write_buffer;
	posix_memalign((void **)&fpga_write_buffer, 4096 /*alignment */ , 32*len + 4096);

	size_t j = 0;
	for (int k = 0; k < len; k++) {
		auto & hex = data[k];
		assert(hex.length() == 32);
		for (int i = hex.length()-1; i >= 0; i--, j++) {
			uint8_t byte = (uint8_t)hex[i];
			if (k%arity == arity - 1 && i == 0) {
				// set MSB to 1 to indicate its the last data in preimages
				assert(byte <= 0x7f);
				byte |= 0x80;
			}
			fpga_write_buffer[j] = byte;
		}
	}
	Timer t;
	auto ret = write_from_buffer(devname, fpga_fd, fpga_write_buffer, 32*len/*size, 32bytes = 256bits*/, 0/*offset*/);


	close(fpga_fd);
	free(fpga_write_buffer);


	log() <<"Wrote "<<ret<<" bytes to FPGA ";
	log() << "Time elapsed: " << t.elapsed() << " seconds. ";
	log() << "Write transfer rate = "<< ret/1000000.0/t.elapsed()<<" MB/s\n";
	return ret;

}



void batch_receiver(int num_preimages, const std::vector<std::string> & outputs)
{
	char devname[] = "/dev/xdma0_c2h_0";
	int fpga_fd = open(devname, O_RDWR);
	if (fpga_fd < 0) {
		log() << "Unable to open device "<< devname <<"\n";
		return;
	}

	bool has_output = false;
	if (outputs.size() > 0) {
		has_output = true;
	}

	size_t read_buffer_size = 32*num_preimages ;


	char *fpga_read_buffer;
	int ret = posix_memalign((void **)&fpga_read_buffer, 4096 /*alignment */ , read_buffer_size+4096);

	if (ret == 0) {
		log()<<"Sucessfully allocated "<<read_buffer_size+4096<<" bytes of read memory\n";
	}


	Timer t;
	size_t total = 0;

	total = read_to_buffer(devname, fpga_fd, fpga_read_buffer, 32*num_preimages/*size*/, 0/*offset*/);
	printf("%ld bytes READ from FPGA\n", total);

	double runtime = t.elapsed();
	log() << "Time elapsed: " << runtime << " seconds ";
	log() << "Read transfer rate = "<<total/1000000.0/runtime<<" MB/s\n";

	if (has_output) {
		char buf[32];
		std::string hash("");
		int hash_num = 0;
		for (int i = 0; i < total; i++) {
			sprintf(buf, "%2.2x", (uint8_t)fpga_read_buffer[i]);
			hash=buf + hash;
			if (i%32 == 31) { 
				auto & output_hash = outputs[hash_num%outputs.size()];
				if (hash != output_hash) {
					log() << "Error Hash don't match!\n";
					log() << "Hash = "<< hash <<" Output(Golden) = "<<output_hash<<"\n";
				}
				hash_num++;
				hash = "";
			} 


		}	
	}
		

	log() << "Num of Poseidon hashes: "<<num_preimages<<"\n";

	free(fpga_read_buffer);
	return;
}



int main(int argc, char *argv[])
{

	if (argc < 4 || argc > 5) { 
		std::cout<<"Usage: "<< argv[0] << " transfer_num arity input_file [output_file]\n";
		return -1;
	}

	int arity = atoi(argv[2]);
	if (!(arity == 3 || arity == 9 || arity == 12)) {
		std::cerr<<"arity " << arity <<" is not supported.\n";
		return -1;
	}

	int transfer_num = atoi(argv[1]);

	std::ifstream infile;
	infile.open(argv[3]);
    	if (!infile) {
        	std::cout<<"Input file cannot be opened!"<<std::endl;
		return -1;
	}

	bool has_output = false;
	std::ifstream output_file;
	if (argc == 5) {
		output_file.open(argv[4]);
		if (!output_file) {
			std::cout<<"Output file cannot be opened!"<<std::endl;
		} else {
			has_output = true;
		}
	}

	if (!has_output) {
		std::cout<<"No output file found, skip comparison."<<"\n";
	}


	std::vector<std::string> preimages;
	std::vector<std::string> processed;
	std::string word_line;

	while (infile >> word_line) {
		transform(word_line.begin(), word_line.end(), word_line.begin(), ::tolower);
		preimages.push_back(word_line);
	} 

	process_preimages(preimages, processed);

	std::vector<std::string> outputs;

	if (has_output) {
		while (output_file >> word_line) {
			transform(word_line.begin(), word_line.end(), word_line.begin(), ::tolower);
			outputs.push_back(word_line);
		}
	}

	std::thread thread(batch_receiver, transfer_num*processed.size()/arity, outputs);
	for (int i = 0; i < transfer_num; i++) batch_write_data(processed, arity);
	
	
	thread.join();

	return 0;
};
