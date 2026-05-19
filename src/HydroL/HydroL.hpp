#pragma once

#include <string>

#include "HydroL/HydroL_Type.hpp"

HydroLInput ReadHydroLInputFile(const std::string &path);

class HydroL
{
public:
	static HydroLInput ReadInputFile(const std::string &path);
	static void ValidateInputOnly(const HydroLInput &input);
	static HydroL Load(const HydroLInput &input);
	static HydroL LoadFromFile(const std::string &path);

	const HydroLInput &Input() const;
	std::string Summary() const;

private:
	HydroLInput input_;
};
