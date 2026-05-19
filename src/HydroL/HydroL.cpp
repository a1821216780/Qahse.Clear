#include "HydroL/HydroL.hpp"

#include <sstream>
#include <stdexcept>

#include "HydroL/IO/HydroL_IO_Subs.hpp"

HydroLInput ReadHydroLInputFile(const std::string &path)
{
	return ReadHydroLInput(path);
}

HydroLInput HydroL::ReadInputFile(const std::string &path)
{
	return ReadHydroLInput(path);
}

void HydroL::ValidateInputOnly(const HydroLInput &input)
{
	if (input.waterDepth <= 0.0)
		throw std::runtime_error("HydroL WaterDepth must be positive");
	if (input.useRadiation && input.potentialRadFile.empty())
		throw std::runtime_error("HydroL UseRadiation requires PotentialRadFile");
	if (input.useExcitation && input.potentialExcFile.empty())
		throw std::runtime_error("HydroL UseExcitation requires PotentialExcFile");
}

HydroL HydroL::Load(const HydroLInput &input)
{
	ValidateInputOnly(input);
	HydroL module;
	module.input_ = input;
	return module;
}

HydroL HydroL::LoadFromFile(const std::string &path)
{
	return Load(ReadInputFile(path));
}

const HydroLInput &HydroL::Input() const
{
	return input_;
}

std::string HydroL::Summary() const
{
	std::ostringstream out;
	out << "HydroL: waterDepth=" << input_.waterDepth
	    << ", floating=" << (input_.isFloating ? "true" : "false")
	    << ", joints=" << input_.subJoints.size()
	    << ", members=" << input_.subMembers.size();
	return out.str();
}
