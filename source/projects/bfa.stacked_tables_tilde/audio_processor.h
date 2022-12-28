#pragma once

#include "morphing_wavetable_oscillator.h"
#include "release_pool.h"

namespace Butterfly {

enum class ParameterType {
	gain,
	frequency,
	morphPos,
	sampleRate
};

struct Event
{
	ParameterType parameterType{};
	double value{};
};


template<typename It>
constexpr auto make_span(It begin, It end) {
	return std::span<std::remove_pointer_t<typename std::iterator_traits<It>::pointer>>(&(*begin), std::distance(begin, end));
}

class AudioProcessor
{
	using Multitable = std::vector<Wavetable<float>>;
	using MultitableCollection = std::vector<Multitable>;
	using State = MultitableCollection;

public:
	void init(double oscFreq, double sampleRate, int maxFrames, double gain = 1.) {
		setSampleRate(sampleRate);
		frequency.set(oscFreq);
		osc.setMaxNumWaveforms(maxFrames);
		waveforms.reserve(maxFrames);
		this->gain.set(gain);
	}

	//=====================================
	//                UI
	//=====================================
	// UI thread only!
	void changeState(State&& newState) {
		auto newSharedState = std::make_shared<State>(newState);
		releasePool.add(newSharedState);
		std::atomic_store(&currentState, newSharedState);
		releasePool.clearUnused();
	}
	// UI thread only!
	void addParamEvent(Event event) {
		eventQueue.enqueue(event);
	}

	//=====================================
	//               AUDIO
	//=====================================

	// Audio thread
	void process(c74::min::audio_bundle& buffer) {
		auto newState = std::atomic_load(&currentState);
		if (previousState != newState.get()) {
			previousState = newState.get();
			waveforms.clear();
			assert(waveforms.capacity() >= newState->size());
			for (const auto& multitable : *newState) {
				waveforms.push_back(make_span(multitable.begin(), multitable.end()));
			}
			osc.setWaveforms(waveforms);
		}
		Event event; //Allocation in process function? -> I would go with atomic<double> value.store() & value.load()
		while (eventQueue.try_dequeue(event)) {
			processEvent(event);
		}

		for (auto i = 0; i < buffer.frame_count(); ++i) {
			buffer.samples(0)[i] = ++osc * ++gain;
			frequency++;
		}
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
		case ParameterType::sampleRate:
			setSampleRate(event.value);
			break;
		}
	}

	void setGain(double gain) {
		this->gain.set(gain);
	}

	void setFrequency(double frequency) {
		this->frequency.set(frequency);
		osc.setFrequency(frequency);
	}

	void setMorphPos(double morphPos) {
		osc.setNormalizedMorphingParam(morphPos);
	}

	void setSampleRate(double sampleRate) {
		osc.setSampleRate(sampleRate);
	}


	MorphingWavetableOscillator<WavetableOscillator<Wavetable<float>>> osc;
	std::vector<std::span<const Wavetable<float>>> waveforms;
	RampedValue<double> gain{ 1. }, frequency{ 10. }; //Probably not needed
	std::shared_ptr<State> currentState{};
	State* previousState{};
	ReleasePool<MultitableCollection> releasePool;
	c74::min::fifo<Event> eventQueue{ 16 };
};
}
