
// Efficient implementation of a wavetable buffer with general interpolation access.


#pragma once
#include <cmath>
#include <array>
#include <vector>
#include "../../math/src/interpolation.h"

namespace Butterfly {


/// @brief Wavetable class with
///         - dynamic length
///         - access function for arbitrary (floating point) positions
///         - hermite interpolation
///         - improved performance with avoided internal wrapping
///         - a maximum recommended frequency for playback can be specified (defaults to 1Hz) for use in antialiased oscillators
/// Usage example:
/// 
/// 	\code
///			std::vector<Wavetable<double>> tables(4);
///			// fill tables
///			WavetableOscillator<Wavetable<double>> osc{ &tables, 40100., 200. };
/// 
/// 	\endcode
/// 
/// @tparam T Value type
/// @tparam Interpolator Interpolator type, e.g. HermiteInterpolator or LinearInterpolator
template<class T, class Interpolator = HermiteInterpolator>
class Wavetable
{
public:
	using value_type = T;

	constexpr Wavetable() = default;

	constexpr Wavetable(const std::vector<T>& data, T maximumPlaybackFrequency = 1.0) {
		setData(data, maximumPlaybackFrequency);
	}

	template<class ForwardIterator>
	constexpr Wavetable(ForwardIterator first, ForwardIterator last, T maximumPlaybackFrequency = 1.0) {
		setData(first, last, maximumPlaybackFrequency);
	}

	constexpr void setData(const std::vector<T>& data, T maximumPlaybackFrequency = 1.0) {
		setData(data.begin(), data.end(), maximumPlaybackFrequency);
	}

	template<class ForwardIterator>
	constexpr void setData(ForwardIterator first, ForwardIterator last, T maximumPlaybackFrequency = 1.0) {
		constexpr auto pre = Interpolator::getLookbehindLength();
		constexpr auto post = Interpolator::getLookaheadLength();
		const auto size = std::distance(first, last);
		assert(size >= pre && size >= post);

		data.resize(size + pre + post);
		std::copy(last - pre, last, data.begin());
		std::copy(first, last, data.begin() + pre);
		std::copy(first, first + post, data.begin() + pre + size);

		this->maximumPlaybackFrequency = maximumPlaybackFrequency;
	}

	// Parameter needs to be in [0, size)
	constexpr T operator()(T position) {
		const T pos = static_cast<T>(std::floor(position));
		const size_t index = static_cast<size_t>(pos) + Interpolator::getLookbehindLength();
		const T offset = position - pos;
		return Interpolator::interpolate(data, index, offset);
	}


	constexpr size_t size() const { return data.size() - Interpolator::getLookbehindLength() - Interpolator::getLookaheadLength(); }
	constexpr T getMaximumPlaybackFrequency() const { return maximumPlaybackFrequency; }

private:
	std::vector<T> data;
	T maximumPlaybackFrequency{};
};

}
