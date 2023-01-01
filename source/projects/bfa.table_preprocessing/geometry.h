
#pragma once
#include <cmath>
#include <algorithm>


namespace Butterfly {

struct Point
{
	double x{};
	double y{};

	constexpr Point& operator+=(const Point& p) {
		x += p.x;
		y += p.y;
		return *this;
	}

	constexpr Point& operator-=(const Point& p) {
		x -= p.x;
		y -= p.y;
		return *this;
	}

	constexpr Point& operator*=(double f) {
		x *= f;
		y *= f;
		return *this;
	}

	constexpr Point& operator/=(double f) {
		x /= f;
		y /= f;
		return *this;
	}

	constexpr friend Point operator+(const Point& p1, const Point& p2) {
		auto c = p1;
		return c += p2;
	}

	constexpr friend Point operator-(const Point& p1, const Point& p2) {
		auto c = p1;
		return c -= p2;
	}

	constexpr Point operator*(double f) const {
		auto c = *this;
		return c *= f;
	}

	constexpr Point operator/(double f) const {
		auto c = *this;
		return c /= f;
	}

	constexpr Point operator-() const {
		return { -x, -y };
	}

	constexpr friend bool operator==(const Point& p1, const Point& p2) { return p1.x == p2.x && p1.y == p2.y; }
};


struct Rect
{

	static constexpr Rect fromBounds(double x0, double x1, double y0, double y1) {
		return { x0, y0, x1 - x0, y1 - y0 };
	}

	constexpr Rect() = default;

	constexpr Rect(double x, double y, double width, double height)
		: x(x), y(y), width(width), height(height) {}

	Rect(const Point& a, const Point& b)
		: x(std::min(a.x, b.x)), y(std::min(a.y, b.y)), width(std::abs(a.x - b.x)), height(std::abs(a.y - b.y)) {}

	constexpr Point topLeft() const {
		return { x, y };
	}

	constexpr Point bottomLeft() const {
		return { x, y + height };
	}

	constexpr Point topRight() const {
		return { x + width, y };
	}

	constexpr Point bottomRight() const {
		return { x + width, y + height };
	}

	constexpr Point center() const {
		return { x + 0.5 * width, y + 0.5 * height };
	}

	constexpr Point size() const {
		return { width, height };
	}

	constexpr Rect& translate(const Point& p) {
		x += p.x;
		y += p.y;
		return *this;
	}

	constexpr Rect& moveTo(const Point& p) {
		x = p.x;
		y = p.y;
		return *this;
	}

	constexpr Rect& resize(double w, double h) {
		width = w;
		height = h;
		return *this;
	}

	double x{};
	double y{};
	double width{};
	double height{};
};

}
