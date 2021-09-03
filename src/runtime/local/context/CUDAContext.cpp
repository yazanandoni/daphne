/*
 * Copyright 2021 The DAPHNE Consortium
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "runtime/local/context/CUDAContext.h"

void CUDAContext::destroy() {
#ifdef NDEBUG
	std::cout << "Destroying CUDA context..." << std::endl;
#endif
	CHECK_CUBLAS(cublasDestroy(cublas_handle));
	CHECK_CUSPARSE(cusparseDestroy(cusparse_handle));
	CHECK_CUDNN(cudnnDestroy(cudnn_handle));
	CHECK_CUDNN(cudnnDestroyPoolingDescriptor(pooling_desc));
	CHECK_CUDNN(cudnnDestroyTensorDescriptor(src_tensor_desc));
	CHECK_CUDNN(cudnnDestroyTensorDescriptor(dst_tensor_desc));
	CHECK_CUDNN(cudnnDestroyTensorDescriptor(bn_tensor_desc));
	CHECK_CUDNN(cudnnDestroyActivationDescriptor(activation_desc));
	CHECK_CUDNN(cudnnDestroyConvolutionDescriptor(conv_desc));
	CHECK_CUDNN(cudnnDestroyFilterDescriptor(filter_desc));

	CHECK_CUDART(cudaFree(cudnn_workspace));
//	CHECK_CUDART(cudaFree(cublas_workspace));
//	CHECK_CUBLAS(cublasLtDestroy(ltHandle));
}

void CUDAContext::init() {
	CHECK_CUDART(cudaSetDevice(device_id));
	CHECK_CUDART(cudaGetDeviceProperties(&device_properties, device_id));
	std::cout << "Using CUDA device " << device_id << ": " << device_properties.name << std::endl;
	size_t available; size_t total;
	cudaMemGetInfo(&available, &total);
	std::cout << "available mem: " << available << " total mem: " << total << std::endl;
	CHECK_CUBLAS(cublasCreate(&cublas_handle));
	CHECK_CUSPARSE(cusparseCreate(&cusparse_handle));
	CHECK_CUDNN(cudnnCreate(&cudnn_handle));
	CHECK_CUDNN(cudnnCreatePoolingDescriptor(&pooling_desc));
	CHECK_CUDNN(cudnnCreateTensorDescriptor(&src_tensor_desc));
	CHECK_CUDNN(cudnnCreateTensorDescriptor(&dst_tensor_desc));
	CHECK_CUDNN(cudnnCreateTensorDescriptor(&bn_tensor_desc));
	CHECK_CUDNN(cudnnCreateActivationDescriptor(&activation_desc));
	CHECK_CUDNN(cudnnCreateConvolutionDescriptor(&conv_desc));
	CHECK_CUDNN(cudnnCreateFilterDescriptor(&filter_desc));

	getCUDNNWorkspace(64 * 1024 * 1024);

//	CHECK_CUBLAS(cublasLtCreate(&cublaslt_Handle));
//	CHECK_CUDART(cudaMalloc(&cublas_workspace, cublas_workspace_size));
}

template<>
cudnnDataType_t CUDAContext::getCUDNNDataType<float>() const {
	return CUDNN_DATA_FLOAT;
}

template<>
cudnnDataType_t CUDAContext::getCUDNNDataType<double>() const {
	return CUDNN_DATA_DOUBLE;
}

template<>
cudaDataType CUDAContext::getCUSparseDataType<float>() const {
	return CUDA_R_32F;
}

template<>
cudaDataType CUDAContext::getCUSparseDataType<double>() const {
	return CUDA_R_64F;
}

void* CUDAContext::getCUDNNWorkspace(size_t size) {
	if (size > cudnn_workspace_size) {
		//#ifdef NDEBUG
		std::cerr << "Allocating cudnn conv workspace of size " << size << " bytes" << std::endl;
		//#endif
		CHECK_CUDART(cudaMalloc(&cudnn_workspace, size));
		cudnn_workspace_size = size;
	}
	//#ifdef NDEBUG
//	else {
//		std::cerr << "Not allocating cudnn conv workspace of size " << size << " bytes" << std::endl;
//	}
	//#endif
	return cudnn_workspace;
}

std::unique_ptr<IContext> CUDAContext::createCudaContext(int device_id) {
	//#ifndef NDEBUG
//	std::cout << "creating CUDA context..." << std::endl;
//#endif
	int device_count = -1;
	CHECK_CUDART(cudaGetDeviceCount(&device_count));

	if(device_count < 1) {
		std::cerr << "Not creating requested CUDA context. No cuda devices available." << std::endl;
		return nullptr;
	}

	if(device_id >= device_count) {
		std::cerr << "Requested device ID " << device_id << " >= device count " << device_count << std::endl;
		return nullptr;
	}

	auto ctx = std::unique_ptr<CUDAContext>(new CUDAContext(device_id));
	ctx->init();
	return ctx;
}
