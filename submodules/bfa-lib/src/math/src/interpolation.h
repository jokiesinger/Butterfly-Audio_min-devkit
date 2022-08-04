
// Interpolation algorithms and interpolation structs with static members providing
// access to interpolation method meta-information and the algorithm itself.


#pragma once

//#include <concepts>

namespace Butterfly {


/// @brief Linear interpolation between values y0 and y1
///
/// @tparam T Value type
/// @param t Interpolation parameter in the interval [0, 1]
/// @param y0 First value
/// @param y1 Second value
/// @return interpolated value
template<class T>
constexpr T linear_interpolation(T t, T y0, T y1) {
	return y0 + t * (y1 - y0);
}


/// @brief 3rd-order hermite interpolation between four values y-1, y0, y1 and y2.
///        The interpolation is performed between y0 and y1.
///
/// @tparam T Value type
/// @param t Interpolation parameter in the interval [0, 1]
/// @param ym1 First value
/// @param y0 Second value
/// @param y1 Third value
/// @param y2 Fourth value
/// @return interpolated value
template<class T>
constexpr T hermite_interpolation(T t, T ym1, T y0, T y1, T y2) {
	const auto c0 = y0;
	const auto c1 = T(0.5) * (y1 - ym1);
	const auto c2 = ym1 - T(2.5) * y0 + T(2.) * y1 - T(0.5) * y2;
	const auto c3 = T(1.5) * (y0 - y1) + T(0.5) * (y2 - ym1);

	return ((c3 * t + c2) * t + c1) * t + c0;
}



// template<class T>
// concept Interpolator = requires(double* data, size_t index, double offset) {
//	{ T::interpolate(data, index, offset) } -> std::convertible_to<double>;
//	{ T::getLookbehindLength() } -> std::convertible_to<size_t>;
//	{ T::getLookaheadLength() } -> std::convertible_to<size_t>;
// };


struct LinearInterpolator
{
	template<class RandomAccessContainer, class T>
	static constexpr T interpolate(const RandomAccessContainer& data, size_t index, T offset) {
		static_assert(std::is_same_v<typename RandomAccessContainer::value_type, T>);
		return linear_interpolation(offset, data[index], data[index + 1]);
	}
	static constexpr size_t getLookbehindLength() { return 0; }
	static constexpr size_t getLookaheadLength() { return 1; }
};

struct HermiteInterpolator
{
	template<class RandomAccessContainer, class T>
	static constexpr T interpolate(const RandomAccessContainer& data, size_t index, T offset) {
		static_assert(std::is_same_v<typename RandomAccessContainer::value_type, T>);
		return hermite_interpolation(offset, data[index - 1], data[index], data[index + 1], data[index + 2]);
	}
	static constexpr size_t getLookbehindLength() { return 1; }
	static constexpr size_t getLookaheadLength() { return 2; }
};


}
