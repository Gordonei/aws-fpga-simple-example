# AWS FPGA HLS Simple Example
This is a simple vector sum example that uses SDx to generate an AFI for the Amazon F1 instance type. The objective is provide a clear, self-contained example of building and running custom logic from OpenCL code.

## Sources
* [AWS SDAccel Readme](https://github.com/aws/aws-fpga-preview/blob/master/sdk/SDAccel/README.md)
* [AWS HDK Readme](https://github.com/aws/aws-fpga/tree/master/hdk/cl/examples)
* [Xilinx SDAccel Vector Sum Example](https://github.com/Xilinx/SDAccel_Examples/tree/d997c8aba85b28c0f82b2084306fee672b5428d3/getting_started/basic/vadd)
* [Simple start with OpenCL and C++](http://simpleopencl.blogspot.co.za/2013/06/tutorial-simple-start-with-opencl-and-c.html)

## Building Hardware
### Setting up the Developer Instance
1. Launch the [FPGA Developer AMI](https://aws.amazon.com/marketplace/pp/B06VVYBLZZ):
  * The user name is `centos`.
  * The AMI-ID for 1.3.0_a in eu-west-1 (Ireland) is `ami-f553ba8c`
  * You can use any instance with > 32 GB of RAM. I find the R4.2xlarge tends to be the cheapest on the spot market.
2. Clone the [aws-fpga-preview](https://github.com/aws/aws-fpga-preview) and this repo:
  * aws-fpga-preview: `git clone git@github.com:aws/aws-fpga-preview.git $AWS_FPGA_REPO_DIR`
  * You'll need probably need to be authed with Github if either repo is private. The simplest is to generate a SSH key and then run a SSH agent with it loaded.
3. Setup the SDK: `cd $AWS_FPGA_REPO_DIR && source sdk_setup.sh && cd sdk/SDAccel && source sdaccel_setup.sh`
  * In addition to setting up some useful environmental variables, it downloads the latest SDAccel board DSA. 
  * Some of the paths are relative to the directories concerned, hence the directory changes.
3. Configure the AWS profile on the instance: `aws configure`
  * Currently, the AWS FPGA infrastructure seems to only be operational in 'us-east-1', hence the us-east-1 endpoints should be used, i.e. region should be `us-east-1`.
4. Optional Extras: `sudo yum install htop vim lsof`

### Building the Design and the Host Code
1. Change to the example repo's root directory: `cd $OPENCL_EXAMPLE_REPO`
1. Build the FPGA logic: 
  * Building .xo file: `OPENCL_FILE=<OpenCL file name> eval 'xocc -c --xp "param:compiler.preserveHlsOutput=1" --xp "param:compiler.generateExtraRunData=true" -s -o build/$OPENCL_FILE.xo -t hw --platform $AWS_FPGA_REPO_DIR/aws_platform/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0.xpfm src/$OPENCL_FILE.cl'`
  * Building the xclbin (this takes a long time): `OPENCL_FILE=<OpenCL file name> eval 'xocc -l --xp "param:compiler.preserveHlsOutput=1" --xp "param:compiler.generateExtraRunData=true" -s -o build/$OPENCL_FILE.xclbin -t hw --platform $AWS_FPGA_REPO_DIR/aws_platform/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0/xilinx_aws-vu9p-f1_4ddr-xpr-1pr_4_0.xpfm build/$OPENCL_FILE.xo'`
2. Create the AFI: `OPENCL_FILE=<OpenCL file name> S3_BUCKET=<S3 bucket name>  eval '$AWS_FPGA_REPO_DIR/tools/create_sdaccel_afi.sh -s3_bucket=$S3_BUCKET -s3_dcp_key=$OPENCL_FILE -s3_logs_key=$OPENCL_FILE\_logs -xclbin=build/$OPENCL_FILE.xclbin -o=build/$OPENCL_FILE'`
  * The AFI and AFGI can be found by: `cat *_afi_id.txt`
  * The status of the AFI can be described: `aws ec2 describe-fpga-images --fpga-image-ids <AFI ID>`   
3. Build the Host code: `HOST_FILE=<Host code file name> eval '$XILINX_SDX/Vivado_HLS/lnx64/tools/gcc/bin/g++ -Wall -o build/$HOST_FILE src/$HOST_FILE.cpp -L$XILINX_SDX/runtime/lib/x86_64 -L$XILINX_SDX/lib/lnx64.o -lOpenCL -pthread'`

## Running the Code
### Setting up the Runtime Instance
1. Launch a F1-instance
   * In theory this could be any OS, but for simplicity's sake use the developer AMI: `ami-df3e6da4` in us-east-1
2. Setup AWS creds: `aws configure`
3. Setup the SDK: 
   1. Pull in aws-fpga-preview: `git clone git@github.com:aws/aws-fpga-preview.git $AWS_FPGA_REPO_DIR`
   2. Switch to the `develop` branch: `git checkout develop`
   3. Setup the SDK: `source sdk_setup.sh`
   4. Setup up the SDAccel OpenCL driver: `cd sdk/SDAccel && source sdaccel_setup.sh`
4. Optional Extras: `sudo yum install htop vim lsof`
### On real hardware
1. Load the AFI onto the FPGA:
   1. Clear the slot: `sudo fpga-clear-local-image  -S 0`
   2. Check that it has been cleared: `sudo fpga-describe-local-image -S 0 -H`
   3. Load the AFI: `sudo fpga-load-local-image -S 0 -I <AFGI of image>` 
     * ***NB that this is the *AFGI*, not the *AFI* ***
   4. Check that it has been loaded: `sudo fpga-describe-local-image -S 0 -R -H`
2. Change to a root bash shell: `sudo bash`
   * I'm not sure, but I think this is because root access is required to access the device
3. Change to the build directory of this repo: `cd $OPENCL_EXAMPLE_REPO/build`
4. Setup up the runtime environment: `source /opt/Xilinx/SDx/2017.1.rte/setup.sh`
5. Run the code: `HOST_FILE=<Host code file name> eval './$HOST_FILE'`

### Software Emulation
1. Setup the Xilinx OpenCL emulation environment:
   1. Setup the Xilinx environment: `source /opt/Xilinx/SDx/2017.1.op/settings64.sh`
   2. Set the emulation environmental variable: `export XCL_EMULATION=sw_emu`
   3. If you haven't, setup the AWS_FPGA_REPO_DIR: `cd $AWS_FPGA_REPO_DIR && source sdk_setup.sh && cd sdk/SDAccel && source sdaccel_setup.sh`
2. Build the FPGA logic with the *sw_emu* target:
   * Building .xo file: `OPENCL_FILE=<OpenCL file name> eval 'xocc -c --xp "param:compiler.preserveHlsOutput=1" --xp "param:compiler.generateExtraRunData=true" -s -o build/$OPENCL_FILE\_emu.xo -t sw_emu --platform $AWS_FPGA_REPO_DIR/aws_platform/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0.xpfm src/$OPENCL_FILE.cl'`
   * Building the xclbin: `OPENCL_FILE=<OpenCL file name> eval 'OPENCL_FILE=sum eval 'xocc -l --xp "param:compiler.preserveHlsOutput=1" --xp "param:compiler.generateExtraRunData=true" -s -o build/$OPENCL_FILE\_emu.xclbin -t sw_emu --platform $AWS_FPGA_REPO_DIR/aws_platform/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0.xpfm build/$OPENCL_FILE\_emu.xo`
3. Build the host code as above: `HOST_FILE=<Host code file name> eval '$XILINX_SDX/Vivado_HLS/lnx64/tools/gcc/bin/g++ -Wall -o build/$HOST_FILE\_emu src/$HOST_FILE.cpp -L$XILINX_SDX/runtime/lib/x86_64 -L$XILINX_SDX/lib/lnx64.o -lOpenCL -pthread'`
4. Setup the emulation configuration: `cd build && emconfigutil -f $SDK_DIR/SDAccel/aws_platform/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0/hw/xilinx_aws-vu9p-f1_4ddr-xpr-2pr_4_0.dsa --nd 1`
5. Run the code: `HOST_FILE=<Host code file name> eval './$HOST_FILE\_emu'`
