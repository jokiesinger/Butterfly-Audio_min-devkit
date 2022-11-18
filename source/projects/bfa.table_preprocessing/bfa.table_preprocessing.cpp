/// @file
/// @ingroup     minexamples
/// @copyright    Copyright 2018 The Min-DevKit Authors. All rights reserved.
/// @license    Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"

#include "waveform_processing.h"
#include "pitch_detection.h"
#include "table_preprocessing_helper_functions.h"
#include "wavetable_oscillator.h"
#include "graphics_transform.h"


using namespace c74::min;
using namespace c74::min::ui;

class table_preprocessing : public object<table_preprocessing>, public ui_operator<160, 80> {
private:
    using Wavetable = Butterfly::Wavetable<float>;
    using Osc = Butterfly::WavetableOscillator<Wavetable>;
    
    float margin{10.f};     //Könnte auch als Attribut
    float yOffset{};
    OverlayRect overlayRectFree{};
    OverlayRect overlayRectZeros{};
    TablePreprocessor tablePreprocessor;
//    SampleSelection sampleSelection{};
    
    enum class Mode {free, zeros, period};
    Mode mode{Mode::free};
    
//    std::optional<Butterfly::PitchInfo> pitchInfoOptional;
//    Butterfly::PitchInfo pitchInfo;
//    float tf0, deviationInSmps{10.f};

    
    float sampleRate{48000.f};
    int internalTablesize{2048};
    float width{}, height{};

    // mouse event related
    bool mouseDown{false};
    enum class Button { Left, Right, Center };
    Butterfly::Point mouseDownPoint, currentMousePoint;
    Button   button {};
    bool     dragging {false};

    // transform related
    Butterfly::Rect      dataRange;    // size of the draw target, will be updated on first draw.
    Butterfly::Point     targetSize {100, 100};    // size of the draw target, will be updated on first draw.
    Butterfly::Rect      waveformView {{}, targetSize};        // size of the draw target, will be updated on first draw.
    Butterfly::Transform transform;
    std::pair<double,double>     freeSelection;
    std::pair<double,double>     zerosSelection;
    
public:
    MIN_DESCRIPTION     { "Read from a buffer~ and display." };
    MIN_TAGS            { "audio, sampling, ui, time" };
    MIN_AUTHOR          { "BFA_JK" };
    MIN_RELATED         { "index~, buffer~, wave~, waveform~" };

    inlet<>  inletNewSample     { this, "(message) new sample dropped" };
    outlet<> outletStatus       { this, "(message) Notification that the content of the buffer~ changed." };
    
    table_preprocessing(const atoms& args = {}) : ui_operator::ui_operator {this, args} {
//        overlayRectFree.visible = true;
    }
    
    ~table_preprocessing() = default;
    
    buffer_reference inputBuffer {
        this, MIN_FUNCTION {
            outletStatus.send(args);
            return {};
        }, false
    };
    
    buffer_reference targetBuffer {
        this, MIN_FUNCTION {
            outletStatus.send(args);
            return {};
        }, false
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
                mode = Mode::free;
                overlayRectFree.visible = true;
                overlayRectZeros.visible = false;
            } else if (args[0] == "Zeros") {
                mode = Mode::zeros;
                overlayRectFree.visible = false;
                overlayRectZeros.visible = true;
            } else if (args[0] == "Period") {
                mode = Mode::period;
            }
            redraw();
            return{};
        }
    };
    
    message<> sampleDropped {
        this, "sampleDropped", "Read again from the buffer~.", MIN_FUNCTION {
            inputBuffer.set(inputBufferName);
            buffer_lock<false> buf(inputBuffer);
            if (buf.channel_count() != 1) {
                cout << "Buffer channel count has to be one." << endl;
                return{};
            }
            if (!buf.valid()) {
                buf.~buffer_lock();
                return{};
            }
            tablePreprocessor.inputSamples.clear();
            for (auto i = 0; i < buf.frame_count(); ++i) {
                tablePreprocessor.inputSamples.push_back(buf.lookup(i, 0));
            }
            Butterfly::peakNormalize(tablePreprocessor.inputSamples.begin(), tablePreprocessor.inputSamples.end());
            tablePreprocessor.analyzeZeroCrossings();
            overlayRectFree = {};
            overlayRectZeros = {};
            if (mode == Mode::free) {overlayRectFree.visible = true;}
            else if (mode == Mode::zeros) {overlayRectZeros.visible = true;}
            redraw();   //show new samples
//            buf.dirty();
            buf.~buffer_lock();

            inputSamplesChanged();
            return{};
        }
    };
    
    double nearestZeroCrossing(double sampleIdx) {
        if(tablePreprocessor.zeroCrossings.empty()) {return 0.;}
        return *std::min_element(tablePreprocessor.zeroCrossings.begin(), tablePreprocessor.zeroCrossings.end(), [sampleIdx](auto a, auto b){return std::abs(a - sampleIdx) < std::abs(b - sampleIdx);});
	}
    
    message<> mousedown {
        this, "mousedown", MIN_FUNCTION {
            if (tablePreprocessor.inputSamples.empty()) {return{};}
            mouseDown = true;
            event e {args};

            if (e.m_modifiers & c74::max::eLeftButton) {

                 if (mode == Mode::free) {
                     overlayRectFree.x2 = e.x();     //Reset width
                     overlayRectFree.x1 = e.x();
                 } else if (mode == Mode::zeros) {
                     //find nearest zeroCrossing
                     
                     
                 }
		  } else if (e.m_modifiers & c74::max::eRightButton) {
		  }
		  mouseDownPoint    = {static_cast<double>(e.x()), static_cast<double>(e.y())};
		  currentMousePoint = mouseDownPoint;
		  dragging          = true;
            redraw();
            return{};
        }
    };
    
    message<> mousewheel {
        this, "mousewheel", MIN_FUNCTION {
            event e {args};	
	       double zoomSpeed{ 1.1 };
	       double fastZoomSpeed{ 1.8 };
            const auto speed = e.is_command_key_down() ? fastZoomSpeed : zoomSpeed;
            
	       const auto d = e.wheel_delta_y() > 0 ? speed : 1 / speed;
	       transform.scaleAround(e.x(), e.y(), d, 1);
	       constrainViewTransform();
            redraw();
            return{};
        }
    };

    
    message<> mouseup {
        this, "mouseup", MIN_FUNCTION {
            if (tablePreprocessor.inputSamples.empty()) {return{};}
            mouseDown = false;
            event e {args};

            
		  const Butterfly::Point current = {static_cast<double>(e.x()), static_cast<double>(e.y())};
		  currentMousePoint           = current;
            if (e.m_modifiers & c74::max::eLeftButton) {
			if (mode == Mode::free) {
                    updateFreeSelection(mouseDownPoint, currentMousePoint);
				overlayRectFree.x2 = static_cast<int>(std::clamp(static_cast<float>(e.x()), margin, width + margin));
			}
			else if (mode == Mode::zeros) {
				float nearestCrossing = nearestZeroCrossing(e.x());
				float factor          = width / static_cast<float>(tablePreprocessor.inputSamples.size());
				overlayRectZeros.x2   = static_cast<int>(std::clamp(nearestCrossing * factor + margin, margin, width + margin));
			}
		  } else if (e.m_modifiers & c74::max::eRightButton) {
			  if (dragging) {
				  Butterfly::Rect r {mouseDownPoint - waveformView.topLeft(), currentMousePoint - waveformView.topLeft()};
				  r.y = waveformView.y;
				  r.height = waveformView.height;
                  r.x -= margin;
				  if (r.width > 0) {
					  auto r2        = transform.from(r);
					  transform = Butterfly::Transform::MapRect(r2, waveformView);
					  constrainViewTransform();
				  }
			  }
		  }
		  dragging = false;
            redraw();
            return{};
        }
    };

    message<> mousedrag {
        this, "mousedrag", MIN_FUNCTION {
            if (tablePreprocessor.inputSamples.empty()) {return{};}
            event e {args};
            currentMousePoint           = {static_cast<double>(e.x()), static_cast<double>(e.y())};

		  if (e.m_modifiers & c74::max::eLeftButton) {
			  if (mode == Mode::free) {
				  updateFreeSelection(mouseDownPoint, currentMousePoint);
			  }
			  else if (mode == Mode::zeros) {
                  updateZerosSelection(mouseDownPoint, currentMousePoint);
			  }
		  }
            redraw();
            return {};
        }
    };
    
    void exportFrame(int begin, int end) {
        if (tablePreprocessor.inputSamples.empty()) return;
        if (end <= begin) return;
        if (begin < 0 || end >= tablePreprocessor.inputSamples.size()) return;
        if (std::abs(begin - end) < Wavetable::minimumInputSize()) return;
        
        targetBuffer.set(targetBufferName);
        buffer_lock<false> buf(targetBuffer);
        if (!buf.valid()) return;
        const int targetTablesize = buf.frame_count();
        const std::vector<float> selectedSamples {tablePreprocessor.inputSamples.begin() + begin, tablePreprocessor.inputSamples.begin() + end};
        const float exportTableOscFreq = sampleRate / static_cast<float>(targetTablesize);

        const Wavetable table {selectedSamples, sampleRate / 2.f};
        std::vector<Wavetable> wavetable {table};
        Osc interpolationOscillator{ &wavetable, sampleRate, exportTableOscFreq};

        for (int i = 0; i <targetTablesize; i++) {
             buf[i] = interpolationOscillator++;
        }
        outletStatus.send("newFrame");
        buf.dirty();
    }
    
    message<> generate_frame {
        this, "generate_frame", MIN_FUNCTION {
            
            if (mode == Mode::free) {
                exportFrame(std::round(freeSelection.first), std::round(freeSelection.second));
            } else if (mode == Mode::zeros) {
                    exportFrame(std::round(zerosSelection.first), std::round(zerosSelection.second));
            }
            return {};
        }
    };

    void updateFreeSelection(const Butterfly::Point& mouseDownPoint, const Butterfly::Point& currentMousePoint) {
		const auto selectionValueRect = transform.from({mouseDownPoint, currentMousePoint});
		freeSelection.first               = std::clamp(selectionValueRect.x, 0., tablePreprocessor.inputSamples.size() - 1.0);
		freeSelection.second = std::clamp(selectionValueRect.x + selectionValueRect.width, 0., tablePreprocessor.inputSamples.size() - 1.0);
    }
    
    void updateZerosSelection(const Butterfly::Point& mouseDownPoint, const Butterfly::Point& currentMousePoint){
        double nearestCrossing1 = nearestZeroCrossing(transform.fromX(mouseDownPoint.x));
        double nearestCrossing2 = nearestZeroCrossing(transform.fromX(currentMousePoint.x));
        zerosSelection.first               = std::clamp(nearestCrossing1, 0., tablePreprocessor.inputSamples.size() - 1.0);
        zerosSelection.second = std::clamp(nearestCrossing2, 0., tablePreprocessor.inputSamples.size() - 1.0);
        if (zerosSelection.second < zerosSelection.first) {std::swap(zerosSelection.second, zerosSelection.first);}
    }
    
    void drawSamples(target t) {        //Das allgemeingültig schreiben und auch bei StackedFrames verwenden!
        if (tablePreprocessor.inputSamples.empty()) return;
        const auto point = transform.apply({static_cast<double>(0), -tablePreprocessor.inputSamples[0] * waveformYScaling});
        auto last = point;
        for (int i = 1; i < tablePreprocessor.inputSamples.size(); ++i) {

            const auto point = transform.apply({static_cast<double>(i), -tablePreprocessor.inputSamples[i] * waveformYScaling});

            line<stroke> {
                t,
                color { waveformColor },
                origin { last.x, last.y },
                destination{point.x, point.y},
                line_width { strokeWidth }
            };
          last = point;
        }
    }
    
    void drawZeroCrossings(target t) {
        for (double value : tablePreprocessor.zeroCrossings) {
            auto p1 = transform.apply({value, 1.});
            auto p2 = transform.apply({value, -1.});
//            int x = static_cast<int>(tablePreprocessor.zeroCrossings[i] * factor + margin);
            line<stroke> {
                t,
                color { zeroCrossingsColor },
                origin { p1.x, p1.y },
                destination { p2.x, p2.y },
                line_width { strokeWidth }
            };
        }
    }
    
    void drawOverlayRects(target t) {
        if (mode == Mode::free) {
            if (freeSelection.first == freeSelection.second)
                return;
            const auto r = transform.apply({{freeSelection.first, 1}, {freeSelection.second, -1}});
              rect<fill> { t, color { overlayColor }, position {r.x, r.y}, size {r.width, r.height}};
        } else if (mode == Mode::zeros) {
            if (zerosSelection.first == zerosSelection.second)
                return;
            const auto r = transform.apply({{zerosSelection.first, 1}, {zerosSelection.second, -1}});
              rect<fill> { t, color { overlayColor }, position {r.x, r.y}, size {r.width, r.height}};
        }
    }

    void inputSamplesChanged() {
		dataRange = Butterfly::Rect::fromBounds(0, tablePreprocessor.inputSamples.size(), -1, 1);
		transform = Butterfly::Transform::MapRect(dataRange, waveformView);
    }
         // Called when the target has been resized through any means
	void targetResized(double width, double height) {
		const auto currentViewRect = transform.from(waveformView);
		targetSize                 = {width, height };
		waveformView.resize(width - 2 * margin, height - 2 * margin).moveTo({margin, margin});
          //transform                  = Butterfly::Transform::MapRect(currentViewRect, waveformView);
		transform                  = Butterfly::Transform::MapRect(dataRange, waveformView);
	}

	void constrainViewTransform() {
		transform.ensureWithin(dataRange, waveformView);
	}

    
    message<> paint {
        this, "paint", MIN_FUNCTION {
            target t        {args};
	       if (targetSize.x != t.width() || targetSize.y != t.height()) {
		       targetResized(t.width(), t.height());
            }
            width = t.width() - (margin * 2.f);
            height = t.height() - (margin * 2.f);
            yOffset = (height / 2.f) + margin;
            
            rect<fill> {
                t,
                color { backgroundColor }
            };
            
            if (tablePreprocessor.inputSamples.size() > 0) {
                drawOverlayRects(t);
                if ((tablePreprocessor.zeroCrossings.size() > 0) && (mode == Mode::zeros)) {
                    drawZeroCrossings(t);
                }
                drawSamples(t);
            }
            
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
