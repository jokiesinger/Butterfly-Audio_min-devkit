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


void fillRect(target& t, const Butterfly::Rect& r, const color& c) {
	rect<fill> rect{ t, color{ c }, position{ r.x, r.y }, size{ r.width, r.height } };
}

void drawRect(target& t, const Butterfly::Rect& r, const color& c, double strokeWidth = 1.0) {
	rect<stroke> rect{ t, color{ c }, position{ r.x, r.y }, size{ r.width, r.height }, line_width{ strokeWidth } };
}

void drawLine(target& t, const Butterfly::Point& p1, const Butterfly::Point& p2, const color& c, double strokeWidth = 1.0) {
	line<stroke> line{ t, color{ c }, origin{ p1.x, p1.y }, destination{ p2.x, p2.y }, line_width{ strokeWidth } };
}

enum class Button {
	Left,
	Right,
	Center
};


class table_preprocessing : public object<table_preprocessing>, public ui_operator<160, 80>
{
private:
	using Wavetable = Butterfly::Wavetable<float>;
	using Osc = Butterfly::WavetableOscillator<Wavetable>;

	float margin{ 10.f }; //Könnte auch als Attribut
	TablePreprocessor tablePreprocessor;

	enum class Mode {
		free,
		zeros,
		period
	};

	Mode mode{ Mode::free };

	//    std::optional<Butterfly::PitchInfo> pitchInfoOptional;
	//    Butterfly::PitchInfo pitchInfo;
	//    float tf0, deviationInSmps{10.f};


	float sampleRate{ 48000.f };

	// mouse event related
	Butterfly::Point mouseDownPoint, currentMousePoint;
	Button button{};
	bool dragging{ false };

	// transform related
	Butterfly::Rect dataRange;						// size of the draw target, will be updated on first draw.
	Butterfly::Point targetSize{ 100, 100 };		// size of the draw target, will be updated on first draw.
	Butterfly::Rect waveformView{ {}, targetSize }; // size of the draw target, will be updated on first draw.
	Butterfly::Transform transform;
	std::pair<double, double> freeSelection;
	std::pair<double, double> zerosSelection;

public:
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
			const double speed = e.is_command_key_down() ? fastZoomSpeed : zoomSpeed;

			const auto d = e.wheel_delta_y() > 0 ? speed : 1 / speed;
			transform.scaleAround(e.x(), e.y(), d, 1);
			constrainViewTransform();
			redraw();
			return {};
		}
	};

	message<> mousedown{
		this, "mousedown", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			event e{ args };
			mousedownImpl({ static_cast<double>(e.x()), static_cast<double>(e.y()) }, getButton(e));
			return {};
		}
	};

	message<> mouseup{
		this, "mouseup", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			event e{ args };
			mouseupImpl({ static_cast<double>(e.x()), static_cast<double>(e.y()) }, getButton(e));
			return {};
		}
	};

	message<> mousedrag{
		this, "mousedrag", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			event e{ args };
			mousedragImpl({ static_cast<double>(e.x()), static_cast<double>(e.y()) }, getButton(e));
			return {};
		}
	};



	message<> generate_frame{
		this, "generate_frame", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			if (mode == Mode::free) {
				exportFrame(std::round(freeSelection.first), std::round(freeSelection.second));
			} else if (mode == Mode::zeros) {
				exportFrame(std::round(zerosSelection.first), std::round(zerosSelection.second));
			}
			return {};
		}
	};

	message<> paint{
		this, "paint", [this](const c74::min::atoms& args, const int inlet) -> c74::min::atoms {
			target t{ args };
			if (targetSize.x != t.width() || targetSize.y != t.height()) {
				targetResized(t.width(), t.height());
			}
			rect<fill>{
				t,
				color{ backgroundColor }
			};

			if (tablePreprocessor.inputSamples.size() > 0) {
				drawOverlayRects(t);
				if ((tablePreprocessor.zeroCrossings.size() > 0) && (mode == Mode::zeros)) {
					drawZeroCrossings(t);
				}
				drawSamples(t);
				drawDraggingRect(t);
			}

			return {};
		}
	};

	void setup(float sampleRate);
	void setModeImpl(Mode newMode);

	void targetResized(double width, double height); // Called when the target has been resized through any means
	void constrainViewTransform();
	void drawSamples(target& t);
	void drawDraggingRect(target& t);
	void drawZeroCrossings(target& t);
	void drawOverlayRects(target& t);

	void inputSamplesChanged();
	void sampleDroppedImpl();
	void exportFrame(int begin, int end);

	void updateFreeSelection(const Butterfly::Point& mouseDownPoint, const Butterfly::Point& currentMousePoint);
	void updateZerosSelection(const Butterfly::Point& mouseDownPoint, const Butterfly::Point& currentMousePoint);


	void mousedownImpl(const Butterfly::Point& point, Button button);
	void mousedragImpl(const Butterfly::Point& point, Button button);
	void mouseupImpl(const Butterfly::Point& point, Button button);

	// Helper functions
	double nearestZeroCrossing(double sampleIdx) const;
	Button getButton(const event& e) const;



	//    void analyse_period_zero_crossings()
	//    {
	//
	//    }

	//void getPitchAndZeroCrossings() {
	//    //Analyze zero-crossings
	//    allZeroCrossings = Butterfly::getCrossings<std::vector<float>::iterator>(inputSamples.begin(), inputSamples.end());
	//    periodZeroCrossings.clear();
	//
	//    //Perform pitch detection
	//    pitchInfoOptional = Butterfly::getPitch(inputSamples.begin(), inputSamples.end());
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
};


void table_preprocessing::setup(float sampleRate) {
	this->sampleRate = sampleRate;
	cout << "dspsetup happend" << endl;
}

void table_preprocessing::setModeImpl(Mode newMode) {
	mode = newMode;
	redraw();
}


void table_preprocessing::updateFreeSelection(const Butterfly::Point& mouseDownPoint, const Butterfly::Point& currentMousePoint) {
	const auto selectionValueRect = transform.from({ mouseDownPoint, currentMousePoint });
	freeSelection.first = std::clamp(selectionValueRect.x, 0., tablePreprocessor.inputSamples.size() - 1.0);
	freeSelection.second = std::clamp(selectionValueRect.x + selectionValueRect.width, 0., tablePreprocessor.inputSamples.size() - 1.0);
}

void table_preprocessing::updateZerosSelection(const Butterfly::Point& mouseDownPoint, const Butterfly::Point& currentMousePoint) {
	double nearestCrossing1 = nearestZeroCrossing(transform.fromX(mouseDownPoint.x));
	double nearestCrossing2 = nearestZeroCrossing(transform.fromX(currentMousePoint.x));
	zerosSelection.first = std::clamp(nearestCrossing1, 0., tablePreprocessor.inputSamples.size() - 1.0);
	zerosSelection.second = std::clamp(nearestCrossing2, 0., tablePreprocessor.inputSamples.size() - 1.0);
	if (zerosSelection.second < zerosSelection.first) { std::swap(zerosSelection.second, zerosSelection.first); }
}

void table_preprocessing::exportFrame(int begin, int end) {
	if (tablePreprocessor.inputSamples.empty()) return;
	if (end <= begin) return;
	if (begin < 0 || end >= tablePreprocessor.inputSamples.size()) return;
	if (std::abs(begin - end) < Wavetable::minimumInputSize()) return;

	targetBuffer.set(targetBufferName);
	buffer_lock<false> buf(targetBuffer);
	if (!buf.valid()) return;
	const int targetTablesize = buf.frame_count();
	const std::vector<float> selectedSamples{ tablePreprocessor.inputSamples.begin() + begin, tablePreprocessor.inputSamples.begin() + end };
	const float exportTableOscFreq = sampleRate / static_cast<float>(targetTablesize);

	const Wavetable table{ selectedSamples, sampleRate / 2.f };
	std::vector<Wavetable> wavetable{ table };
	Osc interpolationOscillator{ &wavetable, sampleRate, exportTableOscFreq };

	for (int i = 0; i < targetTablesize; i++) {
		buf[i] = interpolationOscillator++;
	}
	outletStatus.send("newFrame");
	buf.dirty();
}

void table_preprocessing::drawSamples(target& t) { //Das allgemeingültig schreiben und auch bei StackedFrames verwenden!
	if (tablePreprocessor.inputSamples.empty()) return;

	const auto point = transform.apply({ static_cast<double>(0), -tablePreprocessor.inputSamples[0] * waveformYScaling });
	auto previous = point;
	for (int i = 1; i < tablePreprocessor.inputSamples.size(); ++i) {
		const auto point = transform.apply({ static_cast<double>(i), -tablePreprocessor.inputSamples[i] * waveformYScaling });
		drawLine(t, previous, point, waveformColor, strokeWidth);
		previous = point;
	}
}

void table_preprocessing::drawDraggingRect(target& t) {
	if (dragging && button == Button::Right) {

		c74::max::t_jgraphics* g = t;
		double a[] = { 4, 4 };
		jgraphics_set_dash(g, a, 2, 0);

		//rect<stroke> rect1{ t, color{ waveformColor }, position{ 30, 30 }, size{ 50, 50 } };

		//rect<stroke> rect2{ t, color{ waveformColor }, position{ 20, 20 }, size{ 50, 50 } };
		Butterfly::Rect rect = { currentMousePoint, mouseDownPoint };
		rect.y = margin;
		rect.height = t.height() - 2 * margin;
		if (rect.width > 0) {
			drawRect(t, rect, draggingRectColor, 1.0);
		}
		jgraphics_set_dash(g, 0, 0, 0);
	}
}

void table_preprocessing::drawZeroCrossings(target& t) {
	for (double value : tablePreprocessor.zeroCrossings) {
		const auto p1 = transform.apply({ value, 1. });
		const auto p2 = transform.apply({ value, -1. });
		drawLine(t, p1, p2, zeroCrossingsColor, strokeWidth);
	}
}

void table_preprocessing::drawOverlayRects(target& t) {
	if (mode == Mode::free) {
		if (freeSelection.first == freeSelection.second)
			return;
		const auto r = transform.apply({ { freeSelection.first, 1 }, { freeSelection.second, -1 } });
		fillRect(t, r, overlayColor);
	} else if (mode == Mode::zeros) {
		if (zerosSelection.first == zerosSelection.second)
			return;
		const auto r = transform.apply({ { zerosSelection.first, 1 }, { zerosSelection.second, -1 } });
		fillRect(t, r, overlayColor);
	}
}

void table_preprocessing::inputSamplesChanged() {
	dataRange = Butterfly::Rect::fromBounds(0, tablePreprocessor.inputSamples.size(), -1, 1);
	transform = Butterfly::Transform::MapRect(dataRange, waveformView);
}

void table_preprocessing::targetResized(double width, double height) {
	const auto currentViewRect = transform.from(waveformView);
	targetSize = { width, height };
	waveformView.resize(width - 2 * margin, height - 2 * margin).moveTo({ margin, margin });
	transform = Butterfly::Transform::MapRect(currentViewRect, waveformView);
	//transform = Butterfly::Transform::MapRect(dataRange, waveformView);
}

void table_preprocessing::constrainViewTransform() {
	transform.ensureWithin(dataRange, waveformView);
}



void table_preprocessing::sampleDroppedImpl() {
	inputBuffer.set(inputBufferName);
	buffer_lock<false> buf(inputBuffer);
	if (buf.channel_count() != 1) {
		cout << "Buffer channel count has to be one." << endl;
		return;
	}
	if (!buf.valid()) {
		buf.~buffer_lock();
		return;
	}
	tablePreprocessor.inputSamples.clear();
	for (auto i = 0; i < buf.frame_count(); ++i) {
		tablePreprocessor.inputSamples.push_back(buf.lookup(i, 0));
	}
	Butterfly::peakNormalize(tablePreprocessor.inputSamples.begin(), tablePreprocessor.inputSamples.end());
	tablePreprocessor.analyzeZeroCrossings();
	redraw(); //show new samples

	inputSamplesChanged();
}


void table_preprocessing::mousedownImpl(const Butterfly::Point& point, Button button) {
	if (tablePreprocessor.inputSamples.empty()) return;
	mouseDownPoint = point;
	currentMousePoint = mouseDownPoint;
	dragging = true;
	this->button = button;
	redraw();
}

void table_preprocessing::mousedragImpl(const Butterfly::Point& point, Button button) {
	if (tablePreprocessor.inputSamples.empty())
		return;

	if (button == Button::Left) {
		if (mode == Mode::free) {
			updateFreeSelection(mouseDownPoint, point);
		} else if (mode == Mode::zeros) {
			updateZerosSelection(mouseDownPoint, point);
		}
	} else if (button == Button::Center) {
		auto p = point - currentMousePoint;
		transform.preTranslate(p);
		constrainViewTransform();
	}
	currentMousePoint = point;
	redraw();
}

void table_preprocessing::mouseupImpl(const Butterfly::Point& point, Button button) {
	if (tablePreprocessor.inputSamples.empty()) return;

	currentMousePoint = point;
	if (button == Button::Left) {
		if (mode == Mode::free) {
			updateFreeSelection(mouseDownPoint, point);
		} else if (mode == Mode::zeros) {
			updateZerosSelection(mouseDownPoint, point);
		}
	} else if (button == Button::Right) {
		if (dragging) {
			Butterfly::Rect r{ mouseDownPoint, point };
			r.y = waveformView.y;
			r.height = waveformView.height;
			if (r.width > 0) {
				auto r2 = transform.from(r);
				transform = Butterfly::Transform::MapRect(r2, waveformView);
				constrainViewTransform();
			}
		}
	}
	dragging = false;
	redraw();
}

double table_preprocessing::nearestZeroCrossing(double sampleIdx) const {
	if (tablePreprocessor.zeroCrossings.empty()) { return 0.; }
	return *std::min_element(tablePreprocessor.zeroCrossings.begin(), tablePreprocessor.zeroCrossings.end(), [sampleIdx](auto a, auto b) { return std::abs(a - sampleIdx) < std::abs(b - sampleIdx); });
}

Button table_preprocessing::getButton(const event& e) const {
	if (e.m_modifiers & c74::max::eLeftButton)
		return Button::Left;
	else if (e.m_modifiers & c74::max::eRightButton)
		return Button::Right;
	else if (e.m_modifiers & c74::max::eMiddleButton)
		return Button::Center;
	return Button::Left;
}

MIN_EXTERNAL(table_preprocessing);
