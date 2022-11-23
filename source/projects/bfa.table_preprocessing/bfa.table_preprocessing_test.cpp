

#include "c74_min_unittest.h"
#include "signal_generators.h"
#include "bfa.table_preprocessing.cpp"


event generateMouseEvent(Button b, const Butterfly::Point& p, c74::max::_modifiers modifiers = {}) {
	c74::max::t_mouseevent me{};
	me.type = c74::max::eMouseEvent;
	me.position.x = p.x;
	me.position.y = p.y;
	me.modifiers = modifiers;
	switch (b) {
	case Button::Left: me.modifiers = static_cast<c74::max::_modifiers>(me.modifiers | c74::max::eLeftButton); break;
	case Button::Center: me.modifiers = static_cast<c74::max::_modifiers>(me.modifiers | c74::max::eMiddleButton); break;
	case Button::Right: me.modifiers = static_cast<c74::max::_modifiers>(me.modifiers | c74::max::eRightButton); break;
	}
	return { nullptr, nullptr, me };
}


TEST_CASE("Integration test") {
	ext_main(nullptr); // every unit test must call ext_main() once to configure the class


	test_wrapper<table_preprocessing> an_instance;
	table_preprocessing& my_object = an_instance;

	const auto innerWidth = 200.; //target width minus margins
	std::vector<float> data(1000);
	Butterfly::generateSawtooth(data.begin(), data.end(), 0., 4.);




	// Setup
	my_object.dspsetup(atoms{ 44100.f });
	REQUIRE(my_object.getSampleRate() == 44100.f);
	my_object.targetResized(innerWidth + 2 * my_object.margin, 80);
	REQUIRE(my_object.targetSize == Butterfly::Point{ innerWidth + 2 * my_object.margin, 80 });
	const auto width = my_object.targetSize.x;


	// Set Mode
	{
		REQUIRE(my_object.getMode() == table_preprocessing::Mode::free);
		my_object.setMode(atoms{ "Zeros" });
		REQUIRE(my_object.getMode() == table_preprocessing::Mode::zeros);
		my_object.setMode(atoms{ "Free" });
		REQUIRE(my_object.getMode() == table_preprocessing::Mode::free);
	}


	// Simulate sample drop
	{
		my_object.setSampleData(data);
		auto range = my_object.getDataRange();
		REQUIRE(range.x == 0);
		REQUIRE(range.width == data.size());
		REQUIRE(range.y == Approx(-1));
		REQUIRE(range.height == Approx(2));
	}


	// Free selection
	{
		my_object.mousedown(atoms{ generateMouseEvent(Button::Left, { innerWidth / 2. + my_object.margin, 50 }) });
		REQUIRE(my_object.isDragging());

		my_object.mousedrag(atoms{ generateMouseEvent(Button::Left, { innerWidth / 1.5 + my_object.margin, 55 }) });
		REQUIRE(my_object.isDragging());
		auto z = my_object.getFreeSelection();
		REQUIRE(z.first == Approx(data.size() / 2.));
		REQUIRE(z.second == Approx(data.size() / 1.5));

		my_object.mouseup(atoms{ generateMouseEvent(Button::Left, { innerWidth / 1.2 + my_object.margin, 90 }) });
		REQUIRE(!my_object.isDragging());
		z = my_object.getFreeSelection();
		REQUIRE(z.first == Approx(data.size() / 2.));
		REQUIRE(z.second == Approx(data.size() / 1.2));
	}


	// Export
	{
		// Free selection + export of length 0
		my_object.mousedown(atoms{ generateMouseEvent(Button::Left, { 50, 50 }) });
		my_object.mouseup(atoms{ generateMouseEvent(Button::Left, { 50, 50 }) });
		my_object.generate_frame();

		// Free selection + export of length 2 (too short to export)
		my_object.mousedown(atoms{ generateMouseEvent(Button::Left, { 50, 50 }) });
		my_object.mouseup(atoms{ generateMouseEvent(Button::Left, { 50 + 2. * innerWidth / data.size(), 50 }) });
		my_object.generate_frame();

		// Free selection + export of length 10 (enough to export)
		my_object.mousedown(atoms{ generateMouseEvent(Button::Left, { my_object.margin, 50 }) });
		my_object.mouseup(atoms{ generateMouseEvent(Button::Left, { my_object.margin + 10. * innerWidth / data.size(), 50 }) });
	}
}
