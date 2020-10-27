#pragma once
#include "Layer.h"

namespace dnn
{
	class MaxPooling final : public Layer
	{
	public:
		MaxPooling(const dnn::Device& device, const dnnl::memory::format_tag format, const std::string& name, const std::vector<Layer*>& inputs, const size_t kernelH = 2, const size_t kernelW = 2, const size_t strideH = 2, const size_t strideW = 2, const size_t padH = 0, const size_t padW = 0);

		const size_t KernelH;
		const size_t KernelW;
		const size_t StrideH;
		const size_t StrideW;
		
		std::string GetDescription() const final override;

		size_t FanIn() const final override;
		size_t FanOut() const final override;

		void InitializeDescriptors(const size_t batchSize) final override;

		void ForwardProp(const size_t batchSize, const bool training) final override;
		void BackwardProp(const size_t batchSize) final override;
		
	private:
		const dnnl::memory::dims Kernel;
		const dnnl::memory::dims Stride;
		const dnnl::memory::dims Padding;

		std::unique_ptr<dnnl::memory> WorkspaceMemory;

		std::unique_ptr<dnnl::pooling_forward::primitive_desc> fwdDesc;
		std::unique_ptr<dnnl::pooling_backward::primitive_desc> bwdDesc;
		std::unique_ptr<dnnl::binary::primitive_desc> bwdAddDesc;

		std::unique_ptr<dnnl::pooling_forward> fwd;
		std::unique_ptr<dnnl::pooling_backward> bwd;
		std::unique_ptr<dnnl::binary> bwdAdd;

		bool reorderFwdSrc;
		bool reorderBwdDiffSrc;
	};
}