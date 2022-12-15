//
//  stacked_tables_helper_functions.h
//  Butterfly-Audio_min-devkit
//
//  Created by Jonas Kieser on 07.10.22.
//

//#include "../../../submodules/Butterfly_Audio_Library/src/synth/src/wavetable_oscillator.h"
#include "wavetable_oscillator.h"
#include "ramped_value.h"
#include <cmath>

struct Frame {
	using Wavetable = Butterfly::Wavetable<float>;
	bool isEmpty {true};
	bool isSelected {false};
	//    unsigned int position{};        //zero based counting - necessary?

	std::vector<float>     samples;       // rough data -> braucht es nicht zwingend
	std::vector<Wavetable> multitable;    // antialiased data

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
	frame.isEmpty    = false;
	frame.isSelected = true;
	frame.samples    = data;
	frame.multitable.resize(splitFreqs.size());
	Butterfly::Antialiaser antialiaser {sampleRate, fftCalculator};
	antialiaser.antialiase(data.begin(), splitFreqs.begin(), splitFreqs.end(), frame.multitable);
	return frame;
}

class StackedFrames {
private:
	using Wavetable = Butterfly::Wavetable<float>;
	using osc       = Butterfly::WavetableOscillator<Wavetable>;

	int getSelectedFrameIdx() {
		int selectedFrameIdx {};
		for (int i = 0; i < frames.size(); i++) {
			if (frames[i].isSelected) {
				selectedFrameIdx = i;
				break;
			}
		}
		return selectedFrameIdx;
	}

public:
	unsigned int       maxFrames {0};
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
			}    // static function draus machen?
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
		}
		else if (selectedFrameIdx < frames.size()) {
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

		osc                    interpolationOscillator {};
		std::vector<Wavetable> wavetable;
		float                  exportTableOscFreq = sampleRate / static_cast<float>(exportTablesize);

		for (const auto& frame : frames) {
			Wavetable              table {frame.samples, sampleRate / 2.f};
			std::vector<Wavetable> wavetable {table};
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
class MultiFrameOsc {
	using Wavetable = Butterfly::Wavetable<float>;
	using TableOsc  = Butterfly::WavetableOscillator<Wavetable>;

private:
	void initOscNoFrame() {
		osc.setParam(0.f);
		osc.setTables(&zeroWavetable, &zeroWavetable);
		// Set Idcs and Weightings to sth?
	}
	void initOscOneFrame() {
		osc.setParam(0.f);
		osc.setTables(&stackedFrames.frames[0].multitable, &stackedFrames.frames[0].multitable);
		// Set Idcs and Weightings to sth?
	}
	void setVisibility() {
		if (stackedFrames.frames.size() < 2) {
			isVisible = false;
		}
		else {
			isVisible = true;
		}
	}

public:
	StackedFrames                                   stackedFrames {};
	Butterfly::MorpingWavetableOscillator<TableOsc> osc;

	std::vector<float> morphingSamples;    // for graphics
	bool               isVisible {false};

	std::vector<float>     zeroData;    // for init Osc with no frame
	Wavetable              zeroTable;
	std::vector<Wavetable> zeroWavetable;

	float        pos {1.f};
	unsigned int firstFrameIdx {0};    // reference to frames.samples[i]
	unsigned int secondFrameIdx {1};
	float        firstFrameWeighting {1.f};
	float        secondFrameWeighting {0.f};

	Butterfly::RampedValue<float> morphingParam {1.f, 150};

	// Constructor inits Osc with zero tables to prevent assert
	//    MultiFrameOsc() = default;

	MultiFrameOsc(float sampleRate, int internalTablesize, float oscFreq, int maxFrames) {
		zeroData.resize(internalTablesize, 0.f);
		zeroTable.setData(zeroData);
		zeroTable.setMaximumPlaybackFrequency(sampleRate / 2.f);
		zeroWavetable.push_back(zeroTable);

		initOscNoFrame();

		osc.setSampleRate(sampleRate);
		osc.setFrequency(oscFreq);

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
		calculateIds();
		setVisibility();
		return true;
	}


	/// --------------------------------------------
	enum class InstructionType { Ramp, Jump };
	enum class RampingPhase { NoPhase, RampToSingleTable, RampBetweenNonconsecutiveTables, RampFromSingleTable };

	struct RampingInstruction {
		int             firstTable;
		int             secondTable;
		double          pos;
		InstructionType type;
	};


	RampingPhase rampingPhase {RampingPhase::NoPhase};

	int                             currentFirstTable {-1};
	int                             currentSecondTable {-1};
	double                          currentPos {};
	std::vector<RampingInstruction> instructions;

	void setPos(float _pos) {
		pos = _pos;
		calculateIds();
	}

	void calculateIds() {
		if (stackedFrames.frames.size() < 2)
			return;
		if (currentFirstTable == -1) {    // HACK
			setTables(0, 1);
			currentFirstTable  = 0;
			currentSecondTable = 0;
		}
		float scaledPos  = pos * static_cast<float>(stackedFrames.frames.size() - 1);
		auto  i1         = std::min<int>(static_cast<int>(scaledPos), stackedFrames.frames.size() - 2);
		auto  i2         = i1 + 1;
		auto  fractional = scaledPos - i1;
		setMorphingPos(i1, fractional);
		// Osc.setTable(&stackedFrames.frames[i1].multitable, &stackedFrames.frames[i2].multitable);
		// morphingParam.set(pos);
	}

	// bugs:
	// 1. slow change from first to last. sometimes both tables are same in osc. done
	// 2.  wavetable 1-2 ->  3-4 then click within 3-4. done


	void setMorphingPos(int newFirstTable, double fractional) {
		if (newFirstTable == currentFirstTable) {
			instructions.clear();    // if in phase 1 or 3

			// TODO: what to do in phase 2? continue phase 2 and then start new
			// if (rampingPhase == RampingPhase::RampBetweenNonconsecutiveTables) {
			if (currentSecondTable != currentFirstTable + 1) {    // a.k.a second phase
				// when we jump during the second phase (going-down version) we dont want to finish the
				// ramp but go back to currentFirstTable
				if (morphingParam.getTarget() == 1.) {
					morphingParam.setSteps(stepsPerWavetable);
					morphingParam.set(0.);
				}
				instructions.push_back({newFirstTable, newFirstTable + 1, fractional, InstructionType::Ramp});
			}
			else {
				morphingParam.setSteps(stepsPerWavetable * std::abs(morphingParam() - fractional));

				morphingParam.set(fractional);
			}
		}
		else {
			// TODO: what if we are in a phase? especially the second one?
			// case first phase:  omit first phase
			// case second phase: omit first phase
			// case third phase:  cancel current ramp

			// idea: make ramp time dependant on distance we want to cover

			if (currentSecondTable != currentFirstTable + 1) {    // a.k.a second phase

				// when we jump during the second phase (going-down version) we dont want to finish the
				// ramp but go back to currentFirstTable
				if (morphingParam.getTarget() == 1.) {
					morphingParam.setSteps(stepsPerWavetable * morphingParam());
					morphingParam.set(0.);
				}
			}

			instructions.clear();
			if (newFirstTable > currentFirstTable) {
				instructions.push_back({newFirstTable, newFirstTable + 1, fractional, InstructionType::Ramp});    // third
				// second, can be omitted if newFirstTable == currentFirstTable+1
				instructions.push_back({newFirstTable, currentFirstTable + 1, 0., InstructionType::Jump});
				instructions.push_back({currentFirstTable, currentFirstTable + 1, 1., InstructionType::Ramp});    // first step
			}
			else {
				instructions.push_back({newFirstTable, newFirstTable + 1, fractional, InstructionType::Ramp});
				instructions.push_back({currentFirstTable, newFirstTable + 1, 1., InstructionType::Jump});
				instructions.push_back({currentFirstTable, currentFirstTable + 1, 0., InstructionType::Ramp});
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

	void setFirstTable(int index) {
		osc.setFirstTable(&stackedFrames.frames[index].multitable);
	}

	void setSecondTable(int index) {
		osc.setSecondTable(&stackedFrames.frames[index].multitable);
	}

	void setTables(int firstTableIndex, int secondTableIndex) {
		setFirstTable(firstTableIndex);
		setSecondTable(secondTableIndex);
	}
	int stepsPerWavetable {15000};

	// process() -> handelt morphingParam und Osc update
	constexpr float operator++() {
		++morphingParam;
		if (!morphingParam.isRamping() && !instructions.empty()) {
			auto instruction = instructions.back();
			instructions.pop_back();
			setTables(instruction.firstTable, instruction.secondTable);
			currentFirstTable  = instruction.firstTable;
			currentSecondTable = instruction.secondTable;
			morphingParam.setSteps(
				stepsPerWavetable * std::abs(morphingParam() - instruction.pos) * (currentFirstTable != currentSecondTable));
			morphingParam.set(instruction.pos);
			// if (instruction.type == InstructionType::Ramp) {
			//}
			// else {
			//	// assert(morphingParam() == 1-);
			//	if (instruction.pos == 0.) {
			//		osc.setFirstTable(&stackedFrames.frames[instruction.firstTable].multitable);
			//		currentFirstTable = instruction.firstTable;
			//	}
			//	else {
			//		osc.setSecondTable(&stackedFrames.frames[instruction.firstTable].multitable);
			//		currentFirstTable = instruction.firstTable - 1;
			//	}
			//	morphingParam.set(instruction.pos);
			//}
		}
		osc.setParam(morphingParam());
		return ++osc;
	}


	void calculateIds1() {
		if (stackedFrames.frames.size() < 1) {
			isVisible            = false;    // setVisibility redundant?
			firstFrameIdx        = 0;
			secondFrameIdx       = 0;
			firstFrameWeighting  = 1.f;
			secondFrameWeighting = 0.f;
			initOscNoFrame();
		}
		else if (stackedFrames.frames.size() == 1) {
			isVisible            = false;
			firstFrameIdx        = 0;
			secondFrameIdx       = 1;
			firstFrameWeighting  = 1.f;
			secondFrameWeighting = 0.f;
			initOscOneFrame();
		}
		else {
			float scaledPos = pos * static_cast<float>(stackedFrames.frames.size() - 1);    // Scale position to frame count
			struct Idcs {
				int firstIdx {0}, secondIdx {1};
			};
			//            std::pair<int> tempIdcs;
			Idcs tempIdcs;
			tempIdcs.firstIdx  = floor(scaledPos);    // Können auch gleich sein!
			tempIdcs.secondIdx = ceil(scaledPos);
			float fracPos, intpart;
			fracPos = modf(scaledPos, &intpart);

			// Change only one Idx if possible (to avoid clicks)
			// Switch case group better?
			if (tempIdcs.firstIdx == firstFrameIdx) {
				secondFrameIdx       = tempIdcs.secondIdx;
				firstFrameWeighting  = (1.f - fracPos);    // einmalig ausrechnen
				secondFrameWeighting = (fracPos);
			}
			else if (tempIdcs.firstIdx == secondFrameIdx) {
				firstFrameIdx        = tempIdcs.secondIdx;
				firstFrameWeighting  = (fracPos);
				secondFrameWeighting = (1.f - fracPos);
			}
			else if (tempIdcs.secondIdx == firstFrameIdx) {
				secondFrameIdx       = tempIdcs.firstIdx;
				firstFrameWeighting  = (fracPos);
				secondFrameWeighting = (1.f - fracPos);
			}
			else if (tempIdcs.secondIdx == secondFrameIdx) {
				firstFrameIdx        = tempIdcs.firstIdx;
				firstFrameWeighting  = (1.f - fracPos);
				secondFrameWeighting = (fracPos);
			}
			else {
				firstFrameIdx        = tempIdcs.firstIdx;
				secondFrameIdx       = tempIdcs.secondIdx;
				firstFrameWeighting  = (1.f - fracPos);
				secondFrameWeighting = (fracPos);
			}
			osc.setTables(&stackedFrames.frames[firstFrameIdx].multitable, &stackedFrames.frames[secondFrameIdx].multitable);
			//            Osc.setParam(secondFrameWeighting);
			morphingParam.set(secondFrameWeighting);

			//            std::cout << "fracPos:              " << fracPos << std::endl;
			//            std::cout << "tempFirstIdx:         " << tempIdcs.firstIdx << std::endl;
			//            std::cout << "tempSecondIdx:        " << tempIdcs.secondIdx << std::endl;
			//            std::cout << "firstFrameIdx:        " << firstFrameIdx << std::endl;
			//            std::cout << "secondFrameIdx:       " << secondFrameIdx << std::endl;
			//            std::cout << "firstFrameWeighting:  " << firstFrameWeighting << std::endl;
			//            std::cout << "secondFrameWeighting: " << secondFrameWeighting << std::endl;
			updateMorphingSamples();
		}
	}


	void updateMorphingSamples() {
		if (stackedFrames.frames.size() < 2) {
			return;
		}
		for (int i = 0; i < morphingSamples.size(); i++) {
			morphingSamples[i] = stackedFrames.frames[firstFrameIdx].samples[i] * firstFrameWeighting
				+ stackedFrames.frames[secondFrameIdx].samples[i] * secondFrameWeighting;
		}
	}
};

int calculateSplitFreqs(
	std::vector<float>& splitFreqs, float semitones = 2.f, float highestSplitFreq = 22050.f, int lowestSplitFreq = 5.f) {
	float       currentFreq = highestSplitFreq;
	const float factor      = 1 / pow(2.f, (semitones / 12.f));
	while (currentFreq > lowestSplitFreq) {
		splitFreqs.push_back(currentFreq);
		currentFreq = currentFreq * factor;
	}
	std::reverse(splitFreqs.begin(), splitFreqs.end());
	return splitFreqs.size();
}
