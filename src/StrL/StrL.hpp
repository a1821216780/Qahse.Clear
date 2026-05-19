#pragma once

#include <string>

#include "StrL/StrL_Type.hpp"

StrLInput ReadStrLInputFile(const std::string &path);

class StrL
{
public:
	static StrLInput ReadInputFile(const std::string &path);
	static BladeAeroStructInput ReadBladeFile(const std::string &path);
	static TowerStructInput ReadTowerFile(const std::string &path);
	static void ValidateInputOnly(const StrLInput &input);
	static StrL Load(const StrLInput &input);
	static StrL LoadFromFile(const std::string &path);

	const StrLInput &Input() const;
	std::string Summary() const;

private:
	StrLInput input_;
};
