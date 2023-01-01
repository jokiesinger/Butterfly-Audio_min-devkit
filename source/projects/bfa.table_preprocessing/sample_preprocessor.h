//
//  sample_preprocessor.h
//  Butterfly-Audio_min-devkit
//
//  Created by Jonas Kieser on 07.10.22.
//

#pragma once

#include <chrono>
#include <optional>
#include <span>
#include "painter.h"
#include "event.h"
#include "graphics_transform.h"
#include "wavetable_oscillator.h"


namespace Butterfly {

// Callback for redirecting calls to the "parent" class 
struct Callback
{
	virtual void doRedraw() = 0;
	virtual void doNotifyCanExportStatus() = 0;
};

struct DrawAttributes
{
	Color waveformColor;
	Color zeroCrossingsColor;
	Color overlayColor;
	Color draggingRectColor;
	double strokeWidth;
};

/*
     Klasse könnte input Samples halten, ZeroCrossing Analysen und Pitch Detection durchführen
     Nachricht: "Analysing Input Samples" für die Dauer der Pitch Detection (graphic ausgegraut und nicht klickbar)
     bool für isPeriodic -> Feedback an Nutzer, wenn nicht (period Tab ausgegraut, nicht klickbar)
     Vllt. Setting für Pitchrange/ Schwankung?

     Bei Auswahl mit ZeroCrossings, ganz saubere "Entnahme" mit fractional samples
     Drei selection modes.
        - period (feine Linien markieren Periodenstart, Auswahl der Periode durch Klick nahe einer Linie, ausgewählte samples fett)
        - zeros (feine Linien markieren Zero Crossings, Auswahl eines Bereichs durch Klick auf eine Linie (wird fetter) und Shift+Klick auf eine zweite Linie, ausgewählte Samples fett)
        - custom (Klick - drag - mouseUp markiert den Bereich, ausgewählte Samples fett)
        -> braucht es ein rechteckiges Overlay überhaupt?
     
     Reinzoomen mit Kyanos' Transformationen
     
     Klasse handelt auch die Interpolation in den outputBuffer
     */
struct SamplePreprocessor
{
public:
	using Wavetable = Wavetable<float>;
	using Osc = WavetableOscillator<Wavetable>;


	enum class Mode {
		free,
		zeros,
		period
	};

	SamplePreprocessor(Callback& callback) : callback(callback) {}

	void setup(float sampleRate);
	void setModeImpl(Mode newMode);
	void setSampleData(const std::vector<float>& data);

	void draw(Painter& painter, const DrawAttributes& drawAttributes);

	void mousedownImpl(const MouseEvent& e);
	void mousedragImpl(const MouseEvent& e);
	void mouseupImpl(const MouseEvent& e);
	void mousewheelImpl(const MouseEvent& e);

	bool canExport() const;
	std::optional<std::vector<double>> exportFrame(int targetTablesize);

private:

	void inputSamplesChanged();

	// Drawing
	void redraw() { callback.doRedraw(); }
	void drawSamples(Painter& painter);
	void drawDraggingRect(Painter& painter);
	void drawZeroCrossings(Painter& painter);
	void drawOverlayRects(Painter& painter);
	void targetResized(double width, double height); // Called when the target has been resized through any means
	void resetTransform();
	void constrainViewTransform();

	// Selection/Export
	void updateFreeSelection(const Point& mouseDownPoint, const Point& currentMousePoint);
	void updateZerosSelection(const Point& mouseDownPoint, const Point& currentMousePoint);
	std::pair<int, int> getCurrentExportRange() const;
	void notifyCanExportStatus() { callback.doNotifyCanExportStatus(); }

	// Zero crossings
	void analyzeZeroCrossings();
	double nearestZeroCrossing(double sampleIdx) const;

	// Getters for testing
	Mode getMode() const { return mode; }
	bool isDragging() const { return dragging; }
	double getSampleRate() const { return sampleRate; }
	Rect getDataRange() const { return dataRange; }
	std::pair<double, double> getFreeSelection() const { return freeSelection; }


	// Data
	Callback& callback;

	std::vector<float> inputSamples;
	std::vector<double> zeroCrossings;

	double waveformYScaling = 0.9;
	double zoomSpeed = 1.1;
	double fastZoomSpeed = 1.8;

	float margin{ 10.f }; //Könnte auch als Attribut
	float sampleRate{ 48000.f };
	Mode mode{ Mode::free };

	// std::optional<PitchInfo> pitchInfoOptional;
	// PitchInfo pitchInfo;
	// float tf0, deviationInSmps{10.f};


	// mouse event related
	Point mouseDownPoint, currentMousePoint;
	MouseEvent::Button button{};
	bool dragging{ false };
	std::chrono::time_point<std::chrono::system_clock> clickTime;

	// transform related
	Rect dataRange;						 // size of the draw target, will be updated on first draw.
	Point targetSize{ 100, 100 };		 // size of the draw target, will be updated on first draw.
	Rect waveformView{ {}, targetSize }; // size of the draw target, will be updated on first draw.
	Transform transform;
	std::pair<double, double> freeSelection;
	std::pair<double, double> zerosSelection;
};

}
