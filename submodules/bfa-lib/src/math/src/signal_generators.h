
// Functions for generating basic signals/waveforms into an arbitrary buffer.

#pragma once

#include <cmath>
#include "constants.h"

namespace Butterfly {


/// @brief Fill given range with a sine curve.
/// @tparam ForwardIterator Data iterator, must meet the requirements of LegacyForwardIterator
/// @param first start of range
/// @param last end of range
/// @param offset offset sine by a number of data points
/// @param cycles number of sine cycles to fill data with
template<class ForwardIterator>
void generate_sine(ForwardIterator first, ForwardIterator last, double offset = 0.0, double cycles = 1.0) {
	using T = typename ForwardIterator::value_type;
	const auto size = std::distance(first, last);
	const double f = 2.0 * cycles * pi<double>() / static_cast<double>(size);
	size_t i = 0;
	for (auto it = first; it != last; ++it, ++i) {
		*it = static_cast<T>(std::sin((static_cast<double>(i) + offset) * f));
	}
}

/// @brief Fill given range with a triangle function.
/// @tparam ForwardIterator Data iterator, must meet the requirements of LegacyForwardIterator
/// @param first start of range
/// @param last end of range
/// @param offset offset sine by a number of data points
/// @param cycles number of cycles to fill data with
template<class ForwardIterator>
void generate_triangle(ForwardIterator first, ForwardIterator last, double offset = 0.0, double cycles = 1.0) {
	using T = typename ForwardIterator::value_type;
	const auto size = std::distance(first, last);
	const double f = cycles / static_cast<double>(size);
	const double x0 = offset + static_cast<double>(size) / (4.0 * cycles);
	size_t i = 0;
	for (auto it = first; it != last; ++it, ++i) {
		auto arg = (i + x0) * f;
		*it = static_cast<T>(4.0 * std::abs(std::round(arg) - arg) - 1);
	}
}

/// @brief Fill given range with a rectangle signal.
/// @tparam ForwardIterator Data iterator, must meet the requirements of LegacyForwardIterator
/// @param first start of range
/// @param last end of range
/// @param offset offset sine by a number of data points
/// @param cycles number of cycles to fill data with
template<class ForwardIterator>
void generate_rectangle(ForwardIterator first, ForwardIterator last, double offset = 0.0, double cycles = 1.0) {
	using T = typename ForwardIterator::value_type;
	generate_triangle(first, last, offset, cycles);
	size_t i = 0;
	for (auto it = first; it != last; ++it, ++i) {
		auto& a = *it;
		a = (a > T(0)) - (a < T(0));
	}
}
}
