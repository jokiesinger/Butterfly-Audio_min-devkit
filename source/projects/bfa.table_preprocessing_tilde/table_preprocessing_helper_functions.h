//
//  stacked_tables_helper_functions.h
//  Butterfly-Audio_min-devkit
//
//  Created by Jonas Kieser on 07.10.22.
//

#include "waveform_processing.h"

struct OverlayRect {
    int x1{}, x2{};
//    int width{};
    bool visible{false};
    
    int getWidth() {
        return std::abs(x2 - x1);
    }
    
    int getStartX() {
        int startX = (x1 <= x2) ? x1 : x2;
        return startX;
    }
    
//    std::pair<int, int> getBounds() {
//        std::pair<int, int> bounds;
//        bounds.first = (x1 <= x2) ? x1 : x2;
//        bounds.second = (x2 >= x1) ? x2 : x1;
//        return bounds;
//    }
};

struct SampleSelection {
    int firstSample{};      //refers to indices in input_buffer
    int lastSample{};
    float selectedZeroCrossing{};
};

struct TablePreprocessor {
    std::vector<float> inputSamples{};
    std::vector<double> zeroCrossings{};
    
    void analyzeZeroCrossings() {
        zeroCrossings = Butterfly::getCrossings<std::vector<float>::iterator>(inputSamples.begin(), inputSamples.end());
    }
    
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
    
};
