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

struct Frame
{
    using Wavetable = Butterfly::Wavetable<float>;
    bool isEmpty{true};
    bool isSelected{false};
    //typename std::iterator_traits<It>::value_type normalizationTarget = 0.891251;   //-1dB (could be Max attribute)
    double normalizationTarget = 0.891251;
//    unsigned int position{};        //zero based counting - necessary?
    
    std::vector<float> samples;     //raw data -> braucht es nicht zwingend
    std::vector<Wavetable> multitable;    //antialiased data
    
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

class StackedFrames
{
private:
    
    using Wavetable = Butterfly::Wavetable<float>;
    using Osc = Butterfly::WavetableOscillator<Wavetable>;
    
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
    
    StackedFrames() = default;
    
    template<int internalTablesize>
    bool addFrame(const std::vector<float> &data, float sampleRate, const std::vector<float> &splitFreqs, const Butterfly::FFTCalculator<float, internalTablesize> &fftCalculator)
    {
        if (frames.size() >= maxFrames) {
            return false;
        }
        frames.push_back(createFrame(data, sampleRate, splitFreqs, fftCalculator));
        selectFrame(frames.size() - 1);
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
        
        Osc interpolationOscillator{};
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
    using Wavetable = Butterfly::Wavetable<float>;
    using TableOsc = Butterfly::WavetableOscillator<Wavetable>;
    
private:
    void initOscNoFrame() {
        Osc.setParam(0.f);
        Osc.setTable(&zeroWavetable, &zeroWavetable);
        //Set Idcs and Weightings to sth?
    }
    void initOscOneFrame() {
        Osc.setParam(0.f);
        Osc.setTable(&stackedFrames.frames[0].multitable, &stackedFrames.frames[0].multitable);
        //Set Idcs and Weightings to sth?
    }
    void setVisibility() {
        if (stackedFrames.frames.size() < 2) {isVisible = false;}
        else {isVisible = true;}
    }

public:
    StackedFrames stackedFrames{};
    Butterfly::MorpingWavetableOscillator<TableOsc> Osc;
    
    std::vector<float> morphingSamples;     //for graphics
    bool isVisible{false};
    
    std::vector<float> zeroData;            //for init Osc with no frame
    Wavetable zeroTable;
    std::vector<Wavetable> zeroWavetable;
        
    float pos{1.f};
    unsigned int firstFrameIdx{0};          //reference to frames.samples[i]
    unsigned int secondFrameIdx{1};
    float firstFrameWeighting{1.f};
    float secondFrameWeighting{0.f};
    
    Butterfly::RampedValue<float> morphingParam{1.f, 150};
    //Butterfly::RampedValue<float> pos{1.f, 150};
    
    //Constructor inits Osc with zero tables to prevent assert
//    MultiFrameOsc() = default;
    
    MultiFrameOsc(float sampleRate, int internalTablesize, float oscFreq, int maxFrames) {
        zeroData.resize(internalTablesize, 0.f);
        zeroTable.setData(zeroData);
        zeroTable.setMaximumPlaybackFrequency(sampleRate / 2.f);
        zeroWavetable.push_back(zeroTable);
        
        initOscNoFrame();
        
        Osc.setSampleRate(sampleRate);
        Osc.setFrequency(oscFreq);

        stackedFrames.maxFrames = maxFrames;
        
        morphingSamples.resize(internalTablesize, 0.f);
    };

    //returns success, false if maxFrames reached
    template<int internalTablesize>
    bool addFrame(const std::vector<float> &data, float sampleRate, const std::vector<float> &splitFreqs, const Butterfly::FFTCalculator<float, internalTablesize> &fftCalculator) {
        //TODO: doppelt!
        if (stackedFrames.frames.size() >= stackedFrames.maxFrames) {
            return false;
        }
        stackedFrames.addFrame(data, sampleRate, splitFreqs, fftCalculator);
        calculateIds();
        setVisibility();
        return true;
    }
    
    void setPos(float _pos) {   //makes no sense with public pos
        //pos.set(_pos);
        pos = _pos;
        calculateIds();
    }
    
    void calculateIds() {       //We would have to call this per sample if morphing parameter is changing
        if (stackedFrames.frames.size() < 1) {
            isVisible = false;  //setVisibility redundant?
            firstFrameIdx = 0;
            secondFrameIdx = 0;
            firstFrameWeighting = 1.f;
            secondFrameWeighting = 0.f;
            initOscNoFrame();
        } else if (stackedFrames.frames.size() == 1) {
            isVisible = false;
            firstFrameIdx = 0;
            secondFrameIdx = 1;
            firstFrameWeighting = 1.f;
            secondFrameWeighting = 0.f;
            initOscOneFrame();
        } else {
            //---Bis hier hin könnte man alles auslagern und nur bei Frame changes aufrufen
            //---scaledPos wäre dann ein rampedValue
            float scaledPos = pos * static_cast<float>(stackedFrames.frames.size() - 1); //Scale position to frame count
            struct Idcs {
                int firstIdx{0}, secondIdx{1};
            };
//            std::pair<int> tempIdcs;
            Idcs tempIdcs;
            tempIdcs.firstIdx = floor(scaledPos);    //Können auch gleich sein!
            tempIdcs.secondIdx = ceil(scaledPos);
            float fracPos, intpart;
            fracPos = modf(scaledPos, &intpart);
            
            //Change only one Idx if possible (to avoid clicks)
            //Switch case group better?
            if (tempIdcs.firstIdx == firstFrameIdx) {
                secondFrameIdx = tempIdcs.secondIdx;
                firstFrameWeighting = (1.f - fracPos);  //einmalig ausrechnen
                secondFrameWeighting = (fracPos);
            }
            else if (tempIdcs.firstIdx == secondFrameIdx) {
                firstFrameIdx = tempIdcs.secondIdx;
                firstFrameWeighting = (fracPos);
                secondFrameWeighting = (1.f - fracPos);
            } else if (tempIdcs.secondIdx == firstFrameIdx) {
                secondFrameIdx = tempIdcs.firstIdx;
                firstFrameWeighting = (fracPos);
                secondFrameWeighting = (1.f - fracPos);
            } else if (tempIdcs.secondIdx == secondFrameIdx) {
                firstFrameIdx = tempIdcs.firstIdx;
                firstFrameWeighting = (1.f - fracPos);
                secondFrameWeighting = (fracPos);
            } else {
                firstFrameIdx = tempIdcs.firstIdx;
                secondFrameIdx = tempIdcs.secondIdx;
                firstFrameWeighting = (1.f - fracPos);
                secondFrameWeighting = (fracPos);
            }
            Osc.setTable(&stackedFrames.frames[firstFrameIdx].multitable, &stackedFrames.frames[secondFrameIdx].multitable);
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
    
    //process() -> handelt morphingParam und Osc update
    constexpr float operator++() {
        //calculateIds(++pos);
        Osc.setParam(++morphingParam);
        return ++Osc;;
    }

    
    void updateMorphingSamples() {
        if (stackedFrames.frames.size() < 2) { return;}
        for (int i = 0; i < morphingSamples.size(); i++) {
            morphingSamples[i] = stackedFrames.frames[firstFrameIdx].samples[i] * firstFrameWeighting + stackedFrames.frames[secondFrameIdx].samples[i] * secondFrameWeighting;
        }
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
