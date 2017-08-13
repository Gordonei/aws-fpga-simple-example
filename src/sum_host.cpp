/**********
 * Copyright (c) 2017, Xilinx, Inc. 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * **********/

/***********
 * Copyleft 2017, Gordon Inggs
 * I really don't mind what you do with this code.
 * I would appreciate that if you change it, that you mention that you have.
 ***********/
#pragma once

#define LENGTH (1024)
#define NUM_WORKGROUPS (1)
#define WORKGROUP_SIZE (LENGTH/WORKGROUP_SIZE)

#include <CL/cl.hpp>
#include <vector>
#include <iostream>
#include <fstream>

using namespace std;

std::vector<cl::Device> get_devices(const std::string& vendor_name) {
    size_t i;
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);
    cl::Platform platform;
    for (i  = 0 ; i < platforms.size(); i++){
        platform = platforms[i];
        std::string platformName  = platform.getInfo<CL_PLATFORM_NAME>();
        std::cout << "Platform Name: '" << platformName << "'" << std::endl
                  << "Vendor Name : '" << vendor_name << "'" << std::endl;
        if (platformName.compare(0,vendor_name.length(),vendor_name) == 0){
            std::cout << "Found Platform" << std::endl;
            break;
        }
    }
    if (i == platforms.size()) {
        std::cout << "Error: Failed to find Xilinx platform" << std::endl;
		exit(EXIT_FAILURE);
	}
   
    //Getting ACCELERATOR Devices and selecting 1st such device 
    std::vector<cl::Device> devices;
    platform.getDevices(CL_DEVICE_TYPE_ACCELERATOR, &devices);
    return devices;
}
   
std::vector<cl::Device> get_xil_devices() {
	return get_devices("Xilinx");
}

cl::Program::Binaries import_binary_file(std::string xclbin_file_name) { 
    std::cout << "INFO: Importing " << xclbin_file_name << std::endl;

	if(access(xclbin_file_name.c_str(), R_OK) != 0) {
		//printf("ERROR: %s xclbin not available please build\n", xclbin_file_name.c_str());
		exit(EXIT_FAILURE);
	}
    //Loading XCL Bin into char buffer 
    std::cout << "Loading: '" << xclbin_file_name.c_str() << "'\n";
    std::ifstream bin_file (xclbin_file_name.c_str(), std::ifstream::binary);
    bin_file.seekg (0, bin_file.end);
    unsigned nb = bin_file.tellg();
    bin_file.seekg (0, bin_file.beg);
    char *buf = new char [nb];
    bin_file.read(buf, nb);
   
    cl::Program::Binaries bins;
    bins.push_back({buf,nb});
    return bins;
}

#define ALIGNED(x) __attribute__((aligned(x)))
#define ALIGNMENT_SIZE 64 

int main(int argc, char* argv[]) {
    size_t data_size = sizeof(int) * LENGTH;
    int source_a    [LENGTH] ALIGNED(ALIGNMENT_SIZE);
    int source_b    [LENGTH] ALIGNED(ALIGNMENT_SIZE);
    int result_sim  [LENGTH] ALIGNED(ALIGNMENT_SIZE);
    int result_krnl [LENGTH] ALIGNED(ALIGNMENT_SIZE);

    /* Create the test data and run the vector addition locally */
    for(int i=0; i < LENGTH; i++){
        source_a[i] = i;
        source_b[i] = 2*i;
        result_sim[i] = source_a[i] + source_b[i];
    }

    /* Setting up the device and context*/
    std::vector<cl::Device> devices = get_xil_devices();
    cl::Device device = devices[0];

    cl::Context context(device);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE);
    std::string device_name = device.getInfo<CL_DEVICE_NAME>(); 
    
    #ifdef XCL_EMULATION
    std::string binaryFile = "sum.awsxclbin";
    #else
    std::string binaryFile = "sum_emu.xclbin";
    #endif
    cl::Program::Binaries bins = import_binary_file(binaryFile);

    devices.resize(1);
    cl::Program program(context, devices, bins);
    cl::Kernel krnl(program,"sum");

    /* Allocating the memory on the device */
    cl::Buffer buffer_a(context,CL_MEM_READ_ONLY,data_size,NULL);
    cl::Buffer buffer_b(context,CL_MEM_READ_ONLY,data_size,NULL);
    cl::Buffer buffer_c(context,CL_MEM_WRITE_ONLY,data_size,NULL);
   
    /* Copy input vectors to memory */
    q.enqueueWriteBuffer(buffer_a,CL_TRUE,0,data_size,source_a);
    q.enqueueWriteBuffer(buffer_b,CL_TRUE,0,data_size,source_b);

    /* Set the kernel arguments */
    krnl.setArg(0, buffer_a);
    krnl.setArg(1, buffer_b);
    krnl.setArg(2, buffer_c);

    /* Launch the kernel */
    q.enqueueNDRangeKernel(krnl,cl::NullRange,cl::NDRange(LENGTH),cl::NullRange);

     /* Copy result to local buffer */
    q.enqueueReadBuffer(buffer_c,CL_TRUE,0,data_size,result_krnl);

    /* Make sure we're done */
    q.finish();

    /* Compare the results of the kernel to the simulation */
    int krnl_match = 0;
    for(int i = 0; i < LENGTH; i++){
        if(result_sim[i] != result_krnl[i]){
            std::cout << "Error: Result mismatch" << std::endl;
            std::cout << "i = " << i << ": CPU result = " << result_sim[i] << " Krnl Result = " << result_krnl[i] << std::endl;
            krnl_match = 1;
            break;
        }
    }

    std::cout << "TEST " << (krnl_match ? "FAILED" : "PASSED") << std::endl; 
    return (krnl_match ? EXIT_FAILURE :  EXIT_SUCCESS);
}

