#pragma once

#include "item_collection.h"
#include "antialiase.h"
#include "wavetable.h"
#include "wavetable_oscillator.h"
#include "release_pool.h"
#include "audio_processor.h"

namespace Butterfly {

struct Frame {
    using Wavetable = Butterfly::Wavetable<float>;
    
    std::vector<float>     samples;       // raw data
    std::vector<Wavetable> multitable;    // antialiased data
};

/// @brief …
/// @param normalizedMorphingPos
/// @param numTables
/// @return …
std::pair<int, double> computeMorphingStuff(float normalizedMorphingPos, int numTables) {
    float scaledPos               = normalizedMorphingPos * static_cast<float>(numTables - 1);
    auto  targetFirstTable        = std::min<int>(static_cast<int>(scaledPos), numTables - 2);
    auto targetFractionalMorphingParam = scaledPos - targetFirstTable;
    return {targetFirstTable, targetFractionalMorphingParam};
}

// Frame factory function
template<int internalTablesize>
Frame createFrame(const std::vector<float>& data, float sampleRate, const std::vector<float>& splitFreqs,
                  const Butterfly::FFTCalculator<float, internalTablesize>& fftCalculator) {
    Frame frame;
    frame.samples    = data;
    frame.multitable.resize(splitFreqs.size());
    Butterfly::Antialiaser antialiaser {sampleRate, fftCalculator};
    antialiaser.antialiase(data.begin(), splitFreqs.begin(), splitFreqs.end(), frame.multitable);
    return frame;
}

//Make highestSplitFreq sampleRate dependent? All frames would've to be recalculated after sampleRate change...
std::vector<float> calculateSplitFreqs(float semitones = 2.f, float highestSplitFreq = 22050.f, float lowestSplitFreq = 5.f) {
    std::vector<float> splitFreqs;
    float       currentFreq = highestSplitFreq;
    const float factor      = 1 / pow(2.f, (semitones / 12.f));
    while (currentFreq > lowestSplitFreq) {
        splitFreqs.push_back(currentFreq);
        currentFreq = currentFreq * factor;
    }
    std::reverse(splitFreqs.begin(), splitFreqs.end());
    return splitFreqs;
}

class StackedFrames {
    using Multitable = std::vector<Wavetable<float>>;
    using MultitableCollection = std::vector<Multitable>;
    using State = MultitableCollection;
    using Wavetable = Butterfly::Wavetable<float>;
    using Osc       = Butterfly::WavetableOscillator<Wavetable>;
    
public:
    //Ist es in Ordnung nur diesen Konstruktor zu implementieren?
    StackedFrames(float sR, int iT, float oF, int mF) : maxFrames(mF), internalTablesize(iT) {
        audioProcessor.init(oF, sR);
    }
    
    template<int internalTablesize>
    bool addFrame(const std::vector<float>& data, float sampleRate, const std::vector<float>& splitFreqs,
                  const Butterfly::FFTCalculator<float, internalTablesize>& fftCalculator) {
        if (frames.size() >= maxFrames) { return false; }
        frames.add(createFrame(data, sampleRate, splitFreqs, fftCalculator));
        frames.select(frames.size() - 1);
        sendFramesToAudioProcessor();
        return true;
    }
    
    void flipPhase() {
        size_t idx = frames.getSelectionIndex();
        if (idx == -1) {return;}
        for (float& sample : frames.at(idx).samples) {
            sample *= -1.f;
        }
        for (auto& wavetable : frames.at(idx).multitable) {
            wavetable *= -1.f;
        }
        updateMorphedWaveform();
        sendFramesToAudioProcessor();
    }
    
    void moveUpSelectedFrame() {
        size_t idx = frames.getSelectionIndex();
        if (idx == -1) {return;}
        frames.moveUp(idx, 1);
        updateMorphedWaveform();
        sendFramesToAudioProcessor();
    }
    
    void moveDownSelectedFrame() {
        size_t idx = frames.getSelectionIndex();
        if (idx == -1) {return;}
        frames.moveDown(idx, 1);
        updateMorphedWaveform();
        sendFramesToAudioProcessor();
    }
    
    void removeSelectedFrame() {
        size_t idx = frames.getSelectionIndex();
        if (idx == -1) {return;}
        frames.remove(idx);
        updateMorphedWaveform();
        sendFramesToAudioProcessor();
    }
    
    void clearAll() {
        frames.clear();
        updateMorphedWaveform();
        sendFramesToAudioProcessor();   //What is happening hear?
    }
    
    void selectFrame(size_t idx) {
        if (idx >= frames.size()) { return; }
        frames.clearSelection();
        frames.select(idx);
    }
    
    std::optional<size_t> getSelectedFrameIdx() {
        ///TODO: What if nothing is selected? Possible?
        if (auto idx = frames.getSelectionIndex()) {
            return idx;
        } else {
            return {};
        }
    }
    
    std::optional<std::vector<float>> getConcatenatedFrames(int exportTablesize) {
        if (frames.empty()) { return {}; }
        //Objects for interpolation & concatenation
        std::vector<float> concatenatedFrames{};
        Osc interpolationOsc{};
        double exportTableOscFreq = sampleRate / static_cast<float>(exportTablesize);
        
        for (const auto& frame : frames) {
            Wavetable wavetable {frame.samples, sampleRate / 2.f};
            std::span<Wavetable> wavetableSpan {&wavetable, 1};
            std::vector<float> interpolatedWavetable{};
            interpolationOsc.setTable(&wavetableSpan);
            interpolationOsc.setSampleRate(sampleRate);
            interpolationOsc.setFrequency(exportTableOscFreq);
            for (int i = 0; i < exportTablesize; ++i) {
                interpolatedWavetable.push_back(interpolationOsc++);
            }
            concatenatedFrames.insert(concatenatedFrames.end(), interpolatedWavetable.begin(), interpolatedWavetable.end());
        }
        return concatenatedFrames;
    }
    
    size_t getNumFrames() {
        return frames.size();
    }
    
    std::optional<std::vector<float>> getFrame(size_t idx) {
        if (idx >= frames.size()) { return{}; }
        return frames.at(idx).samples;
    }
    
    float getNormalizedMorphPos() { return normalizedMorphPos; }
    
    bool isMorphedWaveformAvailable() const {
        return frames.size() > 1 && !morphedWaveform.empty();
    }
    
    std::optional<std::vector<float>> getMorphedWaveformSamples() {
        if (isMorphedWaveformAvailable()) { return {}; }
        return morphedWaveform;
    }
    
    //=====================================
    //               AUDIO
    //=====================================
    // Audio thread only!
    void process(c74::min::audio_bundle &buffer) {
        audioProcessor.process(buffer);
    }
    
    void setSampleRate(float sampleRate) {
        this->sampleRate = sampleRate;
        audioProcessor.addParamEvent({ParameterType::sampleRate, sampleRate});
    }
    
    void setNormalizedMorphPos(float morphPos) {
        normalizedMorphPos = std::clamp(morphPos, 0.f, 1.f);
        normalizedMorphPosChanged();
        audioProcessor.addParamEvent({ParameterType::morphPos, normalizedMorphPos});
    }
    
    void setOscFreq(double oscFreq) {
        audioProcessor.addParamEvent({ParameterType::frequency, std::clamp(oscFreq, 1., sampleRate / 2.)});
    }
    
    void setOscGain(double gain) {
        audioProcessor.addParamEvent({ParameterType::gain, std::clamp(gain, 0., 1.)});
    }
    
private:
    
    void updateMorphedWaveform() {
        //copy from helper_functions
    }
    void normalizedMorphPosChanged() {
        auto result = computeMorphingStuff(normalizedMorphPos, frames.size());
        currentFirstTable = result.first;
        fracMorphPos = result.second;
        updateMorphedWaveform();
    }
    
    void sendFramesToAudioProcessor() {
        State state;
        for (const auto &frame : frames) {
            state.push_back(frame.multitable);
        }
        audioProcessor.changeState(std::move(state));
    }
    
    ItemCollection<Frame> frames;
    size_t maxFrames{}, internalTablesize{};
    std::vector<float> morphedWaveform;
    int currentFirstTable{};
    float fracMorphPos{};
    float normalizedMorphPos{};
    float sampleRate{};
    ReleasePool<MultitableCollection> releasePool;
    
    AudioProcessor audioProcessor;
};

}
