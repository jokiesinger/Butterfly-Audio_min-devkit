/// @file
/// @ingroup     minexamples
/// @copyright    Copyright 2018 The Min-DevKit Authors. All rights reserved.
/// @license    Use of this source code is governed by the MIT License found in the License.md file.


#include "c74_min.h"
#include "../../../submodules/Butterfly_Audio_Library/src/wave/src/antialiase.h"
#include "../../../submodules/Butterfly_Audio_Library/src/wave/src/waveform_processing.h"
#include "../../../submodules/Butterfly_Audio_Library/src/synth/src/wavetable_oscillator.h"
#include "../../../submodules/Butterfly_Audio_Library/src/utilities/src/ramped_value.h"
//#include "antialiase.h"
//#include "waveform_processing.h"
//#include "wavetable_oscillator.h"


//using namespace Butterfly;
using namespace c74::min;
using namespace c74::min::ui;

struct Frame
{
    bool is_empty{true};
    bool is_selected{false};
    std::vector<float> samples;
    std::vector<Butterfly::Wavetable<float>> multitable{73};    //Must match number of split freqs
    unsigned int position{};        //zero based counting
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
    
    //Funktionen definieren im Struct!
};

class stacked_tables : public object<stacked_tables>, public vector_operator<>, public ui_operator<160, 160>
{
public:
    MIN_DESCRIPTION     { "Display and edit stacked frames." };
    MIN_TAGS            { "audio, wavetable, ui" };
    MIN_AUTHOR          { "Jonas Kieser" };
    MIN_RELATED         { "index~, buffer~, wave~, wavetable~" };

    inlet<>  message_in      { this, "(message) Messages in."};
    outlet<> message_out     { this, "(message) Messages out."};
    outlet<> output          { this, "(signal) Synthesized wavetable signal out.", "signal"};
            
    stacked_tables(const atoms& args = {}) : ui_operator::ui_operator {this, args}
    {
        split_freqs = calculate_split_freqs();
        std::vector<float> data;
        data.resize(2048, 0);
        for (int i = 0; i < frames.size(); i++)
        {
            for (int j = 0; j < frames[i].multitable.size(); j++)
            {
                frames[i].multitable[j].setData(data);
            }
            
        }
        
        morphingWavetableOscillator.setTable(&frames[0].multitable, &frames[1].multitable);
        morphingWavetableOscillator.setFrequency(80.f);
        morphingWavetableOscillator.setParam(0.f);
    }
    
    buffer_reference input_buffer { this,                   //Constructor of buffer reference before input_buffer_name attribute
        MIN_FUNCTION {                                  // will receive a symbol arg indicating 'binding', 'unbinding', or 'modified'
            message_out.send(args);
            return {};
        }
    };
    
    buffer_reference output_buffer { this,                   //Constructor of buffer reference before input_buffer_name attribute
        MIN_FUNCTION {                                  // will receive a symbol arg indicating 'binding', 'unbinding', or 'modified'
            message_out.send(args);
            return {};
        }
    };
    
    //Members, whose state is addressable, queryable and saveable from Max
    attribute<color> background_color {this, "Background Color", color::predefined::gray, title {"Background Color"}};
    
    attribute<color> frame_color {this, "Frame Color", color::predefined::black};
    attribute<color> morphed_frame_color {this, "Morphed Frame Color", {1.f, 1.f, 1.f, 1.f}};
    
    attribute<int, threadsafe::no, limit::clamp> m_channel
    {
        this, "Channel", 1,
        description {"Channel to read from the buffer~. The channel number uses 1-based counting."},
        range {1, buffer_reference::k_max_channels}
    };
    
    attribute<symbol> output_buffer_name
    {
        this, "Export Buffer", "output_buffer", description {"Name of buffer~ to write to."}
    };          //Kann ich keine MIN_FUNCTION aus dem attribute raus ausrufen? Evtl. im Konstruktor? Oder einfach nach der attribute initialisation?
    
    attribute<symbol> input_buffer_name
    {
        this, "Input Buffer", "target_buffer", description{"Name of buffer~ to read from"}
    };
    
    attribute<int> nframes
    {
        this, "Max stacked Frames", 16, description{"Maximum number of possible stacked frames. Has to match number of stacked_frames_buffer channels."}
    };
    
    attribute<int> tablesize
    {
        this, "Tablesize", 2048, description{"Tablesize of input and stacked frames buffer."}
    };
    
    message<> dspsetup{ this, "dspsetup",
        MIN_FUNCTION {
            sampleRate = static_cast<float>(args[0]);
            
            morphingWavetableOscillator.setSampleRate(sampleRate);
            
            cout << "dspsetup happend" << endl;

            return {};
        }
    };
    
    message<> add_frame
    {
        this, "add_frame", "Read from input buffer", MIN_FUNCTION
        {
            if (nactive_frames >= nframes){cout << "Max frame count reached." << endl;}
            else if (nactive_frames < nframes)
            {
                unsigned int current_pos;
                for (int i = 0; i < frames.size(); i++)         //Get first free frame
                {
                    if (frames[i].is_empty)
                    {
                        current_pos = i;
                        break;
                    }
                }
                input_buffer.set(input_buffer_name);
                buffer_lock<> buf(input_buffer);
                auto chan = std::min<size_t>(m_channel - 1, buf.channel_count());
                if (buf.valid())
                {
                    assert(buf.frame_count() == 2048);
//                    frames[current_pos].samples.clear();
                    frames[current_pos].samples.resize(2048, 0);
                    
                    for (auto i = 0; i < buf.frame_count(); i++)             //Get samples
                    {
                        frames[current_pos].samples[i] = (buf.lookup(i, chan));
                    }
                }
                unselect_all();
                frames[current_pos].is_empty = false;
                frames[current_pos].is_selected = true;
                frames[current_pos].position = current_pos;
                selected_frame = current_pos;
                
                create_multitable_from_samples(current_pos);
                
                nactive_frames++;
                calculate_morphing_frame();
                redraw();
            }
            
            if (nactive_frames == 1)
            {
                morphingWavetableOscillator.setTable(&frames[0].multitable, &frames[1].multitable);
            }
            return{};
        }
    };
    
    message<> clear_all
    {
        this, "clear_all", MIN_FUNCTION
        {
            for (int i = 0; i < nframes; i++)
            {
                frames[i].is_empty = true;
                frames[i].samples.clear();
                for (auto &table : frames[i].multitable)
                {
                    table.data.clear();
                }
            }
            nactive_frames = 0;
            calculate_morphing_frame();
            redraw();
            return{};
        }
    };
    
    message<> mousedown
    {
        this, "mousedown", MIN_FUNCTION
        {
            unselect_all();
            event e {args};
            auto mouse_y = e.y();
            int y_click = floor(mouse_y / (spacing + 1.f));
            frames[y_click].is_selected = true;
            selected_frame = y_click;
            redraw();
            cout << "Selected Frame: " << y_click << endl;
            return{};
        }
    };
    
    message<> delete_selected_frame
    {
        this, "delete_selected_frame", MIN_FUNCTION
        {
            if (nactive_frames > 0)
            {
                nactive_frames -= 1;
                for (int i = selected_frame; i <nframes; i++)
                {
                    if (i < (nframes - 1))
                    {
                        int next_frame = i + 1;
                        frames[i].samples = frames[next_frame].samples;
                        frames[i].position = frames[next_frame].position - 1;
                        frames[i].is_empty = frames[next_frame].is_empty;
                        
                        if (frames[next_frame].is_empty)
                        {
                            frames[selected_frame].is_selected = false;
                            
                            if(selected_frame == 0)
                            {
                                frames[0].is_selected = true;
                            }
                            else
                            {
                                frames[selected_frame - 1].is_selected = true;
                            }
                            break;
                        }
                    }
                    if (i == (nframes - 1))     //last frame
                    {
                        frames[i].samples.clear();
                        frames[i].is_empty = true;
                        frames[i].is_selected = false;
                        frames[i-1].is_selected = true;
                        break;
                    }
                }
                
                if(!(selected_frame == 0)){selected_frame -= 1;}
            }
            calculate_morphing_frame();
            redraw();
            return {};
        }
    };
    
    message<> morph_position
    {
        this, "morph_position", MIN_FUNCTION
        {
            pos = args[0];
            
            calculate_morphing_frame();
            
            return{};
        }
    };
    
    void calculate_morphing_frame()
    {
        float position = pos * static_cast<float>(nactive_frames - 1);
        morphable_frame.samples.clear();                //clearen und neu pushbacken = Katastrophe!
        
        if (nactive_frames == 0 || nactive_frames == 1)
        {
            morphable_frame.is_visible = false;
        }
        else if (nactive_frames > 1 && nactive_frames <= nframes)
        {
            morphable_frame.is_visible = true;
            morphable_frame.y_offset = (spacing * position) + (spacing / 2);
            morphable_frame.lower_frame_idx = floor(position);
            morphable_frame.upper_frame_idx = ceil(position);
            cout << "Lower frame idx: " << morphable_frame.lower_frame_idx << endl;
            cout << "Upper frame idx: " << morphable_frame.upper_frame_idx << endl;
//            morphable_frame.upper_frame_idx = morphable_frame.lower_frame_idx + 1;
            morphable_frame.lower_frame_weighting = morphable_frame.upper_frame_idx - position;
//            morphable_frame.upper_frame_weighting = position - morphable_frame.lower_frame_idx;
            morphable_frame.upper_frame_weighting = 1.f - morphable_frame.lower_frame_weighting;
            cout << "Lower frame weighting: " << morphable_frame.lower_frame_weighting << endl;
            cout << "Upper frame weighting: " << morphable_frame.upper_frame_weighting << endl;
            
            for (int i = 0; i < tablesize; i++)
            {
                if (morphable_frame.lower_frame_idx == morphable_frame.upper_frame_idx)
                {
                    morphable_frame.samples.push_back(frames[morphable_frame.upper_frame_idx].samples[i]);
                }
                else
                {
                    morphable_frame.samples.push_back((frames[morphable_frame.upper_frame_idx].samples[i] * morphable_frame.upper_frame_weighting) + (frames[morphable_frame.lower_frame_idx].samples[i] * morphable_frame.lower_frame_weighting));
                }
            }
            //Setup Morphing Wavetable Oscillator
            morphingWavetableOscillator.setTable(&frames[morphable_frame.lower_frame_idx].multitable, &frames[morphable_frame.upper_frame_idx].multitable);
            
            morphingParam.set(morphable_frame.upper_frame_weighting);
        }
        
        redraw();
    }
    
    message<> set_export_tablesize
    {
        this, "set_export_tablesize", MIN_FUNCTION
        {
            export_tablesize = static_cast<int>(args[0]);
            return{};
        }
    };
    
    message<> export_table
    {
        this, "export_table", MIN_FUNCTION
        {
            if (nactive_frames > 0)
            {
                int buffer_length = export_tablesize * nactive_frames;
                message_out("export_buffer_length", buffer_length);    //set buffer~ size
                
                output_buffer.set(output_buffer_name);
                buffer_lock<> buf(output_buffer);
                auto chan = std::min<size_t>(m_channel - 1, buf.channel_count());
                std::vector<float> concatenated_frames;
                for (int i = 0; i < nactive_frames; i++)
                {
                    concatenated_frames.insert(concatenated_frames.end(), frames[i].samples.begin(), frames[i].samples.end());
                }
                if (buf.valid())
                {
                    if (export_tablesize == tablesize)
                    {
                        for (int i = 0; i < buffer_length; i++)
                        {
                            buf[i] = concatenated_frames[i];
                        }
                    }
                    else        //Interpolate into buffer
                    {
                        float position = 0.f;
                        float frac = static_cast<float>(concatenated_frames.size()) / buffer_length;
                        lib::interpolator::linear<> linear_interpolation;
                        for (int i = 0; i < buffer_length; i++)
                        {
                            int lower_index = floor(position);
                            int upper_index = ceil(position);
                            upper_index = upper_index > (concatenated_frames.size() - 1) ? (concatenated_frames.size() - 1) : upper_index;
                            float delta = position - static_cast<float>(lower_index);
                            float interpolated_value = linear_interpolation.operator()(concatenated_frames[lower_index], concatenated_frames[upper_index], delta);
                            buf[i] = interpolated_value;
                            position += frac;
                        }
                    }
                    //Buffer~ wieder unlocken? Passiert wohl wenn scope beendet ist.
                }

                message_out("exporting_done");
            }

            
            return{};
        }
    };
    
    message<> move_up_selected_frame
    {
        this, "move_up_selected_frame", MIN_FUNCTION
        {
            if (selected_frame == 0){cout << "Can't move up selected frame." << endl;}
            else
            {
                Frame temporary_frame = frames[selected_frame - 1];
                frames[selected_frame - 1] = frames[selected_frame];
                frames[selected_frame - 1].position = frames[selected_frame].position - 1;
                frames[selected_frame] = temporary_frame;
                frames[selected_frame].position = temporary_frame.position + 1;
                frames[selected_frame].is_selected = false;
                
                selected_frame -= 1;
                calculate_morphing_frame();
                redraw();
            }
            return {};
        }
    };

    message<> move_down_selected_frame
    {
        this, "move_down_selected_frame", MIN_FUNCTION
        {
            if(selected_frame == (nactive_frames - 1)){cout << "Can't move down selected framd." << endl;}
            else
            {
                Frame temporary_frame = frames[selected_frame + 1];
                frames[selected_frame + 1] = frames[selected_frame];
                frames[selected_frame + 1].position = frames[selected_frame].position + 1;
                frames[selected_frame] = temporary_frame;
                frames[selected_frame].position = temporary_frame.position - 1;
                frames[selected_frame].is_selected = false;
                selected_frame += 1;
                calculate_morphing_frame();
                redraw();
            }
            return{};
        }
    };
    
    message<> set_freq
    {
        this, "set_freq", MIN_FUNCTION
        {
            float freq = static_cast<float>(args[0]);   ///Range clampen!
            morphingWavetableOscillator.setFrequency(freq);
            return{};
        }
    };
    
    message<> set_output_gain
    {
        this, "set_output_gain", MIN_FUNCTION
        {
            outputGain.set(static_cast<float>(args[0]));         ///Range clampen!
            return{};
        }
    };
    
    message<> flip_phase
    {
        this, "flip_phase", MIN_FUNCTION
        {
            for (auto &frame : frames)
            {
                if (frame.is_selected)
                {
                    for (int i = 0; i < frame.samples.size(); i++)
                    {
                        frame.samples[i] = frame.samples[i] * -1.f;
                    }
                    //Run Antialiaser again and stick to private data member?
                    for (auto &table : frame.multitable)
                    {
                        for (int i = 0; i < table.data.size(); i++)
                        {
                            table.data[i] = table.data[i] * -1.f;
                        }
                    }
                    calculate_morphing_frame();
                    redraw();
                    break;
                }
            }
            return{};
        }
    };
    
    message<> paint
    {
        this, "paint", MIN_FUNCTION
        {
            target t {args};
            float height = t.height() - margin;
            float width = t.width() - margin;
            spacing = height / static_cast<float>(nactive_frames);
            y_scaling = ((height - 10.f) / 2.f) / static_cast<float>(nactive_frames);
            
            rect<fill>              //Background
            {
                t,
                color {background_color}
            };
            
            for (int i = 0; i < nactive_frames; i++){draw_frame(i, t);} //Draw active frames, könnte man mit nem if kombinieren, um nicht bei jedem neuen Morph-Frame zu zeichnen?
            draw_morphable_frame(t);
            return {};
        }
    };
    
    void draw_frame(int f, target t)
    {
        float y_offset = (spacing * static_cast<float>(frames[f].position)) + (spacing / 2.f) + (margin / 2.f);
        float stroke_width = 1.f;
        if (frames[f].is_selected){stroke_width = 2.f;};
        float origin_x = margin / 2.f;
        float origin_y = (frames[f].samples[0] * y_scaling * -1.f) + y_offset;
        float position = 0.f;
        float width = t.width() - margin;
        float frac = static_cast<float>(tablesize) / width;
        lib::interpolator::linear<> linear_interpolation;
        for (int i = 0; i < width; i++)
        {
            int lower_index = floor(position);
            int upper_index = ceil(position);
            upper_index = upper_index > (tablesize - 1) ? (tablesize - 1) : upper_index;
            float delta = position - static_cast<float>(lower_index);
            float interpolated_value = linear_interpolation.operator()(frames[f].samples[lower_index], frames[f].samples[upper_index], delta);
            float y = (interpolated_value * y_scaling * -1.f) + y_offset;
            int x = i + static_cast<int>(margin / 2.f);
            line<stroke>
            {
                t,
                color{frame_color},
                origin{origin_x, origin_y},
                destination{x, y},
                line_width{stroke_width}
            };
            position += frac;
            origin_x = x;
            origin_y = y;
        }
    }
    
    void draw_morphable_frame(target t)
    {
        if (morphable_frame.is_visible)
        {
            float origin_x = 0.f + (margin / 2.f);
            float origin_y = (morphable_frame.samples[0] * y_scaling * -1.f) + morphable_frame.y_offset + (margin / 2.f);
            float position = 0.f;
            float width = t.width() - margin;
            float frac = static_cast<float>(tablesize) / width;
            lib::interpolator::linear<> linear_interpolation;
            for (int i = 0; i < width; i++)
            {
                int lower_index = floor(position);
                int upper_index = ceil(position);
                upper_index = upper_index > (tablesize - 1) ? (tablesize - 1) : upper_index;
                float delta = position - static_cast<float>(lower_index);
                float interpolated_value = linear_interpolation.operator()(morphable_frame.samples[lower_index], morphable_frame.samples[upper_index], delta);
                float y = (interpolated_value * y_scaling * -1.f) + morphable_frame.y_offset + (margin / 2.f);
                int x = i + static_cast<int>(margin / 2.f);
                line<stroke>
                {
                    t,
                    color{morphed_frame_color},
                    origin{origin_x, origin_y},
                    destination{x, y},
                    line_width{2}
                };
                position += frac;
                origin_x = x;
                origin_y = y;
            }
        }
    }

    void unselect_all()
    {
        for (int i = 0; i < nframes; i++){frames[i].is_selected = false;}
        selected_frame -= 1;
    }
    
    void create_multitable_from_samples(int _current_pos)
    {
        Butterfly::Antialiaser antialiaser{sampleRate, fft_calculator};
        antialiaser.antialiase(frames[_current_pos].samples.begin(), split_freqs.begin(), split_freqs.end(), frames[_current_pos].multitable);
    }
    
    //Not a nice function. Refactor! With default arguments 73 intervals!
    std::vector<float> calculate_split_freqs(float interval = 2.f, float highest_split_freq = 22050.f, int lowest_split_freq = 5.f)
    {
        std::vector<float> _split_freqs;
        float current_freq = highest_split_freq;
        float factor = pow(2.f,(interval / 12.f));
        while (current_freq > lowest_split_freq)
        {
            
            _split_freqs.push_back(current_freq);
            current_freq = current_freq / factor;
        }
        std::reverse(_split_freqs.begin(), _split_freqs.end());
        return _split_freqs;
    }
    
    
    void operator()(audio_bundle input, audio_bundle output)
    {
        auto in  = input.samples(0);                           // get vector for channel 0 (first channel)
        auto out = output.samples(0);                          // get vector for channel 0 (first channel)

        for (auto i = 0; i < input.frame_count(); ++i) {
            morphingWavetableOscillator.setParam(++morphingParam);
            out[i]     = ++morphingWavetableOscillator * ++outputGain;
        }
    }
    
private:
    std::vector<Frame> frames{16};
    MorphableFrame morphable_frame;
    unsigned int nactive_frames{0};
    int selected_frame;
    float spacing;
    float y_scaling;
    float margin{10.f};
    float sampleRate{48000.f};
    //float outputGain;
    float pos{1.f};
    int export_tablesize{2048};
    std::vector<float> split_freqs;
    const Butterfly::FFTCalculator<float, 2048> fft_calculator;
    std::vector<Butterfly::Wavetable<float>> tables{11};    //Das ist ja nur für ein Frame!
    Butterfly::MorpingWavetableOscillator<Butterfly::WavetableOscillator<Butterfly::Wavetable<float>>> morphingWavetableOscillator{sampleRate};
    Butterfly::RampedValue<float> morphingParam{1.f, 1000};   //Stimmt das mit UI überein?
    Butterfly::RampedValue<float> outputGain{0.f, 1000};
};

MIN_EXTERNAL(stacked_tables);
