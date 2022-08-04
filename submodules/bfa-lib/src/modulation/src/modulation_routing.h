
// This files provides interface definitions to implement modulation routing.
// It includes interfaces for modulation sources and destinations as well as
// modulation IDs that can be used to link destinations and sources in a typesafe
// way that prevents mixing up destinations and sources


// A MODULATION SOURCE is an object that can be asked (through the
//   IModulationSource interface) for it's current modulation value.
//   This can be any kind of generator, like an EG or LFO.
//   The source also provides additional information about it's polarity
//   (Unipolar: output is in [0,1], Bipolar: output is in [-1,1]) and
//   it's update rate which can be per sample or per processing block.

// A MODULATION DESTINATION is an object where one or multiple modulations
//   can be injected to change some internal value like a frequency or level.
//   To enable applying modulation from multiple sources, a mechanism is used,
//   where for every distinct source the member function modulate() is called
//   on the destination. Then the completely modulated value may be used.
//   Before the next modulation cycle, the destination needs to be reset using
//   startNewModulationCycle().


#pragma once


namespace Butterfly {

struct IModulationSource
{
	enum class Polarity {
		Unipolar,
		Bipolar
	};

	enum class UpdateRate {
		PerSample,
		PerBlock
	};

	virtual ~IModulationSource() = default;
	virtual double getValue() const = 0;
	virtual Polarity getPolarity() const = 0;
	virtual UpdateRate getUpdateRate() const = 0;
};


struct IModulationDestination
{
	enum class Type {
		Frequency,
		Volume,
		Detune,
		Time,
		Resonance,
		Other
	};

	virtual ~IModulationDestination() = default;
	virtual void modulate(double) = 0;		  // perform a modulation on this destination with given value
	virtual void startNewModulationCycle() {} // reset modulation value to the param value (done before each modulation, so multiple sources can be applied to stack up to a total modulation value)
	virtual Type getType() const = 0;
};


struct ModulationIDBase
{
	constexpr ModulationIDBase() : id(invalid_id) {}
	constexpr ModulationIDBase(size_t id) : id(id) {}
	constexpr size_t operator()() const { return id; }
	constexpr bool operator==(const ModulationIDBase& other) const { return id == other.id; }

	constexpr bool isValid() const { return id != invalid_id; }

	static constexpr ModulationIDBase createInvalidID() { return ModulationIDBase(invalid_id); }
	static constexpr size_t invalid_id = static_cast<size_t>(-1);

private:
	size_t id;
};

struct ModulationSourceID : public ModulationIDBase
{
	using ModulationIDBase::ModulationIDBase;
};

struct ModulationDestinationID : public ModulationIDBase
{
	using ModulationIDBase::ModulationIDBase;
};



class ConnectionID
{
public:
	constexpr ConnectionID() : id(invalid_id) {}
	constexpr ConnectionID(size_t id) : id(id) {}
	constexpr size_t operator()() const { return id; }
	constexpr bool isValid() const { return id != invalid_id; }
	constexpr bool operator==(const ConnectionID& other) const { return id == other.id; }

	static constexpr ConnectionID createInvalidID() { return ConnectionID(invalid_id); }
	static constexpr size_t invalid_id = static_cast<size_t>(-1);

private:
	size_t id;
};


struct ConnectionSpecification
{
	ModulationSourceID sourceID;
	ModulationDestinationID destinationID;
	ConnectionID connectionID;
};

}
