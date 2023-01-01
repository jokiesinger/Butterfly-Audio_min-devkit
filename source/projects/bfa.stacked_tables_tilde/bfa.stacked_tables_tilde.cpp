/// @file
/// @ingroup     minexamples
/// @copyright    Copyright 2018 The Min-DevKit Authors. All rights reserved.
/// @license    Use of this source code is governed by the MIT License found in the License.md file.


#include "c74_min.h"

#include "waveform_processing.h"

#include "stacked_frames.h"

/*
#define INTERNAL_TABLESIZE 2048
#define MAX_FRAMES 16
inline constexpr int internalTablesize = 2048;
*/

//using namespace Butterfly;
using namespace c74::min;
using namespace c74::min::ui;

class stacked_tables_tilde : public object<stacked_tables_tilde>, public vector_operator<>, public ui_operator<160, 160>
{
private:
    int nIntervalls;
    float spacing, yScaling, yOffset;
    float margin{10.f};                             //As attribute?
    
    float sampleRate{48000.f};

    static constexpr int internalTablesize{2048};   //Die wollen wir nicht ändern. Als Konstante außerhalb der Klasse definieren?
    static constexpr int maxFrames{16};             //Die wollen wir nicht ändern. Als Konstante außerhalb der Klasse definieren?
    
    std::vector<float> splitFreqs;
    const Butterfly::FFTCalculator<float, internalTablesize> fftCalculator;
    
    Butterfly::StackedFrames stackedFrames;

    //Butterfly::RampedValue<float> outputGain{1.f, 15000}; -> fine to do in Max!

    
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
        }, false
    };
    
    buffer_reference output_buffer {
        this, MIN_FUNCTION {
            message_out.send(args);
            return {};
        }, false
    };

    attribute<color> background_color {this, "Background Color", color::predefined::gray, title {"Background Color"}};
    attribute<color> frame_color {this, "Frame Color", color::predefined::black};
    attribute<color> selection_color {this, "Selection Color", {.8f, .8f, .8f, .8f}};
    attribute<color> morphed_frame_color {this, "Morphed Frame Color", {1.f, 1.f, 1.f, 1.f}};
    attribute<bool> use_fat_lines_for_selection {this, "Draw selected waveforms fat", false};
    attribute<int> rampSteps {this, "Ramp steps per wavetable", 150,
        setter { MIN_FUNCTION {
            stackedFrames.setRampingStepsPerWavetable(args[0]);
            return args;
        }}
    };
    
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
    
    attribute<int> export_tablesize {
        this, "Export tablesize", 1048, description{"Default export tablesize."}
    };
    
    //Attribute könnte man threadsafe=yes setzen und sich vrmtl. die Queue sparen
    attribute<double> oscillatorFreq {
        this, "Osc Freq", 77.78, description{"Oscillator Frequency."}
    };
         

    stacked_tables_tilde(const atoms& args = {}) : ui_operator::ui_operator {this, args}, stackedFrames{sampleRate, internalTablesize, static_cast<float>(oscillatorFreq.get()), maxFrames} {
        splitFreqs = Butterfly::calculateSplitFreqs(2.f, sampleRate / 2.f, 5.f);
        nIntervalls = splitFreqs.size();
        stackedFrames.setRampingStepsPerWavetable(rampSteps.get());
    }
    
    message<> dspsetup {
        this, "dspsetup", MIN_FUNCTION {
            sampleRate = static_cast<float>(args[0]);
            stackedFrames.setSampleRate(sampleRate);
            cout << "dspsetup happend" << endl;
            return {};
        }
    };
    
    message<> add_frame {
        this, "add_frame", "Read from input buffer", MIN_FUNCTION {
            input_buffer.set(input_buffer_name);
            buffer_lock<false> buf(input_buffer);
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
            for (auto i = 0; i < buf.frame_count(); i++){
                data.push_back(buf.lookup(i, chan));
            }
            
            if (stackedFrames.addFrame(data, sampleRate, splitFreqs, fftCalculator)){
                cout << "Frame succesfully added.\n";
            } else {
                message_out.send("userPromt", "Max frame count reached");    //Das dem Nutzer prompten
            }
            notifyStackedTablesStatus();
            redraw();
            buf.dirty();
            return{};
        }
    };
    
    message<> flip_phase {
        this, "flip_phase", MIN_FUNCTION {
            stackedFrames.flipPhase();
            redraw();
            return{};
        }
    };
    
    message<> normalize_frame {
        this, "normalize_frame", MIN_FUNCTION {
            stackedFrames.normalize();
            redraw();
            return{};
        }
    };
    
    message<> move_up_selected_frame {
        this, "move_up_selected_frame", MIN_FUNCTION {
            stackedFrames.moveDownSelectedFrame();      //Not a bug!
            redraw();
            return {};
        }
    };

    message<> move_down_selected_frame {
        this, "move_down_selected_frame", MIN_FUNCTION {
            stackedFrames.moveUpSelectedFrame();
            redraw();
            return{};
        }
    };
    
    message<> delete_selected_frame {
		this, "delete_selected_frame", MIN_FUNCTION {
            stackedFrames.removeSelectedFrame();
            notifyStackedTablesStatus();
            redraw();
            return {};
        }
    };
    
    message<> clear_all {
        this, "clear_all", MIN_FUNCTION {
            //stackedFrames = {sampleRate, internalTablesize, static_cast<float>(oscillatorFreq.get()), maxFrames};
            stackedFrames.clearAll();
            notifyStackedTablesStatus();
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
            stackedFrames.selectFrame(y_click);
            redraw();
            //cout << "Selected Frame: " << y_click << endl;
            return{};
        }
    };
    
    message<> morph_position {
        this, "morph_position", MIN_FUNCTION {
            stackedFrames.setNormalizedMorphPos(static_cast<float>(args[0]));
            redraw();
            return{};
        }
    };
    
    message<> set_freq {
        this, "set_freq", MIN_FUNCTION {
            oscillatorFreq = std::clamp(static_cast<double>(args[0]), 1., static_cast<double>(sampleRate) / 2.);
            stackedFrames.setOscFreq(oscillatorFreq.get());
            return{};
        }
    };
    
    message<> set_output_gain {
        this, "set_output_gain", MIN_FUNCTION {
            stackedFrames.setOscGain(std::clamp(static_cast<double>(args[0]), 0., 1.));
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
            if (auto concatenatedFrames = stackedFrames.getConcatenatedFrames(export_tablesize.get())) {
                message_out("export_buffer_length", concatenatedFrames->size());    //set outbut buffer~ size in Max
                output_buffer.set(output_buffer_name);
                buffer_lock<false> buf(output_buffer);      //false: not accessing via audio thread
                if (buf.valid()) {
                    for (int i = 0; i < buf.frame_count(); i++) {
                        buf[i] = concatenatedFrames->at(i);
                    }
                    message_out("exporting_done");
                } else {
                    message_out("debug", "Output bufer not valid.");
                }
                buf.dirty();
            } else {
                message_out("userPromt", "No Wavetable to export.\n");
            }
            return{};
        }
    };
    
    //Relevant for UI activation states
    void notifyStackedTablesStatus() {
        size_t numFrames = stackedFrames.getNumFrames();
        if (numFrames == 0) {
            message_out.send("stackedTablesState", 0);
        } else if (numFrames == 1) {
            message_out.send("stackedTablesState", 1);
        } else if (numFrames > 1) {
            message_out.send("stackedTablesState", 2);
        }
    }
    
    ///==============
    ///   GRAPHICS
    ///==============
    message<> paint {
        this, "paint", MIN_FUNCTION {
            target t {args};
            float height = t.height() - margin;
            float width = t.width() - margin;
            size_t nActiveFrames = stackedFrames.getNumFrames();
            spacing = height / static_cast<float>(nActiveFrames);
            yScaling = ((height - 10.f) / 2.f) / static_cast<float>(nActiveFrames);
            
            rect<fill> { t, color {background_color} };     //Draw background
            for (int i = 0; i < nActiveFrames; i++) { drawStackedFrames(i, t); }    //Draw frames
            draw_morphable_frame(t);    //Draw morphable frame
            
            return {};
        }
    };
    
    ///TODO: Could be refactored
    void drawStackedFrames(int frameIdx, target t) {
        if (auto frameSamples = stackedFrames.getFrame(frameIdx)) {
            yOffset = (spacing * static_cast<float>(frameIdx)) + (spacing / 2.f) + (margin / 2.f);
            float stroke_width = 1.f;
            float origin_x = margin / 2.f;
            float origin_y = (frameSamples->at(0) * yScaling * -1.f) + yOffset;
            float position = 0.f;
            float width = t.width() - margin;
            float frac     = static_cast<float>(internalTablesize) / width;
            auto selectedIdx = stackedFrames.getSelectedFrameIdx();
            if (selectedIdx && frameIdx == selectedIdx) {
                 if (use_fat_lines_for_selection) {
                      stroke_width = 1.5f;
                 } else {
                      rect<fill> r{t, color {selection_color}, origin {margin / 2., yOffset-yScaling}, size {width, 2 * yScaling}};
                 }
            }
            lib::interpolator::linear<> linear_interpolation;
            for (int i = 0; i < width; i++) {
                int lower_index = floor(position);
                int upper_index = ceil(position);
                upper_index = upper_index > (internalTablesize - 1) ? (internalTablesize - 1) : upper_index;
                float delta = position - static_cast<float>(lower_index);
                float interpolated_value = linear_interpolation.operator()(frameSamples->at(lower_index), frameSamples->at(upper_index), delta);
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
        
    }
    
    float updateMorphFrameYOffset(target t) {
        float yOffset = (spacing / 2.f) + (margin / 2.f);
        float morphFrameYOffset = stackedFrames.getNormalizedMorphPos() * (t.height() - (yOffset * 2.f));
        return morphFrameYOffset + yOffset;
    }
    
    void draw_morphable_frame(target t) {
        if (auto morphedWaveform = stackedFrames.getMorphedWaveformSamples()) {
            float morphFrameYOffset = updateMorphFrameYOffset(t);
            float origin_x = 0.f + (margin / 2.f);
            float origin_y = (morphedWaveform->at(0) * yScaling * -1.f) + morphFrameYOffset;
            float position = 0.f;
            float width = t.width() - margin;
            float frac = static_cast<float>(internalTablesize) / width;
            lib::interpolator::linear<> linear_interpolation;
            for (int i = 0; i < width; i++) {
                int lower_index = floor(position);
                int upper_index = ceil(position);
                upper_index = upper_index > (internalTablesize - 1) ? (internalTablesize - 1) : upper_index;
                float delta = position - static_cast<float>(lower_index);
                float interpolated_value = linear_interpolation.operator()(morphedWaveform->at(lower_index), morphedWaveform->at(upper_index), delta);
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
    
    ///==============
    ///   AUDIO
    ///==============
    void operator()(audio_bundle input, audio_bundle output)
    {
        stackedFrames.process(output);
    }
};

MIN_EXTERNAL(stacked_tables_tilde);
