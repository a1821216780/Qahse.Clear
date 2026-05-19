#include "ControL/ControL.hpp"

#include <sstream>
#include <stdexcept>

#include "ControL/IO/ControL_IO_Subs.hpp"

ControLInput ReadControLInputFile(const std::string &path)
{
	return ReadControLInput(path);
}

ControLInput ControL::ReadInputFile(const std::string &path)
{
	return ReadControLInput(path);
}

void ControL::ValidateInputOnly(const ControLInput &input)
{
	if (input.pcMode != 0 && input.dllFileName.empty())
		throw std::runtime_error("ControL PCMode requires DLL_FileName");
}

ControL ControL::Load(const ControLInput &input)
{
	ValidateInputOnly(input);
	ControL module;
	module.input_ = input;
	return module;
}

ControL ControL::LoadFromFile(const std::string &path)
{
	return Load(ReadInputFile(path));
}

const ControLInput &ControL::Input() const
{
	return input_;
}

std::string ControL::Summary() const
{
	std::ostringstream out;
	out << "ControL: PCMode=" << input_.pcMode
	    << ", DLL=" << input_.dllFileName;
	return out.str();
}
