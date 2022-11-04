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
    
    float margin{10.f};     //Könnte auch als Attribut
    float yOffset{};
    OverlayRect overlayRectFree{};
    OverlayRect overlayRectZeros{};
    TablePreprocessor tablePreprocessor;
//    SampleSelection sampleSelection{};
    
    enum Mode {free, zeros, period} mode;
    
//    std::optional<Butterfly::PitchInfo> pitchInfoOptional;
//    Butterfly::PitchInfo pitchInfo;
//    float tf0, deviationInSmps{10.f};

    
    float sampleRate{48000.f};
    int internalTablesize{2048};
    float width{}, height{};
    bool mouseDown{false};
    
public:
    MIN_DESCRIPTION     { "Read from a buffer~ and display." };
    MIN_TAGS            { "audio, sampling, ui, time" };
    MIN_AUTHOR          { "BFA_JK" };
    MIN_RELATED         { "index~, buffer~, wave~, waveform~" };

    inlet<>  inletNewSample         { this, "(message) new sample dropped" };
    outlet<> outletStatus       { this, "(message) Notification that the content of the buffer~ changed." };
    
    table_preprocessing(const atoms& args = {}) : ui_operator::ui_operator {this, args} {
//        overlayRectFree.visible = true;
    }
    
    buffer_reference inputBuffer {
        this, MIN_FUNCTION {
            outletStatus.send(args);
            return {};
        }
    };
    
    buffer_reference targetBuffer {
        this, MIN_FUNCTION {
            outletStatus.send(args);
            return {};
        }
    };
    
    //Members, whose state is addressable, queryable and saveable from Max
    attribute<color> backgroundColor {this, "Color Background", color::predefined::gray, title {"Color Background"}};
    
    attribute<color> waveformColor {this, "Color Waveform ", color::predefined::black};
    
    attribute<color> zeroCrossingsColor {this, "Color Zero Crossings", color::predefined::white};
    
    attribute<color> overlayColor {this, "Color Overlay", {0.f, .9f, .9f, .3f}};
    
    attribute<number> strokeWidth {this, "Stroke Width Samples", 1.f};
    
    attribute<number> strokeWidthSelection {this, "Stroke Width Selected Samples", 1.f};

    attribute<symbol> inputBufferName {
        this, "Input Buffer", "inputBuffer", description {"Name of buffer~ to read from."}
    };
    
    attribute<symbol> targetBufferName {
        this, "Target Buffer", "targetBuffer", description {"Name of buffer~ to write to"}
    };
    
    attribute<number> waveformYScaling {
        this, "Waveform Y Scaling Factor", 0.9
    };
    
    message<> dspsetup { this, "dspsetup",
        MIN_FUNCTION {
            sampleRate = static_cast<float>(args[0]);
            cout << "dspsetup happend" << endl;
            return {};
        }
    };
    
    message<> setMode {
        this, "setMode", "Set frame selection mode (custom, period).", MIN_FUNCTION {
            if (args[0] == "Free") {
                mode = free;
                overlayRectFree.visible = true;
                overlayRectZeros.visible = false;
            } else if (args[0] == "Zeros") {
                mode = zeros;
                overlayRectFree.visible = false;
                overlayRectZeros.visible = true;
            } else if (args[0] == "Period") {
                mode = period;
            }
            redraw();
            return{};
        }
    };
    
    message<> sampleDropped {
        this, "sampleDropped", "Read again from the buffer~.", MIN_FUNCTION {
            inputBuffer.set(inputBufferName);
            buffer_lock<> buf(inputBuffer);
            if (buf.channel_count() != 1) {
                cout << "Buffer channel count has to be one." << endl;
                return{};
            }
            if (!buf.valid()) { return{}; }
            tablePreprocessor.inputSamples.clear();
            for (auto i = 0; i < buf.frame_count(); ++i) {
                tablePreprocessor.inputSamples.push_back(buf.lookup(i, 0));
            }
            Butterfly::peakNormalize(tablePreprocessor.inputSamples.begin(), tablePreprocessor.inputSamples.end());
            tablePreprocessor.analyzeZeroCrossings();
            overlayRectFree = {};
            overlayRectZeros = {};
            if (mode == free) {overlayRectFree.visible = true;}
            else if (mode == zeros) {overlayRectZeros.visible = true;}
            redraw();   //show new samples
            return{};
        }
    };
    
    float closest(std::vector<double> &vec, float value) {
        auto const it = std::lower_bound(vec.begin(), vec.end(), value);
        if (it == vec.end()) { return -1; }
//        if (it == vec.end()) { return *vec.end(); }

        return *it;
    }
    
    float nearestZeroCrossing(int mouseX) {
        float zeroCrossing{};
        float factor = static_cast<float>(tablePreprocessor.inputSamples.size()) / width;
        //find idx of element that value is closest to
        zeroCrossing = closest(tablePreprocessor.zeroCrossings, static_cast<float>(mouseX - margin) * factor);
        ///TODO: Weniger hässlich schreiben!
        if (zeroCrossing == -1) {
            zeroCrossing = tablePreprocessor.zeroCrossings[tablePreprocessor.zeroCrossings.size() - 1];
        }
        cout << "ZeroCrossing: " << zeroCrossing << endl;
        return zeroCrossing;
    }
    
    message<> mousedown {
        this, "mousedown", MIN_FUNCTION {
            mouseDown = true;
            event e {args};
            if (mode == free) {
                overlayRectFree.x2 = e.x();     //Reset width
                overlayRectFree.x1 = e.x();
            } else if (mode == zeros) {
                //find nearest zeroCrossing
                float nearestCrossing = nearestZeroCrossing(e.x());
                float factor = width / static_cast<float>(tablePreprocessor.inputSamples.size());
//                cout << "XPixelPosition: " << nearestCrossing * factor << endl;
                //set closest xPixel in overlayRect
                overlayRectZeros.x2 = static_cast<int>(nearestCrossing * factor + margin);
                overlayRectZeros.x1 = static_cast<int>(nearestCrossing * factor + margin);
            }
            redraw();
            return{};
        }
    };
    
//    message<> mousewheel {
//        this, "mousewheel", MIN_FUNCTION {
//            event e {args};
//            cout << "WheelX: " << e.wheel_delta_x() << endl;
//            cout << "WheelY: " << e.wheel_delta_y() << endl;
//
//            return{};
//        }
//    };
    
    message<> mouseup {
        this, "mouseup", MIN_FUNCTION {
            mouseDown = false;
            event e {args};
            if (mode == free) {
                overlayRectFree.x2 = static_cast<int>(std::clamp(static_cast<float>(e.x()), margin, width + margin)) ;
            } else if (mode == zeros) {
                float nearestCrossing = nearestZeroCrossing(e.x());
                float factor = width / static_cast<float>(tablePreprocessor.inputSamples.size());
                overlayRectZeros.x2 = static_cast<int>(std::clamp(nearestCrossing * factor + margin, margin, width + margin));
            }
            redraw();
            return{};
        }
    };
    
    message<> mousedrag {
        this, "mousedrag", MIN_FUNCTION {
            event e {args};
            if (mode == free) {
                overlayRectFree.x2 = static_cast<int>(std::clamp(static_cast<float>(e.x()), margin, width + margin));

            } else if (mode == zeros) {
                float nearestCrossing = nearestZeroCrossing(e.x());
                float factor = width / static_cast<float>(tablePreprocessor.inputSamples.size());
                overlayRectZeros.x2 = static_cast<int>(std::clamp(nearestCrossing * factor + margin, margin, width + margin));
            }
            redraw();
            return {};
        }
    };
    
    //Interpolation with Hermite & fractional (espacially in "zeros" mode)
//    message<> generate_frame {
//        this, "generate_frame", MIN_FUNCTION {
//            if (overlayRect.visible || (sampleSelection.selectedZeroCrossing > 0)) {
//                buffer_lock<> buf(targetBuffer);
//                int target_tablesize = buf.frame_count();
//                int source_tablesize = selectedSamples.size();
//                float frac = static_cast<float>(source_tablesize) / static_cast<float>(target_tablesize);
//                float position = 0.f;
//
//                if (buf.valid()) {
//                    lib::interpolator::linear<> linear_interpolator;
//                    for (int i = 0; i < target_tablesize; i++) {
//                        int lower_index = floor(position);
//                        int upper_index = ceil(position);
//                        upper_index = upper_index > (source_tablesize - 1) ? (source_tablesize - 1) : upper_index;
//                        float delta = position - static_cast<float>(lower_index);
//                        float interpolated_value = linear_interpolator.operator()(selectedSamples[lower_index], selectedSamples[upper_index], delta);
//                        buf[i] = interpolated_value;
//                        position += frac;
//                    }
//                }
//                outletStatus("newFrame");
//            }
//            return{};
//        }
//    };
    
    void drawSamples(target t) {        //Das allgemeingültig schreiben und auch bei StakcedFrames verwenden!
        lib::interpolator::linear<> linearInterpolator;
        float position{}, lastX{margin}, lastY{yOffset};
        int inputSampleSize = static_cast<int>(tablePreprocessor.inputSamples.size()) - 1;      //Zero based counting
        float delta = static_cast<float>(inputSampleSize) / width;
        for (int i = 1; i < static_cast<int>(width); ++i) {
            int lowerSmplIdx = floor(position);
            int upperSmplIdx = ceil(position);
            upperSmplIdx = upperSmplIdx > inputSampleSize ? (inputSampleSize) : upperSmplIdx;       //prevent segmentation fault
            float interpolatedValue = linearInterpolator(tablePreprocessor.inputSamples[lowerSmplIdx], tablePreprocessor.inputSamples[upperSmplIdx], delta) * waveformYScaling;
            float currentY = ((interpolatedValue - 1.f) * -0.5f * height) + margin;
            int currentX = i + static_cast<int>(margin);
            line<stroke> {
                t,
                color { waveformColor },
                origin { lastX, lastY },
                destination { currentX, currentY },
                line_width { strokeWidth }
            };
            position += delta;
            lastX = currentX;
            lastY = currentY;
        }
    }
    
    void drawZeroCrossings(target t) {
        float factor = width / static_cast<float>(tablePreprocessor.inputSamples.size());
        int lowerY{static_cast<int>(margin)}, upperY{static_cast<int>(height + margin)};
        for (int i = 0; i < tablePreprocessor.zeroCrossings.size(); ++i) {
            int x = static_cast<int>(tablePreprocessor.zeroCrossings[i] * factor + margin);
            line<stroke> {
                t,
                color { zeroCrossingsColor },
                origin { x, lowerY },
                destination { x, upperY },
                line_width { strokeWidth }
            };
        }
    }
    
    void drawOverlayRects(target t) {
        if (overlayRectFree.visible && (overlayRectFree.x1 != overlayRectFree.x2)) {
            cout << "OverlayRectFree Width: "<< overlayRectFree.getWidth() << endl; //size 0 ist schlecht
            rect<fill> {
                t,
                color { overlayColor },
                position { static_cast<float>(overlayRectFree.getStartX()), margin },
                size { static_cast<float>(overlayRectFree.getWidth()), height }
            };
        } else if (overlayRectZeros.visible  && (overlayRectZeros.x1 != overlayRectZeros.x2)) {
            rect<fill> {
                t,
                color { overlayColor },
                position { static_cast<float>(overlayRectZeros.getStartX()), margin },
                size { static_cast<float>(overlayRectZeros.getWidth()), height }
            };
        }
    }
    
    message<> paint {
        this, "paint", MIN_FUNCTION {
            target t        {args};
            width = t.width() - (margin * 2.f);
            height = t.height() - (margin * 2.f);
            yOffset = (height / 2.f) + margin;
            
            rect<fill> {
                t,
                color { backgroundColor }
            };
            
            if (tablePreprocessor.inputSamples.size() > 0) {
                drawOverlayRects(t);
                if ((tablePreprocessor.zeroCrossings.size() > 0) && (mode == zeros)) {
                    drawZeroCrossings(t);
                }
                drawSamples(t);
            }
            
//            float factor = static_cast<float>(inputSamples.size()) / t.width();
//            float factorInv = static_cast<float>(t.width() / static_cast<float>(inputSamples.size()));
//            sampleSelection.firstSample = static_cast<int>(floor(overlayRect.x * factor));
//            sampleSelection.lastSample = static_cast<int>(floor((overlayRect.x + overlayRect.width) * factor));
//            selectedSamples.clear();
//
//            rect<fill> {                //Background
//                t,
//                color {bgColor}
//            };
//            if (inputSamples.size() > 0) {        //Check if input_buffer is non-zero
//                float originX = 0.f;
//                float originY = t.height() * .5f;
//                for (int i = 0; i < (inputSamples.size()); i++){
//                    float frac = static_cast<float>(i) / static_cast<float>(inputSamples.size());
//                    float x = frac * t.width();
//                    float y = (inputSamples[i] * t.height() * -.5f) + (t.height() * .5f);
//                    number strokeWidth = 1;
//
//                    if (mode == 0){
//                        if (i >= floor(sampleSelection.selectedZeroCrossing) && i <= floor(sampleSelection.selectedZeroCrossing + tf0)){
//                            strokeWidth = 2;
//                            selectedSamples.push_back(inputSamples[i]);
//                        }
//                    }
//                    if (mode == 1){
//                        if (i >= sampleSelection.firstSample && i <= sampleSelection.lastSample){
//                            strokeWidth = 2;
//                            selectedSamples.push_back(inputSamples[i]);
//                        }
//                    }
//                    line<stroke>{
//                        t,
//                        color {elementColor},
//                        origin {originX, originY},
//                        destination{x, y},
//                        line_width{strokeWidth}
//                    };
//                    originX = x;
//                    originY = y;
//                }
//            }
//
//            if (overlayRect.visible){
//                overlayRect.height = t.height();
//                rect<fill> {
//                    t,
//                    color {overlay_color},
//                    position{overlayRect.x, overlayRect.y},
//                    size{overlayRect.width, overlayRect.height}
//                };
//            }
//
//            if (mode == 0) {
//            float scaleHeight = (t.height() * 0.2f);
//                for (auto value : periodZeroCrossings) {
//                    float xPos = value * factorInv;
//                    line<stroke> {
//                        t,
//                        color {color::predefined::white},
//                        origin {xPos, 0.f + scaleHeight},
//                        destination {xPos, t.height() - scaleHeight},
//                        line_width{1}
//                    };
//                }
//                line<stroke> {
//                    t,
//                    color {color::predefined::black},
//                    origin {sampleSelection.selectedZeroCrossing * factorInv, 0.f + scaleHeight},
//                    destination {sampleSelection.selectedZeroCrossing * factorInv, t.height() - scaleHeight},
//                    line_width{2}
//
//                };
//            }
        return {};
        }
    };
    
//    void analyse_period_zero_crossings()
//    {
//
//    }
    /*
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
     */
    
};


MIN_EXTERNAL(table_preprocessing);
