/// @file
/// @ingroup     minexamples
/// @copyright    Copyright 2018 The Min-DevKit Authors. All rights reserved.
/// @license    Use of this source code is governed by the MIT License found in the License.md file.


#include "c74_min.h"

#include "waveform_processing.h"
#include "ramped_value.h"

#include "stacked_tables_helper_functions.h"

//using namespace Butterfly;
using namespace c74::min;
using namespace c74::min::ui;


class stacked_tables : public object<stacked_tables>, public vector_operator<>, public ui_operator<160, 160>
{
private:
//    std::vector<Frame> frames;      //Das könnte auch ein Attribut werden
//    StackedFrames stackedFrames;
    MorphableFrame morphableFrame;
    
    int selected_frame;
    int n_intervals;
    float spacing;
    float y_scaling;
    float margin{10.f};
    
    float sampleRate{48000.f};

    float pos{1.f};
    static constexpr int internalTablesize{2048};
    
    std::vector<float> split_freqs;
    const Butterfly::FFTCalculator<float, internalTablesize> fft_calculator;
//    Butterfly::MorpingWavetableOscillator<Butterfly::WavetableOscillator<Butterfly::Wavetable<float>>> morphingWavetableOscillator{sampleRate};
    
    Butterfly::RampedValue<float> morphingParam{1.f, 1000};   //Stimmt das mit UI überein?
    Butterfly::RampedValue<float> outputGain{0.f, 1000};
    
public:
    MIN_DESCRIPTION     { "Display and edit stacked frames." };
    MIN_TAGS            { "audio, wavetable, ui" };
    MIN_AUTHOR          { "BFA_JK" };
    MIN_RELATED         { "index~, buffer~, wave~, wavetable~" };

    inlet<>  message_in      { this, "(message) Messages in."};
    outlet<> message_out     { this, "(message) Messages out."};
    outlet<> output          { this, "(signal) Synthesized wavetable signal out.", "signal"};
    
    buffer_reference input_buffer { this,                   //Constructor of buffer reference before input_buffer_name attribute
        MIN_FUNCTION {                                      // will receive a symbol arg indicating 'binding', 'unbinding', or 'modified'
            message_out.send(args);
            return {};
        }
    };
    
    buffer_reference output_buffer { this,                   //Constructor of buffer reference before input_buffer_name attribute
        MIN_FUNCTION {                                      // will receive a symbol arg indicating 'binding', 'unbinding', or 'modified'
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
    
    attribute<int> maxFrames
    {
        this, "Max stacked Frames", 16, description{"Maximum number of possible stacked frames. Has to match number of stacked_frames_buffer channels."}
    };
    
    attribute<int> export_tablesize
    {
        this, "Export tablesize", 1048, description{"Default export tablesize."}
    };
    
    attribute<double> oscillatorFreq
    {
        this, "Osc Freq", 80., description{"Oscillator Frequency."}
    };
         

    stacked_tables(const atoms& args = {}) : ui_operator::ui_operator {this, args}
    {
        n_intervals = calculate_split_freqs(split_freqs);
        
        morphableFrame.stackedFrames.init(maxFrames);
        
//        for (auto &frame : frames)
//        {
//            frame.multitable.resize(n_intervals);
//            for (auto &table : frame.multitable)
//            {
//                table.resize(internal_tablesize, 0.f);
//            }
//        }
        
//        morphingWavetableOscillator.setTable(&frames[0].multitable, &frames[1].multitable);
//        morphingWavetableOscillator.setFrequency(80.f);     //Das könnte ein Attribut sein
//        morphingWavetableOscillator.setParam(0.f);
    }
    
    message<> dspsetup{ this, "dspsetup",
        MIN_FUNCTION {
            sampleRate = static_cast<float>(args[0]);
            
            morphableFrame.init(sampleRate, internalTablesize, oscillatorFreq);
            
            cout << "dspsetup happend" << endl;

            return {};
        }
    };
    
    //Nicht so viele if - else schachteln
    //Lieber mehrere, kleine member functions anlegen
    message<> add_frame
    {
        this, "add_frame", "Read from input buffer", MIN_FUNCTION
        {
            //Read from buffer
            input_buffer.set(input_buffer_name);
            buffer_lock<> buf(input_buffer);
            auto chan = std::min<size_t>(m_channel - 1, buf.channel_count());
            if (buf.channel_count() != 1)
            {
                cout << "Buffer channel count has to be one.\n";
                return{};
            }
            if (!buf.valid())
            {
                return{};
            }
            
            if(!(buf.frame_count() == internalTablesize))
            {
                cout << "Buffer size has to be 2048 samples.\n";
                return{};
            }
            
            std::vector<float> data;
            for (auto i = 0; i < buf.frame_count(); i++)         //Get samples
            {
                data.push_back(buf.lookup(i, chan));
            }
            
            int state;
            state = morphableFrame.addFrame(data, sampleRate, split_freqs, fft_calculator);
            if (state == 0)
            {
                cout << "Frame succesfully added.\n";
            }
            if (state == 1)
            {
                cout << "Max frame count reached." << endl;
                message_out.send("Max frame count reached");    //Das dem Nutzer prompten
            }
            redraw();
            return{};
        }
    };
    
    message<> flip_phase
    {
        this, "flip_phase", MIN_FUNCTION
        {
            morphableFrame.stackedFrames.flipPhase(sampleRate, split_freqs, fft_calculator);
            redraw();
            return{};
        }
    };
    
    message<> move_up_selected_frame    {
        this, "move_up_selected_frame", MIN_FUNCTION
        {
            if (morphableFrame.stackedFrames.moveUpSelectedFrame() != 0)
            {
                cout << "Can't move up selected frame.\n";
            }
            
            redraw();
            return {};
        }
    };

    message<> move_down_selected_frame
    {
        this, "move_down_selected_frame", MIN_FUNCTION
        {
            if (morphableFrame.stackedFrames.moveDownSelectedFrame() != 0)
            {
                cout << "Can't move down selected frame.\n";
            }

            redraw();
            return{};
        }
    };
    
    message<> delete_selected_frame
    {
        this, "delete_selected_frame", MIN_FUNCTION
        {
            if (morphableFrame.removeSelectedFrame() != 0)
            {
                cout << "No frame to delete.\n";
            }
            redraw();
            return {};
        }
    };
    
    message<> clear_all
    {
        this, "clear_all", MIN_FUNCTION
        {
            morphableFrame.stackedFrames.clearFrames();
            
            redraw();
            
            return{};
        }
    };
    
    message<> mousedown
    {
        this, "mousedown", MIN_FUNCTION
        {
            event e {args};
            auto mouse_y = e.y();
            ///TODO: spacing Berechnung und Verwendung überprüfen
            int y_click = floor(mouse_y / (spacing + 1.f));
            morphableFrame.stackedFrames.selectFrame(y_click);
            redraw();
            cout << "Selected Frame: " << y_click << endl;
            return{};
        }
    };
    
    message<> morph_position
    {
        this, "morph_position", MIN_FUNCTION
        {
            morphableFrame.setMorphingPos(static_cast<float>(args[0])); //Range check
            redraw();
            
            return{};
        }
    };
    
    message<> set_freq
    {
        this, "set_freq", MIN_FUNCTION
        {
            oscillatorFreq = static_cast<double>(args[0]);   ///Range clampen!
            morphableFrame.morphingWavetableOscillator.setFrequency(oscillatorFreq);
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
    
    message<> set_export_tablesize
    {
        this, "set_export_tablesize", MIN_FUNCTION
        {
            export_tablesize = static_cast<int>(args[0]);   //Range check
            return{};
        }
    };
    
    message<> export_table
    {
        this, "export_table", MIN_FUNCTION
        {
            std::vector<float> stackedTable;
            if (morphableFrame.stackedFrames.getStackedTable(stackedTable, export_tablesize, 44100) != 0)
            {
                cout << "No table to export.\n";
            }
            
            message_out("export_buffer_length", stackedTable.size());    //set buffer~ size
            
            output_buffer.set(output_buffer_name);
            buffer_lock<> buf(output_buffer);
            
            if (buf.channel_count() > 1)
            {
                cout << "Output buffer has more than one channel (has to be one).\n";
                return{};
            }

            if (buf.valid())
            {
                for (int i = 0; i < buf.frame_count(); i++)
                {
                    buf[i] = stackedTable[i];
                }

                message_out("exporting_done");
            }

            
            return{};
        }
    };
    
    ///-----GRAPHICS-----
    message<> paint
    {
        this, "paint", MIN_FUNCTION
        {
            target t {args};
            float height = t.height() - margin;
            float width = t.width() - margin;
            int nActiveFrames = morphableFrame.stackedFrames.frames.size();
            spacing = height / static_cast<float>(nActiveFrames);
            y_scaling = ((height - 10.f) / 2.f) / static_cast<float>(nActiveFrames);
            
            rect<fill>              //Background
            {
                t,
                color {background_color}
            };
            
            for (int i = 0; i < nActiveFrames; i++)
            {
                drawStackedFrames(i, t);
            } //Draw active frames, könnte man mit nem if kombinieren, um nicht bei jedem neuen Morph-Frame zu zeichnen?
            draw_morphable_frame(t);
            return {};
        }
    };
    ///TODO: Check und kann vrmtl. weg
    /*
    void calculate_morphing_frame()
    {
        float position = pos * static_cast<float>(nactive_frames - 1);
        morphable_frame.samples.clear();                //clearen und neu pushbacken = Katastrophe!
        
        if (nactive_frames == 0 || nactive_frames == 1)
        {
            morphable_frame.is_visible = false;
        }
        else if (nactive_frames > 1 && nactive_frames <= maxFrames)
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
            
            for (int i = 0; i < internal_tablesize; i++)
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
    */
     
    //Das ist hier ok -> Max spezifisch
    void drawStackedFrames(int f, target t)
    {
        float y_offset = (spacing * static_cast<float>(morphableFrame.stackedFrames.frames[f].position)) + (spacing / 2.f) + (margin / 2.f);
        float stroke_width = 1.f;
        if (morphableFrame.stackedFrames.frames[f].is_selected){stroke_width = 2.f;};
        float origin_x = margin / 2.f;
        float origin_y = (morphableFrame.stackedFrames.frames[f].samples[0] * y_scaling * -1.f) + y_offset;
        float position = 0.f;
        float width = t.width() - margin;
        float frac = static_cast<float>(internalTablesize) / width;
        lib::interpolator::linear<> linear_interpolation;
        for (int i = 0; i < width; i++)
        {
            int lower_index = floor(position);
            int upper_index = ceil(position);
            upper_index = upper_index > (internalTablesize - 1) ? (internalTablesize - 1) : upper_index;
            float delta = position - static_cast<float>(lower_index);
            float interpolated_value = linear_interpolation.operator()(morphableFrame.stackedFrames.frames[f].samples[lower_index], morphableFrame.stackedFrames.frames[f].samples[upper_index], delta);
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
    
    ///TODO: Refactor!
    void draw_morphable_frame(target t)
    {
        if (morphableFrame.is_visible)
        {
            float origin_x = 0.f + (margin / 2.f);
            float origin_y = (morphableFrame.morphingSamples[0] * y_scaling * -1.f) + morphableFrame.yOffset + (margin / 2.f);
            float position = 0.f;
            float width = t.width() - margin;
            float frac = static_cast<float>(internalTablesize) / width;
            lib::interpolator::linear<> linear_interpolation;
            for (int i = 0; i < width; i++)
            {
                int lower_index = floor(position);
                int upper_index = ceil(position);
                upper_index = upper_index > (internalTablesize - 1) ? (internalTablesize - 1) : upper_index;
                float delta = position - static_cast<float>(lower_index);
                float interpolated_value = linear_interpolation.operator()(morphableFrame.morphingSamples[lower_index], morphableFrame.morphingSamples[upper_index], delta);
                float y = (interpolated_value * y_scaling * -1.f) + morphableFrame.yOffset + (margin / 2.f);
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
    
    
    void operator()(audio_bundle input, audio_bundle output)
    {
        auto in  = input.samples(0);                           // get vector for channel 0 (first channel)
        auto out = output.samples(0);                          // get vector for channel 0 (first channel)

        for (auto i = 0; i < input.frame_count(); ++i) {
            morphableFrame.morphingWavetableOscillator.setParam(++morphingParam);
            out[i]     = morphableFrame.morphingWavetableOscillator++ * outputGain++;
        }
    }
    
};

MIN_EXTERNAL(stacked_tables);
