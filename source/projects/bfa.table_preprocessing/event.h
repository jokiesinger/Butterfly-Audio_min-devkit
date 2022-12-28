#pragma once


namespace Butterfly {

enum class Modifier {
	Control = 1 << 0, // :command
	Shift = 1 << 1,
	Alt = 1 << 2 // option
};

constexpr int operator|(Modifier a, Modifier b) {
	return static_cast<int>(a) | static_cast<int>(b);
}

struct Event
{
	constexpr bool isControlDown() const { return modifiers & static_cast<int>(Modifier::Control); }
	constexpr bool isShiftDown() const { return modifiers & static_cast<int>(Modifier::Shift); }
	constexpr bool isAltDown() const { return modifiers & static_cast<int>(Modifier::Alt); }

	int modifiers{};
};


struct MouseEvent : public Event
{

	enum class Button {
		None,
		Left,
		Middle,
		Right,
		X1,
		X2
	};

	enum class Action {
		None,
		Down,
		Up,
		Drag,
		Move,
		Wheel,
		Enter,
		Exit
	};

	constexpr MouseEvent() = default;

	constexpr MouseEvent(double x, double y, Action action, Button button, double deltaY = 0) : action(action), button(button), x(x), y(y), deltaY(deltaY) {}

	constexpr MouseEvent Mousedown(double x, double y, Button button) { return { x, y, Action::Down, button }; }
	constexpr MouseEvent Mouseup(double x, double y, Button button) { return { x, y, Action::Up, button }; }
	constexpr MouseEvent Mousemove(double x, double y) { return { x, y, Action::Up, Button::None }; }
	constexpr MouseEvent Mousewheel(double x, double y, double deltaY) { return { x, y, Action::Wheel, Button::None, deltaY }; }

	Action action{};
	Button button{};
	double x{};
	double y{};
	double deltaX{};
	double deltaY{};
};

}
