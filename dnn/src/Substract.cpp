#include "Model.h"

namespace dnn
{
	Substract::Substract(const dnn::Device& device, const dnnl::memory::format_tag format, const std::string& name, const std::vector<Layer*>& inputs) :
		Layer(device, format, name, LayerTypes::Substract, 0, 0, inputs[0]->C, inputs[0]->D, inputs[0]->H, inputs[0]->W, 0, 0, 0, inputs)
	{
		assert(Inputs.size() > 1);

		for (size_t i = 0; i < Inputs.size(); i++)
		{
			assert(Inputs[i]->C == C);
			assert(Inputs[i]->D == D);
			assert(Inputs[i]->H == H);
			assert(Inputs[i]->W == W);
		}

		Scales = std::vector<Float>(Inputs.size(), Float(-1));
		Scales[0] = Float(1);
	}

	std::string Substract::GetDescription() const
	{
		return GetDescriptionHeader();
	}

	size_t Substract::FanIn() const
	{
		return 1;
	}

	size_t Substract::FanOut() const
	{
		return 1;
	}

	void Substract::InitializeDescriptors(const size_t batchSize)
	{
		DstMemDesc = std::make_unique<dnnl::memory::desc>(*InputLayer->DstMemDesc);
		DiffDstMemDesc = std::make_unique<dnnl::memory::desc>(*InputLayer->DiffDstMemDesc);

		for (auto i = 1ull; i < Inputs.size(); i++)
		{
			assert(*DstMemDesc == *Inputs[i]->DstMemDesc);
			if (*DstMemDesc != *Inputs[i]->DstMemDesc)
				throw std::invalid_argument("Incompatible memory formats in Substract layer");
		}

		srcsMemsDesc = std::vector<dnnl::memory::desc>(Inputs.size());
		for (auto i = 0ull; i < Inputs.size(); i++)
			srcsMemsDesc[i] = *Inputs[i]->DstMemDesc;

		fwdDesc = std::make_unique<dnnl::sum::primitive_desc>(dnnl::sum::primitive_desc(*DstMemDesc, Scales, srcsMemsDesc, Device.first));

		fwdArgs = std::unordered_map<int, dnnl::memory>{ { DNNL_ARG_DST, dnnl::memory(*DstMemDesc, Device.first, Neurons.data()) } };
		for (auto i = 0ull; i < Inputs.size(); i++)
			fwdArgs.insert({ DNNL_ARG_MULTIPLE_SRC + int(i), dnnl::memory(srcsMemsDesc[i], Device.first, Inputs[i]->Neurons.data()) });

		fwd = std::make_unique<dnnl::sum>(dnnl::sum(*fwdDesc));
	}

	void Substract::ForwardProp(const size_t batchSize, const bool training)
	{
		fwd->execute(Device.second, fwdArgs);
		Device.second.wait();

#ifndef DNN_LEAN
		if (training)
			ZeroFloatVector(NeuronsD1.data(), batchSize * PaddedCDHW);
#else
		DNN_UNREF_PAR(batchSize);
#endif
	}

	void Substract::BackwardProp(const size_t batchSize)
	{
#ifdef DNN_LEAN
		ZeroGradientMulti(batchSize);
#endif // DNN_LEAN

#ifdef DNN_STOCHASTIC
		if (batchSize == 1)
		{
			if (Inputs.size() == 2)
			{
				for (auto n = 0ull; n < CDHW; n++)
				{
					Inputs[0]->NeuronsD1[n] += NeuronsD1[n];
					Inputs[1]->NeuronsD1[n] -= NeuronsD1[n];
				}
			}
			else
			{
				for (auto i = 1ull; i < Inputs.size(); i++)
					for (auto n = 0ull; n < CDHW; n++)
					{
						Inputs[0]->NeuronsD1[n] += NeuronsD1[n];
						Inputs[i]->NeuronsD1[n] -= NeuronsD1[n];
					}
			}
		}
		else
		{
#endif
			if (Inputs.size() == 2)
			{
				for_i(batchSize, LIGHT_COMPUTE, [=](size_t b)
				{
					const auto start = b * PaddedCDHW;
					const auto end = start + CDHW;
					for (auto n = start; n < end; n++)
					{
						Inputs[0]->NeuronsD1[n] += NeuronsD1[n];
						Inputs[1]->NeuronsD1[n] -= NeuronsD1[n];
					}
				});
			}
			else
			{
				for_i(batchSize, LIGHT_COMPUTE, [=](size_t b)
				{
					const auto start = b * PaddedCDHW;
					const auto end = start + CDHW;
					for (auto n = start; n < end; n++)
						Inputs[0]->NeuronsD1[n] += NeuronsD1[n];
					for (auto i = 1ull; i < Inputs.size(); i++)
						for (auto n = start; n < end; n++)
							Inputs[i]->NeuronsD1[n] -= NeuronsD1[n];
				});
			}
#ifdef DNN_STOCHASTIC
		}
#endif

#ifdef DNN_LEAN
		ReleaseGradient();
#endif // DNN_LEAN
	}
}