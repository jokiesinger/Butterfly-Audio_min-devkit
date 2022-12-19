
#pragma once
#include "wavetable_oscillator.h"
#include "ramped_value.h"

namespace Butterfly {


template<class Oscillator = Butterfly::WavetableOscillator<Wavetable<double>>>
class MultiMorphingWavetableOscillator
{
public:
	using oscillator_type = Butterfly::MorpingWavetableOscillator<Oscillator>;
	using value_type = typename oscillator_type::value_type;
	using param_type = typename oscillator_type::param_type;
	using wavetable_type = typename oscillator_type::wavetable_type;
	using T = value_type;

	MultiMorphingWavetableOscillator(int maxNumWaveforms = 0) {
		setMaxNumWaveforms(maxNumWaveforms);
		//const wavetable_type zeroTable{ std::vector<value_type>(50, 0.f) };
		//zeroWavetable.push_back(zeroTable);
		setNoTables();
	}

	MultiMorphingWavetableOscillator(param_type sampleRate, param_type oscFreq) : MultiMorphingWavetableOscillator() {
		setSampleRate(sampleRate);
		setFrequency(oscFreq);
	};

	/// @brief Warning! This function may allocate memory. Don't call it from a realtime context.
	/// @param maxNumWaveforms
	constexpr void setMaxNumWaveforms(int maxNumWaveforms) {
		waveforms.reserve(maxNumWaveforms);
	}

	constexpr int getMaxNumWaveforms() const {
		return waveforms.capacity();
	}

	/// @brief The parameter vector may not contain more elements than the return value of getMaxNumWaveforms().
	/// @param newWaveforms
	void setWaveforms(const std::vector<std::span<wavetable_type>>& newWaveforms) {
		const auto numTablesChanged = waveforms.size() != newWaveforms.size();
		// set waveforms without allocating memory.
		assert(newWaveforms.size() <= waveforms.capacity());
		waveforms.clear();
		std::copy(newWaveforms.begin(), newWaveforms.end(), std::back_inserter(waveforms));
		//waveforms = newWaveforms;
		if (numTablesChanged) {
			numWaveformsChanged();
			setNormalizedMorphingParam(getNormalizedMorphingParam());
		} else {
			if (waveforms.size() > 0) {
				setTables(currentFirstTable, currentSecondTable);
			}
		}
	}

	constexpr void setSampleRate(param_type sampleRate) { osc.setSampleRate(sampleRate); }
	constexpr void setFrequency(param_type frequency) { osc.setFrequency(frequency); }

	/// @brief Set morphing parameter in a range of [0, 1]. 0 corresponds to the first table fully and 1
	/// to the last table fully
	/// @param param
	constexpr void setNormalizedMorphingParam(param_type param) {
		normalizedMorphingParam = std::clamp(param, param_type{ 0 }, param_type{ 1 });
		morphingParamChanged();
	}

	void setRampingStepsPerWavetable(int steps) {
		rampingStepsPerWavetable = steps;
	}

	constexpr T operator++() {
		processRamping();
		return ++osc;
	}

	constexpr T operator++(int) {
		processRamping();
		return osc++;
	}

	constexpr T operator()() const {
		return osc();
	}

	constexpr void retrigger() { osc.retrigger(); }
	constexpr void reset() { osc.reset(); }

	constexpr param_type getFrequency() const { return osc.getFrequency(); }
	constexpr param_type getSampleRate() const { return osc.getSampleRate(); }
	constexpr param_type getNormalizedMorphingParam() const { return static_cast<param_type>(normalizedMorphingParam); }
	constexpr int getRampingStepsPerWavetable() const { return rampingStepsPerWavetable; }

private:
	void processRamping() {
		++morphingParam;
		if (!morphingParam.isRamping() && !instructions.empty()) {
			const auto instruction = instructions.back();
			instructions.pop_back();
			setTables(instruction.firstTable, instruction.secondTable);
			morphingParam.setSteps(rampingStepsPerWavetable * std::abs(morphingParam() - instruction.normalizedMorphingParam) * (currentFirstTable != currentSecondTable));
			morphingParam.set(instruction.normalizedMorphingParam);
		}
		osc.setParam(morphingParam());
	}

	void morphingParamChanged() {
		if (waveforms.size() < 2)
			return;
		const float scaledPos = normalizedMorphingParam * static_cast<float>(waveforms.size() - 1);
		const auto targetFirstTable = std::min<int>(static_cast<int>(scaledPos), waveforms.size() - 2);
		const auto targetFractionalMorphingParam = scaledPos - targetFirstTable;
		setWavetableMorphingPosition(targetFirstTable, targetFractionalMorphingParam);
	}

	/// @param newFirstTable  index to new first table
	/// @param fractional     mixing amount firstTable/secondTable in the range [0,1]
	void setWavetableMorphingPosition(int newFirstTable, double fractional) {
		if (newFirstTable == currentFirstTable) {
			instructions.clear(); // if in phase 1 or 3

			// Done: TODO: what to do in phase 2? continue phase 2 and then start new
			// if (rampingPhase == RampingPhase::RampBetweenNonconsecutiveTables) {
			if (currentSecondTable != currentFirstTable + 1) { // a.k.a second phase
				// when we jump during the second phase (going-down version) we dont want to finish the
				// ramp but go back to currentFirstTable
				if (morphingParam.getTarget() == 1.) {
					morphingParam.setSteps(rampingStepsPerWavetable);
					morphingParam.set(0.);
				}
				instructions.push_back({ newFirstTable, newFirstTable + 1, fractional });
			} else {
				morphingParam.setSteps(rampingStepsPerWavetable * std::abs(morphingParam() - fractional));
				morphingParam.set(fractional);
			}
		} else {
			// Done: TODO: what if we are in a phase? especially the second one?
			// case first phase:  omit first phase
			// case second phase: omit first phase
			// case third phase:  cancel current ramp

			if (currentSecondTable != currentFirstTable + 1) { // a.k.a second phase

				// when we jump during the second phase (going-down version) we dont want to finish the
				// ramp but go back to currentFirstTable
				if (morphingParam.getTarget() == 1.) {
					morphingParam.setSteps(rampingStepsPerWavetable * morphingParam());
					morphingParam.set(0.);
				}
			}

			instructions.clear();
			if (newFirstTable > currentFirstTable) {
				instructions.push_back({ newFirstTable, newFirstTable + 1, fractional }); // third
				// second, can be omitted if newFirstTable == currentFirstTable+1
				instructions.push_back({ newFirstTable, currentFirstTable + 1, 0. });
				instructions.push_back({ currentFirstTable, currentFirstTable + 1, 1. }); // first step
			} else {
				instructions.push_back({ newFirstTable, newFirstTable + 1, fractional });
				instructions.push_back({ currentFirstTable, newFirstTable + 1, 1. });
				instructions.push_back({ currentFirstTable, currentFirstTable + 1, 0. });
			}
		}
		assert(instructions.size() <= 3);
	}
	void numWaveformsChanged() {
		const auto numFrames = waveforms.size();
		if (currentFirstTable < numFrames && currentSecondTable < numFrames) {
			setTables(currentFirstTable, currentSecondTable);
		} else {
			if (numFrames == 0) {
				setNoTables();
			} else if (numFrames == 1) {
				setTables(0, 0);
			} else {
				setTables(numFrames - 2, numFrames - 1);
			}
		}
		instructions.clear();
	}

	void setFirstTable(int index) {
		assert(waveforms.size() > index && "MultiOsc::setFirstTable(): index out of range");
		osc.setFirstTable(waveforms[index]);
		currentFirstTable = index;
	}

	void setSecondTable(int index) {
		assert(waveforms.size() > index && "MultiOsc::setSecondTable(): index out of range");
		osc.setSecondTable(waveforms[index]);
		currentSecondTable = index;
	}

	void setTables(int firstTableIndex, int secondTableIndex) {
		setFirstTable(firstTableIndex);
		setSecondTable(secondTableIndex);
	}

	void setNoTables() {
		osc.setTables(&zeroWavetable, &zeroWavetable);
		currentFirstTable = currentSecondTable = 0;
	}

	struct RampingInstruction
	{
		int firstTable;
		int secondTable;
		double normalizedMorphingParam;
	};


	int currentFirstTable{ -1 };
	int currentSecondTable{ -1 }; // might also not be (currentFirstTable + 1) !
	param_type normalizedMorphingParam{ 1 };

	int rampingStepsPerWavetable{ 15000 };
	std::vector<RampingInstruction> instructions;

	Butterfly::RampedValue<double> morphingParam{ 1., 150 };


	oscillator_type osc;
	std::vector<std::span<wavetable_type>> waveforms;

	//std::vector<wavetable_type> zeroWavetable;

	std::vector<wavetable_type> zeroWavetable{ wavetable_type{ std::vector<value_type>(5, 0.) } };
};

} // namespace Butterfly
