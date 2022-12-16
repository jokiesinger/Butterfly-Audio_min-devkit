#pragma once
#include "wavetable_oscillator.h"
#include "release_pool.h"

namespace Butterfly {

enum class ParameterType {
    gain, frequency, morphPos
};

struct Event {
    ParameterType parameterType;
    double value{};
};

class AudioProcessor {
    using Multitable = std::vector<Wavetable<float>>;
    using MultitableCollection = std::vector<Multitable>;
    using State = MultitableCollection;
public:
    
    AudioProcessor() {}
    
    // UI thread only!
    void changeState(State&& newState) {
        auto newSharedState = std::make_shared<State>(newState);
        releasePool.add(newSharedState);
        std::atomic_store(&currentState, newSharedState);
        releasePool.clearUnused();
    }
    // UI thread only!
    void sendEvent(Event event) {
        eventQueue.push(event);
    }
    
    // Audio thread
    void process() {
        auto newState = std::atomic_load(&currentState);
        if (previousState != newState.get()) {
            previousState = newState.get();
            //osc.setTables(...)
        }
        Event event
        while (eventQueue.try_dequeue(event)) {
            processEvent(event);
        }
        //TODO: Audio
        
        
    }
    
private:
    
    void processEvent(const Event& event) {
        switch (event.parameterType) {
            case ParameterType::gain:
                setGain(event.value);
                break;
            case ParameterType::frequency:
                setFrequency(event.value);
                break;
            case ParameterType::morphPos:
                setMorphPos(event.value);
                break;
        }
    }
    
    void setGain(double gain) {
        this->gain.set(gain);
    }

    void setFrequency(double frequency) {
        this->frequency.set(frequency);
    }
    
    void setMorphPos(double morphPos) {
        ///TODO: …
    }
    
    Osc osc;
    RampedValue gain{1.}, frequency{10.};
    std::shared_ptr<State> currentState{};
    State* previousState{};
    ReleasePool<MultitableCollection> releasePool;
    c74::min::fifo<Event> eventQueue{16};
};
}
