#pragma once
#include "painter.h"

namespace Butterfly {

// Color conversion helpers
c74::min::ui::color to(const Color& c) { return { c.r, c.g, c.b, c.a }; }
Color to(const c74::min::ui::color& c) { return { c.red(), c.green(), c.blue(), c.alpha() }; }


class MaxPainter : public Painter
{
public:
	MaxPainter(c74::min::ui::target& t) : t(t) {}

	using Painter::line;
	using Painter::rect;
	using Painter::rectOutline;

	using size = c74::min::ui::size;
	using color = c74::min::ui::color;
	using origin = c74::min::ui::origin;
	using destination = c74::min::ui::destination;
	using line_width = c74::min::ui::line_width;
	using position = c74::min::ui::position;
	using corner = c74::min::ui::corner;

	double getWidth() override { return t.width(); }
	double getHeight() override { return t.height(); }

	void fillColor(const Color& c) override { fill = c; }
	void strokeColor(const Color& c) override { stroke = c; }
	void strokeWidth(float strokeWidth) override { lineWidth = strokeWidth; }

	Color getFillColor() override { return fill; }
	Color getStrokeColor() override { return stroke; }
	float getStrokeWidth() override { return lineWidth; }

	void line(double x1, double y1, double x2, double y2) override {
		c74::min::ui::line<c74::min::ui::draw_style::stroke> l{ t, color{ to(getStrokeColor()) }, origin{ x1, y1 }, destination{ x2, y2 }, line_width{ getStrokeWidth() } };
	}

	void rect(double x, double y, double width, double height) override {
		c74::min::ui::rect<c74::min::ui::draw_style::fill> r{ t, color{ to(getFillColor()) }, position{ x, y }, size{ width, height } };
	}
	void rectOutline(double x, double y, double width, double height) override {
		c74::min::ui::rect<c74::min::ui::draw_style::stroke> r{ t, color{ to(getStrokeColor()) }, position{ x, y }, size{ width, height }, line_width{ getStrokeWidth() } };
	}
	void rect(double x, double y, double width, double height, double borderRadius) override {
		c74::min::ui::rect<c74::min::ui::draw_style::fill> r{ t, color{ to(getFillColor()) }, position{ x, y }, size{ width, height }, corner{ borderRadius * 2., borderRadius * 2. } };
	}
	void rectOutline(double x, double y, double width, double height, double borderRadius) override {
		c74::min::ui::rect<c74::min::ui::draw_style::stroke> r{ t, color{ to(getStrokeColor()) }, position{ x, y }, size{ width, height }, line_width{ getStrokeWidth() }, corner{ borderRadius * 2., borderRadius * 2. } };
	}


	void ellipse(double x, double y, double width, double height) override {
		c74::min::ui::ellipse<c74::min::ui::draw_style::fill> e{ t, color{ to(getFillColor()) }, position{ x, y }, size{ width, height } };
	}
	void ellipseOutline(double x, double y, double width, double height) override {
		c74::min::ui::ellipse<c74::min::ui::draw_style::stroke> e{ t, color{ to(getStrokeColor()) }, position{ x, y }, size{ width, height }, line_width{ getStrokeWidth() } };
	}

	void text(const std::string& text, double x, double y) override {}
	void text(const std::string& text, double x, double y, double width, double height) override {}

	void clip(double x, double y, double width, double height) override {}

	void translate(double x, double y) override {}


	c74::min::ui::target& t;
	Color fill{};
	Color stroke{};
	double lineWidth{ 1 };
};


}
