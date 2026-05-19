#pragma once

#include <string>

#include "SimL/SimL_Type.hpp"

SimLInput ReadSimLInputFile(const std::string &path);

class SimL
{
public:
	static SimLInput ReadInputFile(const std::string &path);
	static void ValidateInputOnly(const SimLInput &input);
	static SimL Load(const SimLInput &input);
	static SimL LoadFromFile(const std::string &path);

	const SimLInput &Input() const;
	std::string Summary() const;

private:
	SimLInput input_;
};
