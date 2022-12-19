//
//  stacked_tables_helper_functions.h
//  Butterfly-Audio_min-devkit
//
//  Created by Jonas Kieser on 07.10.22.
//

#include "wavetable_oscillator.h"
#include "ramped_value.h"
#include "morphing_wavetable_oscillator.h"
#include <cmath>

struct Frame
{
	using Wavetable = Butterfly::Wavetable<float>;
	bool isEmpty{ true };
	bool isSelected{ false };
	//    unsigned int position{};        //zero based counting - necessary?

	std::vector<float> samples;		   // rough data -> braucht es nicht zwingend
	std::vector<Wavetable> multitable; // antialiased data

	void flipPhase() {
		for (auto& wavetable : multitable) {
			wavetable *= -1.f;
		}
		for (float& sample : samples) {
			sample *= -1.f;
		}
	}
};

// Frame factory function
template<int internalTablesize>
Frame createFrame(const std::vector<float>& data, float sampleRate, const std::vector<float>& splitFreqs,
	const Butterfly::FFTCalculator<float, internalTablesize>& fftCalculator) {
	Frame frame;
	frame.isEmpty = false;
	frame.isSelected = true;
	frame.samples = data;
	frame.multitable.resize(splitFreqs.size());
	Butterfly::Antialiaser antialiaser{ sampleRate, fftCalculator };
	antialiaser.antialiase(data.begin(), splitFreqs.begin(), splitFreqs.end(), frame.multitable);
	return frame;
}

class StackedFrames
{
private:
	using Wavetable = Butterfly::Wavetable<float>;
	using osc = Butterfly::WavetableOscillator<Wavetable>;

	int getSelectedFrameIdx() {
		int selectedFrameIdx{};
		for (int i = 0; i < frames.size(); i++) {
			if (frames[i].isSelected) {
				selectedFrameIdx = i;
				break;
			}
		}
		return selectedFrameIdx;
	}

public:
	unsigned int maxFrames{ 0 };
	std::vector<Frame> frames;

	StackedFrames() = default;

	template<int internalTablesize>
	bool addFrame(const std::vector<float>& data, float sampleRate, const std::vector<float>& splitFreqs,
		const Butterfly::FFTCalculator<float, internalTablesize>& fftCalculator) {
		if (frames.size() >= maxFrames) {
			return false;
		}
		frames.push_back(createFrame(data, sampleRate, splitFreqs, fftCalculator));
		selectFrame(frames.size() - 1);
		return true;
	}


	void selectFrame(int idx) {
		if (idx < frames.size()) {
			for (auto& frame : frames) {
				frame.isSelected = false;
			} // static function draus machen?
			frames[idx].isSelected = true;
		}
	}

	bool moveUpSelectedFrame() {
		if (frames.empty()) {
			return false;
		}
		int selectedFrameIdx = getSelectedFrameIdx();
		if (selectedFrameIdx == 0) {
			return false;
		}
		std::swap(frames[selectedFrameIdx], frames[selectedFrameIdx - 1]);
		return true;
	}

	bool moveDownSelectedFrame() {
		if (frames.empty()) {
			return false;
		}
		int selectedFrameIdx = getSelectedFrameIdx();
		if (selectedFrameIdx >= (frames.size() - 1)) {
			return false;
		}
		std::swap(frames[selectedFrameIdx], frames[selectedFrameIdx + 1]);
		return true;
	}

	void flipPhase() {
		for (auto& frame : frames) {
			if (frame.isSelected) {
				frame.flipPhase();
				break;
			}
		}
	}

	bool removeSelectedFrame() {
		if (frames.empty()) {
			return false;
		}
		int selectedFrameIdx = getSelectedFrameIdx();
		if (frames.size() < selectedFrameIdx) {
			return false;
		}
		frames.erase(frames.begin() + selectedFrameIdx);
		if (selectedFrameIdx >= frames.size() && frames.size() != 0) {
			frames.back().isSelected = true;
		} else if (selectedFrameIdx < frames.size()) {
			frames[selectedFrameIdx].isSelected = true;
		}
		return true;
	}

	void clearFrames() {
		frames.clear();
	}

	bool getStackedTable(std::vector<float>& stackedTable, int exportTablesize, float sampleRate) {
		if (frames.size() <= 0) {
			return false;
		}

		osc interpolationOscillator{};
		std::vector<Wavetable> wavetable;
		float exportTableOscFreq = sampleRate / static_cast<float>(exportTablesize);

		for (const auto& frame : frames) {
			Wavetable table{ frame.samples, sampleRate / 2.f };
			std::vector<Wavetable> wavetable{ table };
			interpolationOscillator.setTable(&wavetable);
			interpolationOscillator.setSampleRate(sampleRate);
			interpolationOscillator.setFrequency(exportTableOscFreq);
			std::vector<float> interpolatedTable;
			for (int i = 0; i < exportTablesize; i++) {
				interpolatedTable.push_back(interpolationOscillator++);
			}
			stackedTable.insert(stackedTable.end(), interpolatedTable.begin(), interpolatedTable.end());
		}
		return true;
	}
};

// Manages a MorphingWavetableOscillator with StackedFrames wavetable data
class MultiFrameOsc
{
	using Wavetable = Butterfly::Wavetable<float>;
	using TableOsc = Butterfly::WavetableOscillator<Wavetable>;
	using Osc = Butterfly::MultiMorphingWavetableOscillator<TableOsc>;
public:
	StackedFrames stackedFrames{};
	Butterfly::MorpingWavetableOscillator<TableOsc> osc;
	Osc osc1;


	std::vector<float> morphingSamples; // for graphics


	// Constructor inits Osc with zero tables to prevent assert
	//    MultiFrameOsc() = default;

	MultiFrameOsc(float sampleRate, int internalTablesize, float oscFreq, int maxFrames) {

		osc1.setSampleRate(44100);
		osc1.setFrequency(200);
		osc1.setMaxNumWaveforms(16);
		//osc1.setNormalizedMorphingParam(.4);

		zeroTable.setData(std::vector<float>(internalTablesize, 0.f));
		zeroTable.setMaximumPlaybackFrequency(sampleRate / 2.f);
		zeroWavetable.push_back(zeroTable);

		osc.setSampleRate(sampleRate);
		osc.setFrequency(oscFreq);
		setNoTables();

		stackedFrames.maxFrames = maxFrames;

		morphingSamples.resize(internalTablesize, 0.f);
	};

	// returns success, false if maxFrames reached
	template<int internalTablesize>
	bool addFrame(const std::vector<float>& data, float sampleRate, const std::vector<float>& splitFreqs,
		const Butterfly::FFTCalculator<float, internalTablesize>& fftCalculator) {
		// TODO: doppelt!
		if (stackedFrames.frames.size() >= stackedFrames.maxFrames) {
			return false;
		}
		stackedFrames.addFrame(data, sampleRate, splitFreqs, fftCalculator);
		numFramesChanged();
		setMorphingParam(getMorphingParam());
		return true;
	}

	void framesChanged() {
		std::vector<std::span<Wavetable>> data;
		for (auto& frame : stackedFrames.frames) {
			data.push_back(std::span<Wavetable>{ frame.multitable.begin(), frame.multitable.end() });
		}
		osc1.setWaveforms(data);
	}

	void removeSelectedFrame() {
		if (stackedFrames.removeSelectedFrame()) {
			numFramesChanged();
		}
	}

	void setMorphingParam(float newMorphingParam) {
		normalizedMorphingParam = std::clamp(newMorphingParam, 0.f, 1.f);
		morphingParamChanged();
		osc1.setNormalizedMorphingParam(newMorphingParam); /**/
	}

	float getMorphingParam() const {
		return normalizedMorphingParam;
	}

	bool isMorphedWaveformAvailable() const {
		return stackedFrames.frames.size() > 1;
	}

	constexpr float operator++() {
		++morphingParam;
		if (!morphingParam.isRamping() && !instructions.empty()) {
			auto instruction = instructions.back();
			instructions.pop_back();
			setTables(instruction.firstTable, instruction.secondTable);
			morphingParam.setSteps(rampingStepsPerWavetable * std::abs(morphingParam() - instruction.normalizedMorphingParam) * (currentFirstTable != currentSecondTable));
			morphingParam.set(instruction.normalizedMorphingParam);
		}
		osc.setParam(morphingParam());
		//return ++osc;
		return ++osc1;  /**/
	}

	void updateMorphedSamples(int firstTable) {
		if (stackedFrames.frames.size() < 2) {
			return;
		}
		const auto& firstFrame = stackedFrames.frames[firstTable];
		const auto& secondFrame = stackedFrames.frames[firstTable + 1];
		for (int i = 0; i < morphingSamples.size(); i++) {
			morphingSamples[i] = firstFrame.samples[i] * (1.f - targetFractionalMorphingParam) + secondFrame.samples[i] * targetFractionalMorphingParam;
		}
	}

	void updateMorphedSamples() {
		updateMorphedSamples(currentFirstTable);
	}

	void setRampSpeed(int rampingStepsPerWavetable) {
		this->rampingStepsPerWavetable = rampingStepsPerWavetable;
		osc1.setRampingStepsPerWavetable(rampingStepsPerWavetable);  /**/
	}

	void moveUpSelectedFrame() {
		stackedFrames.moveUpSelectedFrame();
		tablesRearranged();
	}

	void moveDownSelectedFrame() {
		stackedFrames.moveDownSelectedFrame();
		tablesRearranged();
	}

private:
	void setFirstTable(int index) {
		assert(stackedFrames.frames.size() > index && "MultiOsc::setFirstTable(): index out of range");
		osc.setFirstTable(&stackedFrames.frames[index].multitable);
		currentFirstTable = index;
	}

	void setSecondTable(int index) {
		assert(stackedFrames.frames.size() > index && "MultiOsc::setSecondTable(): index out of range");
		osc.setSecondTable(&stackedFrames.frames[index].multitable);
		currentSecondTable = index;
	}

	void setTables(int firstTableIndex, int secondTableIndex) {
		setFirstTable(firstTableIndex);
		setSecondTable(secondTableIndex);
	}

	void setNoTables() {
		osc.setTables(&zeroWavetable, &zeroWavetable);
		currentFirstTable = currentSecondTable = -1;
	}

	void numFramesChanged() {
		const auto numFrames = stackedFrames.frames.size();
		if (currentFirstTable >= numFrames || currentSecondTable >= numFrames) {
			if (numFrames == 0) {
				setNoTables();
			} else if (numFrames == 1) {
				setTables(0, 0);
			} else {
				setTables(numFrames - 2, numFrames - 1);
			}
		}
		updateMorphedSamples();
		instructions.clear();
		framesChanged();  /**/
	}

	void tablesRearranged() {
		if (stackedFrames.frames.size() > 0) {
			setTables(currentFirstTable, currentSecondTable);
			updateMorphedSamples();
		}
		framesChanged(); /**/
	}

	void morphingParamChanged() {
		if (stackedFrames.frames.size() < 2)
			return;
		float scaledPos = normalizedMorphingParam * static_cast<float>(stackedFrames.frames.size() - 1);
		auto targetFirstTable = std::min<int>(static_cast<int>(scaledPos), stackedFrames.frames.size() - 2);
		targetFractionalMorphingParam = scaledPos - targetFirstTable;
		setWavetableMorphingPosition(targetFirstTable, targetFractionalMorphingParam);
		updateMorphedSamples(targetFirstTable);
	}


	// bugs:
	// 1. slow change from first to last. sometimes both tables are same in osc. done
	// 2.  wavetable 1-2 ->  3-4 then click within 3-4. done

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

	/*
	3 cases:
	A) both surrounding tables are same as current -> just ramp
	B) Second current is new first or first current is new second (special case of C)
	C) no same tables -> ramp to one of current surrounding tables, then ramp to one of the new surrounding tables, then ramp to target pos
	  ____
		.  |
	  ____ ↓
			   |
	  ____     | (tables in between are skipped)
			   |
	  ____     ↓
		.         ↓
	  ____


	  ramp time is adjusted to the distance that is to cover. This also deals with omitting some phase which is then not necessary

	*/


	struct RampingInstruction
	{
		int firstTable;
		int secondTable;
		double normalizedMorphingParam;
	};


	int currentFirstTable{ -1 };
	int currentSecondTable{ -1 }; // may not be (currentFirstTable + 1) !
	double targetFractionalMorphingParam{};
	float normalizedMorphingParam{ 1.f };

	int rampingStepsPerWavetable{ 15000 };
	std::vector<RampingInstruction> instructions;

	Butterfly::RampedValue<float> morphingParam{ 1.f, 150 };

	Wavetable zeroTable; // for init osc with no frame
	std::vector<Wavetable> zeroWavetable;
};

int calculateSplitFreqs(
	std::vector<float>& splitFreqs, float semitones = 2.f, float highestSplitFreq = 22050.f, int lowestSplitFreq = 5.f) {
	float currentFreq = highestSplitFreq;
	const float factor = 1 / pow(2.f, (semitones / 12.f));
	while (currentFreq > lowestSplitFreq) {
		splitFreqs.push_back(currentFreq);
		currentFreq = currentFreq * factor;
	}
	std::reverse(splitFreqs.begin(), splitFreqs.end());
	return splitFreqs.size();
}
