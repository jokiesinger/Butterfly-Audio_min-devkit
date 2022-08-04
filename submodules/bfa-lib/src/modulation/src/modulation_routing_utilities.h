// Here, implementations for the modulation routing are provided.
//
//
// ModulationConnection: Takes an IModulationSource and an IModulationDestination and
//   applies the modulation from the source to the destination when update() is called.
//   A conversion function can be set up to manipulate the values from the source before
//   they are passed to the destination.
//
// ValueRefOutput: Basic implementatino of IModulationSource which takes a reference to a
//   double and outputs its current values when IModulationSource::getValue() is called.
//
// ModulatableValue: Generic implementation of IModulationDestination that features a value
//   that can be set traditionally and also modulated. A callback is invoked whenever the
//   modulated value changes.
//
// ConnectionManager: A class that handles a preallocated number of ModulationConnection's
//   by calling update on all active connections and startNewModulationCycle() on all
//   modulation sources.
//


#pragma once

#include <functional>
#include <vector>
#include <algorithm>
#include "modulation_routing.h"
#include "stable_id_array.h"
#include "stable_id_vector.h"


namespace Butterfly {


/*
 * Establish a connection between an source and an destination through a conversion function which is passed as a pointer.
 * - The conversion functions needs to have the signature
 *      double f(double value, double param)
 * - The param is used to parameterize the conversion function (i.e. multiplication with param)
 * - The conversion function needs to be valid. For performance reasons, no nullptr check is made.
 */
class ModulationConnection
{
public:
	using ConversionFunc = double (*)(double, double);

	constexpr ModulationConnection() = default;
	constexpr ModulationConnection(IModulationSource& source, IModulationDestination& destination, ConversionFunc conversionFunc, double param = 0.0)
		: source(&source), destination(&destination), conversionFunc(conversionFunc), param(param) {}

	// Transfer the current value from the source to the destination.
	void update() { destination->modulate(conversionFunc(source->getValue(), param)); }

	constexpr void setParam(double value) { param = value; }
	constexpr void setActive(bool a) { active = a; }
	constexpr void setConversionFunc(ConversionFunc conversionFunction) { this->conversionFunc = conversionFunction; }

	constexpr double getParam() const { return param; }
	constexpr bool isActive() const { return active; }
	constexpr ConversionFunc getConversionFunc() const { return conversionFunc; }
	constexpr IModulationDestination* getDestination() const { return destination; }
	constexpr IModulationSource* getSource() const { return source; }
	constexpr bool isValid() const { return source != nullptr && destination != nullptr && conversionFunc != nullptr; }


private:
	IModulationSource* source{};
	IModulationDestination* destination{};
	ConversionFunc conversionFunc{};
	bool active{ false };
	double param{};
};



/*
 * IModulationSource implementations
 */

// Output that reads from the given location of a (double) value (which may not change).
template<IModulationSource::Polarity mode, IModulationSource::UpdateRate updateRate>
class ValueRefOutput : public IModulationSource
{
public:
	constexpr ValueRefOutput(double& value) : value(value) {}
	ValueRefOutput(const ValueRefOutput&) = delete;
	ValueRefOutput& operator=(const ValueRefOutput&) = delete;

	double getValue() const override { return value; }
	Polarity getPolarity() const override { return mode; }
	UpdateRate getUpdateRate() const override { return updateRate; }

private:
	double& value;
};

using LFOOutput = ValueRefOutput<IModulationSource::Polarity::Bipolar, IModulationSource::UpdateRate::PerBlock>;
using EnvelopeOutput = ValueRefOutput<IModulationSource::Polarity::Unipolar, IModulationSource::UpdateRate::PerSample>;



/*
 * IModulationDestination implementations
 */

// template<class T>
// concept ModulationOperation = requires(double a) {
//	{ T::chain_modulation(a, a) } -> std::convertible_to<double>;
//	{ T::apply_modulation(a, a) } -> std::convertible_to<double>;
//	{ T::neutral_element() } -> std::convertible_to<double>;
// };



// General value that can be modulated via the IModulationDestination interface.
// Depending on the operation, modulation values can for example be multiplied or added.
template</*ModulationOperation*/ class Operation, IModulationDestination::Type mode>
class ModulatableValue : public IModulationDestination
{
public:
	template<class F>
	constexpr ModulatableValue(F&& callback, double paramValue = 0.0) : callback(callback) {
		setParamValue(paramValue);
	}

	constexpr void setParamValue(double v) {
		paramValue = v;
		updateModulatedValue();
		callback(modulationValue);
	}

	void modulate(double modValue) override {
		modulationValue = Operation::chain_modulation(modulationValue, modValue);
		updateModulatedValue();
		callback(modulationValue);
	}

	void startNewModulationCycle() override {
		modulationValue = Operation::neutral_element();
		updateModulatedValue();
	}

	constexpr double getParamValue() const { return paramValue; }
	constexpr double getModulatedValue() const { return modulatedValue; }
	constexpr double getModulationValue() const { return modulationValue; }
	Type getType() const override { return mode; }


protected:
	constexpr void updateModulatedValue() {
		modulatedValue = Operation::apply_modulation(paramValue, modulationValue);
	}

	double paramValue{};
	double modulationValue{ Operation::neutral_element() };
	double modulatedValue{};
	std::function<void(double)> callback;
};

// Operations that may be used together with ModulatableValue.

struct Addition
{
	static constexpr double chain_modulation(double a, double b) { return a + b; }
	static constexpr double apply_modulation(double a, double b) { return a + b; }
	static constexpr double neutral_element() { return 0.0; }
};

struct ClampedAddition
{
	static constexpr double chain_modulation(double a, double b) { return a + b; }
	static constexpr double apply_modulation(double a, double b) { return std::max(0.0, std::min(1.0, a + b)); }
	static constexpr double neutral_element() { return 0.0; }
};

struct Multiplication
{
	static constexpr double chain_modulation(double a, double b) { return a * b; }
	static constexpr double apply_modulation(double a, double b) { return a * b; }
	static constexpr double neutral_element() { return 1.0; }
};

template<IModulationDestination::Type mode>
class AdditivelyModulatableValue : public ModulatableValue<Addition, mode>
{
public:
	using ModulatableValue<Addition, mode>::ModulatableValue;
};

template<IModulationDestination::Type mode>
class MultiplicativelyModulatableValue : public ModulatableValue<Multiplication, mode>
{
public:
	using ModulatableValue<Multiplication, mode>::ModulatableValue;
};

template<IModulationDestination::Type mode>
class AdditivelyClampedModulatableValue : public ModulatableValue<ClampedAddition, mode>
{
public:
	using ModulatableValue<ClampedAddition, mode>::ModulatableValue;
};

using VolumeValue = MultiplicativelyModulatableValue<IModulationDestination::Type::Volume>;
using DetuneValue = AdditivelyModulatableValue<IModulationDestination::Type::Detune>;
using FrequencyValue = AdditivelyClampedModulatableValue<IModulationDestination::Type::Frequency>;
using FilterResonanceValue = AdditivelyClampedModulatableValue<IModulationDestination::Type::Resonance>;


class ConnectionManager : public StableIDVector<ModulationConnection>
{
public:
	void setModulationParam(size_t id, double value) {
		(*this)[id].setParam(value);
	}

	void updateAllConnections() {
		resetAllModulationValues();
		for (auto& el : *this) {
			el.update();
		}
	}

	ConnectionID addConnection(const ModulationConnection& c) {
		const auto id = add(c);
		if (id == invalid_id) return ConnectionID::createInvalidID();
		return ConnectionID{ id };
	}

	auto getState() const {
		std::vector<std::pair<ModulationConnection, ConnectionID>> pairs;
		for (size_t i = 0; i < size(); i++) {
			pairs.push_back({ get_by_index(i), get_id(i) });
		}
		return pairs;
	}

	bool setState(const std::vector<std::pair<ModulationConnection, ConnectionID>>& connections) {
		if (connections.size() > capacity()) return false;

		for (const auto& p : connections) {
			insert(p.first, p.second());
		}
		return true;
	}



private:
	void resetAllModulationValues() {
		for (auto& el : *this) {
			el.getDestination()->startNewModulationCycle();
		}
	}
};



class ConnectionInfo
{
public:
	using ConversionFunc = double (*)(double, double);

	ModulationSourceID sourceID{};
	ModulationDestinationID destinationID{};

	double param{};
	ConversionFunc conversionFunc{};
};


template<int numSources, int numDestinations, int maxNumConnections>
class ConnectionManager2 : public StableIDArray<ConnectionInfo, maxNumConnections>
{
public:
	using ModulationSourceMap = std::array<IModulationSource*, numSources>;
	using ModulationDestinationMap = std::array<IModulationDestination*, numDestinations>;

	void updateAllConnections(
		const ModulationSourceMap& sources,
		const ModulationDestinationMap& destinations) {
		resetAllModulationValues(destinations);
		for (auto& el : *this) {
			auto& destination = destinations[el.destinationID()];
			auto& source = sources[el.sourceID()];
			auto value = source->getValue();
			destination->modulate(el.conversionFunc(source->getValue(), el.param));
		}
	}

	void setParam(ConnectionID id, double param) {
		this->operator[](id).param = param;
	}

private:
	void resetAllModulationValues(const ModulationDestinationMap& destinations) {
		for (auto& el : *this) {
			auto g = el.destinationID();
			auto destination = destinations[g];
			destination->startNewModulationCycle();
		}
	}
};

}
