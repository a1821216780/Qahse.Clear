#include "SimL/SimL.hpp"

#include <sstream>
#include <stdexcept>

#include "SimL/IO/SimL_IO_Subs.hpp"

SimLInput ReadSimLInputFile(const std::string &path)
{
	return ReadSimLInput(path);
}

SimLInput SimL::ReadInputFile(const std::string &path)
{
	return ReadSimLInput(path);
}

void SimL::ValidateInputOnly(const SimLInput &input)
{
	if (input.dt <= 0.0)
		throw std::runtime_error("SimL DT must be positive");
	if (input.tMax <= 0.0)
		throw std::runtime_error("SimL TMax must be positive");
}

SimL SimL::Load(const SimLInput &input)
{
	ValidateInputOnly(input);
	SimL module;
	module.input_ = input;
	return module;
}

SimL SimL::LoadFromFile(const std::string &path)
{
	return Load(ReadInputFile(path));
}

const SimLInput &SimL::Input() const
{
	return input_;
}

std::string SimL::Summary() const
{
	std::ostringstream out;
	out << "SimL: TMax=" << input_.tMax
	    << ", DT=" << input_.dt
	    << ", WTType=" << input_.wtType;
	return out.str();
}
