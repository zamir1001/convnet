#include "Model.h"

namespace dnn
{
	Cost::Cost(const dnn::Device& device, const dnnl::memory::format_tag format, const std::string& name, const Costs cost, const size_t groupIndex, const size_t labelIndex, const size_t c, const std::vector<Layer*>& inputs, const Float labelTrue, const Float labelFalse, const Float weight, const Float eps) :
		Layer(device, format, name, LayerTypes::Cost, 0, 0, c, 1, 1, 1, 0, 0, 0, inputs),
		CostFunction(cost),
		GroupIndex(groupIndex),
		LabelIndex(labelIndex),
		LabelTrue(labelTrue),
		LabelFalse(labelFalse),
		Weight(weight),
		Eps(eps),
		isLogSoftmax(false)
	{
		assert(Inputs.size() == 1);
		
		InputLayer->LayerBeforeCost = true;

		TrainErrors = 0;
		TrainErrorPercentage = Float(0);
		TrainLoss = Float(0);
		AvgTrainLoss = Float(0);

		TestErrors = 0;
		TestErrorPercentage = Float(0);
		TestLoss = Float(0);
		AvgTestLoss = Float(0);

		ConfusionMatrix = std::vector<std::vector<size_t>>(C, std::vector<size_t>(C, 0));
	}

	std::string Cost::GetDescription() const
	{
		std::string description = GetDescriptionHeader();

		description.append(nwl + " Cost:" + dtab + std::string(magic_enum::enum_name<Costs>(CostFunction)));
		description.append(nwl + " Channels:" + tab + std::to_string(C));
		description.append(nwl + " LabelTrue:" + tab + FloatToString(LabelTrue));
		description.append(nwl + " LabelFalse:" + tab + FloatToString(LabelFalse));
		description.append(nwl + " Weight:" + tab + FloatToString(Weight));
		if (CostFunction == Costs::MeanAbsoluteEpsError || CostFunction == Costs::CategoricalCrossEntropy)
			description.append(nwl + " Epsilon:" + tab + FloatToString(Eps, 6));

		return description;
	}

	size_t Cost::FanIn() const
	{
		return 1;
	}

	size_t Cost::FanOut() const
	{
		return 1;
	}

	void Cost::InitializeDescriptors(const size_t batchSize)
	{
		DstMemDesc = std::make_unique<dnnl::memory::desc>(dnnl::memory::desc(dnnl::memory::dims({ int(batchSize), int(C) }), dnnl::memory::data_type::f32, dnnl::memory::format_tag::nc));
		DiffDstMemDesc = std::make_unique<dnnl::memory::desc>(dnnl::memory::desc(dnnl::memory::dims({ int(batchSize), int(C) }), dnnl::memory::data_type::f32, dnnl::memory::format_tag::nc));
		isLogSoftmax = static_cast<Activation*>(InputLayer)->ActivationFunction == Activations::LogSoftmax;
	}

	void Cost::SetSampleLabel(const std::vector<size_t>& sampleLabel)
	{
		SampleLabel = sampleLabel;
	}

	void Cost::SetSampleLabels(const std::vector<std::vector<size_t>>& sampleLabels)
	{
		SampleLabels = sampleLabels;
	}
	
	void Cost::Reset()
	{
		TrainErrors = 0;
		TrainErrorPercentage = Float(0);
		TrainLoss = Float(0);
		AvgTrainLoss = Float(0);

		TestErrors = 0;
		TestErrorPercentage = Float(0);
		TestLoss = Float(0);
		AvgTestLoss = Float(0);

		ConfusionMatrix = std::vector<std::vector<size_t>>(C, std::vector<size_t>(C, 0));
	}

	void Cost::ForwardProp(const size_t batchSize, const bool training)
	{
		switch (CostFunction)
		{
		case Costs::BinaryCrossEntropy:
		{
#ifdef DNN_STOCHASTIC
			if (batchSize == 1)
			{
				for (auto i = 0ull; i < C; i++)
				{
					Neurons[i] = -LabelFalse * std::log(InputLayer->Neurons[i]) - (Float(1) - LabelFalse) * std::log(Float(1) - InputLayer->Neurons[i]);
#ifndef DNN_LEAN
					NeuronsD1[i] = Float(0);
#endif
				}
				const auto label = SampleLabel[LabelIndex];
				Neurons[label] = -LabelTrue * std::log(InputLayer->Neurons[label]) - (Float(1) - LabelTrue) * std::log(Float(1) - InputLayer->Neurons[label]);
			}
			else
			{
#endif
				for (auto i = 0ull; i < C * batchSize; i++)
				{
					Neurons[i] = -LabelFalse * std::log(InputLayer->Neurons[i]) - (Float(1) - LabelFalse) * std::log(Float(1) - InputLayer->Neurons[i]);
#ifndef DNN_LEAN
					NeuronsD1[i] = Float(0);
#endif
				}
				for (auto b = 0ull; b < batchSize; b++)
				{
					const auto label = SampleLabels[b][LabelIndex] + (b * C);
					Neurons[label] = -LabelTrue * std::log(InputLayer->Neurons[label]) - (Float(1) - LabelTrue) * std::log(Float(1) - InputLayer->Neurons[label]);
				}
#ifdef DNN_STOCHASTIC
			}
#endif
		}
		break;

		case Costs::CategoricalCrossEntropy:
		{
			if (isLogSoftmax)
			{
#ifdef DNN_STOCHASTIC
				if (batchSize == 1)
				{
					for (auto i = 0ull; i < C; i++)
					{
						Neurons[i] = Float(0);
#ifndef DNN_LEAN
						NeuronsD1[i] = Float(0);
#endif
					}
					const auto label = SampleLabel[LabelIndex];
					Neurons[label] = -InputLayer->Neurons[label];
				}
				else
				{
#endif
					for (auto i = 0ull; i < C * batchSize; i++)
					{
						Neurons[i] = Float(0);
#ifndef DNN_LEAN
						NeuronsD1[i] = Float(0);
#endif
					}
					for (size_t b = 0; b < batchSize; b++)
					{
						const auto label = SampleLabels[b][LabelIndex] + (b * C);
						Neurons[label] = -InputLayer->Neurons[label];
					}
#ifdef DNN_STOCHASTIC
				}
#endif
			}
			else
			{
#ifdef DNN_STOCHASTIC
				if (batchSize == 1)
				{
					for (auto i = 0ull; i < C; i++)
					{
						Neurons[i] = Float(0);
#ifndef DNN_LEAN
						NeuronsD1[i] = Float(0);
#endif
					}
					const auto label = SampleLabel[LabelIndex];
					Neurons[label] = -std::log(InputLayer->Neurons[label]);
				}
				else
				{
#endif
					for (auto i = 0ull; i < C * batchSize; i++)
					{
						Neurons[i] = Float(0);
#ifndef DNN_LEAN
						NeuronsD1[i] = Float(0);
#endif
					}
					for (size_t b = 0; b < batchSize; b++)
					{
						const auto label = SampleLabels[b][LabelIndex] + (b * C);
						Neurons[label] = -std::log(InputLayer->Neurons[label]);
					}
#ifdef DNN_STOCHASTIC
				}
#endif
			}
		}
		break;

		case Costs::MeanAbsoluteError:
		{
#ifdef DNN_STOCHASTIC
			if (batchSize == 1)
			{
				for (auto i = 0ull; i < C; i++)
					Neurons[i] = std::abs(InputLayer->Neurons[i] - LabelFalse);

				const size_t label = SampleLabel[LabelIndex];
				Neurons[label] = std::abs(InputLayer->Neurons[label] - LabelTrue);
					}
			else
			{
#endif
				for (auto i = 0ull; i < C * batchSize; i++)
					Neurons[i] = std::abs(InputLayer->Neurons[i] - LabelFalse);

				for (auto b = 0ull; b < batchSize; b++)
				{
					const auto label = SampleLabels[b][LabelIndex] + (b * C);
					Neurons[label] = std::abs(InputLayer->Neurons[label] - LabelTrue);
				}
#ifdef DNN_STOCHASTIC
				}
#endif
			}
		break;

		case Costs::MeanAbsoluteEpsError:
		{
#ifdef DNN_STOCHASTIC
			if (batchSize == 1)
			{
				for (auto i = 0ull; i < C; i++)
				{
					const auto diff = std::abs(InputLayer->Neurons[i] - LabelFalse);
					Neurons[i] = diff > Eps ? diff : Float(0);
				}
				const size_t label = SampleLabel[LabelIndex];
				const auto diff = std::abs(InputLayer->Neurons[label] - LabelTrue);
				Neurons[label] = diff > Eps ? diff : Float(0);
			}
			else
			{
#endif
				for (auto i = 0ull; i < C * batchSize; i++)
				{
					const auto diff = std::abs(InputLayer->Neurons[i] - LabelFalse);
					Neurons[i] = diff > Eps ? diff : Float(0);
				}
				for (auto b = 0ull; b < batchSize; b++)
				{
					const auto label = SampleLabels[b][LabelIndex] + (b * C);
					const auto diff = std::abs(InputLayer->Neurons[label] - LabelTrue);
					Neurons[label] = diff > Eps ? diff : Float(0);
				}
#ifdef DNN_STOCHASTIC
		}
#endif
			}
		break;

		case Costs::MeanSquaredError:
		{
#ifdef DNN_STOCHASTIC
			if (batchSize == 1)
			{
				for (auto i = 0ull; i < C; i++)
					Neurons[i] = FloatSquare(InputLayer->Neurons[i] - LabelFalse);

				const size_t label = SampleLabel[LabelIndex];
				Neurons[label] = FloatSquare(InputLayer->Neurons[label] - LabelTrue);
			}
			else
			{
#endif
				for (auto i = 0ull; i < C * batchSize; i++)
					Neurons[i] = FloatSquare(InputLayer->Neurons[i] - LabelFalse);

				for (auto b = 0ull; b < batchSize; b++)
				{
					const auto label = SampleLabels[b][LabelIndex] + (b * C);
					Neurons[label] = FloatSquare(InputLayer->Neurons[label] - LabelTrue);
				}
#ifdef DNN_STOCHASTIC
			}
#endif
		}
		break;

		case Costs::SmoothHinge:
		{
#ifdef DNN_STOCHASTIC
			if (batchSize == 1)
			{
				for (auto i = 0ull; i < C; i++)
				{
					const auto ty = LabelFalse * InputLayer->Neurons[i];
					Neurons[i] = ty <= 0 ? Float(0.5) - ty : ty < Float(1) ? FloatSquare(1 - ty) * Float(0.5) : Float(0);
#ifndef DNN_LEAN
					NeuronsD1[i] = Float(0);
#endif
		}
				const auto label = SampleLabel[LabelIndex];
				const auto ty = LabelTrue * InputLayer->Neurons[label];

				Neurons[label] = ty <= 0 ? Float(0.5) - ty : ty < Float(1) ? FloatSquare(1 - ty) * Float(0.5) : Float(0);
				}
			else
			{
#endif
				for (auto i = 0ull; i < C * batchSize; i++)
				{
					const auto ty = LabelFalse * InputLayer->Neurons[i];

					Neurons[i] = ty <= 0 ? Float(0.5) - ty : ty < Float(1) ? FloatSquare(1 - ty) * Float(0.5) : Float(0);
#ifndef DNN_LEAN
					NeuronsD1[i] = Float(0);
#endif
				}
				for (size_t b = 0; b < batchSize; b++)
				{
					const auto label = SampleLabels[b][LabelIndex] + (b * C);
					const auto ty = LabelTrue * InputLayer->Neurons[label];

					Neurons[label] = ty <= 0 ? Float(0.5) - ty : ty < Float(1) ? FloatSquare(1 - ty) * Float(0.5) : Float(0);
				}
#ifdef DNN_STOCHASTIC
			}
#endif
		}
		break;
		}
	}

	void Cost::BackwardProp(const size_t batchSize)
	{
#ifdef DNN_LEAN
		ZeroGradient(batchSize);
#endif // DNN_LEAN

		switch (CostFunction)
		{
		case Costs::BinaryCrossEntropy:
		{
#ifdef DNN_STOCHASTIC
			if (batchSize == 1)
			{
				for (auto i = 0ull; i < C; i++)
					InputLayer->NeuronsD1[i] = (InputLayer->Neurons[i] - LabelFalse) / (InputLayer->Neurons[i] * (Float(1) - InputLayer->Neurons[i]));

				const size_t label = SampleLabel[LabelIndex];
				InputLayer->NeuronsD1[label] = (InputLayer->Neurons[label] - LabelTrue) / (InputLayer->Neurons[label] * (Float(1) - InputLayer->Neurons[label]));
			}
			else
			{
#endif
				for (auto i = 0ull; i < C * batchSize; i++)
					InputLayer->NeuronsD1[i] = (InputLayer->Neurons[i] - LabelFalse) / (InputLayer->Neurons[i] * (Float(1) - InputLayer->Neurons[i]));

				for (auto b = 0ull; b < batchSize; b++)
				{
					const auto label = SampleLabels[b][LabelIndex] + (b * C);
					InputLayer->NeuronsD1[label] = (InputLayer->Neurons[label] - LabelTrue) / (InputLayer->Neurons[label] * (Float(1) - InputLayer->Neurons[label]));
				}
#ifdef DNN_STOCHASTIC
			}
#endif
		}
		break;
		
		case Costs::CategoricalCrossEntropy:
		{
			if (isLogSoftmax)
			{
#ifdef DNN_STOCHASTIC
				if (batchSize == 1)
				{
					for (auto i = 0ull; i < C; i++)
						InputLayer->NeuronsD1[i] = std::exp(InputLayer->Neurons[i]) - (Eps / (C-1));

					const size_t label = SampleLabel[LabelIndex];
					InputLayer->NeuronsD1[label] = std::exp(InputLayer->Neurons[label]) - ((Float(1) - Eps) + (Eps / C));
				}
				else
				{
#endif
					for (auto i = 0ull; i < C * batchSize; i++)
						InputLayer->NeuronsD1[i] = std::exp(InputLayer->Neurons[i]) - (Eps / C);
						
					for (auto b = 0ull; b < batchSize; b++)
					{
						const auto label = SampleLabels[b][LabelIndex] + (b * C);
						InputLayer->NeuronsD1[label] = std::exp(InputLayer->Neurons[label]) - ((Float(1) - Eps) + (Eps / C));
					}
#ifdef DNN_STOCHASTIC
				}
#endif
			}
			else
			{
#ifdef DNN_STOCHASTIC
				if (batchSize == 1)
				{
					for (auto i = 0ull; i < C; i++)
						InputLayer->NeuronsD1[i] = InputLayer->Neurons[i] - (Eps / C);

					const auto label = SampleLabel[LabelIndex];
					InputLayer->NeuronsD1[label] = InputLayer->Neurons[label] - ((Float(1) - Eps) + (Eps / C));
				}
				else
				{
#endif
					for (auto i = 0ull; i < C * batchSize; i++)
						InputLayer->NeuronsD1[i] = InputLayer->Neurons[i] - (Eps / C);
						
					for (auto b = 0ull; b < batchSize; b++)
					{
						const auto label = SampleLabels[b][LabelIndex] + (b * C);
						InputLayer->NeuronsD1[label] = InputLayer->Neurons[label] - ((Float(1) - Eps) + (Eps / C));
					}
#ifdef DNN_STOCHASTIC
				}
#endif
			}
		}
		break;

		case Costs::MeanAbsoluteError:
		{
			const auto factor = Float(1) / static_cast<Float>(C);

#ifdef DNN_STOCHASTIC
			if (batchSize == 1)
			{
				for (auto i = 0ull; i < C; i++)
				{
					const auto sign = InputLayer->Neurons[i] - LabelFalse;
					InputLayer->NeuronsD1[i] = sign < 0 ? -factor : sign > 0 ? factor : 0;
				}
				const size_t label = SampleLabel[LabelIndex];
				const auto sign = InputLayer->Neurons[label] - LabelTrue;
				InputLayer->NeuronsD1[label] = sign < 0 ? -factor : sign > 0 ? factor : 0;
			}
			else
			{
#endif
				for (auto i = 0ull; i < C * batchSize; i++)
				{
					const auto sign = InputLayer->Neurons[i] - LabelFalse;
					InputLayer->NeuronsD1[i] = sign < 0 ? -factor : sign > 0 ? factor : 0;
				}

				for (auto b = 0ull; b < batchSize; b++)
				{
					const auto label = SampleLabels[b][LabelIndex] + (b * C);
					const auto sign = InputLayer->Neurons[label] - LabelTrue;
					InputLayer->NeuronsD1[label] = sign < 0 ? -factor : sign > 0 ? factor : 0;
				}
#ifdef DNN_STOCHASTIC
			}
#endif
		}
		break;

		case Costs::MeanAbsoluteEpsError:
		{
			const auto factor = Float(1) / static_cast<Float>(C);

#ifdef DNN_STOCHASTIC
			if (batchSize == 1)
			{
				for (auto i = 0ull; i < C; i++)
				{
					const auto sign = InputLayer->Neurons[i] - LabelFalse;
					InputLayer->NeuronsD1[i] = sign < -Eps ? -factor : sign > Eps ? factor : 0;
				}
				const size_t label = SampleLabel[LabelIndex];
				const auto sign = InputLayer->Neurons[label] - LabelTrue;
				InputLayer->NeuronsD1[label] = sign < -Eps ? -factor : sign > Eps ? factor : 0;
			}
			else
			{
#endif
				for (auto i = 0ull; i < C * batchSize; i++)
				{
					const auto sign = InputLayer->Neurons[i] - LabelFalse;
					InputLayer->NeuronsD1[i] = sign < -Eps ? -factor : sign > Eps ? factor : 0;
				}

				for (auto b = 0ull; b < batchSize; b++)
				{
					const auto label = SampleLabels[b][LabelIndex] + (b * C);
					const auto sign = InputLayer->Neurons[label] - LabelTrue;
					InputLayer->NeuronsD1[label] = sign < -Eps ? -factor : sign > Eps ? factor : 0;
				}
#ifdef DNN_STOCHASTIC
				}
#endif
			}
		break;

		case Costs::MeanSquaredError:
		{
			const auto factor = Float(2) / static_cast<Float>(C);

#ifdef DNN_STOCHASTIC
			if (batchSize == 1)
			{
				for (auto i = 0ull; i < C; i++)
					InputLayer->NeuronsD1[i] = (InputLayer->Neurons[i] - LabelFalse) * factor;

				const size_t label = SampleLabel[LabelIndex];
				InputLayer->NeuronsD1[label] = (InputLayer->Neurons[label] - LabelTrue) * factor;
			}
			else
			{
#endif
				for (auto i = 0ull; i < C * batchSize; i++)
					InputLayer->NeuronsD1[i] = (InputLayer->Neurons[i] - LabelFalse) * factor;

				for (auto b = 0ull; b < batchSize; b++)
				{
					const auto label = SampleLabels[b][LabelIndex] + (b * C);
					InputLayer->NeuronsD1[label] = (InputLayer->Neurons[label] - LabelTrue) * factor;
				}
#ifdef DNN_STOCHASTIC
			}
#endif
		}
		break;

		case Costs::SmoothHinge:
		{
#ifdef DNN_STOCHASTIC
			if (batchSize == 1)
			{
				for (auto i = 0ull; i < C; i++)
				{
					const auto ty = LabelFalse * InputLayer->Neurons[i];
					InputLayer->NeuronsD1[i] = ty <= 0 ? -Float(1) : ty < Float(1) ? ty - Float(1) : Float(0);

				}
				const auto label = SampleLabel[LabelIndex];
				const auto ty = LabelTrue * InputLayer->Neurons[label];

				InputLayer->NeuronsD1[label] = ty <= 0 ? -Float(1) : ty < Float(1) ? ty - Float(1) : Float(0);
			}
			else
			{
#endif
				for (auto i = 0ull; i < C * batchSize; i++)
				{
					const auto ty = LabelFalse * InputLayer->Neurons[i];

					InputLayer->NeuronsD1[i] = ty <= 0 ? -Float(1) : ty < Float(1) ? ty - Float(1) : Float(0);
				}
				for (size_t b = 0; b < batchSize; b++)
				{
					const auto label = SampleLabels[b][LabelIndex] + (b * C);
					const auto ty = LabelTrue * InputLayer->Neurons[label];

					InputLayer->NeuronsD1[label] = ty <= 0 ? -Float(1) : ty < Float(1) ? ty - Float(1) : Float(0);
				}
#ifdef DNN_STOCHASTIC
			}
#endif
		}
		break;
		}

#ifdef DNN_LEAN
		ReleaseGradient();
#endif // DNN_LEAN
	}
}