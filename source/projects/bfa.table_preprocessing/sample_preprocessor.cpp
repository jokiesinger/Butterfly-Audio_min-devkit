
#include "sample_preprocessor.h"
#include "waveform_processing.h"

using namespace Butterfly;


void Butterfly::SamplePreprocessor::setSampleData(const std::vector<float>& data) {
	inputSamples = data;
	Butterfly::peakNormalize(inputSamples.begin(), inputSamples.end());
	analyzeZeroCrossings();
	freeSelection = {};
	zerosSelection = {};
	inputSamplesChanged();
}

void SamplePreprocessor::setup(float sampleRate) {
	this->sampleRate = sampleRate;
	//cout << "dspsetup happend" << endl;
}

void SamplePreprocessor::setModeImpl(Mode newMode) {
	mode = newMode;
	notifyCanExportStatus();
}

void SamplePreprocessor::updateFreeSelection(const Point& mouseDownPoint, const Point& currentMousePoint) {
	const auto selectionValueRect = transform.from({ mouseDownPoint, currentMousePoint });
	freeSelection.first = std::clamp(selectionValueRect.x, 0., inputSamples.size() - 1.0);
	freeSelection.second = std::clamp(selectionValueRect.x + selectionValueRect.width, 0., inputSamples.size() - 1.0);
	notifyCanExportStatus();
}

void SamplePreprocessor::updateZerosSelection(const Point& mouseDownPoint, const Point& currentMousePoint) {
	double nearestCrossing1 = nearestZeroCrossing(transform.fromX(mouseDownPoint.x));
	double nearestCrossing2 = nearestZeroCrossing(transform.fromX(currentMousePoint.x));
	zerosSelection.first = std::clamp(nearestCrossing1, 0., inputSamples.size() - 1.0);
	zerosSelection.second = std::clamp(nearestCrossing2, 0., inputSamples.size() - 1.0);
	if (zerosSelection.second < zerosSelection.first) { std::swap(zerosSelection.second, zerosSelection.first); }
	notifyCanExportStatus();
}

bool SamplePreprocessor::canExport() const {
	const auto [begin, end] = getCurrentExportRange();
	if (inputSamples.empty()) return false;
	if (end <= begin) return false;
	if (begin < 0 || end >= inputSamples.size()) return false;
	if (std::abs(begin - end) < Wavetable::minimumInputSize()) return false;
	return true;
}

std::pair<int, int> SamplePreprocessor::getCurrentExportRange() const {
	if (mode == Mode::free) {
		return { static_cast<int>(std::round(freeSelection.first)), static_cast<int>(std::round(freeSelection.second)) };
	} else if (mode == Mode::zeros) {
		return { static_cast<int>(std::round(zerosSelection.first)), static_cast<int>(std::round(zerosSelection.second)) };
	}
	return {};
}

std::optional<std::vector<double>> SamplePreprocessor::exportFrame(int targetTablesize) {
	if (!canExport()) return std::nullopt;

	std::vector<double> data(targetTablesize);
	const auto [begin, end] = getCurrentExportRange();

	const std::vector<float> selectedSamples{ inputSamples.begin() + begin, inputSamples.begin() + end };
	const float exportTableOscFreq = sampleRate / static_cast<float>(targetTablesize);

	Wavetable table{ selectedSamples, sampleRate / 2.f };
	//std::vector<Wavetable> wavetable{ table };
    std::span<Wavetable> wavetable{ &table, 1};
	Osc interpolationOscillator{ wavetable, sampleRate, exportTableOscFreq };

	for (int i = 0; i < targetTablesize; i++) {
		data[i] = interpolationOscillator++;
	}
	return std::move(data);
}

void SamplePreprocessor::draw(Painter& painter, const DrawAttributes& drawAttributes) {
	if (targetSize.x != painter.getWidth() || targetSize.y != painter.getHeight()) {
		targetResized(painter.getWidth(), painter.getHeight());
	}


	if (inputSamples.size() > 0) {
		painter.fillColor(drawAttributes.overlayColor);
		drawOverlayRects(painter);

		if ((zeroCrossings.size() > 0) && (mode == SamplePreprocessor::Mode::zeros)) {
			painter.strokeColor(drawAttributes.zeroCrossingsColor);
			painter.strokeWidth(drawAttributes.strokeWidth);
			drawZeroCrossings(painter);
		}

		painter.strokeWidth(drawAttributes.strokeWidth);
		painter.strokeColor(drawAttributes.waveformColor);
		painter.fillColor(drawAttributes.waveformColor);
		drawSamples(painter);

		painter.strokeColor(drawAttributes.draggingRectColor);
		drawDraggingRect(painter);
	}
}

void SamplePreprocessor::drawSamples(Painter& painter) { //Das allgemeingültig schreiben und auch bei StackedFrames verwenden!
	if (inputSamples.empty()) return;

	// get first/last visible sample
	const int first = std::max(0., transform.fromX(0) - 1.);
	const int last = std::min<double>(inputSamples.size(), std::ceil(transform.fromX(targetSize.x) + 1.));
	const int step = std::max<int>(1, (last - first) / (targetSize.x * 10.));

	if (step == 1) {
		const auto point = transform.apply({ static_cast<double>(first), -inputSamples[first] * waveformYScaling });
		auto previous = point;
		for (int i = first + 1; i < last; ++i) {
			const auto point = transform.apply({ static_cast<double>(i), -inputSamples[i] * waveformYScaling });
			painter.line(previous, point);
			previous = point;
		}
	} else if (step < 10) {
		const auto point = transform.apply({ static_cast<double>(first), -inputSamples[first] * waveformYScaling });
		auto previous = point;
		for (int i = first + step; i < last; i += step) {
			auto sample = *std::max_element(inputSamples.begin() + i - step, inputSamples.begin() + i, [](auto a, auto b) { return a * a < b * b; });
			const auto point = transform.apply({ static_cast<double>(i), -sample * waveformYScaling });
			painter.line(previous, point);
			previous = point;
		}
	} else {
		for (int i = first + step; i < last; i += step) {
			auto [min, max] = std::minmax_element(inputSamples.begin() + i - step, inputSamples.begin() + i);
			const auto p1 = transform.apply({ static_cast<double>(i), -*min * waveformYScaling });
			const auto p2 = transform.apply({ static_cast<double>(i), -*max * waveformYScaling });
			painter.line(p1, p2);
		}
	}

	if (transform.apply({ 0, 0, 1, 0 }).width > targetSize.x / 20.) {
		for (int i = first; i < last; ++i) {
			const auto point = transform.apply({ static_cast<double>(i), -inputSamples[i] * waveformYScaling });
			painter.point(point, 5.0);
		}
	}
}

void SamplePreprocessor::drawDraggingRect(Painter& painter) {
	if (dragging && button == MouseEvent::Button::Right) {
		Rect rect = { currentMousePoint, mouseDownPoint };
		rect.y = margin;
		rect.height = painter.getHeight() - 2 * margin;
		if (rect.width > 0) {
			painter.setDashPattern({ 4, 4 });
			painter.rectOutline(rect, 1.0);
			painter.setSolid();
		}
	}
}

void SamplePreprocessor::drawZeroCrossings(Painter& painter) {
	// get first/last visible sample
	const int first = std::max(0., transform.fromX(0) - 1.);
	const int last = std::min<double>(inputSamples.size(), std::ceil(transform.fromX(targetSize.x) + 1.));
	const int step = std::max<int>(1, (last - first) / (targetSize.x * 10.));

	if (step > 5) return; // dont draw crossings when they get too dense
	for (double value : zeroCrossings) {
		if (value < first) continue; // only draw visible crossings
		if (value > last) break;
		const auto p1 = transform.apply({ value, 1. });
		const auto p2 = transform.apply({ value, -1. });
		painter.line(p1, p2);
	}
}

void SamplePreprocessor::drawOverlayRects(Painter& painter) {
	if (mode == Mode::free) {
		if (freeSelection.first == freeSelection.second)
			return;
		auto r = transform.apply({ { freeSelection.first, 1 }, { freeSelection.second, -1 } });
		// prevent huge rects to be drawn which isnt handled well by jgraphics
		r.width = std::min(r.x + r.width, painter.getWidth()) - r.x;
		const auto newX1 = std::max(r.x, -1.0);
		const auto newX2 = std::min(r.x + r.width, painter.getWidth() + 1.0);
		r.x = newX1;
		r.width = std::max(newX2 - newX1, 0.01);

		painter.rect(r);
	} else if (mode == Mode::zeros) {
		if (zerosSelection.first == zerosSelection.second)
			return;
		const auto r = transform.apply({ { zerosSelection.first, 1 }, { zerosSelection.second, -1 } });
		painter.rect(r);
	}
}

void SamplePreprocessor::inputSamplesChanged() {
	resetTransform();
}

void SamplePreprocessor::resetTransform() {
	dataRange = Rect::fromBounds(0, inputSamples.size(), -1, 1);
	transform = Transform::MapRect(dataRange, waveformView);
}

void SamplePreprocessor::targetResized(double width, double height) {
	const auto currentViewRect = transform.from(waveformView);
	targetSize = { width, height };
	waveformView.resize(width - 2 * margin, height - 2 * margin).moveTo({ margin, margin });
	transform = Transform::MapRect(currentViewRect, waveformView);
	//transform = Transform::MapRect(dataRange, waveformView);
}

void Butterfly::SamplePreprocessor::analyzeZeroCrossings() {
	zeroCrossings = Butterfly::getCrossings<std::vector<float>::iterator>(inputSamples.begin(), inputSamples.end());
}

void SamplePreprocessor::constrainViewTransform() {
	transform.ensureWithin(dataRange, waveformView);
}

void SamplePreprocessor::mousedownImpl(const MouseEvent& e) {
	if (inputSamples.empty()) return;
	mouseDownPoint = { e.x, e.y };
	currentMousePoint = mouseDownPoint;
	dragging = true;
	this->button = e.button;
	redraw();
}

void SamplePreprocessor::mousedragImpl(const MouseEvent& e) {
	if (inputSamples.empty())
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

void SamplePreprocessor::mouseupImpl(const MouseEvent& e) {
	if (inputSamples.empty()) return;
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

void SamplePreprocessor::mousewheelImpl(const MouseEvent& e) {
	const double speed = e.isControlDown() ? fastZoomSpeed : zoomSpeed;
	const auto delta = e.deltaY > 0 ? speed : 1 / speed;
	if (delta > 1. && transform.apply({ 0, 0, 1, 0 }).width > targetSize.x) return;
	transform.scaleAround(e.x, e.y, delta, 1);
	constrainViewTransform();
	redraw();
}

double SamplePreprocessor::nearestZeroCrossing(double sampleIdx) const {
	if (zeroCrossings.empty()) { return 0.; }
	return *std::min_element(zeroCrossings.begin(), zeroCrossings.end(), [sampleIdx](auto a, auto b) { return std::abs(a - sampleIdx) < std::abs(b - sampleIdx); });
}
