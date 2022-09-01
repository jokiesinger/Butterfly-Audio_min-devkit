/// @file
/// @ingroup     minexamples
/// @copyright    Copyright 2018 The Min-DevKit Authors. All rights reserved.
/// @license    Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"
#include "../../../submodules/Butterfly_Audio_Library/src/wave/src/waveform_processing.h"
#include "../../../submodules/Butterfly_Audio_Library/src/wave/src/pitch_detection.h"

using namespace c74::min;
using namespace c74::min::ui;

struct OverlayRect
{
    int x{0}, y{0};
    float width{0.f}, height{0.f};
    bool visible{false};
};

class table_preprocessing : public object<table_preprocessing>, public vector_operator<>, public ui_operator<160, 80> {
public:
    MIN_DESCRIPTION     { "Read from a buffer~ and display." };
    MIN_TAGS            { "audio, sampling, ui" };
    MIN_AUTHOR          { "Jonas Kieser" };
    MIN_RELATED         { "index~, buffer~, wave~, waveform~" };

    inlet<>  m_inlet_new_sample         { this, "(message) new sample dropped" };
    outlet<> m_outlet_changed           { this, "(message) Notification that the content of the buffer~ changed." };
    
    table_preprocessing(const atoms& args = {}) : ui_operator::ui_operator {this, args} {
        overlay_rect.width = 0.f;
    }
    
    buffer_reference m_input_buffer { this,                   //Constructor of buffer reference before input_buffer_name attribute
        MIN_FUNCTION {                                  // will receive a symbol arg indicating 'binding', 'unbinding', or 'modified'
            m_outlet_changed.send(args);
            return {};
        }
    };
    
    buffer_reference m_target_buffer { this,                   //Constructor of buffer reference before input_buffer_name attribute
        MIN_FUNCTION {                                  // will receive a symbol arg indicating 'binding', 'unbinding', or 'modified'
            m_outlet_changed.send(args);
            return {};
        }
    };
    
    //Members, whose state is addressable, queryable and saveable from Max
    attribute<color> m_bgcolor {this, "Background Color", color::predefined::gray, title {"Background Color"}};
    
    attribute<color> m_elementcolor {this, "Waveform Color", color::predefined::black};
    attribute<color> overlay_color {this, "Overlay Color", {0.f, .9f, .9f, .3f}};
    
    attribute<int, threadsafe::no, limit::clamp> m_channel
    {
        this, "Channel", 1,
        description {"Channel to read from the buffer~. The channel number uses 1-based counting."},
        range {1, buffer_reference::k_max_channels}
    };
    
    attribute<symbol> m_input_buffer_name
    {
        this, "Input Buffer", "input_buffer", description {"Name of buffer~ to read from."}
    };          //Kann ich keine MIN_FUNCTION aus dem attribute raus ausrufen? Evtl. im Konstruktor? Oder einfach nach der attribute initialisation?
    
    attribute<symbol> m_target_buffer_name
    {
        this, "Target Buffer", "target_buffer", description{"Name of buffer~ to write to"}
    };
    
    message<> dspsetup{ this, "dspsetup",
        MIN_FUNCTION {
            sampleRate = static_cast<float>(args[0]);
            
            cout << "dspsetup happend" << endl;

            return {};
        }
    };
    
    message<> set_mode
    {
        this, "set_mode", "Set frame selection mode. 0: Period, 1: Custom.", MIN_FUNCTION
        {
            mode = static_cast<int>(args[0]);
            
            if (mode == 0)
            {
                overlay_rect.visible = false;
            }
            else if (mode == 1)
            {
                overlay_rect.visible = true;
            }
            
            redraw();
            
            return{};
        }
    };
    
    message<> m_sample_dropped
    {
        this, "sample_dropped", "Read again from the buffer~.", MIN_FUNCTION
        {
            m_input_buffer.set(m_input_buffer_name);
            m_target_buffer.set(m_target_buffer_name);
            buffer_lock<> buf(m_input_buffer);
            auto chan = std::min<size_t>(m_channel - 1, buf.channel_count());
            
            if (buf.valid())
            {
                input_array.clear();
                for (auto i = 0; i < buf.frame_count(); ++i)
                {
                    input_array.push_back(buf.lookup(i, chan));
                }
            }
            
            //Analyze zero-crossings
            zero_crossings = Butterfly::getCrossings<std::vector<float>::iterator>(input_array.begin(), input_array.end());
            period_zero_crossings.clear();
            
            //Perform pitch detection
            pitchInfoOptional = Butterfly::getPitch(input_array.begin(), input_array.end());
            if (pitchInfoOptional) {pitchInfo = *pitchInfoOptional;}
            
            //Period duration
            tf0 = 1.f / pitchInfo.frequency;
            
            //Relevant zero-crossings
            for (int i = 0; i < zero_crossings.size(); i++)
            {
                float second_period_crossing_idx = zero_crossings[i] + tf0;
                for (int j = i + 1; j < zero_crossings.size(); j++)
                {
                    if (zero_crossings[j] > (second_period_crossing_idx - deviation_in_smps) && zero_crossings[j] < (second_period_crossing_idx + deviation_in_smps))
                    {
                        period_zero_crossings.push_back(zero_crossings[i]);
                        break;
                    }
                }
            }
            redraw();
            return{};
        }
    };
    
    message<> mousedown
    {
        this, "mousedown", MIN_FUNCTION
        {
            event e {args};
            overlay_rect.x = e.x();
//            overlay_rect.y = e.y();
//            auto x {e.x()};
//            auto y {e.y()};
//
//            mouse_position[0] = x;
//            mouse_position[1] = y;
            
            return{};
        }
    };
    
    message<> mouseup
    {
        this, "mouseup", MIN_FUNCTION
        {
            event e {args};
            auto mouseup_x = e.x();
            
            if (mode == 0)
            {
                //scale
                float factor = static_cast<float>(input_array.size()) / external_width;
                float scaled_mouse_x = mouseup_x * factor;
                
                //find nearest
                selected_zero_crossing = period_zero_crossings[0];
                for (int i = 0; i < (period_zero_crossings.size() - 1); i++)
                {
                    if ((abs(period_zero_crossings[i + 1] - scaled_mouse_x)) < (abs(selected_zero_crossing - scaled_mouse_x)))
                    {
                        selected_zero_crossing = period_zero_crossings[i + 1];
                    }
                }
            }
            else if (mode == 1)
            {
                if (mouseup_x > overlay_rect.x)
                {
                    overlay_rect.visible = true;
                    overlay_rect.width = mouseup_x - overlay_rect.x;
                }
                else if (mouseup_x < overlay_rect.x)
                {
                    overlay_rect.visible = true;
                    overlay_rect.width = overlay_rect.x - mouseup_x;
                    overlay_rect.x = mouseup_x;
                }
                else if (mouseup_x == overlay_rect.x)
                {
                    overlay_rect.visible = false;
                }
            }
            
            //Hier die selection array geschichte machen…
            
            redraw();
            
            return{};
        }
    };
    
    //Probably mousedown position for constant selection updates
//    message<> mousedragdelta {this, "mousedragdelta",
//        MIN_FUNCTION
//        {
//            event e {args};
//
//            mouse_position[0] = e.x();
//            mouse_position[1] = e.y();
//
//            cout << "Drag mouse_position X: " << mouse_position[0] << endl;
//            cout << "Drag mouse_position Y: " << mouse_position[1] << endl;
//
//            return{};
//        }
//    };
    
    message<> generate_frame
    {
        this, "generate_frame", MIN_FUNCTION
        {
            if (overlay_rect.visible || (selected_zero_crossing > 0))
            {
                buffer_lock<> buf(m_target_buffer);
                int target_tablesize = buf.frame_count();
                int source_tablesize = selection_array.size();
                float frac = static_cast<float>(source_tablesize) / static_cast<float>(target_tablesize);
                float position = 0.f;
                
                if (buf.valid())
                {
                    lib::interpolator::linear<> linear_interpolator;
                    for (int i = 0; i < target_tablesize; i++)
                    {
                        int lower_index = floor(position);
                        int upper_index = ceil(position);
                        upper_index = upper_index > (source_tablesize - 1) ? (source_tablesize - 1) : upper_index;
                        float delta = position - static_cast<float>(lower_index);
                        float interpolated_value = linear_interpolator.operator()(selection_array[lower_index], selection_array[upper_index], delta);
                        buf[i] = interpolated_value;
                        position += frac;
                    }
                }
                m_outlet_changed("newFrame");
            }
            
            return{};
        }
    };
    
    message<> paint
    {
        this, "paint", MIN_FUNCTION
        {
            target t        {args};
            external_height = t.height();
            external_width = t.width();
            
            float factor = static_cast<float>(input_array.size()) / external_width;
            float factor_inv = static_cast<float>(external_width / static_cast<float>(input_array.size()));
            selection_start = static_cast<int>(floor(overlay_rect.x * factor));
            selection_end = static_cast<int>(floor((overlay_rect.x + overlay_rect.width) * factor));
            selection_array.clear();
            
            rect<fill>
            {                //Background
                t,
                color {m_bgcolor}
            };
            
            
            if (input_array.size() > 0)         //Check if input_buffer is non-zero
            {
                float origin_x = 0.f;
                float origin_y = external_height * .5f;
                for (int i = 0; i < (input_array.size()); i++)
                {
                    float frac = static_cast<float>(i) / static_cast<float>(input_array.size());
                    float x = frac * external_width;
                    float y = (input_array[i] * external_height * -.5f) + (external_height * .5f);
                    number stroke_width = 1;
                    
                    if (mode == 0)
                    {
                        if (i >= floor(selected_zero_crossing) && i <= floor(selected_zero_crossing + tf0))
                        {
                            stroke_width = 2;
                            selection_array.push_back(input_array[i]);
                        }
                    }
                    
                    if (mode == 1)
                    {
                        if (i >= selection_start && i <= selection_end)
                        {
                            stroke_width = 2;
                            selection_array.push_back(input_array[i]);
                        }
                    }

                    line<stroke>
                    {
                        t,
                        color {m_elementcolor},
                        origin {origin_x, origin_y},
                        destination{x, y},
                        line_width{stroke_width}
                    };
                    
                    origin_x = x;
                    origin_y = y;
                }
            }
            
            if (overlay_rect.visible)
            {
                overlay_rect.height = t.height();
                rect<fill>
                {
                    t,
                    color {overlay_color},
                    position{overlay_rect.x, overlay_rect.y},
                    size{overlay_rect.width, overlay_rect.height}
                };
            }
            
            if (mode == 0)
            {
            float scale_height = (external_height * 0.2f);
                for (auto value : period_zero_crossings)
                {
                    float x_position = value * factor_inv;
                    line<stroke>
                    {
                        t,
                        color {color::predefined::white},
                        origin {x_position, 0.f + scale_height},
                        destination {x_position, external_height - scale_height},
                        line_width{1}
                    };
                }
                line<stroke>
                {
                    t,
                    color {color::predefined::black},
                    origin {selected_zero_crossing * factor_inv, 0.f + scale_height},
                    destination {selected_zero_crossing * factor_inv, external_height - scale_height},
                    line_width{2}
                        
                };
            }

        return {};
        }
    };
    
//    void analyse_period_zero_crossings()
//    {
//
//    }

    void operator()(audio_bundle input, audio_bundle output) {
        
//        auto          in  = input.samples(0);                                     // get vector for channel 0 (first channel)
//        auto          out = output.samples(0);                                    // get vector for channel 0 (first channel)
//        buffer_lock<> b(m_buffer);                                                  // gain access to the buffer~ content -> outside process()
//        auto          chan = std::min<size_t>(m_channel - 1, b.channel_count());    // convert from 1-based indexing to 0-based
//
//        if (b.valid()) {
//            for (auto i = 0; i < input.frame_count(); ++i) {
//                auto frame = size_t(in[i] + 0.5);
//                out[i]     = b.lookup(frame, chan);
//            }
//        }
//        else {
//            output.clear();
//        }
    }
    
private:
    std::vector<float> input_array;             //from buffer~
    std::vector<float> selection_array;         //copy selection
    std::vector<float> target_array;            //resampling target
    std::vector<double> zero_crossings, period_zero_crossings;   //store zero crossings that mark full period
    OverlayRect overlay_rect;
    int selection_start, selection_end;         //refers to indices in input_buffer
    float selected_zero_crossing;
    int mode{0};
    std::optional<Butterfly::PitchInfo> pitchInfoOptional;
    Butterfly::PitchInfo pitchInfo;
    float sampleRate{48000.f};
    float tf0, deviation_in_smps{10.f};
    float external_height, external_width;
    
};


MIN_EXTERNAL(table_preprocessing);


//    message<> input_buffer_name
//    {
//        this, "input_buffer_name", "Set input buffer~ reference.", MIN_FUNCTION
//        {
//            m_input_buffer.set(args[0]);
//            redraw();
//            return{};
//        }
//    };
//
//    message<> target_buffer_name
//    {
//        this, "target_buffer_name", "Set target buffer~ reference.", MIN_FUNCTION
//        {
//            m_target_buffer.set(args[0]);
//            redraw();
//            return{};
//        }
//    };
