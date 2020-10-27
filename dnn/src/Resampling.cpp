#include "Model.h"

namespace dnn
{
	Resampling::Resampling(const dnn::Device& device, const dnnl::memory::format_tag format, const std::string& name, const std::vector<Layer*>& inputs, const Algorithms algorithm, const Float factorH, const Float factorW) :
		Layer(device, format, name, LayerTypes::Resampling, 0, 0, inputs[0]->C, inputs[0]->D, static_cast<size_t>(inputs[0]->H * double(factorH)), static_cast<size_t>(inputs[0]->W * double(factorW)), 0, 0, 0, inputs),
		Algorithm(algorithm),
		FactorH(factorH),
		FactorW(factorW)
	{
		assert(Inputs.size() == 1);
	}

	std::string Resampling::GetDescription() const
	{
		std::string description = GetDescriptionHeader();

		description.append(nwl + " Scaling:" + tab + FloatToString(FactorH, 4) + "x" + FloatToString(FactorW, 4));
		if (Algorithm == Algorithms::Linear)
			description.append(nwl + " Algorithm:\tlinear");
		else
			description.append(nwl + " Algorithm:\tnearest");
		return description;
	}

	size_t Resampling::FanIn() const
	{
		return 1;
	}

	size_t Resampling::FanOut() const
	{
		return 1;
	}

	void Resampling::InitializeDescriptors(const size_t batchSize)
	{
		dnnl::algorithm algorithm;
		switch (Algorithm)
		{
		case Algorithms::Linear:
			algorithm = dnnl::algorithm::resampling_linear;
			break;
		case Algorithms::Nearest:
			algorithm = dnnl::algorithm::resampling_nearest;
			break;
		default:
			algorithm = dnnl::algorithm::resampling_linear;
		}

		const auto factor = std::vector<float>({ FactorH, FactorW });

		auto memDesc = std::vector<dnnl::memory::desc>({
			dnnl::memory::desc(dnnl::memory::dims({ dnnl::memory::dim(batchSize), dnnl::memory::dim(InputLayer->C), dnnl::memory::dim(InputLayer->H), dnnl::memory::dim(InputLayer->W) }), dnnl::memory::data_type::f32, Format),
			dnnl::memory::desc(dnnl::memory::dims({ dnnl::memory::dim(batchSize), dnnl::memory::dim(C), dnnl::memory::dim(H), dnnl::memory::dim(W) }), dnnl::memory::data_type::f32, Format) });

		fwdDesc = std::make_unique<dnnl::resampling_forward::primitive_desc>(dnnl::resampling_forward::primitive_desc(dnnl::resampling_forward::desc(dnnl::prop_kind::forward, algorithm, factor, *InputLayer->DstMemDesc, memDesc[1]), Device.first));
		fwd = std::make_unique<dnnl::resampling_forward>(dnnl::resampling_forward(*fwdDesc));

		DstMemDesc = std::make_unique<dnnl::memory::desc>(fwdDesc->dst_desc());
		DiffDstMemDesc = std::make_unique<dnnl::memory::desc>(fwdDesc->dst_desc());

		bwdDesc = std::make_unique<dnnl::resampling_backward::primitive_desc>(dnnl::resampling_backward::primitive_desc(dnnl::resampling_backward::desc(algorithm, factor, memDesc[0], *DiffDstMemDesc), Device.first, *fwdDesc));
		bwd = std::make_unique<dnnl::resampling_backward>(dnnl::resampling_backward(*bwdDesc));
		
		bwdAddDesc = std::make_unique<dnnl::binary::primitive_desc>(dnnl::binary::primitive_desc(dnnl::binary::desc(dnnl::algorithm::binary_add, *InputLayer->DiffDstMemDesc, *InputLayer->DiffDstMemDesc, *InputLayer->DiffDstMemDesc), Device.first));
		bwdAdd = std::make_unique<dnnl::binary>(dnnl::binary(*bwdAddDesc));
	}

	void Resampling::ForwardProp(const size_t batchSize, const bool training)
	{
		auto memSrc = dnnl::memory(*InputLayer->DstMemDesc, Device.first, InputLayer->Neurons.data());
		auto dstMem = dnnl::memory(*DstMemDesc, Device.first, Neurons.data());

		fwd->execute(Device.second, std::unordered_map<int, dnnl::memory>{ {DNNL_ARG_SRC, memSrc}, {DNNL_ARG_DST, dstMem} });
		Device.second.wait();

#ifndef DNN_LEAN
		if (training)
			ZeroFloatVector(NeuronsD1.data(), batchSize * PaddedCDHW);
#else
		DNN_UNREF_PAR(batchSize);
		DNN_UNREF_PAR(training);
#endif
	}

	void Resampling::BackwardProp(const size_t batchSize)
	{
#ifdef DNN_LEAN
		ZeroGradient(batchSize);
#else
		DNN_UNREF_PAR(batchSize);
#endif // DNN_LEAN

		auto diffDstMem = dnnl::memory(*DiffDstMemDesc, Device.first, NeuronsD1.data());
		auto memDiffSrc = SharesInput ? dnnl::memory(*InputLayer->DiffDstMemDesc, Device.first) : dnnl::memory(*InputLayer->DiffDstMemDesc, Device.first, InputLayer->NeuronsD1.data());
	
		bwd->execute(Device.second, std::unordered_map<int, dnnl::memory>{ {DNNL_ARG_DIFF_DST, diffDstMem}, {DNNL_ARG_DIFF_SRC, memDiffSrc} });
		Device.second.wait();

		if (SharesInput)
		{
			bwdAdd->execute(Device.second, std::unordered_map<int, dnnl::memory>{ { DNNL_ARG_SRC_0, dnnl::memory(*InputLayer->DiffDstMemDesc, Device.first, InputLayer->NeuronsD1.data()) }, { DNNL_ARG_SRC_1, memDiffSrc }, { DNNL_ARG_DST, dnnl::memory(*InputLayer->DiffDstMemDesc, Device.first, InputLayer->NeuronsD1.data()) } });
			Device.second.wait();
		}

#ifdef DNN_LEAN
		ReleaseGradient();
#endif // DNN_LEAN
	}
}