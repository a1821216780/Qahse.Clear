#pragma once

#include <filesystem>
#include <string>

#include "IO/ModuleIO.hpp"

struct ControLInput
{
	std::filesystem::path inputPath;

	int pcMode = 0;
	std::string dllFileName;
	std::string dllInFile;
	std::string dllProcName = "DISCON";

	OutputConfig output;
};
