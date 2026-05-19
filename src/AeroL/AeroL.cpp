#include "AeroL/AeroL.hpp"

#include <sstream>
#include <stdexcept>

#include "AeroL/IO/AeroL_IO_Subs.hpp"

AeroLInput ReadAeroLInputFile(const std::string &path)
{
	return ReadAeroLInput(path);
}

AeroLInput AeroL::ReadInputFile(const std::string &path)
{
	return ReadAeroLInput(path);
}

void AeroL::ValidateInputOnly(const AeroLInput &input)
{
	if (input.bladeAeroStructFile.empty())
		throw std::runtime_error("AeroL BladeAeroStructFile is required");
	if (input.airfoils.count > 0 && input.airfoils.files.empty())
		throw std::runtime_error("AeroL NumAFfiles is nonzero but AFNames is empty");
}

AeroL AeroL::Load(const AeroLInput &input)
{
	ValidateInputOnly(input);
	AeroL module;
	module.input_ = input;
	return module;
}

AeroL AeroL::LoadFromFile(const std::string &path)
{
	return Load(ReadInputFile(path));
}

const AeroLInput &AeroL::Input() const
{
	return input_;
}

std::string AeroL::Summary() const
{
	std::ostringstream out;
	out << "AeroL: ApOfMb=" << input_.apOfMb
	    << ", bladeNum=" << input_.bladeNum
	    << ", airfoils=" << input_.airfoils.files.size();
	return out.str();
}
