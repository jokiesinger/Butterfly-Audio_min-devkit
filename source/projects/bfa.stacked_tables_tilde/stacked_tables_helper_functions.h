//
//  stacked_tables_helper_functions.h
//  Butterfly-Audio_min-devkit
//
//  Created by Jonas Kieser on 07.10.22.
//

#include <concepts>
#include "wavetable_oscillator.h"

struct Idcs
{
    int firstIdx{0};
    int secondIdx{1};
};

template<int internal_tablesize>
Frame createFrame(const std::vector<float>& data, float sampleRate, int pos, const std::vector<float>& split_freqs,
	const Butterfly::FFTCalculator<float, internal_tablesize>& fft_calculator) {
	Frame f;
	f.is_empty    = false;
	f.is_selected = true;
	f.position    = pos;
	f.samples     = data;
	multitable.resize(split_freqs.size());
	Butterfly::Antialiaser antialiaser {sampleRate, fft_calculator};
	antialiaser.antialiase(samples.begin(), split_freqs.begin(), split_freqs.end(), multitable);
	return frame;
}


//More object orientated, struct might be ok but more intelligent
struct Frame
{
        
    using Wavetable = Butterfly::Wavetable<float>>;
    bool is_empty{true};
    bool is_selected{false};
    std::vector<float> samples;     //rough data -> braucht es nicht zwingend
    std::vector<Wavetable> multitable;     // antialiased data
    unsigned int position{};        //zero based counting
    

    // weg
    template<int internal_tablesize>
    void create_multitable_from_samples(float sampleRate, const std::vector<float> &split_freqs, const Butterfly::FFTCalculator<float, internal_tablesize> &fft_calculator)
    {
        multitable.resize(split_freqs.size());
        Butterfly::Antialiaser antialiaser{sampleRate, fft_calculator};
        antialiaser.antialiase(samples.begin(), split_freqs.begin(), split_freqs.end(), multitable);
    }

    // -> factory function
	template<int internal_tablesize>
	void init(const std::vector<float>& data, float sampleRate, int pos, const std::vector<float>& split_freqs,
		const Butterfly::FFTCalculator<float, internal_tablesize>& fft_calculator) {
		is_empty    = false;
		is_selected = true;
		position    = pos;
		samples     = data;
		create_multitable_from_samples(sampleRate, split_freqs, fft_calculator);
	}
    void unselect() // frame.is_selected = false
    {
        is_selected = false;
    }
    
    void reset() // -> frame = {};
    {
        is_selected = false;
        samples.clear();
        multitable.clear();     //multitable.empty() -> true
        is_empty = true;
    }
    

    // replace with operator*=(double) for wavetable
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
    
};


class StackedFrames
{
private:

//    unsigned int internalTablesize{2048};
    float sampleRate{}; // kann weg
    

    // redundant
    //template<int internal_tablesize>
    //Frame createFrame(const std::vector<float> &data, float sampleRate, const std::vector<float> &split_freqs, const Butterfly::FFTCalculator<float, internal_tablesize> &fft_calculator)
    //{
    //    Frame frame;
    //    unselectAllFrames();
    //    frame.init(data, sampleRate, static_cast<int>(frames.size()), split_freqs, fft_calculator);
    //    
    //    return frame;
    //}
    
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
            {
                selectedFrameIdx = i;
                break;
            }

        }
        return selectedFrameIdx;
    }
    
public:
    unsigned int maxFrames{0};
    std::vector<Frame> frames;

    //MorphableFrame morphableFrame;
    
    StackedFrames() = default;
    
    int init(int _maxFrames)
    {
        maxFrames = _maxFrames;
//        sampleRate = _sampleRate;
        
        return 0;
    }
    
    template<int internal_tablesize>
    int addFrame(const std::vector<float> &data, float sampleRate, const std::vector<float> &split_freqs, const Butterfly::FFTCalculator<float, internal_tablesize> &fft_calculator)
    {
        if (frames.size() >= maxFrames)
        {
            return 1;
		}
		unselectAllFrames();
        frames.push_back(createFrame(data, sampleRate, split_freqs, fft_calculator)); // use global factory function
//        internalTablesize = internal_tablesize;
        
        return 0;
    }
    
    template<int internal_tablesize>
    void flipPhase(float sampleRate, std::vector<float> &split_freqs, const Butterfly::FFTCalculator<float, internal_tablesize> &fft_calculator)
    {
        for (auto &frame : frames)
        {
            if (frame.is_selected)
            {
                frame.flipPhase(sampleRate, split_freqs, fft_calculator);
                break;
            }
        }
    }
    
    void clearFrames()
    {
        frames.clear();
//        morphableFrame.clear();
    }
    
    void selectFrame(int idx)
    {
        unselectAllFrames();
        frames[idx].is_selected = true;
    }
    

    int moveUpSelectedFrame()    // -> return type bool
    {
        int selectedFrameIdx = getSelectedFrameIdx();
        
        if (selectedFrameIdx == 0)
        {
            return 1;
        }

		// swap
        Frame tempFrame = frames[selectedFrameIdx];
        frames[selectedFrameIdx] = frames[selectedFrameIdx - 1];
        frames[selectedFrameIdx].position += 1;
        frames[selectedFrameIdx - 1] = tempFrame;
        frames[selectedFrameIdx - 1].position -= 1;
        
        return 0;
    }
    
    int moveDownSelectedFrame()    // -> return type bool
    {
        int selectedFrameIdx = getSelectedFrameIdx();
        
        if (selectedFrameIdx >= (frames.size() - 1))
        {
            return 1;
        }
        
        //swap
        Frame tempFrame = frames[selectedFrameIdx];
        frames[selectedFrameIdx] = frames[selectedFrameIdx + 1];
        frames[selectedFrameIdx].position -= 1;
        frames[selectedFrameIdx + 1] = tempFrame;
        frames[selectedFrameIdx + 1].position += 1;
        
        return 0;
    }
    
    int removeSelectedFrame() // -> return type bool
    {
        int selectedFrameIdx = getSelectedFrameIdx();
        
        frames.erase(frames.begin() + selectedFrameIdx);
        unselectAllFrames();    //Unnötig?
        frames[selectedFrameIdx].is_selected = true; /// Oh oh oh !!!!!!!!!!!!!!!
        //for (int i = selectedFrameIdx; i < frames.size(); i++)
        //{
        //    frames[i].position -= 1;
        //}
        
        return 0;
    }
    
    int getStackedTable(std::vector<float>& stackedTable, int exportTablesize, float samplerate)    // -> return type bool
    {
        if (frames.size() <= 0)
        {
            return 1;
	     }

        
    using Wavetable = typename Frame::Wavetable;
    using Osc = Butterfly::WavetableOscillator<Wavetable>;
        // -> local oscillator
		 Osc wavetableOscillator(samplerate);
	std::vector<Wavetable> wavetable;

        float exportTableOscFreq = sampleRate / static_cast<float>(exportTablesize);
        wavetableOscillator.setFrequency(exportTableOscFreq);
        for (const auto& frame : frames) // (double for looop with i)
        {
            Butterfly::Wavetable<float> table {frame.samples, sampleRate / 2.f};

            std::vector<Butterfly::Wavetable<float>> wavetable {table};
			wavetableOscillator.setTable(&wavetable); // probably works like this. "{}" will create a vector because that is what is exoected
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

//Sollte das nicht besser von StackedFrames erben?

class MorphableFrame
{
private:
    void initMorphingOscillatorWithNoTable()
    {
        morphingWavetableOscillator.setParam(0.f);
        morphingWavetableOscillator.setTable(&zeroWavetable, &zeroWavetable);
        setMorphingPos(0.f);
    }
    void initMorphingOscillatorWithOneTable()
    {
        morphingWavetableOscillator.setParam(0.f);
        morphingWavetableOscillator.setTable(&stackedFrames.frames[0].multitable, &zeroWavetable);
        setMorphingPos(0.f);
    }
    
    float pos{0.f};

public:
    
    MorphableFrame() = default;
    
    StackedFrames stackedFrames{};        //Factory Function? // composition!! very good job
    
    using Wavetable = typename Frame::Wavetable;
    using Osc = Butterfly::WavetableOscillator<Wavetable>;
    using MorpingOsc = Butterfly::MorpingWavetableOscillator<Osc>;
	MorpingOsc morphingWavetableOscillator {}; // rename -> osc
    
    std::vector<float> morphingSamples;     //for graphics
    bool is_visible{false};
    float yOffset{};
    float yScaling{1.};
    
    std::vector<float> zeroData;
    Wavetable zeroTable;
	std::vector<Wavetable> zeroWavetable;
        
    unsigned int firstFrameIdx{0};  //reference to frames.samples[i]
    unsigned int secondFrameIdx{1};
    float firstFrameWeighting{1.f};
    float secondFrameWeighting{0.f};

    void calculateNewIds(float pos)
    {
        if (stackedFrames.frames.size() == 0 || 1)
        {
            is_visible = false;
        }
    }
    
    void clear()
    {
        morphingSamples.clear();
        is_visible = false;
    }
    
    void init(float sampleRate, int internalTablesize, float oscFreq)
    {
        //Nicht der beste Ort
        zeroData.resize(internalTablesize, 0.f);
        zeroTable.setData(zeroData);
        zeroTable.setMaximumPlaybackFrequency(sampleRate / 2.f);
        zeroWavetable.push_back(zeroTable);
        
        morphingWavetableOscillator.setSampleRate(sampleRate);
        morphingWavetableOscillator.setFrequency(oscFreq);
        
        if (stackedFrames.frames.size() == 0)
        {
            initMorphingOscillatorWithNoTable();
        }
        else if (stackedFrames.frames.size() == 1)
        {
            initMorphingOscillatorWithOneTable();
        }
        else
        {
//            morphingWavetableOscillator.setParam(0.f);
            morphingWavetableOscillator.setTable(&stackedFrames.frames[firstFrameIdx].multitable, &stackedFrames.frames[secondFrameIdx].multitable);
        }
    }
    
    void updateMorphingSamples()
    {
        for (int i = 0; i < morphingSamples.size(); i++)
        {
            morphingSamples[i] = stackedFrames.frames[firstFrameIdx].samples[i] * firstFrameWeighting + stackedFrames.frames[secondFrameIdx].samples[i] * secondFrameWeighting;
        }
    }
    
    void refreshMorphingFrame()
    {
        float position = pos * static_cast<float>(stackedFrames.frames.size() - 1);
        
        //Calculate Idcs -> geht wahrscheinlicher besser und in eigener Funktion!
        Idcs tempIdcs;
        tempIdcs.firstIdx = floor(position);
        tempIdcs.secondIdx = ceil(position);
        
        //Change only one Idx if possible (to avoid clicks)
        if (tempIdcs.firstIdx == firstFrameIdx)
        {
            secondFrameIdx = tempIdcs.secondIdx;
            
            firstFrameWeighting = (1.f - pos);
            secondFrameWeighting = (pos);
            ///TODO: Testen, k.A. ob das passt
            morphingWavetableOscillator.setTable(&stackedFrames.frames[firstFrameIdx].multitable, &stackedFrames.frames[secondFrameIdx].multitable);
            ///TODO: Pos ist ramped value und muss von process() geupdated werden!
            morphingWavetableOscillator.setParam(firstFrameWeighting);
        }
        else if (tempIdcs.firstIdx == secondFrameIdx)
        {
            firstFrameIdx = tempIdcs.secondIdx;
        }
        else if (tempIdcs.secondIdx == firstFrameIdx)
        {
            secondFrameIdx = tempIdcs.firstIdx;
        }
        else if (tempIdcs.secondIdx == secondFrameIdx)
        {
            firstFrameIdx = tempIdcs.firstIdx;
        }
        else
        {
            firstFrameIdx = tempIdcs.firstIdx;
            secondFrameIdx = tempIdcs.secondIdx;
        }
        updateMorphingSamples();
        
    }
    
    template<int internal_tablesize>
    int addFrame(const std::vector<float> &data, float sampleRate, const std::vector<float> &split_freqs, const Butterfly::FFTCalculator<float, internal_tablesize> &fft_calculator)
    {
        if (stackedFrames.frames.size() >= stackedFrames.maxFrames)
        {
            return 1;
        }
        stackedFrames.addFrame(data, sampleRate, split_freqs, fft_calculator);
        
        //Will man, dass bei jedem neu hinzugefügtem Frame, dieses direkt hörbar ist? -> Man müsste an die UI kommunizieren. Erstmal nicht.
        refreshMorphingFrame();

        
        return 0;
    }
    
    int removeSelectedFrame()
    {
        if (stackedFrames.frames.size() < 1)
        {
            return 1;
        }
        stackedFrames.removeSelectedFrame();
        
        refreshMorphingFrame();
        
        return 0;
    }
    
    void setMorphingPos(float _pos)
    {
        pos = _pos;
        //TableIds & Weightings ausrechnung & übergeben
        refreshMorphingFrame();
        
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
