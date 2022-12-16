#pragma once
#include "item_collection.h"
#include "antialiase.h"
#include "wavetable.h"
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
public:
    ///TODO: same for gain & freq
    void setNormalizedMorphPos(float morphPos) {
        normalizedMorphPos = std::clamp(morphPos, 0.f, 1.f);
        audioProcessor.sendEvent({ParameterType::morphPos, normalizedMorphPos});
    }
    
    bool addFrame(const std::vector<float>& data, float sampleRate, const std::vector<float>& splitFreqs,
                  const Butterfly::FFTCalculator<float, internalTablesize>& fftCalculator) {
        if (frames.size() >= maxFrames) { return false; }
        frames.add(createFrame(data, sampleRate, splitFreqs, fftCalculator));
        frames.selectOne(frames.size() - 1);
        sendFramesToAudioProcessor();
        return true;
    }
    
    // Audio thread only!
    void process() {
        audioProcessor.process();
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
    size_t maxFrames{};
    std::vector<float> morphedWaveform;
    int currentFirstTable{};
    float fracMorphPos{};
    float normalizedMorphPos{};
    ReleasePool<MultitableCollection> releasePool;
    
    AudioProcessor audioProcessor;
};

}
