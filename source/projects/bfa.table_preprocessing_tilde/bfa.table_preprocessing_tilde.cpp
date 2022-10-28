/// @file
/// @ingroup     minexamples
/// @copyright    Copyright 2018 The Min-DevKit Authors. All rights reserved.
/// @license    Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"

#include "waveform_processing.h"
#include "pitch_detection.h"
#include "table_preprocessing_helper_functions.h"


using namespace c74::min;
using namespace c74::min::ui;

class table_preprocessing : public object<table_preprocessing>, public ui_operator<160, 80> {
private:
    std::vector<float> inputSamples;             //from buffer~
    std::vector<float> selectedSamples;         //copy selection
    std::vector<float> resampledSamples;            //resampling target
    std::vector<double> allZeroCrossings, periodZeroCrossings;   //store zero crossings that mark full period
    OverlayRect overlayRect{};
    SampleSelection sampleSelection{};
    
    enum Mode {custom, period} mode;
    
    std::optional<Butterfly::PitchInfo> pitchInfoOptional;
    Butterfly::PitchInfo pitchInfo;
    
    float sampleRate{48000.f};
    float tf0, deviationInSmps{10.f};
    float width{};
    
public:
    MIN_DESCRIPTION     { "Read from a buffer~ and display." };
    MIN_TAGS            { "audio, sampling, ui" };
    MIN_AUTHOR          { "Jonas Kieser" };
    MIN_RELATED         { "index~, buffer~, wave~, waveform~" };

    inlet<>  inletNewSample         { this, "(message) new sample dropped" };
    outlet<> outletStatus       { this, "(message) Notification that the content of the buffer~ changed." };
    
    table_preprocessing(const atoms& args = {}) : ui_operator::ui_operator {this, args} {
        overlayRect.width = 0.f;
    }
    
    buffer_reference inputBuffer { this,                   //Constructor of buffer reference before input_buffer_name attribute
        MIN_FUNCTION {                                  // will receive a symbol arg indicating 'binding', 'unbinding', or 'modified'
            outletStatus.send(args);
            return {};
        }
    };
    
    buffer_reference targetBuffer { this,                   //Constructor of buffer reference before input_buffer_name attribute
        MIN_FUNCTION {                                  // will receive a symbol arg indicating 'binding', 'unbinding', or 'modified'
            outletStatus.send(args);
            return {};
        }
    };
    
    //Members, whose state is addressable, queryable and saveable from Max
    attribute<color> bgColor {this, "Background Color", color::predefined::gray, title {"Background Color"}};
    
    attribute<color> elementColor {this, "Waveform Color", color::predefined::black};
    attribute<color> overlay_color {this, "Overlay Color", {0.f, .9f, .9f, .3f}};
    
    attribute<int, threadsafe::no, limit::clamp> m_channel {
        this, "Channel", 1,
        description {"Channel to read from the buffer~. The channel number uses 1-based counting."},
        range {1, buffer_reference::k_max_channels}
    };
    
    attribute<symbol> inputBufferName {
        this, "Input Buffer", "input_buffer", description {"Name of buffer~ to read from."}
    };          //Kann ich keine MIN_FUNCTION aus dem attribute raus ausrufen? Evtl. im Konstruktor? Oder einfach nach der attribute initialisation?
    
    attribute<symbol> targetBufferName {
        this, "Target Buffer", "target_buffer", description {"Name of buffer~ to write to"}
    };
    
    message<> dspsetup { this, "dspsetup",
        MIN_FUNCTION {
            sampleRate = static_cast<float>(args[0]);
            cout << "dspsetup happend" << endl;
            return {};
        }
    };
    
    message<> setMode {
        this, "set_mode", "Set frame selection mode (custom, period).", MIN_FUNCTION {
            if (args[0] == "custom") {
                mode = custom;
                overlayRect.visible = true;
            }
            else if (args[0] == "period") {
                mode = period;
                overlayRect.visible = false;
            }
            redraw();
            return{};
        }
    };
    
    message<> sampleDropped {
        this, "sample_dropped", "Read again from the buffer~.", MIN_FUNCTION {
            inputBuffer.set(inputBufferName);
//            targetBuffer.set(targetBufferName);
            buffer_lock<> buf(inputBuffer);
            auto chan = std::min<size_t>(m_channel - 1, buf.channel_count());
            if (buf.channel_count() != 1) {
                cout << "Buffer channel count has to be one.\n";
                return{};
            }
            if (!buf.valid()) { return{}; }
            //Read from buffer
            inputSamples.clear();
            for (auto i = 0; i < buf.frame_count(); ++i) {
                inputSamples.push_back(buf.lookup(i, chan));
            }
            
            getPitchAndZeroCrossings();
            
            redraw();
            return{};
        }
    };
    
    void getPitchAndZeroCrossings() {
        //Analyze zero-crossings
        allZeroCrossings = Butterfly::getCrossings<std::vector<float>::iterator>(inputSamples.begin(), inputSamples.end());
        periodZeroCrossings.clear();
        
        //Perform pitch detection
        pitchInfoOptional = Butterfly::getPitch(inputSamples.begin(), inputSamples.end());
        if (pitchInfoOptional) { pitchInfo = *pitchInfoOptional; }
        
        //Period duration
        tf0 = 1.f / pitchInfo.frequency;
        
        //Relevant zero-crossings
        for (int i = 0; i < allZeroCrossings.size(); i++) {
            float secondPeriodCrossingIdx = allZeroCrossings[i] + tf0;
            for (int j = i + 1; j < allZeroCrossings.size(); j++) {
                if (allZeroCrossings[j] > (secondPeriodCrossingIdx - deviationInSmps) && allZeroCrossings[j] < (secondPeriodCrossingIdx + deviationInSmps)) {
                    periodZeroCrossings.push_back(allZeroCrossings[i]);
                    break;
                }
            }
        }
    }
    
    message<> mousedown {
        this, "mousedown", MIN_FUNCTION {
            event e {args};
            overlayRect.x = e.x();
//            overlay_rect.y = e.y();
//            auto x {e.x()};
//            auto y {e.y()};
//
//            mouse_position[0] = x;
//            mouse_position[1] = y;
            
            return{};
        }
    };
    
    message<> mouseup {
        this, "mouseup", MIN_FUNCTION {
            event e {args};
            auto mouseUp = e.x();
            if (mode == 0) {
                if (periodZeroCrossings.size() > 0) {
                    //scale
                    float factor = static_cast<float>(inputSamples.size()) / width;
                    float ScaledMouseX = mouseUp * factor;
                    
                    //find nearest
                    sampleSelection.selectedZeroCrossing = periodZeroCrossings[0];
                    for (int i = 0; i < (periodZeroCrossings.size() - 1); i++)
                    {
                        if ((abs(periodZeroCrossings[i + 1] - ScaledMouseX)) < (abs(sampleSelection.selectedZeroCrossing - ScaledMouseX))) {
                            sampleSelection.selectedZeroCrossing = periodZeroCrossings[i + 1];
                        }
                    }
                }
            }
            else if (mode == 1) {
                if (mouseUp > overlayRect.x) {
                    overlayRect.visible = true;
                    overlayRect.width = mouseUp - overlayRect.x;
                }
                else if (mouseUp < overlayRect.x) {
                    overlayRect.visible = true;
                    overlayRect.width = overlayRect.x - mouseUp;
                    overlayRect.x = mouseUp;
                }
                else if (mouseUp == overlayRect.x) {
                    overlayRect.visible = false;
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
    
    message<> generate_frame {
        this, "generate_frame", MIN_FUNCTION {
            if (overlayRect.visible || (sampleSelection.selectedZeroCrossing > 0)) {
                buffer_lock<> buf(targetBuffer);
                int target_tablesize = buf.frame_count();
                int source_tablesize = selectedSamples.size();
                float frac = static_cast<float>(source_tablesize) / static_cast<float>(target_tablesize);
                float position = 0.f;
                
                if (buf.valid()) {
                    lib::interpolator::linear<> linear_interpolator;
                    for (int i = 0; i < target_tablesize; i++) {
                        int lower_index = floor(position);
                        int upper_index = ceil(position);
                        upper_index = upper_index > (source_tablesize - 1) ? (source_tablesize - 1) : upper_index;
                        float delta = position - static_cast<float>(lower_index);
                        float interpolated_value = linear_interpolator.operator()(selectedSamples[lower_index], selectedSamples[upper_index], delta);
                        buf[i] = interpolated_value;
                        position += frac;
                    }
                }
                outletStatus("newFrame");
            }
            return{};
        }
    };
    
    message<> paint {
        this, "paint", MIN_FUNCTION {
            target t        {args};
            width = t.width();
            float factor = static_cast<float>(inputSamples.size()) / t.width();
            float factorInv = static_cast<float>(t.width() / static_cast<float>(inputSamples.size()));
            sampleSelection.firstSample = static_cast<int>(floor(overlayRect.x * factor));
            sampleSelection.lastSample = static_cast<int>(floor((overlayRect.x + overlayRect.width) * factor));
            selectedSamples.clear();
            
            rect<fill> {                //Background
                t,
                color {bgColor}
            };
            if (inputSamples.size() > 0) {        //Check if input_buffer is non-zero
                float originX = 0.f;
                float originY = t.height() * .5f;
                for (int i = 0; i < (inputSamples.size()); i++){
                    float frac = static_cast<float>(i) / static_cast<float>(inputSamples.size());
                    float x = frac * t.width();
                    float y = (inputSamples[i] * t.height() * -.5f) + (t.height() * .5f);
                    number strokeWidth = 1;
                    
                    if (mode == 0){
                        if (i >= floor(sampleSelection.selectedZeroCrossing) && i <= floor(sampleSelection.selectedZeroCrossing + tf0)){
                            strokeWidth = 2;
                            selectedSamples.push_back(inputSamples[i]);
                        }
                    }
                    if (mode == 1){
                        if (i >= sampleSelection.firstSample && i <= sampleSelection.lastSample){
                            strokeWidth = 2;
                            selectedSamples.push_back(inputSamples[i]);
                        }
                    }
                    line<stroke>{
                        t,
                        color {elementColor},
                        origin {originX, originY},
                        destination{x, y},
                        line_width{strokeWidth}
                    };
                    originX = x;
                    originY = y;
                }
            }
            
            if (overlayRect.visible){
                overlayRect.height = t.height();
                rect<fill> {
                    t,
                    color {overlay_color},
                    position{overlayRect.x, overlayRect.y},
                    size{overlayRect.width, overlayRect.height}
                };
            }
            
            if (mode == 0) {
            float scaleHeight = (t.height() * 0.2f);
                for (auto value : periodZeroCrossings) {
                    float xPos = value * factorInv;
                    line<stroke> {
                        t,
                        color {color::predefined::white},
                        origin {xPos, 0.f + scaleHeight},
                        destination {xPos, t.height() - scaleHeight},
                        line_width{1}
                    };
                }
                line<stroke> {
                    t,
                    color {color::predefined::black},
                    origin {sampleSelection.selectedZeroCrossing * factorInv, 0.f + scaleHeight},
                    destination {sampleSelection.selectedZeroCrossing * factorInv, t.height() - scaleHeight},
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

    
};


MIN_EXTERNAL(table_preprocessing);
