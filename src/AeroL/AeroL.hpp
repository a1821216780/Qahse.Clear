#pragma once

#include <string>

#include "AeroL/AeroL_Type.hpp"

AeroLInput ReadAeroLInputFile(const std::string &path);

class AeroL
{
public:
	static AeroLInput ReadInputFile(const std::string &path);
	static void ValidateInputOnly(const AeroLInput &input);
	static AeroL Load(const AeroLInput &input);
	static AeroL LoadFromFile(const std::string &path);

	const AeroLInput &Input() const;
	std::string Summary() const;

private:
	AeroLInput input_;
};
