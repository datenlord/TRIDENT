# TRIDENT: A Hardware Implemented Poseidon Hasher

# Abstract:

Poseidon hasher is widely used in blockchain projects such as Filecoin, Mina Protocol, and Dusk Network for their zero-knowledge cryptographic proof, aka ZK-SNARK. This project aims to accelerate ZK-SNARK by implementing the Poseidon hasher on hardware.  To our knowledge, there was no open-source hardware implementation for Poseidon hasher.
Compare to traditional hashers such as SHA256 and SHA3, Poseidon operates directly on finite fields, therefore it is far more efficient when it comes to zero-knowledge proof. TRIDENT implemented the “Filecoin” version of Poseidon which operates on BLS12-381’s scalar field, but it can be modified to use on other elliptic curves as well.
We implemented TRIDENT as a block design in Vivado Design Suite 2021.2. The Poseidon hasher is written in Spinal-HDL, then converted to Verilog, and used as an IP in Vivado. Xilinx Varium C1100 Blockchain Accelerator Card is used in this project. We use the XDMA PCI-E IP in AXI4-Stream mode to write/read data to/from FPGA. Filecoin’s “Neptune” Rust API is modified to switch Poseidon hashing from on GPU to FPGA. And currently, TRIDENT has shown better performance than CPU implementation and better power consumption than GPUs. In general, TRIDENT provides a complete solution for accelerating Poseidon Hasher in FPGA, including hardware design and software API. Filecoin storage providers can conveniently deploy it in the sealing process of mining, which can improve efficiency vastly.
The remainder of our project documentation is organized as follows: In Section I, we will give a brief introduction to the Poseidon Hash function. And for Section II and Section III, we will illustrate details of our implementation including IP and system design. Section IV shows the detailed performance results of our implementation. And finally, we will share how to use the project repository and the deployment of TRIDENT.

# Section 1: A Brief Introduction Of Poseidon

The area of practical computational integrity proof systems, like zk-SNARK, is seeing a very dynamic development with several constructions having appeared recently with improved properties and relaxed setup requirements. Many use cases of such systems involve, often as their most expensive part, proving the knowledge of a preimage under a certain cryptographic hash function, which is expressed as a circuit over a large prime field. And Poseidon is a new hash function which hasfunctionigned to be friendly to zero-knowledge applications, specifically, in minimizing the proof generation time, the proof size, and the verification time.
For example, Poseidon hasher is used in the zero-knowledge proof system of FileCoin, an IPFS based decentralized storage network. The computation of Poseidon Hasher involves a large amount of compute-intensive modular multiplications, making it one of the performance bottlenecks in the mining process of FileCoin. Currently, GPU is often used to accelerate the computation process of Poseidon with greater power consumption. And our project, TRIDENT, is attempting to implement Poseidon hasher in FPGA to reach a better performance-power ratio than GPU.

The implementation of TRIDENT is specified for Filecoin’s Poseidon Instantiation, but it can be transformed and applied in other zero-knowledge systems using Poseidon easily.  And in the remainder of Section 1, we take Filecoin’s Poseidon Instantiation for example to show the details of the Poseidon hash function. 

The Poseidon hash function takes a preimage of (t-1) prime field elements to a single field element. For Filecoin, t can be 3, 5, 9, and 12, which means that the length of preimages can be 2, 4, 8, and 11 and each prime field element is 255-bit. Firstly, the preimage of Poseidon is initiated to the internal state of t prime field elements in different ways which are specified by the exact hash type. And then the internal state is transformed over R rounds of constant addition, S-boxes, and MDS matrix mixing. Once all rounds have been performed, Poseidon outputs the second element of the internal state. The data flow of Poseidon is shown in the picture below:

![poseidon01.JPG](TRIDENT%20A%20%206bf84/poseidon01.jpg)

 From the picture above, we can find that Poseidon has two kinds of rounds, which are partial round and full round. Poseidon calculates half of the full rounds first and then all of partial rounds and finally the remaining half of the full rounds.  And the only difference between the two is that: partial rounds only compute the first element of the internal state in SBox stages, but full rounds transform all elements through SBox. In the round constant addition stage, each prime field element is added by its corresponding round constant, and the constants are different in each round. Therefore, for the unoptimized Poseidon hash function, the total number of round constants is Rt. For Filecoin’s Poseidon instantiation, S-Box computes the fifth power of the state element. And in the MDS mixing stage, a vector-matrix multiplication is applied in the internal state, where the MDS matrix is t*t and consistent in every round.

In addition to the unoptimized Poseidon hasher shown above, there is also an optimized version of Poseidon Hasher which computes less multiplication in each hash. The data flow picture of optimized Poseidon is shown below. The main advantage of optimized Poseidon hasher over unoptimized one is that the constants matrix of partial round MDS mixing stages is sparse, which averts a lot of compute-intensive multiplications.

![poseidon02.JPG](TRIDENT%20A%20%206bf84/poseidon02.jpg)

![poseidon03.JPG](TRIDENT%20A%20%206bf84/poseidon03.jpg)

The hash results of optimized and unoptimized Poseidon for the same preimage are consistent. In TRIDENT, we implement both kinds of Poseidon hasher in FPGA and the optimized Poseidon can achieve higher throughput but with a more complicated hardware structure.

The Hardware Implementation of TRIDENT mainly includes two parts: Poseidon IP design and FPGA system design. And we will introduce details of these two parts in Section2 and Section3 respectively.

# Section2: Poseidon IP Design

In Section2, we will introduce the details of the design of the Poseidon accelerator IP. In general, the computation of Poseidon hasher can be perceived as a continuous stream of modular arithmetic operations. So the design of Poseidon IP is about two things: how to implement high performance-area ratio modular arithmetic circuits and then how to organize these arithmetic modules to achieve better utilization and throughput. Additionally, IP design and verification in TRIDENT are implemented through SpinalHDL and Cocotb, which improves the efficiency and quality of our design vastly. So we will first introduce the usage of SpinalHDL and Cocotb in TRIDENT.

## 2.1 Digital Design With [SpinalHDL](https://github.com/SpinalHDL/SpinalHDL) And [Cocotb](https://github.com/cocotb/cocotb)

 Spinal is a scala-based HCL(HCL: Hardware Construction Language) or more precisely a scala package featured in alige chip design, which is similar to chisel also based on scala and mostly used in RISC-V CPU design. The process of designing hardware in Spinal can be mainly divided into three steps: 1) use scala and Spinal package to describe the structure and logic of your hardware design; 2) compile and execute the scala program to generate corresponding System Verilog/VHDL codes, which describes the same structure as scala one; 3) using any kinds of simulators such as Iverilog, Verilator or Vivado simulator for hardware simulation and verification. A design flow of SpinalHDL is shown in the picture below:

![figure1-第 2 页.jpg](TRIDENT%20A%20%206bf84/figure1-%E7%AC%AC_2_%E9%A1%B5.jpg)

 Spinal has various advantages over traditional HDLs and it can ease the design of digital hardware. However, unlike high-level language, it eases and expedites the design process without sacrificing the performance and resources utility of generated hardware. SpinalHDL has almost the same level of precision or granularity of description as traditional HDLs like Verilog or VHDL. It can control the amount of registers and the length of logical path between registers finely. The best proof of approximate description granularity is that: for all RTL-level syntax elements in Verilog/VHDL, SpinalHDL has a counterpart.

Besides the same description level as Verilog, SpinalHDL is more expressive than Verilog. Developers can use abundant advanced language features of scala, like functional programming, object-oriented, and recursion, to describe their hardware design. Taking the realization of the adder tree module used in TRIDENT, we can use the recursion feature in scala to facilitate it. The structure and scala codes of the adder tree are shown below. The adder tree is used in the MDS vector-matrix multiplication, which takes 12 finite field elements as input and outputs the addition result of them. Using recursion, we can easily describe this structure and reuse the code to generate an adder tree with any amount of input elements, which is impossible to realize in Verilog. 

![adderTree.JPG](TRIDENT%20A%20%206bf84/adderTree.jpg)

```scala
object AdderTreeGenerator{
  def apply(opNum:Int, dataWidth:Int, input:Vec[UInt]):(UInt,Int) = {
    if(opNum == 2) {
      (ModAdderPiped(input(0), input(1)), ModAdderPiped.latency)
    }
    else {
      val adderOutputs = for(i <- 0 until opNum/2) yield
        ModAdderPiped(input(2*i),input(2*i+1))
      if(opNum%2==0){
        val next = Vec(UInt(dataWidth bits),opNum/2)
        next.assignFromBits(adderOutputs.asBits())
        val (nextOutput, latency) = AdderTreeGenerator(opNum/2, dataWidth, next)
        (nextOutput, latency+2)
      } else{
        val next = Vec(UInt(dataWidth bits),opNum/2+1)
        val temp = Delay(input(opNum-1), ModAdderPiped.latency)
        next.assignFromBits(temp.asBits##adderOutputs.asBits())
        val (nextOutput,latency)= AdderTreeGenerator(opNum/2+1, dataWidth, next)
        (nextOutput, latency+2)
      }
    }
  }
}
```

In addition, Spinal has many good encapsulations or abstractions of some classic circuits, like Counter, StateMachine, FIFO, which are frequently used in digital hardware design. Using these abstractions can reduce the workload of designers and the possibility of error during development. The most used circuit in TRIDENT is Stream in SpinalHDL. Stream class implements the handshake protocol in digital design, which includes three signals: valid, ready, and payload. Calling the member methods of Stream class, we can realize different data transfer operations under handshake protocol. For example, we can use the stage() function in the Stream class to implement a Pipeline with backpressure.

Cocotb is a python-based testbench environment for verifying VHDL, Verilog, and SystemVerilog RTL designs. The most important advantage of Cocotb is that: Python is far more productive, expressive and succinct than traditional languages used for verification like Verilog, VHDL, and System Verilog. At the same time, we can build a reference model more quickly and conveniently based on python’s abundant package and community. For example, In TRIDENT, we also implement Poseidon haser in python, which is used as the golden model for verification. One of the difficulties in realizing it in software is that variables in Poseidon hasher are 255-bit and are usually unsupported in other program languages. But python can realize computations of Integers with any width, which facilitates the implementation greatly.

Besides the points we mentioned above, SpinalHDL and Cocotb still have a lot of advantages over traditional HDL in digital design and verification, which we believe will simplify FPGA design and promote the application of FPGA in many fields. But currently, SpinalHDL and Cocotb haven’t been fully supported by Vivado, for example, we can’t run cocotb testbench in Vivado simulator and it’s also troublesome to instantiate Xilinx IP in SpinalHDL. So we hope that Vivado and other Xilinx design tools can better support SpinalHDL and Cocotb in the future. 

## 2.2 Modular Arithmetic Operator

It’s worth noting that Poseidon Hasher operates elements in finite field, which means all arithmetic operations are modular. Compared to normal arithmetic, modular arithmetic operations are more complicated in circuit implementation and lack existing mature IP to use. There are two kinds of modular operations in Poseidon Hasher, that is modular addition and modular multiplication. In TRIDENT, we implement these two kinds of circuits based on the existing adder and multiplier provided by Vivado.

### Modular Adder:

The modulo m addition of two numbers x and y is defined by:

![modadd.JPG](TRIDENT%20A%20%206bf84/modadd.jpg)

The regular modular addition can be straightforwardly implemented by a normal adder, a comparator, and a subtracter. The output of the adder is compared with the modulus to decide whether it exceeds the modulus and needs to be subtracted by the modulus. But this implementation is expensive both in terms of area and delay. In TRIDENT, we implement the modular adder through the algorithm below:

![modadder2.JPG](TRIDENT%20A%20%206bf84/modadder2.jpg)

In this algorithm, we just need two adders and a Mux to realize the function of modular addition. The circuit structure is shown in the picture below.

![modadder.JPG](TRIDENT%20A%20%206bf84/modadder.jpg)

And because the width of elements in Poseidon is 255 and 255-bit addition causes long logic delay, we implement the 255-bit addition through Xilinx Adder/Sub IP in which we can specify the number of pipestages in an adder. The configuration of Adder IP is:

![Screenshot from 2022-04-01 15-06-18.png](TRIDENT%20A%20%206bf84/Screenshot_from_2022-04-01_15-06-18.png)

## Modular Multiplication:

The implementation of modular multiplication adopts the Montgomery reduction algorithm which avoids the division in modular multiplication and Karatusba-Ofman multiplication which use less low-bit multipliers to get a high-bit multiplier. The Montgomery Algorithm:

![mont.JPG](TRIDENT%20A%20%206bf84/mont.jpg)

In Montgomery Algorithm we can replace the compute-intensive division with shifting and we only need some normal multipliers to get a modular multiplier. But the 255-bit multiplier can’t be accessed directly through multiplication notation, so we need to combine low-bit multiplier to get a 255-bit multiplier first. In TRIDENT, we use Karatusba-Ofman Algorithm to complete this:

![ag.JPG](TRIDENT%20A%20%206bf84/ag.jpg)

In TRIDENT, we build 255-bit multiplier from 32-bit Xilinx Multiplier IP, the structure is as below:

![basic.JPG](TRIDENT%20A%20%206bf84/basic.jpg)

Based on 255-bit multiplier, we build the modular multiplier following the structure below:

![mont2.JPG](TRIDENT%20A%20%206bf84/mont2.jpg)

## 2.2 Accelerator Architecture

The main two design ideas of accelerating the computation of *Poseidon* hash function in this accelerator are parallelization and pipeline. Each element of the internal state can be computed in each round independently before matrix adder, which needs to add up the multiplication result of each element. In order to make full use of hardware resource when computing each king length of internal state,   the parallelism is chosen to be 3, which is the common factor of 3,9,12 and for the state of 5 elements, only one channel of computation resource will be wasted. Besides the hardware design used pipeline trick to compute more internal states simultaneously and increase the working frequency of the circuit. The whole architecture of the accelerator is as below:

![TopLevel.drawio.png](TRIDENT%20A%20%206bf84/TopLevel.drawio.png)

The accelerator uses AXI4-Stream interfaces to communicate with outsiders. ***AXI4StreamReceiver*** is in charge of receiving data from outsiders and parallelizing the serial input to the degree of parallelism of 3 for parallel computation. Correspondingly,  ***AXI4StreamTransmitter*** is designed to transmit the final result of one prime field element to outsider.  ***DataProcess*** area consists of all computation modules in the accelerator, including ***thread*** in charge of independent computation which can be executed parallelly and ***MDSMatrixAdder*** is used to add up multiplication result of each element in matrix multiplication and get a complete internal state.  ***DataLoopbackBuffer*** is designed to serialize the complete internal state from ***MDSMatrixAdder*** to the degree of parallelism of 3 when the length of internal state is 5, 9 or 12. When all computation rounds are completed, ***DataDeMux*** is designed to transmit the internal state to ***AXI4StreamTransmitter***, If not completed the state is transmitted to ***DataLoopbackBuffer*** to start the next round. ***DataMux*** in the top of the accelerator is a priority mux which chooses loopback data when both of the inputs are valid.

# Section3: FPGA System Design

The overall architecture block diagram of the TRIDENT system in the project is shown in the figure below. In the CPU part, TRIDENT provides a rust API for lotus, a popular Filecoin node implementation, to send to and receive data from FPGA and the API is adapted from the Xilinx XDMA driver. And the FPGA hardware part is mainly composed of three modules: Xilinx XDMA IP, asynchronous FIFO, Poseidon accelerator IP; Among them, XDMA provides a unified PCIe interface for the upper CPU Server, and provides a unified AXI-Stream interface for the hardware design in FPGA. The working frequency of xdma IP, 250MHz, is higher than the frequency of Poseidon IP ranging from 100MHz to 200MHz. So asynchronous FIFO is in charge of cross-clock domain data transfer from XDMA to the accelerator. And finally, the Poseidon accelerator IP with standard AXI4-Stream interface is the computation core of the hardware system and is responsible for the acceleration of Hash functions.

![schematic-第 2 页.jpg](TRIDENT%20A%20%206bf84/schematic-%E7%AC%AC_2_%E9%A1%B5.jpg)

We implement the whole FPGA hardware system design through the Block design tool in Vivado.The block design diagram which shows the connections between different modules is as below:

![Screenshot from 2022-04-01 09-44-51.png](TRIDENT%20A%20%206bf84/Screenshot_from_2022-04-01_09-44-51.png)

The specific Xilinx IP configuration is shown in the pictures below:

 **Xilinx XDMA Configuration:**

- Basic Tab:

![Screenshot from 2022-04-01 09-46-32.png](TRIDENT%20A%20%206bf84/Screenshot_from_2022-04-01_09-46-32.png)

- PCIe ID Tab:

![Screenshot from 2022-04-01 09-47-15.png](TRIDENT%20A%20%206bf84/Screenshot_from_2022-04-01_09-47-15.png)

- PCIe BARs Tab:

![Screenshot from 2022-04-01 09-47-50.png](TRIDENT%20A%20%206bf84/Screenshot_from_2022-04-01_09-47-50.png)

- PCIe: MISC Tab:

![Screenshot from 2022-04-01 09-48-20.png](TRIDENT%20A%20%206bf84/Screenshot_from_2022-04-01_09-48-20.png)

- PCIe: DMA Tab:

![Screenshot from 2022-04-01 09-48-47.png](TRIDENT%20A%20%206bf84/Screenshot_from_2022-04-01_09-48-47.png)

**Clock Wizard Configuration:** 

![Screenshot from 2022-04-01 09-49-31.png](TRIDENT%20A%20%206bf84/Screenshot_from_2022-04-01_09-49-31.png)

**Asynchronous FIFO Configuration:**

General Tab:

![Screenshot from 2022-04-01 09-49-56.png](TRIDENT%20A%20%206bf84/Screenshot_from_2022-04-01_09-49-56.png)

Flags Tab:

![Screenshot from 2022-04-01 09-50-15.png](TRIDENT%20A%20%206bf84/Screenshot_from_2022-04-01_09-50-15.png)

# Section3: Performance Comparation

## 3.1 Implementation Results:

The implementation result and performance of TRIDENT are shown in this Section. After implementation, the FPGA hardware resource utilization is shown in the table below. The shortage of DSP slices and LUTs used in modular multipliers is the main bottleneck in the design to achieve better performance.

|  | Usage | Total | Percentage |
| --- | --- | --- | --- |
| CLB LUT | 533050 | 871680 | 61.15% |
| CLB Registers | 835492 | 1743360 | 47.92% |
| LUT as Memory | 87548 | 403200 | 21.71% |
| Block Ram Tile | 61.5 | 1344 | 5% |
| DSP slices | 4108 | 5952 | 70.01% |
| Carry8 | 54657 | 108960 | 50.16% |
| F7 Muxes | 1213 | 435840 | 0.2% |
| F8 Muxes | 37 | 217920 | 0.01% |

**Implementation Results:** 

![device.png](TRIDENT%20A%20%206bf84/device.png)

**Timing Information:** 

## 3.2 Performance Of TRIDENT

In TRIDENT, we use two ways to measure the performance of the whole system. The first one: we write a C program, which directly sends and receives hash results from FPGA, and calculates the whole duration time from sending the first preimage to the reception of the last hash result. The exact performance of TRIDENT in three kinds of length preimages is shown below. For arity2, TRIDENT’s data transfer rate can reach 29.1651 MB/s which means in one second it can finish the computation of approximately 1M hashes.

|  | preimage num | hash duration(s) | transfer rate(MB/s) | hash rate(hash/s) |
| --- | --- | --- | --- | --- |
| arity 2 | 850000 | 0.877 | 29.1651 MB/s | 0.991 Mhash/s |
| arity 8 | 250000 | 0.697 s | 11.4776 MB/s | 358 Khash/s |
| arity 11 | 100000 | 0.328 s | 9.75053 MB/s | 305 Khash/s |

The second one: as we mention above, TRIDENT also provides a Rust-based software API for Lotus, a Filecoin node implementation, so that Lotus can use the FPGA to accelerate the sealing process as the usage of GPU. And Lotus has a specific benchmark program, Lotus-bench, which can measure the performance of different computation processes in Filecoin mining precisely. We use lotus-bench to test the performance of GPU, CPU, and TRIDENT in the sealing process which needs Poseidon hashers. The result is shown below:

|  | Model | Sealing Sector Size | Duration | Transfer Rate  |
| --- | --- | --- | --- | --- |
| GPU | Nvidia RTX 3070 | 512 MiB | 10.855 s | 47.17 MiB/s |
| CPU | AMD Ryzen 5900 | 512 MiB | 40.723 s | 7.41 MiB/s |
| TRIDENT | Xilinx Varium C1100 | 512 MiB | 32.713 | 15.65 MiB/s |

In the performance results above, we can see that TRIDENT‘s hash rate can achieve more than two times of CPU’s rate but still fall behind GPU‘s hash speed. But given the great power consumption of GPU, TRIDENT can still have a better performance-power ratio. And the power consumption of TRIDENT is shown in the picture below: 

![power.png](TRIDENT%20A%20%206bf84/power.png)

We can see that the total On-chip power of FPGA is 24.823W and the maximum power consumption in the specification of RTX 3070 revealed on Nvidia’s official website is 220W. It’s obvious that FPGA is much more efficient than GPU in terms of performance-power ratio.

Although TRIDENT is currently weaker than GPU in performance, we have seen a lot of room for improvement. And we perceive that the improvement mainly comes from three aspects: 1) optimizing the timing of the current design to reach a higher frequency; 2) innovating the structure of the arithmetic modules to achieve a better performance-area ratio; 3) modifying the architecture of the accelerator IP and the whole FPGA system to improve the utilization of arithmetic modules. In the future, we will still work on this project and we believe that TRIDENT can reach the performance much better than GPU.

# Section 4: Instructions

In this section, we will introduce the repository of TRIDENT and how to deploy TRIDENT in the Filecoin application.

## 4.1 TRIDENT Repository

The file directory structure of TRIDENT is shown below:

```bash
├── build.sc
├── images
│   ├── AdderBasedModAdder.drawio.png
│   ├── MDSMatrixAdder.drawio.png
│   ├── MDSMatrixMultiplier.drawio.png
│   ├── ModMultiplier.drawio.png
│   ├── src
│   ├── Thread.drawio.png
│   └── TopLevel.drawio.png
├── LICENSE
├── poseidon_constants
│   ├── compressed_round_constants
│   ├── compressed_round_constants_ff
│   ├── mds_matrixs
│   ├── mds_matrixs_ff
│   ├── pre_sparse_matrix
│   ├── pre_sparse_matrix_ff
│   ├── round_constants
│   ├── round_constants_ff
│   ├── sparse_matrix
│   └── sparse_matrix_ff
├── README.md
├── run.sh
├── src
│   ├── main
│   ├── reference_model
│   ├── software_design
│   └── tests
└── vivado_project
    ├── poseidon_ip.tar.gz
    ├── poseidon.tar.gz
    └── trident100MhzC1100.tar.gz
```

- build.sc: the configuration of mill, a scala building tool, in which we can set the dependent library of SpinalHDL and some scala compile options;
- poseidon_constants: includes all constants used in both optimized and unoptimized Poseidon Hasher in *.txt file format;
- run.sh: the shell script which you can use to generate Verilog codes from SpinalHDL and execute the verification process;
- src: contains all source codes files in TRIDENT, including hardware design codes in “main”, a reference model of Poseidon in Python in “reference_model”, software API and performance test C-program in “software_design” and cocotb verification codes in “tests”;
- vivado_project: includes the Poseidon IP vivado project, whole TRIDENT system project and a generated .bit file which can be directly deployed in Varium C1100 FPGA;

## 4.2 Deployment

You mainly have three ways of deploying the TRIDENT: 

- Use the .bit file in “vivado_project “ and execute it in Xilinx Varium C1100 FPGA Card, and then you need to connect the card with PC server through PCIe interface. You can use Xilinx official XDMA driver to interact with FPGA directly and we also provide a C-program in “/src/software design/trident_tester” that you can refer to. At the same time, we also produce a rust API in “/src/software design/lotus” which can be used in Lotus to accelerate the sealing process of mining.
- Besides using the .bit file, you can also edit the Vivado project we provide to customize your own FPGA design in the Varium C1100 card. For example, you can replace the PCIe interface with another communication protocol or you can add the existing Poseidon Vivado IP in another block design.
- And if you want to deploy TRIDENT in another FPGA platform or you want to modify the design of Poseidon IP, you can also edit our design in SpinalHDL and then generate Verilog codes using “mill” or the script we provide. For example, you can replace AXI4-Stream with AXI-Lite or AXI-full and add Poseidon IP in your SoC design.