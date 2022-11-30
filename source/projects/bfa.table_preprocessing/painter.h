#pragma once
#include "geometry.h"

namespace Butterfly {

struct Color
{
	double r{};
	double g{};
	double b{};
	double a{};
};

class Painter
{
public:
	virtual double getWidth() = 0;
	virtual double getHeight() = 0;

	virtual void fillColor(const Color& c) = 0;
	virtual void strokeColor(const Color& c) = 0;
	virtual void strokeWidth(float strokeWidth) = 0;

	virtual Color getFillColor() = 0;
	virtual Color getStrokeColor() = 0;
	virtual float getStrokeWidth() = 0;


	void point(double x, double y, double size) { ellipse(x - size * 0.5, y - size * 0.5, size, size); }
	void point(const Point& p, double size) { point(p.x, p.y, size); }

	virtual void line(double x1, double y1, double x2, double y2) = 0;
	void line(const Point& p1, const Point& p2) { line(p1.x, p1.y, p2.x, p2.y); }

	virtual void rect(double x, double y, double width, double height) = 0;
	virtual void rectOutline(double x, double y, double width, double height) = 0;
	virtual void rect(double x, double y, double width, double height, double borderWidth) = 0;
	virtual void rectOutline(double x, double y, double width, double height, double borderWidth) = 0;
	void rect(const Rect& r) { rect(r.x, r.y, r.width, r.height); }
	void rectOutline(const Rect& r) { rectOutline(r.x, r.y, r.width, r.height); }
	void rect(const Rect& r, double borderRadius) { rect(r.x, r.y, r.width, r.height, borderRadius); }
	void rectOutline(const Rect& r, double borderRadius) { rectOutline(r.x, r.y, r.width, r.height, borderRadius); }

	virtual void ellipse(double x, double y, double width, double height) = 0;
	virtual void ellipseOutline(double x, double y, double width, double height) = 0;

	virtual void text(const std::string& text, double x, double y) = 0;
	virtual void text(const std::string& text, double x, double y, double width, double height) = 0;


	virtual void clip(double x, double y, double width, double height) = 0;
	void clip(const Rect& r) { clip(r.x, r.y, r.width, r.height); }


	virtual void setDashPattern(const std::vector<double>& onOffPattern) = 0;
	virtual void setSolid() = 0;


	virtual void translate(double x, double y) = 0;
	void translate(const Point& p) { translate(p.x, p.y); }
};

}
