HowTo use FPGA for Poseidon hashing:

1. Replace gpu.rs with the given one in your ~/.cargo/registry/src/github.com-.../neptune-5.1.0/src/proteus directory.
2. In your "lotus" directory, eg. ~/lotus, do "make clean && make all", ignore the "undefined reference to `hash_on_fpga'" error. 
3. Do "gcc -fPIC -c fpga.cpp -Wall && ar -rsc ~/lotus/extern/filecoin-ffi/libfilcrypto.a fpga.o"
4. Then "make all && make lotus-bench"
5. Try "./lotus-bench sealing --sector-size 512MiB", FPGA Poseidon hashing will kick in.


By default we use /dev/xdma0_c2h_0 and /dev/xdma0_h2c_0 to read/write FPGA.
