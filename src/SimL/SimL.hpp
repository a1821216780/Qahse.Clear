#pragma once

#include <string>

#include "SimL/SimL_Type.hpp"

SimLInput ReadSimLInputFile(const std::string &path);
SimLModuleInputs ResolveSimLModuleInputs(const SimLInput &input);
SimLResolvedInput ReadSimLResolvedInputFile(const std::string &path);
void WriteSimLResolvedInputFile(const SimLResolvedInput &input, const std::string &path);
void ConvertSimLInputToSimFile(const std::string &inputPath, const std::string &outputPath);

class SimL
{
public:
	static SimLInput ReadInputFile(const std::string &path);
	static SimLResolvedInput ReadResolvedInputFile(const std::string &path);
	static void ValidateInputOnly(const SimLInput &input);
	static SimL Load(const SimLInput &input);
	static SimL Load(const SimLResolvedInput &input);
	static SimL LoadFromFile(const std::string &path);

	const SimLInput &Input() const;
	const SimLModuleInputs &Modules() const;
	const SimLResolvedInput &ResolvedInput() const;
	std::string Summary() const;

private:
	SimLResolvedInput input_;
};
