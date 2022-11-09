/// @file
/// @ingroup     minexamples
/// @copyright    Copyright 2018 The Min-DevKit Authors. All rights reserved.
/// @license    Use of this source code is governed by the MIT License found in the License.md file.


#include "c74_min.h"

#include "antialiase.h"
#include "waveform_processing.h"
#include "wavetable_oscillator.h"
#include "ramped_value.h"


#include "stacked_tables_helper_functions.h"

//using namespace Butterfly;
using namespace c74::min;
using namespace c74::min::ui;

class stacked_tables : public object<stacked_tables>, public vector_operator<>, public ui_operator<160, 160>
{
private:
    int nIntervalls;
    float spacing;
    float yScaling;
    float yOffset;
    float margin{10.f};
    
    float sampleRate{48000.f};

    float oscFreq{80.f};
    static constexpr int internalTablesize{2048};   //Die wollen wir nicht ändern
    static constexpr int maxFrames{16};
    
    std::vector<float> splitFreqs;
    const Butterfly::FFTCalculator<float, internalTablesize> fftCalculator;
    
    MultiFrameOsc multiFrameOsc;

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
    
    buffer_reference output_buffer {
        this, MIN_FUNCTION {
            message_out.send(args);
            return {};
        }
    };

    attribute<color> background_color {this, "Background Color", color::predefined::gray, title {"Background Color"}};
    attribute<color> frame_color {this, "Frame Color", color::predefined::black};
    attribute<color> morphed_frame_color {this, "Morphed Frame Color", {1.f, 1.f, 1.f, 1.f}};
    
    attribute<int, threadsafe::no, limit::clamp> m_channel {
        this, "Channel", 1,
        description {"Channel to read from the buffer~. The channel number uses 1-based counting."},
        range {1, buffer_reference::k_max_channels}
    };
    
    attribute<symbol> output_buffer_name {
        this, "Export Buffer", "outputBuffer", description {"Name of buffer~ to write to."}
    };
    
    attribute<symbol> input_buffer_name {
        this, "Input Buffer", "targetBuffer", description{"Name of buffer~ to read from"}
    };
  
    //Static for now
//    attribute<int> maxFrames
//    {
//        this, "Max stacked Frames", 16, description{"Maximum number of possible stacked frames. Has to match number of stacked_frames_buffer channels."}
//    };
    
    attribute<int> export_tablesize {
        this, "Export tablesize", 1048, description{"Default export tablesize."}
    };
    
    attribute<double> oscillatorFreq {
        this, "Osc Freq", 80., description{"Oscillator Frequency."}
    };
         

    stacked_tables(const atoms& args = {}) : ui_operator::ui_operator {this, args}, multiFrameOsc{sampleRate, internalTablesize, oscFreq, maxFrames} {
//        multiFrameOsc = {sampleRate, internalTablesize, oscFreq, maxFrames};
        nIntervalls = calculateSplitFreqs(splitFreqs);
    }
    
    message<> dspsetup {
        this, "dspsetup", MIN_FUNCTION {
            sampleRate = static_cast<float>(args[0]);
            multiFrameOsc.Osc.setSampleRate(sampleRate);
            cout << "dspsetup happend" << endl;
            return {};
        }
    };
    
    message<> add_frame {
        this, "add_frame", "Read from input buffer", MIN_FUNCTION {
            //Read from buffer
            input_buffer.set(input_buffer_name);
            buffer_lock<> buf(input_buffer);
            auto chan = std::min<size_t>(m_channel - 1, buf.channel_count());
            if (buf.channel_count() != 1) {
                cout << "Buffer channel count has to be one.\n";
                return{};
            }
            if (!buf.valid()) { return{}; }
            if(!(buf.frame_count() == internalTablesize)) {
                cout << "Buffer size has to be 2048 samples.\n";
                return{};
            }
            std::vector<float> data;
            for (auto i = 0; i < buf.frame_count(); i++){         //Get samples
                data.push_back(buf.lookup(i, chan));
            }
            
            if (multiFrameOsc.addFrame(data, sampleRate, splitFreqs, fftCalculator)){
                cout << "Frame succesfully added.\n";
            } else {
                cout << "Max frame count reached." << endl;
                message_out.send("Max frame count reached");    //Das dem Nutzer prompten
            }
            redraw();
            return{};
        }
    };
    
    message<> flip_phase {
        this, "flip_phase", MIN_FUNCTION {
            multiFrameOsc.stackedFrames.flipPhase();
            multiFrameOsc.updateMorphingSamples();
            redraw();
            return{};
        }
    };
    
    message<> move_up_selected_frame {
        this, "move_up_selected_frame", MIN_FUNCTION {
            if (!multiFrameOsc.stackedFrames.moveUpSelectedFrame()) {
                cout << "Can't move up selected frame.\n";
            } else {
                multiFrameOsc.calculateIds();
            }
            redraw();
            return {};
        }
    };

    message<> move_down_selected_frame {
        this, "move_down_selected_frame", MIN_FUNCTION {
            if (!multiFrameOsc.stackedFrames.moveDownSelectedFrame()) {
                cout << "Can't move down selected frame.\n";
            } else {
                multiFrameOsc.calculateIds();
            }
            redraw();
            return{};
        }
    };
    
    message<> delete_selected_frame {
        this, "delete_selected_frame", MIN_FUNCTION {
            if (!multiFrameOsc.stackedFrames.removeSelectedFrame()) {
                cout << "No frame to delete.\n";
            } else {
                multiFrameOsc.calculateIds();
            }
            redraw();
            return {};
        }
    };
    
    message<> clear_all {
        this, "clear_all", MIN_FUNCTION {
            multiFrameOsc = {sampleRate, internalTablesize, oscFreq, maxFrames};
            redraw();
            return{};
        }
    };
    
    message<> mousedown {
        this, "mousedown", MIN_FUNCTION {
            event e {args};
            auto mouse_y = e.y();
            ///TODO: spacing Berechnung und Verwendung überprüfen
            int y_click = floor(mouse_y / (spacing + 1.f));
            multiFrameOsc.stackedFrames.selectFrame(y_click);
            redraw();
            cout << "Selected Frame: " << y_click << endl;
            return{};
        }
    };
    
    message<> morph_position {
        this, "morph_position", MIN_FUNCTION {
            multiFrameOsc.setPos(std::clamp(static_cast<float>(args[0]), 0.f, 1.f));
            redraw();
            return{};
        }
    };
    
    message<> set_freq {
        this, "set_freq", MIN_FUNCTION {
            oscillatorFreq = std::clamp(static_cast<double>(args[0]), 1., static_cast<double>(sampleRate) / 2.);
            multiFrameOsc.Osc.setFrequency(oscillatorFreq);
            return{};
        }
    };
    
    message<> set_output_gain {
        this, "set_output_gain", MIN_FUNCTION {
            outputGain.set(std::clamp(static_cast<float>(args[0]), 0.f, 1.f));
            return{};
        }
    };
    
    message<> set_export_tablesize {
        this, "set_export_tablesize", MIN_FUNCTION {
            export_tablesize = static_cast<int>(args[0]);   //Range check
            return{};
        }
    };
    
    message<> export_table {
        this, "export_table", MIN_FUNCTION {
            std::vector<float> stackedTable;
            if (multiFrameOsc.stackedFrames.getStackedTable(stackedTable, export_tablesize, sampleRate) != 0) {
                cout << "No table to export.\n";
            }
            message_out("export_buffer_length", stackedTable.size());    //set buffer~ size
            output_buffer.set(output_buffer_name);
            buffer_lock<> buf(output_buffer);
            
            if (buf.channel_count() > 1) {
                cout << "Output buffer has more than one channel (has to be one).\n";
                return{};
            }
            if (buf.valid()) {
                for (int i = 0; i < buf.frame_count(); i++) {
                    buf[i] = stackedTable[i];
                }
                message_out("exporting_done");
            }
            return{};
        }
    };
    
    ///-----GRAPHICS-----
    message<> paint {
        this, "paint", MIN_FUNCTION {
            target t {args};
            float height = t.height() - margin;
            float width = t.width() - margin;
            int nActiveFrames = multiFrameOsc.stackedFrames.frames.size();
            spacing = height / static_cast<float>(nActiveFrames);
            yScaling = ((height - 10.f) / 2.f) / static_cast<float>(nActiveFrames);
            
            rect<fill> {              //Background
                t,
                color {background_color}
            };
            draw_morphable_frame(t);
            for (int i = 0; i < nActiveFrames; i++) { drawStackedFrames(i, t); }
            
            return {};
        }
    };
     
    void drawStackedFrames(int f, target t) {
        yOffset = (spacing * static_cast<float>(f)) + (spacing / 2.f) + (margin / 2.f);
        float stroke_width = 1.f;
        if (multiFrameOsc.stackedFrames.frames[f].isSelected){stroke_width = 1.5f;};
        float origin_x = margin / 2.f;
        float origin_y = (multiFrameOsc.stackedFrames.frames[f].samples[0] * yScaling * -1.f) + yOffset;
        float position = 0.f;
        float width = t.width() - margin;
        float frac = static_cast<float>(internalTablesize) / width;
        lib::interpolator::linear<> linear_interpolation;
        for (int i = 0; i < width; i++) {
            int lower_index = floor(position);
            int upper_index = ceil(position);
            upper_index = upper_index > (internalTablesize - 1) ? (internalTablesize - 1) : upper_index;
            float delta = position - static_cast<float>(lower_index);
            float interpolated_value = linear_interpolation.operator()(multiFrameOsc.stackedFrames.frames[f].samples[lower_index], multiFrameOsc.stackedFrames.frames[f].samples[upper_index], delta);
            float y = (interpolated_value * yScaling * -1.f) + yOffset;
            int x = i + static_cast<int>(margin / 2.f);
            line<stroke> {
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
    
    float updateMorphFrameYOffset(target t) {
        float yOffset = (spacing / 2.f) + (margin / 2.f);
        float morphFrameYOffset = multiFrameOsc.pos * (t.height() - (yOffset * 2.f));
        return morphFrameYOffset + yOffset;
    }
    
    void draw_morphable_frame(target t) {
        if (multiFrameOsc.isVisible) {
            float morphFrameYOffset = updateMorphFrameYOffset(t);
            float origin_x = 0.f + (margin / 2.f);
            float origin_y = (multiFrameOsc.morphingSamples[0] * yScaling * -1.f) + morphFrameYOffset;
            float position = 0.f;
            float width = t.width() - margin;
            float frac = static_cast<float>(internalTablesize) / width;
            lib::interpolator::linear<> linear_interpolation;
            for (int i = 0; i < width; i++) {
                int lower_index = floor(position);
                int upper_index = ceil(position);
                upper_index = upper_index > (internalTablesize - 1) ? (internalTablesize - 1) : upper_index;
                float delta = position - static_cast<float>(lower_index);
                float interpolated_value = linear_interpolation.operator()(multiFrameOsc.morphingSamples[lower_index], multiFrameOsc.morphingSamples[upper_index], delta);
                float y = (interpolated_value * yScaling * -1.f) + morphFrameYOffset;
                int x = i + static_cast<int>(margin / 2.f);
                line<stroke> {
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
            out[i] = ++multiFrameOsc * outputGain++;
        }
    }
};

MIN_EXTERNAL(stacked_tables);
