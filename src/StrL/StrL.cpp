#include "StrL/StrL.hpp"

#include <sstream>
#include <stdexcept>

#include "StrL/IO/StrL_IO_Subs.hpp"

StrLInput ReadStrLInputFile(const std::string &path)
{
	return ReadStrLInput(path);
}

StrLInput StrL::ReadInputFile(const std::string &path)
{
	return ReadStrLInput(path);
}

BladeAeroStructInput StrL::ReadBladeFile(const std::string &path)
{
	return ReadBladeAeroStructInput(path);
}

TowerStructInput StrL::ReadTowerFile(const std::string &path)
{
	return ReadTowerStructInput(path);
}

void StrL::ValidateInputOnly(const StrLInput &input)
{
	if (input.bladeAeroStructFile.empty())
		throw std::runtime_error("StrL BladeAeroStructFile is required");
	if (input.towerFile.empty())
		throw std::runtime_error("StrL TowerFile is required");
}

StrL StrL::Load(const StrLInput &input)
{
	ValidateInputOnly(input);
	StrL module;
	module.input_ = input;
	return module;
}

StrL StrL::LoadFromFile(const std::string &path)
{
	return Load(ReadInputFile(path));
}

const StrLInput &StrL::Input() const
{
	return input_;
}

std::string StrL::Summary() const
{
	std::ostringstream out;
	out << "StrL: numBld=" << input_.numBld
	    << ", towerHeight=" << input_.towerHeight
	    << ", bladeFile=" << input_.bladeAeroStructFile;
	return out.str();
}
