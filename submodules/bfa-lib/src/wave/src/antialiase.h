#pragma once


#include "../../math/src/fft.h"

namespace Butterfly {



/// @brief Remove spectral components in given DFT so the signal will not aliase in time domain when
/// played back with given maximum playback frequency at given samplerate.
///
/// @tparam RandomAccessIterator   Input/output iterator, must meet the requirements of LegacyRandomAccessIterator
/// @param  first                  Signal range start
/// @param  last                   Signal range end
/// @param  samplerate             Sampling rate
/// @param  max_playback_frequency Maximum frequency at which the buffer may be played back periodically without aliasing at given samplerate
template<class RandomAccessIterator>
void antialiase_dft(
	RandomAccessIterator first, RandomAccessIterator last,
	double samplerate,
	double max_playback_frequency) {
	auto size = static_cast<size_t>(std::distance(first, last));

	const auto nyquist = samplerate * 0.5;
	const auto nyquist_index = nyquist / max_playback_frequency;
	const auto cutoff_index = static_cast<size_t>(std::floor(nyquist_index)) + 1;

	if (cutoff_index > size / 2) return;

	(*first).imag(0);

	for (auto it = first + cutoff_index; it != last - cutoff_index + 1; ++it) {
		*it = {};
	}
}


/// @brief Antialiase given signal for a number of maximum frequencies in [freq_first, freq_last) using fourier bandlimiting.
/// The signal length needs to be a power of 2 and must match the size of the FFTCalculator. The latter defines type and size of the signal.
/// In order to write the antialiased signals in <code>std::distance(freq_first, freq_last)</code> outputs, the iterator
/// <paramref name="out_table_first"></paramref> needs to be dereferencable to a type which has a random access iterator
/// that can be fetched by <code>std::begin(*out_table_first + n)</code>.
///
/// @tparam T               Sample type (i.e. float or double)
/// @tparam size            Size of signal (needs to be a power of 2)
/// @tparam InputIterator   Input data iterator, needs to fulfill the requirements of LegacyRandomAccessIterator
/// @tparam FreqIterator    Frequency input iterator, needs to fulfill the requirements of LegacyRandomAccessIterator
/// @tparam OutputIterator  Output data iterator, needs to fulfill the requirements of LegacyRandomAccessIterator and must point to something that <code>std::begin(T&)</code> can be called upon and is itself a LegacyRandomAccessIterator
/// @param  signal_first    Iterator to first input data
/// @param  freq_first      Iterator to first frequency input element
/// @param  freq_last       Iterator to last frequency input element
/// @param  out_table_first Iterator to first range
/// @param  samplerate      Sampling rate
/// @param  fft_calculator  FFT calculator
template<class T, int size, class InputIterator, class FreqIterator, class OutputIterator>
void antialiase(
	InputIterator signal_first,
	FreqIterator freq_first, FreqIterator freq_last,
	OutputIterator out_table_first,
	double samplerate,
	const Butterfly::FFTCalculator<T, size>& fft_calculator) {
	static_assert((size & (size - 1)) == 0, "Size N has to be a power of 2");

	std::array<T, size> data;
	std::array<std::complex<T>, size> fft;
	std::copy(signal_first, signal_first + size, data.begin());

	fft_calculator.fft(signal_first, fft.begin());

	auto freq_it = freq_first;
	auto table_it = out_table_first;
	for (; freq_it != freq_last; ++freq_it, ++table_it) {
		auto copy = fft;
		antialiase_dft(copy.begin(), copy.end(), samplerate, *freq_it);

		// for (auto el : copy)
		//	std::cout << el << ' ';
		// std::cout << "\n\n";

		auto iter = std::begin(*table_it);
		fft_calculator.ifft_real(copy.begin(), iter);
		// for (auto it = iter; it != iter + size; ++it)
		//	std::cout << *it << ' ';
		// std::cout << "\n\n";
	}
}






///// @brief Antialiase given signal for a number of maximum frequencies using fourier bandlimiting.
/////
///// @tparam T               Sample type (i.e. float or double)
///// @tparam size            Size of signal (needs to be a power of 2)
///// @param  data            Signal
///// @param  top_frequencies List of maximum frequencies for which the signal should be antialiased
///// @param  samplerate      Sampling rate
///// @return                 Array of arrays of antialiased signals
// template<class T, int size, int numtables>
// std::array<std::array<T, size>, numtables> antialiase(
//	const std::array<T, size>& data,
//	const std::array<double, numtables>& top_frequencies,
//	double samplerate
//) {
//	static_assert((size & (size - 1)) == 0, "Size N has to be a power of 2");

//	std::array<std::array<T, size>, numtables> results;

//	antialiase(data.begin(), top_frequencies.begin(), top_frequencies)
//		Butterfly::FFTCalculator2<T, size> fft_calculator;
//	const auto fft = fft_calculator.fft(data);

//	for (size_t i = 0; i < numtables; i++) {
//		auto copy = fft;
//		antialiase_dft(copy.begin(), copy.end(), samplerate, top_frequencies[i]);

//		//for (auto el : copy)
//		//	std::cout << el << ' ';
//		//std::cout << "\n\n";
//		results[i] = fft_calculator.ifft_real(copy);
//	}
//	return results;
//}



}
