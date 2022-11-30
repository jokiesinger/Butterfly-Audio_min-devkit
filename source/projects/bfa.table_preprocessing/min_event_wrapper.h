#pragma once

#include "event.h"

namespace Butterfly {

Butterfly::MouseEvent::Button getButton(const c74::min::event& e) {
	if (e.m_modifiers & c74::max::eLeftButton)
		return Butterfly::MouseEvent::Button::Left;
	else if (e.m_modifiers & c74::max::eRightButton)
		return Butterfly::MouseEvent::Button::Right;
	else if (e.m_modifiers & c74::max::eMiddleButton)
		return Butterfly::MouseEvent::Button::Middle;
	return Butterfly::MouseEvent::Button::Left;
}

MouseEvent createMouseEvent(const c74::min::event& e, MouseEvent::Action action) {
	MouseEvent me;
	me.action = action;
	me.x = e.x();
	me.y = e.y();
	me.deltaX = e.wheel_delta_x();
	me.deltaY = e.wheel_delta_y();
	if (e.is_command_key_down()) me.modifiers |= static_cast<int>(Modifier::Control);
	if (e.is_shift_key_down()) me.modifiers |= static_cast<int>(Modifier::Shift);
	me.button = getButton(e);
	return me;
}

}
