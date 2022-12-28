
#pragma once
#include "geometry.h"

namespace Butterfly {


struct Transform
{

	static constexpr Transform MapRect(const Rect& from, const Rect& to) {
		Transform t;
		t.translate(to.topLeft());
		t.scale(to.width / from.width, to.height / from.height);
		t.translate(-from.topLeft());
		return t;
	}

	constexpr double applyX(double x) const {
		return x * sx + x0;
	}

	constexpr double applyY(double y) const {
		return y * sy + y0;
	}

	constexpr Point apply(const Point& p) const {
		return { applyX(p.x), applyY(p.y) };
	}

	constexpr Rect apply(const Rect& r) const {
		const auto p = apply(r.topLeft());
		return { p.x, p.y, r.width * sx, r.height * sy };
	}

	constexpr double fromX(double x) const {
		return (x - x0) / sx;
	}

	constexpr double fromY(double y) const {
		return (y - y0) / sy;
	}

	constexpr Point from(const Point& p) const {
		return { fromX(p.x), fromY(p.y) };
	}

	constexpr Rect from(const Rect& r) const {
		const auto p = from(r.topLeft());
		return { p.x, p.y, r.width / sx, r.height / sy };
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

	double x0{};
	double y0{};
	double sx{ 1. };
	double sy{ 1. };
};


struct DragInfo
{
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

}
