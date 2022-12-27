/// @file
/// @ingroup     minexamples
/// @copyright    Copyright 2018 The Min-DevKit Authors. All rights reserved.
/// @license    Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"
#include "sample_preprocessor.h"
#include "min_painter.h"
#include "min_event_wrapper.h"


using namespace c74::min;
using namespace c74::min::ui;
using namespace Butterfly;



/*
void fillRect(target& t, const Rect& r, const color& c) {
	rect<fill> rect{ t, color{ c }, position{ r.x, r.y }, size{ r.width, r.height } };
}

void drawRect(target& t, const Rect& r, const color& c, double strokeWidth = 1.0) {
	rect<stroke> rect{ t, color{ c }, position{ r.x, r.y }, size{ r.width, r.height }, line_width{ strokeWidth } };
}

void drawLine(target& t, const Point& p1, const Point& p2, const color& c, double strokeWidth = 1.0) {
	line<stroke> line{ t, color{ c }, origin{ p1.x, p1.y }, destination{ p2.x, p2.y }, line_width{ strokeWidth } };
}

void drawPoint(target& t, const Point& p1, const color& c, double siz = 1.0) {
	ellipse<fill> line{ t, color{ c }, position{ p1.x - siz * .5, p1.y - siz * .5 }, size{ siz, siz } };
}
*/


class table_preprocessing : public object<table_preprocessing>, public ui_operator<160, 80>, public Callback
{
public:
	using Wavetable = Wavetable<float>;
	using Osc = WavetableOscillator<Wavetable>;


	MIN_DESCRIPTION{ "Read from a buffer~ and display." };
	MIN_TAGS{ "audio, sampling, ui, time" };
	MIN_AUTHOR{ "BFA_JK" };
	MIN_RELATED{ "index~, buffer~, wave~, waveform~" };

	table_preprocessing(const atoms& args = {}) : ui_operator::ui_operator{ this, args }, samplePreprocessor(*this) {}

	~table_preprocessing() = default;


	inlet<> inletNewSample{ this, "(message) new sample dropped" };
	outlet<> outletStatus{ this, "(message) Notification that the content of the buffer~ changed." };

	// Members, whose state is addressable, queryable and saveable from Max
	attribute<color> backgroundColor{ this, "Color Background", color::predefined::gray, title{ "Color Background" } };
	attribute<color> waveformColor{ this, "Color Waveform ", color::predefined::black };
	attribute<color> zeroCrossingsColor{ this, "Color Zero Crossings", { .88f, .88f, .88f, 1.f } };
	attribute<color> overlayColor{ this, "Color Overlay", { 0.f, .9f, .9f, .3f } };
	attribute<color> draggingRectColor{ this, "Color of rectangle that is shown when drag-zooming into the sample", { .2f, .2f, .2f, 1.f } };

	attribute<number> strokeWidth{ this, "Stroke Width Samples", 1.f };
	attribute<number> strokeWidthSelection{ this, "Stroke Width Selected Samples", 1.f };

	attribute<symbol> inputBufferName{ this, "Input Buffer", "inputBuffer", description{ "Name of buffer~ to read from." } };
	attribute<symbol> targetBufferName{ this, "Target Buffer", "targetBuffer", description{ "Name of buffer~ to write to" } };

	attribute<number> waveformYScaling{ this, "Waveform Y Scaling Factor", 0.9 };
	attribute<number> zoomSpeed{ this, "Mouse wheel zoom speed", 1.1 };
	attribute<number> fastZoomSpeed{ this, "Mouse wheel zoom speed (applied when Ctrl is down)", 1.8 };


	buffer_reference inputBuffer{
		this, [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			outletStatus.send(args);
			return {};
		},
		false
	};

	buffer_reference targetBuffer{
		this, [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			outletStatus.send(args);
			return {};
		},
		false
	};

	message<> dspsetup{ this, "dspsetup",
		[this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			samplePreprocessor.setup(static_cast<float>(args[0]));
			return {};
		} };


	message<> setMode{
		this, "setMode", "Set frame selection mode (custom, period).", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			if (args[0] == "Free") {
				samplePreprocessor.setModeImpl(SamplePreprocessor::Mode::free);
			} else if (args[0] == "Zeros") {
				samplePreprocessor.setModeImpl(SamplePreprocessor::Mode::zeros);
			} else if (args[0] == "Period") {
				samplePreprocessor.setModeImpl(SamplePreprocessor::Mode::period);
			}
			redraw();
			return {};
		}
	};

	message<> sampleDropped{
		this, "sampleDropped", "Read again from the buffer~.", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			sampleDroppedImpl();
			return {};
		}
	};

	message<> mousewheel{
		this, "mousewheel", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			event e{ args };
			samplePreprocessor.mousewheelImpl(createMouseEvent(e, MouseEvent::Action::Wheel));
			return {};
		}
	};


	message<> mousedown{
		this, "mousedown", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			event e{ args };
			samplePreprocessor.mousedownImpl(createMouseEvent(e, MouseEvent::Action::Down));
			return {};
		}
	};

	message<> mouseup{
		this, "mouseup", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			event e{ args };
			samplePreprocessor.mouseupImpl(createMouseEvent(e, MouseEvent::Action::Up));
			return {};
		}
	};

	message<> mousedrag{
		this, "mousedrag", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			event e{ args };
			samplePreprocessor.mousedragImpl(createMouseEvent(e, MouseEvent::Action::Drag));
			return {};
		}
	};


	message<> generate_frame{
		this, "generate_frame", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			exportFrame();
			return {};
		}
	};

	message<> paint{
		this, "paint", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			target t{ args };

			rect<fill> rect{ t, color{ backgroundColor } };
			MaxPainter painter{ t };
			samplePreprocessor.draw(painter, { to(waveformColor), to(zeroCrossingsColor), to(overlayColor), to(draggingRectColor), strokeWidth });


			return {};
		}
	};

	void doRedraw() override {
		redraw();
	}

	void doNotifyCanExportStatus() override {
		notifyCanExportStatus();
	}
	//    void analyse_period_zero_crossings()
	//    {
	//
	//    }

	//void getPitchAndZeroCrossings() {
	//    //Analyze zero-crossings
	//    allZeroCrossings = getCrossings<std::vector<float>::iterator>(inputSamples.begin(), inputSamples.end());
	//    periodZeroCrossings.clear();
	//
	//    //Perform pitch detection
	//    pitchInfoOptional = getPitch(inputSamples.begin(), inputSamples.end());
	//    if (pitchInfoOptional) { pitchInfo = *pitchInfoOptional; }
	//
	//    //Period duration
	//    tf0 = 1.f / pitchInfo.frequency;
	//
	//    //Relevant zero-crossings
	//    for (int i = 0; i < allZeroCrossings.size(); i++) {
	//        float secondPeriodCrossingIdx = allZeroCrossings[i] + tf0;
	//        for (int j = i + 1; j < allZeroCrossings.size(); j++) {
	//            if (allZeroCrossings[j] > (secondPeriodCrossingIdx - deviationInSmps) && allZeroCrossings[j] < (secondPeriodCrossingIdx + deviationInSmps)) {
	//                periodZeroCrossings.push_back(allZeroCrossings[i]);
	//                break;
	//            }
	//        }
	//    }
	//}

	//private:
	void sampleDroppedImpl();
	void setSampleData(const std::vector<float>& data);
	void notifyCanExportStatus();
	bool exportFrame();
	MouseEvent::Button getButton(const event& e) const;

	SamplePreprocessor samplePreprocessor;
};


MIN_EXTERNAL(table_preprocessing);


bool table_preprocessing::exportFrame() {
	targetBuffer.set(targetBufferName);
	buffer_lock<false> buf(targetBuffer);
	if (!buf.valid()) return false;
	const auto size = buf.frame_count();

	auto result = samplePreprocessor.exportFrame(size);
	if (result) {
		auto data = *result;
		for (int i = 0; i < size; i++) {
			buf[i] = data[i];
		}
	}
	outletStatus.send("newFrame");
	buf.dirty();
	return true;
}

void table_preprocessing::notifyCanExportStatus() {
	if (samplePreprocessor.canExport()) {
		outletStatus.send("CanExportStatus", 1);
	} else {
		outletStatus.send("CanExportStatus", 0);
	}
}

void table_preprocessing::sampleDroppedImpl() {
	inputBuffer.set(inputBufferName);
	buffer_lock<false> buf(inputBuffer);
	if (buf.channel_count() > 2) {
		cout << "Buffer channel count has to be mono or stereo." << endl;
		return;
	}
	if (!buf.valid()) {
		buf.~buffer_lock();
		return;
	}
	std::vector<float> data;
	for (auto i = 0; i < buf.frame_count(); ++i) {
		data.push_back(buf.lookup(i, 0));
	}
	setSampleData(data);
	notifyCanExportStatus();
}

void table_preprocessing::setSampleData(const std::vector<float>& data) {
	samplePreprocessor.setSampleData(data);
	redraw(); //show new samples
}

MouseEvent::Button table_preprocessing::getButton(const event& e) const {
	if (e.m_modifiers & c74::max::eLeftButton)
		return MouseEvent::Button::Left;
	else if (e.m_modifiers & c74::max::eRightButton)
		return MouseEvent::Button::Right;
	else if (e.m_modifiers & c74::max::eMiddleButton)
		return MouseEvent::Button::Middle;
	return MouseEvent::Button::Left;
}

