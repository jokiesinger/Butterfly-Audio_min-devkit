/// @file
/// @ingroup     minexamples
/// @copyright    Copyright 2018 The Min-DevKit Authors. All rights reserved.
/// @license    Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"

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

    message<> input_buffer_name
    {
        this, "input_buffer_name", "Set input buffer~ reference.", MIN_FUNCTION
        {
            m_input_buffer.set(args[0]);
            redraw();
            return{};
        }
    };
    
    message<> target_buffer_name
    {
        this, "target_buffer_name", "Set target buffer~ reference.", MIN_FUNCTION
        {
            m_target_buffer.set(args[0]);
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
//
            cout << "Down mouse_position X: " << e.x() << endl;
//            cout << "Down mouse_position Y: " << e.y() << endl;
            
            return{};
        }
    };
    
    message<> mouseup
    {
        this, "mouseup", MIN_FUNCTION
        {
            event e {args};
            auto mouseup_x = e.x();
            
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
            
//            cout << "Up mouse_position X: " << e.x() << endl;
            
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
            if (overlay_rect.visible)
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
            float array_height = t.height();
            float array_width = t.width();
            
            float factor = static_cast<float>(input_array.size()) / array_width;
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
                float origin_y = array_height * .5f;
                for (int i = 0; i < (input_array.size()); i++)
                {
                    float frac = static_cast<float>(i) / static_cast<float>(input_array.size());
                    float x = frac * array_width;
                    float y = (input_array[i] * array_height * -.5f) + (array_height * .5f);
                    number stroke_width = 1;
                    if (i >= selection_start && i <= selection_end)
                    {
                        stroke_width = 2;
                        selection_array.push_back(input_array[i]);
                    }
                    line<stroke> {
                        t,
                        color {m_elementcolor},
                        origin {origin_x, origin_y},
                        destination{x, y},
                        line_width{stroke_width},
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

            
            return {};
        }
    };

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
    OverlayRect overlay_rect;
    int selection_start, selection_end;         //refers to indices in input_buffer
    
};


MIN_EXTERNAL(table_preprocessing);
