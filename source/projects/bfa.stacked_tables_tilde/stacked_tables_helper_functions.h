//
//  stacked_tables_helper_functions.h
//  Butterfly-Audio_min-devkit
//
//  Created by Jonas Kieser on 07.10.22.
//

//More object orientated, struct might be ok but more intelligent
struct Frame
{
    bool is_empty{true};
    bool is_selected{false};
    std::vector<float> samples;     //rough data
    std::vector<Butterfly::Wavetable<float>> multitable;    //antialiased data
    unsigned int position{};        //zero based counting
    
    template<int internal_tablesize>
    void create_multitable_from_samples(float sampleRate, std::vector<float> &split_freqs, const Butterfly::FFTCalculator<float, internal_tablesize> &fft_calculator)
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
};

//Factory function (screenshot)

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
    
    //Funktionen definieren im Struct!
};

//Not a nice function. Refactor! With default arguments 73 intervals!
//Not sample rate dependent
std::vector<float> calculate_split_freqs(float semitones = 2.f, float highest_split_freq = 22050.f, int lowest_split_freq = 5.f)
{
    std::vector<float> split_freqs;
    float current_freq = highest_split_freq;
    const float factor = 1 / pow(2.f,(semitones / 12.f));
    while (current_freq > lowest_split_freq)
    {
        split_freqs.push_back(current_freq);
        current_freq = current_freq * factor;
    }
    std::reverse(split_freqs.begin(), split_freqs.end());
    return split_freqs;
}

//StackedFrames Klasse schreiben?
//MorphableFrame könnte member von StackedFrames Klasse sein
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

void unselect_all_frames(std::vector<Frame> &frames)
{
    for (auto &frame : frames)
    {
        frame.unselect();
    }
}

void select_current_frame(int current_pos, std::vector<Frame> &frames)
{
    unselect_all_frames(frames);
    frames[current_pos].is_empty = false;
    frames[current_pos].is_selected = true;
    frames[current_pos].position = current_pos;
}

void clear_all_frames(std::vector<Frame> &frames)
{
    for (auto &frame : frames)
    {
        frame.reset();
    }
}
