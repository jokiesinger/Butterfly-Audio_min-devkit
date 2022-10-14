//
//  stacked_tables_helper_functions.h
//  Butterfly-Audio_min-devkit
//
//  Created by Jonas Kieser on 07.10.22.
//

#include "../../../submodules/Butterfly_Audio_Library/src/synth/src/wavetable_oscillator.h"

//More object orientated, struct might be ok but more intelligent
struct Frame
{
    bool is_empty{true};
    bool is_selected{false};
    std::vector<float> samples;     //rough data -> braucht es nicht zwingend
    std::vector<Butterfly::Wavetable<float>> multitable;    //antialiased data
    unsigned int position{};        //zero based counting
    
    template<int internal_tablesize>
    void create_multitable_from_samples(float sampleRate, const std::vector<float> &split_freqs, const Butterfly::FFTCalculator<float, internal_tablesize> &fft_calculator)
    {
        Butterfly::Antialiaser antialiaser{sampleRate, fft_calculator};
        antialiaser.antialiase(samples.begin(), split_freqs.begin(), split_freqs.end(), multitable);
    }
    
    void unselect()
    {
        is_selected = false;
    }
    
    void reset()
    {
        is_selected = false;
        samples.clear();
        multitable.clear();     //multitable.empty() -> true
        is_empty = true;
    }
    
    template <int internal_tablesize>
    void flipPhase(float sampleRate, std::vector<float> &split_freqs, const Butterfly::FFTCalculator<float, internal_tablesize> &fft_calculator)
    {
        for (float &sample : samples)
        {
            sample *= -1.f;
        }
        //data public member? -> muss iwie atomic sein, da process function drauf zugreift!
//        for (auto &table : multitable)
//        {
//            for (auto &sample : table.data)
//            {
//                sample *= -1.f;
//            }
//        }
        //Noch umständlicher
        multitable.clear();
        create_multitable_from_samples(sampleRate, split_freqs, fft_calculator);
    }
    
    template<int internal_tablesize>
    void init(const std::vector<float> &data, float sampleRate, int pos, const std::vector<float> &split_freqs, const Butterfly::FFTCalculator<float, internal_tablesize> &fft_calculator)
    {
        is_empty = false;
        is_selected = true;
        position = pos;
        samples = data;
        create_multitable_from_samples(sampleRate, split_freqs, fft_calculator);
        
    }
};

struct MorphableFrame
{
    std::vector<float> samples;
    bool is_visible{false};
    float y_offset;
    float y_scaling;
    unsigned int lower_frame_idx{0};
    unsigned int upper_frame_idx{1};
    float lower_frame_weighting{0.f};
    float upper_frame_weighting{1.f};
    Butterfly::RampedValue<float> upperFrameWeighting{1.f, 100};
    
    void clear()
    {
        samples.clear();
        is_visible = false;
    }
    //Funktionen definieren im Struct!
};

class StackedFrames
{
private:
    unsigned int maxFrames{0};
//    unsigned int internalTablesize{2048};
    float sampleRate{};
    std::vector<Butterfly::Wavetable<float>> wavetable;
    Butterfly::WavetableOscillator<Butterfly::Wavetable<float>> wavetableOscillator;
    
    template<int internal_tablesize>
    Frame createFrame(const std::vector<float> &data, float sampleRate, const std::vector<float> &split_freqs, const Butterfly::FFTCalculator<float, internal_tablesize> &fft_calculator)
    {
        Frame frame;
        unselectAllFrames();
        frame.init(data, sampleRate, static_cast<int>(frames.size()), split_freqs, fft_calculator);
        
        return frame;
    }
    
    void unselectAllFrames()
    {
        for (auto &frame : frames)
        {
            frame.unselect();
        }
    }
    
    int getSelectedFrameIdx()
    {
        int selectedFrameIdx{};
        for (int i = 0; i < frames.size(); i++)
        {
            if(frames[i].is_selected)
                selectedFrameIdx = i;
            break;
        }
        return selectedFrameIdx;
    }
    
public:
    std::vector<Frame> frames;
    MorphableFrame morphableFrame;
    
    int init(int _maxFrames, float _sampleRate)
    {
        maxFrames = _maxFrames;
        sampleRate = _sampleRate;
        wavetableOscillator.setSampleRate(sampleRate);
        return 0;
    }
    
    template<int internal_tablesize>
    int addFrame(const std::vector<float> &data, float sampleRate, const std::vector<float> &split_freqs, const Butterfly::FFTCalculator<float, internal_tablesize> &fft_calculator)
    {
        if (frames.size() >= maxFrames)
        {
            return 1;
        }
        frames.push_back(createFrame(data, sampleRate, split_freqs, fft_calculator));
//        internalTablesize = internal_tablesize;
        
        return 0;
    }
    
    template<int internal_tablesize>
    void flipPhase(float sampleRate, std::vector<float> &split_freqs, const Butterfly::FFTCalculator<float, internal_tablesize> &fft_calculator)
    {
        for (auto &frame : frames)
        {
            frame.flipPhase(sampleRate, split_freqs, fft_calculator);
        }
    }
    
    void clearFrames()
    {
        frames.clear();
        morphableFrame.clear();
    }
    
    void selectFrame(int idx)
    {
        unselectAllFrames();
        frames[idx].is_selected = true;
    }
    
    int moveFrontSelectedFrame()
    {
        int selectedFrameIdx = getSelectedFrameIdx();
        
        if (selectedFrameIdx == 0)
        {
            return 1;
        }
        
        Frame tempFrame = frames[selectedFrameIdx];
        frames[selectedFrameIdx] = frames[selectedFrameIdx - 1];
        frames[selectedFrameIdx].position += 1;
        frames[selectedFrameIdx - 1] = tempFrame;
        frames[selectedFrameIdx - 1].position -= 1;
        
        return 0;
    }
    
    int moveBackSelectedFrame()
    {
        int selectedFrameIdx = getSelectedFrameIdx();
        
        if (selectedFrameIdx >= (frames.size() - 1))
        {
            return 1;
        }
        
        Frame tempFrame = frames[selectedFrameIdx];
        frames[selectedFrameIdx] = frames[selectedFrameIdx + 1];
        frames[selectedFrameIdx].position -= 1;
        frames[selectedFrameIdx + 1] = tempFrame;
        frames[selectedFrameIdx + 1].position += 1;
        
        return 0;
    }
    
    int removeSelectedFrame()
    {
        int selectedFrameIdx = getSelectedFrameIdx();
        
        if (selectedFrameIdx <= 0)
        {
            return 1;
        }
        
        frames.erase(frames.begin() + selectedFrameIdx);
        unselectAllFrames();    //Unnötig?
        frames[selectedFrameIdx].is_selected = true;
        for (int i = selectedFrameIdx; i < frames.size(); i++)
        {
            frames[i].position -= 1;
        }
        
        return 0;
    }
    
    int getStackedTable(std::vector<float> &stackedTable, int exportTablesize)
    {
        if (frames.size() <= 0)
        {
            return 1;
        }
        float exportTableOscFreq = sampleRate / static_cast<float>(exportTablesize);
        wavetableOscillator.setFrequency(exportTableOscFreq);
        for (int i = 0; i < frames.size(); i++)
        {
            Butterfly::Wavetable<float> table;
            table.setData(frames[i].samples, sampleRate / 2.f);
            wavetable.push_back(table);
            wavetableOscillator.setTable(&wavetable);
            std::vector<float> interpolatedTable;
            for (int i = 0; i < exportTablesize; i++)
            {
                interpolatedTable.push_back(wavetableOscillator++);
            }
            //Interpolation has to be done to each individual frame
            stackedTable.insert(stackedTable.end(), interpolatedTable.begin(), interpolatedTable.end());
        }
        return 0;
    }
    
};

int calculate_split_freqs(std::vector<float> &split_freqs, float semitones = 2.f, float highest_split_freq = 22050.f, int lowest_split_freq = 5.f)
{
    float current_freq = highest_split_freq;
    const float factor = 1 / pow(2.f,(semitones / 12.f));
    while (current_freq > lowest_split_freq)
    {
        split_freqs.push_back(current_freq);
        current_freq = current_freq * factor;
    }
    std::reverse(split_freqs.begin(), split_freqs.end());
    return split_freqs.size();
}

//StackedFrames Klasse schreiben? -> gute Idee
//MorphableFrame könnte member von StackedFrames Klasse sein -> yes
std::optional<int> get_next_free_frame(const std::vector<Frame> &frames)
{
    for (int i = 0; i < frames.size(); i++)         //Get first free frame
    {
        if (frames[i].is_empty)                     //multitable.empty() -> remove is_empty flag -> make sure to reset frame!
        {
            return i;
        }
    }
    return std::nullopt;
}
