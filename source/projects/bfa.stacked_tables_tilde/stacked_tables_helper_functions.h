//
//  stacked_tables_helper_functions.h
//  Butterfly-Audio_min-devkit
//
//  Created by Jonas Kieser on 07.10.22.
//

#include "wavetable_oscillator.h"
#include "ramped_value.h"
#include "waveform_processing.h"
#include <cmath>

using Wavetable = Butterfly::Wavetable<float>;
using TableOsc = Butterfly::WavetableOscillator<Wavetable>;

struct Frame
{
    using Wavetable = Butterfly::Wavetable<float>;
    bool isEmpty{true};
    bool isSelected{false};
    double normalizationTarget = 0.891251;      //-1dB
    
    std::vector<float> samples;     //raw data -> braucht es nicht zwingend
    std::vector<Wavetable> multitable;    //antialiased data
    
    TableOsc Osc;
    Butterfly::RampedValue<float> gain{0.f, 500};
    
    void flipPhase() {
        for (auto &wavetable : multitable) {    //antialiased multitable (audio)
            wavetable *= -1.f;
        }
        for (float &sample : samples) {         //raw data (visualisation)
            sample *= -1.f;
        }
    }
    
    void normalize() {
        //Get peak out of raw data (same normalization value for all tables in multitable)
        const auto inv = normalizationTarget / Butterfly::peak(samples.begin(), samples.end());
        for (auto &wavetable : multitable) {
            wavetable *= inv;
        }
        for (float &sample : samples) {
            sample *= inv;
        }
    }
};

//Frame factory function
template<int internalTablesize>
Frame createFrame(const std::vector<float>& data, float sampleRate, const std::vector<float>& splitFreqs,
    const Butterfly::FFTCalculator<float, internalTablesize>& fftCalculator, float frequency) {
    Frame frame;
    frame.isEmpty    = false;
    frame.isSelected = true;
    frame.samples    = data;
    frame.multitable.resize(splitFreqs.size());
    Butterfly::Antialiaser antialiaser {sampleRate, fftCalculator};
    antialiaser.antialiase(data.begin(), splitFreqs.begin(), splitFreqs.end(), frame.multitable);
    frame.Osc = {&frame.multitable, sampleRate, frequency};
    //frame.Osc.setSampleRate(sampleRate);
    //frame.Osc.setFrequency(frequency);
    //frame.Osc.setTable(&frame.multitable);
    return std::move(frame);
}

class StackedFrames
{
private:
    
    int getSelectedFrameIdx() {
        int selectedFrameIdx{};
        for (int i = 0; i < frames.size(); i++) {
            if(frames[i].isSelected) {
                selectedFrameIdx = i;
                break;
            }
        }
        return selectedFrameIdx;
    }
    
public:
    unsigned int maxFrames{0};
    std::vector<Frame> frames;
    
    StackedFrames() {
		frames.reserve(16);
    }
    
    template<int internalTablesize>
    bool addFrame(const std::vector<float> &data, float sampleRate, const std::vector<float> &splitFreqs, const Butterfly::FFTCalculator<float, internalTablesize> &fftCalculator, float frequency)
    {
        if (frames.size() >= maxFrames) {
            return false;
        }
        frames.push_back(std::move(createFrame(data, sampleRate, splitFreqs, fftCalculator, frequency)));
	   frames.back().Osc = {&frames.back().multitable, frames.back().Osc.getSampleRate(), frames.back().Osc.getFrequency()};
        int lastFrameIdx = frames.size() - 1;
        //frames[lastFrameIdx].Osc = {&frames[lastFrameIdx].multitable, sampleRate, frequency};
        selectFrame(lastFrameIdx);
        return true;
    }
    
    void selectFrame(int idx) {
        if (idx < frames.size()) {
            for (auto &frame : frames) { frame.isSelected = false; }   //static function draus machen?
            frames[idx].isSelected = true;
        }
    }
    
    bool moveUpSelectedFrame() {
        if (frames.empty()) {return false;}
        int selectedFrameIdx = getSelectedFrameIdx();
        if (selectedFrameIdx == 0) { return false; }
        std::swap(frames[selectedFrameIdx], frames[selectedFrameIdx - 1]);
        return true;
    }
    
    bool moveDownSelectedFrame() {
        if (frames.empty()) {return false;}
        int selectedFrameIdx = getSelectedFrameIdx();
        if (selectedFrameIdx >= (frames.size() - 1)) { return false; }
        std::swap(frames[selectedFrameIdx], frames[selectedFrameIdx + 1]);
        return true;
    }
    
    void flipPhase() {
        for (auto &frame : frames) {
            if (frame.isSelected) {
                frame.flipPhase();
                break;
            }
        }
    }
    
    bool normalizeFrame() {
        if (frames.empty()) {return false;}
        for (auto &frame : frames) {
            if (frame.isSelected) {
                frame.normalize();
                return true;
            }
        }
        return false;
    }
    
    bool removeSelectedFrame()
    {
        if (frames.empty()) {return false;}
        int selectedFrameIdx = getSelectedFrameIdx();
        if (frames.size() < selectedFrameIdx) {return false;}
        frames.erase(frames.begin() + selectedFrameIdx);
        if (selectedFrameIdx >= frames.size() && frames.size() != 0) {
            frames.back().isSelected = true;
        } else if (selectedFrameIdx < frames.size()) {
            frames[selectedFrameIdx].isSelected = true;
        }
        return true;
    }
    
    void clearFrames() { frames.clear(); }
    
    bool getStackedTable(std::vector<float> &stackedTable, int exportTablesize, float sampleRate) {
        if (frames.size() <= 0) { return false; }
        
        TableOsc interpolationOscillator{};
        std::vector<Wavetable> wavetable;
        float exportTableOscFreq = sampleRate / static_cast<float>(exportTablesize);
        
        for (const auto& frame : frames) {
            Wavetable table {frame.samples, sampleRate / 2.f};
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

//Manages a MorphingWavetableOscillator with StackedFrames wavetable data
class MultiFrameOsc
{
    
private:
    void setVisibility() {
        if (stackedFrames.frames.size() < 2) {isVisible = false;}
        else {isVisible = true;}
    }
    
    bool phaseRetrigger{false};

public:
    StackedFrames stackedFrames{};
    
    std::vector<float> morphingSamples;     //for graphics
    bool isVisible{false};
        
    float pos{1.f};
    unsigned int firstFrameIdx{0};          //reference to frames.samples[i]
    float firstFrameWeighting{1.f};
    
    Butterfly::RampedValue<float> morphingParam{1.f, 150};
    
    MultiFrameOsc(float sampleRate, int internalTablesize, float oscFreq, int maxFrames) {
        stackedFrames.maxFrames = maxFrames;
        morphingSamples.resize(internalTablesize, 0.f);
    };

    //returns success, false if maxFrames reached
    template<int internalTablesize>
    bool addFrame(const std::vector<float> &data, float sampleRate, const std::vector<float> &splitFreqs, const Butterfly::FFTCalculator<float, internalTablesize> &fftCalculator, float frequency) {
        //TODO: doppelt!
        if (stackedFrames.frames.size() >= stackedFrames.maxFrames) {
            return false;
        }
        stackedFrames.addFrame(data, sampleRate, splitFreqs, fftCalculator, frequency);
        phaseRetrigger = true;
        calculateIds();
        setVisibility();
        return true;
    }
    
    void setSampleRate(float sampleRate) {
        for (auto &frame : stackedFrames.frames) {
            frame.Osc.setSampleRate(sampleRate);
        }
    }
    
    void setFrequency(float freq) {
        for (auto &frame : stackedFrames.frames) {
            frame.Osc.setFrequency(freq);
        }
    }
    
    void setPos(float pos_) {   //makes no sense with public pos
        if (stackedFrames.frames.size() < 2) {return;}
        pos = pos_;
        calculateIds();
    }
    
    void calculateIds() {       //We would have to call this per sample if morphing parameter is changing
        if (stackedFrames.frames.size() == 0) {
            return;
        } else if (stackedFrames.frames.size() == 1) {
            firstFrameIdx = 0;
            firstFrameWeighting = 1.f;
            stackedFrames.frames[firstFrameIdx].gain = firstFrameWeighting;
            morphingSamples = stackedFrames.frames[0].samples;
        } else {

            float scaledPos = pos * static_cast<float>(stackedFrames.frames.size() - 1); //Scale position to frame count
            firstFrameIdx = std::min(static_cast<size_t>(scaledPos), stackedFrames.frames.size() - 2);
            firstFrameWeighting = 1.f - (scaledPos - firstFrameIdx);
            /*
            std::cout << firstFrameIdx << std::endl;
            std::cout << firstFrameIdx + 1 << std::endl;
            std::cout << firstFrameWeighting << std::endl;
            std::cout << 1.f - firstFrameWeighting << "\n" << std::endl;
            */
            for (auto &frame : stackedFrames.frames) {
                frame.gain.set(0.f);
            }
            stackedFrames.frames[firstFrameIdx].gain.set(firstFrameWeighting);
            stackedFrames.frames[firstFrameIdx + 1].gain.set(1.f - firstFrameWeighting);
            updateMorphingSamples();
        }
    }
    
    void updateMorphingSamples() {      //Über timer regeln, GUI only
        if (stackedFrames.frames.size() < 2) { return; }
        for (int i = 0; i < morphingSamples.size(); i++) {
            morphingSamples[i] = stackedFrames.frames[firstFrameIdx].samples[i] * firstFrameWeighting + stackedFrames.frames[firstFrameIdx + 1].samples[i] * (1.f - firstFrameWeighting);
        }
    }
    
    //process() -> handelt morphingParam und Osc update
    float operator++() {        //not constexpr anymore, problme?
        float sample{0.f};
        if (phaseRetrigger) {
            for (Frame &frame : stackedFrames.frames) {
                frame.Osc.retrigger();
            }
            phaseRetrigger = false;
        }
        for (Frame &frame : stackedFrames.frames) {
            sample += ++frame.Osc * ++frame.gain;
        }
        return sample;
    }
};

int calculateSplitFreqs(std::vector<float> &splitFreqs, float semitones = 2.f, float highestSplitFreq = 22050.f, int lowestSplitFreq = 5.f) {
    float currentFreq = highestSplitFreq;
    const float factor = 1 / pow(2.f,(semitones / 12.f));
    while (currentFreq > lowestSplitFreq)
    {
        splitFreqs.push_back(currentFreq);
        currentFreq = currentFreq * factor;
    }
    std::reverse(splitFreqs.begin(), splitFreqs.end());
    return splitFreqs.size();
}
