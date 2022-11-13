#pragma once
#include <cmath>

namespace Butterfly {

	struct Point {
		double x {}, y {};

		Point& operator+=(const Point& p) {
			x += p.x;
			y += p.y;
			return *this;
		}
		Point& operator-=(const Point& p) {
			x -= p.x;
			y -= p.y;
			return *this;
		}
		Point& operator*=(double f) {
			x *= f;
			y *= f;
			return *this;
		}
		Point& operator/=(double f) {
			x /= f;
			y /= f;
			return *this;
		}
		Point operator+(const Point& p) const {
			auto c = *this;
			return c += p;
		}
		Point operator-(const Point& p) const {
			auto c = *this;
			return c -= p;
		}
		Point operator*(double f) const {
			auto c = *this;
			return c *= f;
		}
		Point operator/(double f) const {
			auto c = *this;
			return c /= f;
		}
		Point operator-() const {
			return {-x, -y};
		}
	};

	struct Rect {

		static constexpr Rect fromBounds(double x0, double x1, double y0, double y1) {
			return {x0, y0, x1 - x0, y1 - y0};
		}

		constexpr Rect() = default;
		constexpr Rect(double x, double y, double width, double height)
		: x(x)
		, y(y)
		, width(width)
		, height(height) { }
		Rect(const Point& a, const Point& b)
		: x(std::min(a.x, b.x))
		, y(std::min(a.y, b.y))
		, width(std::abs(a.x - b.x))
		, height(std::abs(a.y - b.y)) { }

		constexpr Point topLeft() const {
			return {x, y};
		}
		constexpr Point bottomLeft() const {
			return {x, y + height};
		}
		constexpr Point topRight() const {
			return {x + width, y};
		}
		constexpr Point bottomRight() const {
			return {x + width, y + height};
		}
		constexpr Point center() const {
			return {x + 0.5 * width, y + 0.5 * height};
		}
		constexpr Point size() const {
			return {width, height};
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
			width  = w;
			height = h;
			return *this;
		}

		double x {}, y {};
		double width {}, height {};
	};

	struct Transform {

		static constexpr Transform MapRect(const Rect& from, const Rect& to) {
			Transform t;
			t.translate(to.topLeft());
			t.scale(to.width / from.width, to.height / from.height);
			t.translate(-from.topLeft());
			return t;
		}
		constexpr Point apply(const Point& p) const {
			return {p.x * sx + x0, p.y * sy + y0};
		}
		constexpr Rect apply(const Rect& r) const {
			const auto p = apply(r.topLeft());
			return {p.x, p.y, r.width * sx, r.height * sy};
		}
		constexpr Point from(const Point& p) const {
			return {(p.x - x0) / sx, (p.y - y0) / sy};
		}
		constexpr Rect from(const Rect& r) const {
			const auto p = from(r.topLeft());
			return {p.x, p.y, r.width / sx, r.height / sy};
		}
		constexpr void translateX(double x) {
			x0 += x * sx;
		}
		constexpr void translateY(double y) {
			y0 += y * sy;
		}
		constexpr void preTranslateX(double x) {
			x0 += x;
		}
		constexpr void preTranslateY(double y) {
			y0 += y;
		}
		constexpr void translate(double x, double y) {
			translateX(x);
			translateY(y);
		}
		constexpr void translate(const Point& p) {
			translate(p.x, p.y);
		}
		constexpr void preTranslate(double x, double y) {
			preTranslateX(x);
			preTranslateY(y);
		}
		constexpr void preTranslate(const Point& p) {
			preTranslate(p.x, p.y);
		}
		constexpr void scaleX(double s) {
			sx *= s;
		}
		constexpr void scaleY(double s) {
			sy *= s;
		}
		constexpr void scale(double sx, double sy) {
			scaleX(sx);
			scaleY(sy);
		}
		constexpr void scale(double s) {
			scale(s, s);
		}

		constexpr void scaleAround(double x, double y, double sx, double sy) {
			scale(sx, sy);
			x0 = (sx * x0) + x * (1.0 - sx);
			y0 = (sy * y0) + y * (1.0 - sy);
		}
		constexpr void scaleAround(double x, double y, double s) {
			scaleAround(x, y, s, s);
		}
		constexpr void scaleAround(const Point& p, double sx, double sy) {
			scaleAround(p.x, p.y, sx, sy);
		}
		constexpr void scaleAround(const Point& p, double s) {
			scaleAround(p.x, p.y, s, s);
		}

		constexpr void ensureWithin(const Rect& from, const Rect& to) {
			auto target = apply(from);
			if (target.width < to.width)
				scaleX(to.width / target.width);
			if (target.height < to.height)
				scaleY(to.height / target.height);
			target = apply(from);
			if (target.x > to.x)
				preTranslateX(to.x - target.x);
			else if (target.x + target.width < to.x + to.width)
				preTranslateX(-target.x - target.width + (to.x + to.width));
			if (target.y > to.y)
				preTranslateY(-target.y + to.y);
			else if (target.y + target.height < to.y + to.height)
				preTranslateY(-target.y - target.height + (to.y + to.height));
		}

		double x0 {}, y0 {}, sx {1}, sy {1};
	};


	struct DragInfo {
		Point startPos;
		Point currentPos;

		constexpr void reset() {
			startPos = currentPos = {};
		}
		constexpr void start(const Point& p) {
			startPos = p;
		}
		constexpr void drag(const Point& p) {
			currentPos = p;
		}
		constexpr void release(const Point& p) {
			reset();
		}
	};

}    // namespace Butterfly