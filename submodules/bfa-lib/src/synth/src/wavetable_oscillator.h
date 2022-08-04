
// Implementation of a wavetable oscillator

#pragma once
#include <cmath>
#include "wavetable.h"

namespace Butterfly {


/// @brief Table selector for a wavetable oscillator which iterates forward through
///        the given range until a table is found which has a playback frequency above the
///        given frequency.
struct ForwardSearchTableSelector
{
	template<class Iterator, class T>
	static constexpr Iterator selectTable(Iterator begin, Iterator end, T frequency) {
		size_t i = 0;
		for (auto it = begin; it != end; ++it, ++i) {
			if (it->getMaximumPlaybackFrequency() >= frequency) {
				return it;
			}
		}
		return end;
	}
};


/// @brief Wavetable oscillator wrapping access to multiple wavetables depending on the
///        frequency used (i.e. in order to prevent aliasing). 
///        
///        It is assumed that the tables are sorted in an ascending order, so that the table with 
///        the lowest frequency is the first one.
///        
///        The oscillator is only in a valid and usable state when the tables and the frequency is
///        set (as done in the first constructor). 
///        
///        The frequency may technically exceed the frequency of the last table which may result 
///        in aliasing. However, the frequency shall not exceed the sample rate (which is also  
///        asserted internally). 
///
/// Usage example using a std::vector of wavetables (which is the default storage type):
/// \code
///	    std::vector<Wavetable<double>> tables(4);
///	    // fill tables
///	    WavetableOscillator<Wavetable<double>> osc{ &tables, 40100., 200. };
///	    auto sample = ++osc;
/// \endcode
/// 
/// You can also use a std::array (or any other random access storage type) for your tables. 
/// \code
///     using Table = Wavetable<double>;
///     using MultiTable = std::array<Table, 5>;
///     using Osc = WavetableOscillator<Table, MultiTable>;
///     
///     MultiTable tables;
///     // fill tables
///	    Osc{ tables, 40100., 200 };
/// \endcode
/// 
/// @tparam Wavetable Wavetable class (needs to feature size_t size(), T get(T) and getMaximumPlaybackFrequency()).
/// @tparam TableSelector Functor which is used to select the ideal table for the current playback frequency.
template<class Wavetable, class StorageType = std::vector<Wavetable>, class TableSelector = ForwardSearchTableSelector>
class WavetableOscillator
{
public:
	using value_type = typename Wavetable::value_type;
	using T = typename Wavetable::value_type;
	using MultiWavetable = StorageType;

	constexpr WavetableOscillator(MultiWavetable* wavetables, T sampleRate, T frequency)
		: sampleRateInv(T(1.) / sampleRate),
		  frequency(frequency) {
		setTables(wavetables);
	}

	constexpr WavetableOscillator(T sampleRate) : sampleRateInv(T(1.) / sampleRate) {}

	constexpr void setTables(MultiWavetable* wavetables) {
		this->wavetables = wavetables;
		topFreq = bottomFreq = T(0);
		setFrequency(frequency);
	}

	constexpr void setFrequency(T frequency) {
		this->frequency = frequency;
		assert(frequency * sampleRateInv < 1 && "The frequency needs to be lower that the sample rate");
		selectTable();
		updateDelta();
	}

	/// @brief Increment the oscillator by one step and get the current value.
	/// @return current value
	constexpr T operator++() {
		currentSamplePosition += delta;
		if (currentSamplePosition >= currentTableSize) {
			currentSamplePosition -= currentTableSize;
		}
		value = (*currentTable)(currentSamplePosition);
		return value;
	}

	/// @brief Increment the oscillator by one step and get the former current value.
	/// @return former value
	constexpr T operator++(int) {
		const auto tmp = value;
		currentSamplePosition += delta;
		if (currentSamplePosition >= currentTableSize) {
			currentSamplePosition -= currentTableSize;
		}
		value = (*currentTable)(currentSamplePosition);
		return tmp;
	}

	/// @brief Get current value of the oscillator without changing its state.
	/// @return current  value
	constexpr T operator()() { return value; }

	/// @brief Reset the position/phase to 0. Also updates the current value.
	constexpr void retrigger() {
		currentSamplePosition = 0;
		value = (*currentTable)(currentSamplePosition);
	}

	constexpr void reset() { retrigger(); }
	constexpr Wavetable* getSelectedTable() const { return currentTable; }
	constexpr T getFrequency() const { return frequency; }
	constexpr T getSampleRate() const { return 1.0 / sampleRateInv; }

private:
	constexpr void selectTable() {
		// chances are good that the frequency has changed just a little bit, so
		// only select table if antialiasing condition really does not hold.
		if (frequency <= topFreq && frequency > bottomFreq) return;

		assert(wavetables);
		assert(wavetables->size() > 0);
		auto it = TableSelector::selectTable(wavetables->begin(), wavetables->end(), frequency);

		if (it == wavetables->end()) { // No table can be selected without aliasing -> then we just get aliasing
			it--;
		}

		auto newTable = &(*it);

		if (currentTableSize != 0) {
			const auto oldTableSize = currentTableSize;
			const auto newTableSize = newTable->size();
			currentSamplePosition = currentSamplePosition * static_cast<T>(newTableSize) / static_cast<T>(oldTableSize);
			currentSamplePosition = std::max(T(0), std::min(T(newTableSize - 1e-7), currentSamplePosition));
		}

		currentTable = &(*it);
		currentTableSize = newTable->size();
		value = (*currentTable)(currentSamplePosition);


		topFreq = currentTable->getMaximumPlaybackFrequency();
		if (it == wavetables->begin()) {
			bottomFreq = T(0);
		} else {
			bottomFreq = (it - 1)->getMaximumPlaybackFrequency();
		}
	}

	constexpr void updateDelta() {
		delta = frequency * static_cast<T>(currentTableSize) * sampleRateInv;
	}


	T sampleRateInv{};
	T frequency{};
	T delta{};

	T currentSamplePosition{};
	T value{};

	MultiWavetable* wavetables{};
	Wavetable* currentTable{};
	size_t currentTableSize{};

	T topFreq{}, bottomFreq{}; // Frequency interval of current table
};


/// @brief Wavetable oscillator for morphing between two wavetables. A parameter in the interval
///        [0, 1] is used to blend between the first and the second table.
///
/// @tparam WavetableOscillator Wavetable oscillator class
template<class WavetableOscillator>
class MorpingWavetableOscillator
{

public:
	using value_type = typename WavetableOscillator::value_type;
	using MultiWavetable = typename WavetableOscillator::MultiWavetable;
	using T = value_type;

	constexpr MorpingWavetableOscillator(MultiWavetable* firstTable, MultiWavetable* secondTable, T sampleRate, T frequency)
		: osc1(firstTable, sampleRate, frequency),
		  osc2(secondTable, sampleRate, frequency) {}

	constexpr MorpingWavetableOscillator(T sampleRate) : osc1(sampleRate), osc2(sampleRate) {}

	constexpr void setTables(MultiWavetable* firstTable, MultiWavetable* secondTable) {
		osc1.setTables(firstTable);
		osc2.setTables(secondTable);
	}

	constexpr void setFrequency(T frequency) {
		osc1.setFrequency(frequency);
		osc2.setFrequency(frequency);
	}

	constexpr void setParam(T param) {
		this->param = param;
	}

	constexpr T operator++() {
		return (T(1) - param) * ++osc1 + param * ++osc2;
	}

	constexpr T operator++(int) {
		return (T(1) - param) * osc1++ + param * osc2++;
	}

	constexpr T operator()() {
		return (T(1) - param) * osc1() + param * osc2();
	}

	constexpr void retrigger() {
		osc1.retrigger();
		osc2.retrigger();
	}

	constexpr void reset() {
		osc1.reset();
		osc2.reset();
	}

	constexpr T getFrequency() const { return osc1.getFrequency(); }
	constexpr T getSampleRate() const { return osc1.getSampleRate(); }
	constexpr T getParam() const { return param; }

private:
	T param{};
	WavetableOscillator osc1, osc2;
};
}
