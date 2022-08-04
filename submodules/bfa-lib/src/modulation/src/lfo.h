
// Implementation of low frequency oscillator (LFO).

#pragma once

#include <cmath>
#include "modulation_routing_utilities.h"


namespace Butterfly {


template<class T, int size>
struct LFOTable
{
	const double* get() const { return &table[0]; }
	T table[size + 1]{};
};

template<class T, int size>
struct SinLFOTable : public LFOTable<T, size>
{
	constexpr SinLFOTable() {
		for (int i = 0; i < size + 1; i++)
			this->table[i] = std::sin(2.0 * 3.141592653589793 * i / size);
	}
};

template<class T, int size>
struct TriangleLFOTable : public LFOTable<T, size>
{
	constexpr TriangleLFOTable() {
		const T size_quarters = size / T(4);
		for (int i = 0; i < size / 4; i++) {
			const T ii = i;
			this->table[i] = ii / size_quarters;
			this->table[i + size / 4] = (size_quarters - i) / size_quarters;
			this->table[i + size / 2] = -i / size_quarters;
			this->table[i + 3 * size / 4] = -(size_quarters - i) / size_quarters;
		}
		this->table[size] = 0.0f;
	}
};

template<class T, int size>
struct SawtoothLFOTable : public LFOTable<T, size>
{
	constexpr SawtoothLFOTable() {
		for (int i = 0; i < size; i++) {
			this->table[i] = 2.0 * (i / (size - T(1))) - 1.0;
		}
		this->table[size] = -1.0f;
	}
};

template<class T, int size>
struct SquareLFOTable : public LFOTable<T, size>
{
	constexpr SquareLFOTable() {
		for (int i = 0; i < size / 2; i++) {
			this->table[i] = T(1);
			this->table[i + size / 2] = -T(1);
		}
		this->table[size] = 1.0f;
	}
};

template<class T, int size>
struct ExpLFOTable : public LFOTable<T, size>
{
	constexpr ExpLFOTable() {
		const T size_halfs = size / T(2);
		const T e = exp(1.0);
		for (int i = 0; i < size / 2; i++) {
			this->table[i] = 2.0 * ((std::exp(i / size_halfs) - 1.0) / (e - 1.0)) - 1.0;
			this->table[i + size / 2] = 2.0 * ((std::exp((size_halfs - i) / size_halfs) - 1.0) / (e - 1.0)) - 1.0;
		}
		this->table[size] = -1.0f;
	}
};


struct ClampedAdditionLFOFreq
{
	static constexpr double chain_modulation(double a, double b) { return a + b; }
	static constexpr double apply_modulation(double a, double b) { return std::max(0.01, std::min(300.0, a + b)); }
	static constexpr double neutral_element() { return 0.0; }
};

class LFOFreqValue : public ModulatableValue<ClampedAdditionLFOFreq, IModulationDestination::Type::Frequency>
{
public:
	using ModulatableValue<ClampedAdditionLFOFreq, IModulationDestination::Type::Frequency>::ModulatableValue;
};

/// @brief Lookup-based LFO class that supports multiple lfo shapes.
///         - features a modulation source
///         - frequency and width can be modulated
///         - linearily interpolated lookup tables
///         - "smooth" parameter that applies a lowpass filter on the source
///         - starting phase can be configured which is used at reset()
///
/// @tparam T Value type.
class MultiLookupLFO : public IModulationSource
{
public:
	enum class Shape {
		Sine,
		Triangle,
		Sawtooth,
		Square,
		Exp
	};

	MultiLookupLFO(double samplerate, double frequency)
		: frequency([this](double) { this->updatePhaseInc(); }, 1.0),
		  width([](double) {}, 1.0) {
		setSamplerate(samplerate);
		setFrequency(samplerate);
	}


	void setFrequency(double frequency) {
		this->frequency.setParamValue(frequency);
		updatePhaseInc();
	}

	void setWidth(double width) {
		this->width.setParamValue(width);
	}

	void setSmoothingTime(double seconds) {
		smoothingTime = seconds;
		smoothingParameter = 1.0 - std::exp(-3.14159265358 * 2.0 / (smoothingTime * samplerate));
	}

	void setStartPhase(double normalizedStartPhase) {
		startPhase = static_cast<fixed_float>(normalizedStartPhase * tablesize * fixedPointMultiplicator);
	}

	void setShape(Shape shape) {
		switch (shape) {
		case Shape::Sine: table = sinTable.get(); break;
		case Shape::Triangle: table = triTable.get(); break;
		case Shape::Sawtooth: table = sawTable.get(); break;
		case Shape::Square: table = sqTable.get(); break;
		case Shape::Exp: table = expTable.get(); break;
		}
		this->shape = shape;
	}

	double getSamplerate() const { return samplerate; }
	double getFrequency() const { return frequency.getParamValue(); }
	double getWidth() const { return width.getParamValue(); }
	double getSmoothingTime() const { return smoothingTime; }
	double getStartPhase() const { return startPhase / (tablesize * fixedPointMultiplicator); }
	Shape getShape() const { return shape; }

	double operator+=(int samples) {
		// if (rampSamples > 0) {
		//	value += samples * rampInc;
		//	rampSamples -= samples;
		//	phase += phaseInc * samples;
		//	return value;
		// }
		const auto index = phase >> fixedPointFractionalBits;
		const auto fractional = (phase & fractionalMask) * fixedPointMultiplicatorInv;
		phase += phaseInc * samples;

		const auto current = table[index] * (1.0 - fractional) + table[index + 1] * fractional;
		return value += (current * width.getModulatedValue() - value) * smoothingParameter;
	}

	double operator++() {
		(*this) += (1);
	}

	double operator()() const { return value; }

	void retrigger() {
		phase = startPhase;
	}

	void reset() {
		retrigger();
		value = 0;
	}

	double getValue() const override { return 1; }
	Polarity getPolarity() const override { return Polarity::Bipolar; }
	UpdateRate getUpdateRate() const override { return UpdateRate::PerBlock; }

	IModulationSource& getOutput() { return *this; }

	IModulationDestination& getFrequencyInput() { return frequency; }
	IModulationDestination& getWidthInput() { return width; }


private:
	void setSamplerate(double samplerate) {
		this->samplerate = samplerate;
		samplerate_inv = 1.0 / samplerate;
		setSmoothingTime(smoothingTime);
		updatePhaseInc();
	}

	void updatePhaseInc() {
		phaseInc = static_cast<fixed_float>(fixedPointMax * frequency.getModulatedValue() * samplerate_inv * fixedPointMultiplicator);
	}

	using int_type = uint32_t; // may also be uint64_t
	using fixed_float = int_type;
	static constexpr int_type fixedPointIntegerBits = 8; // each increase by one doubles the table size
	static constexpr int_type fixedPointFractionalBits = 8 * sizeof(fixed_float) - fixedPointIntegerBits;
	static constexpr double fixedPointMax = int_type(1) << fixedPointIntegerBits;
	static constexpr double fixedPointMultiplicator = int_type(1) << fixedPointFractionalBits;
	static constexpr double fixedPointMultiplicatorInv = 1.0 / fixedPointMultiplicator;
	static constexpr int_type fractionalMask = (int_type(1) << fixedPointFractionalBits) - 1;

	static constexpr int_type tablesize = int_type(1) << fixedPointIntegerBits;


	double samplerate{ 1. };
	double samplerate_inv{ 1. };

	fixed_float phase{}, phaseInc{}, startPhase{};
	double value{};

	LFOFreqValue frequency;
	VolumeValue width;

	double smoothingTime{ 0 };
	double smoothingParameter{ 1 };

	const double* table = sinTable.get(); // currently used table

	Shape shape{ Shape::Sine };
	inline static SinLFOTable<double, tablesize> sinTable{};
	inline static TriangleLFOTable<double, tablesize> triTable{};
	inline static SawtoothLFOTable<double, tablesize> sawTable{};
	inline static SquareLFOTable<double, tablesize> sqTable{};
	inline static ExpLFOTable<double, tablesize> expTable{};
};

}
