#pragma once
#include <algorithm>


namespace Butterfly {

/// @brief Get absolute maximum of signal (peak value).
/// @tparam InputIterator Input iterator, must meet the requirements of LegacyInputIterator
/// @param first Signal range start
/// @param last Signal range end
/// @return Peak value
template<class InputIterator>
typename InputIterator::value_type peak(InputIterator first, InputIterator last) {
	typename InputIterator::value_type abs_max{};
	std::for_each(first, last, [&abs_max](const auto& v) { abs_max = std::max(abs_max, std::abs(v)); });
	return abs_max;
}


/// @brief Get RMS (root mean square) of signal,
/// @tparam InputIterator Input iterator, must meet the requirements of LegacyInputIterator
/// @param first Signal range start
/// @param last Signal range end
/// @return RMS
template<class InputIterator>
typename InputIterator::value_type rms(InputIterator first, InputIterator last) {
	typename InputIterator::value_type rms{};
	std::for_each(first, last, [&rms](const auto& v) { rms += v * v; });
	return static_cast<typename InputIterator::value_type>(std::sqrt(rms / static_cast<double>(std::distance(first, last))));
}


/// @brief Normalize signal by peak to range [-\value,\value].
/// @tparam ForwardIterator Input-output iterator, must meet the requirements of LegacyForwardIterator
/// @param first start of range
/// @param last end of range
/// @param value normalization value (default is 1.0)
template<class ForwardIterator>
void peak_normalize(ForwardIterator first, ForwardIterator last, typename ForwardIterator::value_type value = 1.0) {
	const auto inv = value / peak(first, last);
	for (auto it = first; it != last; ++it)
		*it *= inv;
}


/// @brief  Normalize signal by RMS to range [-\value,\value].
/// @tparam ForwardIterator Input-output iterator, must meet the requirements of LegacyForwardIterator
/// @param first start of range
/// @param last end of range
/// @param value normalization value (default is 1.0)
template<class ForwardIterator>
void rms_normalize(ForwardIterator first, ForwardIterator last, typename ForwardIterator::value_type value = 1.0) {
	const auto inv = value / rms(first, last);
	for (auto it = first; it != last; ++it)
		*it *= inv;
}


}