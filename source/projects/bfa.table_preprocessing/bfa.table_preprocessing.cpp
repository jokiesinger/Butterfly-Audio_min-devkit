/// @file
/// @ingroup     minexamples
/// @copyright    Copyright 2018 The Min-DevKit Authors. All rights reserved.
/// @license    Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"

#include "waveform_processing.h"
#include "pitch_detection.h"
#include "sample_preprocessor.h"
#include "wavetable_oscillator.h"
#include "graphics_transform.h"
#include "min_event_wrapper.h"
#include "min_painter.h"

#include <chrono>

using namespace c74::min;
using namespace c74::min::ui;
using namespace Butterfly;




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



class table_preprocessing : public object<table_preprocessing>, public ui_operator<160, 80>
{
public:
	using Wavetable = Wavetable<float>;
	using Osc = WavetableOscillator<Wavetable>;

	enum class Mode {
		free,
		zeros,
		period
	};

	MIN_DESCRIPTION{ "Read from a buffer~ and display." };
	MIN_TAGS{ "audio, sampling, ui, time" };
	MIN_AUTHOR{ "BFA_JK" };
	MIN_RELATED{ "index~, buffer~, wave~, waveform~" };

	table_preprocessing(const atoms& args = {}) : ui_operator::ui_operator{ this, args } {}

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
			setup(static_cast<float>(args[0]));
			return {};
		} };


	message<> setMode{
		this, "setMode", "Set frame selection mode (custom, period).", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			if (args[0] == "Free") {
				setModeImpl(Mode::free);
			} else if (args[0] == "Zeros") {
				setModeImpl(Mode::zeros);
			} else if (args[0] == "Period") {
				setModeImpl(Mode::period);
			}
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
			mousewheelImpl(createMouseEvent(e, MouseEvent::Action::Wheel));
			return {};
		}
	};


	message<> mousedown{
		this, "mousedown", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			event e{ args };
			mousedownImpl(createMouseEvent(e, MouseEvent::Action::Down));
			return {};
		}
	};

	message<> mouseup{
		this, "mouseup", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			event e{ args };
			mouseupImpl(createMouseEvent(e, MouseEvent::Action::Up));
			return {};
		}
	};

	message<> dblclick{
		this, "dblclick", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			event e{ args };
			resetTransform();
			return {};
		}
	};

	message<> mousedrag{
		this, "mousedrag", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			event e{ args };
			mousedragImpl(createMouseEvent(e, MouseEvent::Action::Drag));
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
			if (targetSize.x != t.width() || targetSize.y != t.height()) {
				targetResized(t.width(), t.height());
			}

			rect<fill> rect{ t, color{ backgroundColor } };
			MaxPainter painter{ t };

			if (samplePreprocessor.inputSamples.size() > 0) {
				painter.fillColor(to(overlayColor));
				drawOverlayRects(painter);

				if ((samplePreprocessor.zeroCrossings.size() > 0) && (mode == Mode::zeros)) {
					painter.strokeColor(to(zeroCrossingsColor));
					painter.strokeWidth(strokeWidth);
					drawZeroCrossings(painter);
				}

				painter.strokeWidth(strokeWidth);
				painter.strokeColor(to(waveformColor));
				painter.fillColor(to(waveformColor));
				drawSamples(painter);

				painter.strokeColor(to(draggingRectColor));
				drawDraggingRect(painter);
			}

			return {};
		}
	};

	void setup(float sampleRate);
	void setModeImpl(Mode newMode);

	void targetResized(double width, double height); // Called when the target has been resized through any means
	void constrainViewTransform();
	void drawSamples(MaxPainter& painter);
	void drawDraggingRect(MaxPainter& painter);
	void drawZeroCrossings(MaxPainter& painter);
	void drawOverlayRects(MaxPainter& painter);

	void inputSamplesChanged();
	void resetTransform();
	void sampleDroppedImpl();
	void setSampleData(const std::vector<float>& data);
	bool canExport() const;
	std::pair<int, int> getCurrentExportRange() const;
	bool exportFrame();
	void notifyCanExportStatus();

	void updateFreeSelection(const Point& mouseDownPoint, const Point& currentMousePoint);
	void updateZerosSelection(const Point& mouseDownPoint, const Point& currentMousePoint);


	void mousedownImpl(const MouseEvent& e);
	void mousedragImpl(const MouseEvent& e);
	void mouseupImpl(const MouseEvent& e);
	void mousewheelImpl(const MouseEvent& e);

	// Helper functions
	double nearestZeroCrossing(double sampleIdx) const;
	MouseEvent::Button getButton(const event& e) const;


	// Getters for testing
	Mode getMode() const { return mode; }
	bool isDragging() const { return dragging; }
	double getSampleRate() const { return sampleRate; }
	Rect getDataRange() const { return dataRange; }
	std::pair<double, double> getFreeSelection() const { return freeSelection; }

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

	float margin{ 10.f }; //Könnte auch als Attribut
	float sampleRate{ 48000.f };
	Mode mode{ Mode::free };

	SamplePreprocessor samplePreprocessor;
	// std::optional<PitchInfo> pitchInfoOptional;
	// PitchInfo pitchInfo;
	// float tf0, deviationInSmps{10.f};


	// mouse event related
	Point mouseDownPoint, currentMousePoint;
	MouseEvent::Button button{};
	bool dragging{ false };
	std::chrono::time_point<std::chrono::system_clock> clickTime;

	// transform related
	Rect dataRange;						// size of the draw target, will be updated on first draw.
	Point targetSize{ 100, 100 };		// size of the draw target, will be updated on first draw.
	Rect waveformView{ {}, targetSize }; // size of the draw target, will be updated on first draw.
	Transform transform;
	std::pair<double, double> freeSelection;
	std::pair<double, double> zerosSelection;
};


void table_preprocessing::setup(float sampleRate) {
	this->sampleRate = sampleRate;
	cout << "dspsetup happend" << endl;
}

void table_preprocessing::setModeImpl(Mode newMode) {
	mode = newMode;
	redraw();
	notifyCanExportStatus();
}


void table_preprocessing::updateFreeSelection(const Point& mouseDownPoint, const Point& currentMousePoint) {
	const auto selectionValueRect = transform.from({ mouseDownPoint, currentMousePoint });
	freeSelection.first = std::clamp(selectionValueRect.x, 0., samplePreprocessor.inputSamples.size() - 1.0);
	freeSelection.second = std::clamp(selectionValueRect.x + selectionValueRect.width, 0., samplePreprocessor.inputSamples.size() - 1.0);
	notifyCanExportStatus();
}

void table_preprocessing::updateZerosSelection(const Point& mouseDownPoint, const Point& currentMousePoint) {
	double nearestCrossing1 = nearestZeroCrossing(transform.fromX(mouseDownPoint.x));
	double nearestCrossing2 = nearestZeroCrossing(transform.fromX(currentMousePoint.x));
	zerosSelection.first = std::clamp(nearestCrossing1, 0., samplePreprocessor.inputSamples.size() - 1.0);
	zerosSelection.second = std::clamp(nearestCrossing2, 0., samplePreprocessor.inputSamples.size() - 1.0);
	if (zerosSelection.second < zerosSelection.first) { std::swap(zerosSelection.second, zerosSelection.first); }
	notifyCanExportStatus();
}

bool table_preprocessing::canExport() const {
	const auto [begin, end] = getCurrentExportRange();
	if (samplePreprocessor.inputSamples.empty()) return false;
	if (end <= begin) return false;
	if (begin < 0 || end >= samplePreprocessor.inputSamples.size()) return false;
	if (std::abs(begin - end) < Wavetable::minimumInputSize()) return false;
	return true;
}

std::pair<int, int> table_preprocessing::getCurrentExportRange() const {
	if (mode == Mode::free) {
		return { std::round(freeSelection.first), std::round(freeSelection.second) };
	} else if (mode == Mode::zeros) {
		return { std::round(zerosSelection.first), std::round(zerosSelection.second) };
	}
	return {};
}

bool table_preprocessing::exportFrame() {
	if (!canExport()) return false;
	const auto [begin, end] = getCurrentExportRange();

	targetBuffer.set(targetBufferName);
	buffer_lock<false> buf(targetBuffer);
	if (!buf.valid()) return false;
	const int targetTablesize = buf.frame_count();
	const std::vector<float> selectedSamples{ samplePreprocessor.inputSamples.begin() + begin, samplePreprocessor.inputSamples.begin() + end };
	const float exportTableOscFreq = sampleRate / static_cast<float>(targetTablesize);

	const Wavetable table{ selectedSamples, sampleRate / 2.f };
	std::vector<Wavetable> wavetable{ table };
	Osc interpolationOscillator{ &wavetable, sampleRate, exportTableOscFreq };

	for (int i = 0; i < targetTablesize; i++) {
		buf[i] = interpolationOscillator++;
	}
	outletStatus.send("newFrame");
	buf.dirty();
	return true;
}

void table_preprocessing::notifyCanExportStatus() {
	if (canExport()) {
		outletStatus.send("CanExportStatus", 1);
	} else {
		outletStatus.send("CanExportStatus", 0);
	}
}

void table_preprocessing::drawSamples(MaxPainter& painter) { //Das allgemeingültig schreiben und auch bei StackedFrames verwenden!
	if (samplePreprocessor.inputSamples.empty()) return;

	// get first/last visible sample
	const int first = std::max(0., transform.fromX(0) - 1.);
	const int last = std::min<double>(samplePreprocessor.inputSamples.size(), std::ceil(transform.fromX(targetSize.x) + 1.));
	const int step = std::max<int>(1, (last - first) / (targetSize.x * 10.));

	if (step == 1) {
		const auto point = transform.apply({ static_cast<double>(first), -samplePreprocessor.inputSamples[first] * waveformYScaling });
		auto previous = point;
		for (int i = first + 1; i < last; ++i) {
			const auto point = transform.apply({ static_cast<double>(i), -samplePreprocessor.inputSamples[i] * waveformYScaling });
			painter.line(previous, point);
			previous = point;
		}
	} else if (step < 10) {
		const auto point = transform.apply({ static_cast<double>(first), -samplePreprocessor.inputSamples[first] * waveformYScaling });
		auto previous = point;
		for (int i = first + step; i < last; i += step) {
			auto sample = *std::max_element(samplePreprocessor.inputSamples.begin() + i - step, samplePreprocessor.inputSamples.begin() + i, [](auto a, auto b) { return a * a < b * b; });
			const auto point = transform.apply({ static_cast<double>(i), -sample * waveformYScaling });
			painter.line(previous, point);
			previous = point;
		}
	} else {
		for (int i = first + step; i < last; i += step) {
			auto [min, max] = std::minmax_element(samplePreprocessor.inputSamples.begin() + i - step, samplePreprocessor.inputSamples.begin() + i);
			const auto p1 = transform.apply({ static_cast<double>(i), -*min * waveformYScaling });
			const auto p2 = transform.apply({ static_cast<double>(i), -*max * waveformYScaling });
			painter.line(p1, p2);
		}
	}

	if (transform.apply({ 0, 0, 1, 0 }).width > targetSize.x / 20.) {
		for (int i = first; i < last; ++i) {
			const auto point = transform.apply({ static_cast<double>(i), -samplePreprocessor.inputSamples[i] * waveformYScaling });
			painter.point(point, 5.0);
		}
	}
}

void table_preprocessing::drawDraggingRect(MaxPainter& painter) {
	if (dragging && button == MouseEvent::Button::Right) {

		// enable dashed stroke style
		c74::max::t_jgraphics* g = painter.t;
		double a[] = { 4, 4 };
		jgraphics_set_dash(g, a, 2, 0);

		Rect rect = { currentMousePoint, mouseDownPoint };
		rect.y = margin;
		rect.height = painter.getHeight() - 2 * margin;
		if (rect.width > 0) {
			painter.rectOutline(rect, 1.0);
		}
		// disable dashed stroke style
		jgraphics_set_dash(g, 0, 0, 0);
	}
}

void table_preprocessing::drawZeroCrossings(MaxPainter& painter) {
	// get first/last visible sample
	const int first = std::max(0., transform.fromX(0) - 1.);
	const int last = std::min<double>(samplePreprocessor.inputSamples.size(), std::ceil(transform.fromX(targetSize.x) + 1.));
	const int step = std::max<int>(1, (last - first) / (targetSize.x * 10.));

	if (step > 5) return; // dont draw crossings when they get too dense
	for (double value : samplePreprocessor.zeroCrossings) {
		if (value < first) continue; // only draw visible crossings
		if (value > last) break;
		const auto p1 = transform.apply({ value, 1. });
		const auto p2 = transform.apply({ value, -1. });
		painter.line(p1, p2);
	}
}

void table_preprocessing::drawOverlayRects(MaxPainter& painter) {
	if (mode == Mode::free) {
		if (freeSelection.first == freeSelection.second)
			return;
		const auto r = transform.apply({ { freeSelection.first, 1 }, { freeSelection.second, -1 } });
		painter.rect(r);
	} else if (mode == Mode::zeros) {
		if (zerosSelection.first == zerosSelection.second)
			return;
		const auto r = transform.apply({ { zerosSelection.first, 1 }, { zerosSelection.second, -1 } });
		painter.rect(r);
	}
}

void table_preprocessing::inputSamplesChanged() {
	resetTransform();
}

void table_preprocessing::resetTransform() {
	dataRange = Rect::fromBounds(0, samplePreprocessor.inputSamples.size(), -1, 1);
	transform = Transform::MapRect(dataRange, waveformView);
}

void table_preprocessing::targetResized(double width, double height) {
	const auto currentViewRect = transform.from(waveformView);
	targetSize = { width, height };
	waveformView.resize(width - 2 * margin, height - 2 * margin).moveTo({ margin, margin });
	transform = Transform::MapRect(currentViewRect, waveformView);
	//transform = Transform::MapRect(dataRange, waveformView);
}

void table_preprocessing::constrainViewTransform() {
	transform.ensureWithin(dataRange, waveformView);
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

	inputSamplesChanged();
}


void table_preprocessing::mousedownImpl(const MouseEvent& e) {
	if (samplePreprocessor.inputSamples.empty()) return;
	mouseDownPoint = { e.x, e.y };
	currentMousePoint = mouseDownPoint;
	dragging = true;
	this->button = e.button;
	redraw();
}

void table_preprocessing::mousedragImpl(const MouseEvent& e) {
	if (samplePreprocessor.inputSamples.empty())
		return;
	Point point{ e.x, e.y };

	if (e.button == MouseEvent::Button::Left) {
		if (mode == Mode::free) {
			updateFreeSelection(mouseDownPoint, point);
		} else if (mode == Mode::zeros) {
			updateZerosSelection(mouseDownPoint, point);
		}
	} else if (e.button == MouseEvent::Button::Middle) {
		auto p = point - currentMousePoint;
		transform.preTranslate(p);
		constrainViewTransform();
	}
	currentMousePoint = point;
	redraw();
}

void table_preprocessing::mouseupImpl(const MouseEvent& e) {
	if (samplePreprocessor.inputSamples.empty()) return;
	Point point{ e.x, e.y };

	currentMousePoint = point;
	if (e.button == MouseEvent::Button::Left) {
		if (mode == Mode::free) {
			updateFreeSelection(mouseDownPoint, point);
		} else if (mode == Mode::zeros) {
			updateZerosSelection(mouseDownPoint, point);
		}
	} else if (e.button == MouseEvent::Button::Right) {
		if (dragging) {
			Rect r{ mouseDownPoint, point };
			r.y = waveformView.y;
			r.height = waveformView.height;
			if (r.width > 0) {
				auto r2 = transform.from(r);
				transform = Transform::MapRect(r2, waveformView);
				constrainViewTransform();
			}
		}
	}
	dragging = false;
	const auto now = std::chrono::system_clock::now();
	if (now - clickTime < std::chrono::milliseconds(300)) {
		resetTransform();
	}
	clickTime = now;
	redraw();
}

void table_preprocessing::mousewheelImpl(const MouseEvent& e) {
	const double speed = e.isControlDown() ? fastZoomSpeed : zoomSpeed;
	const auto delta = e.deltaY > 0 ? speed : 1 / speed;
	if (delta > 1. && transform.apply({ 0, 0, 1, 0 }).width > targetSize.x) return;
	transform.scaleAround(e.x, e.y, delta, 1);
	constrainViewTransform();
	redraw();
}

double table_preprocessing::nearestZeroCrossing(double sampleIdx) const {
	if (samplePreprocessor.zeroCrossings.empty()) { return 0.; }
	return *std::min_element(samplePreprocessor.zeroCrossings.begin(), samplePreprocessor.zeroCrossings.end(), [sampleIdx](auto a, auto b) { return std::abs(a - sampleIdx) < std::abs(b - sampleIdx); });
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


MIN_EXTERNAL(table_preprocessing);