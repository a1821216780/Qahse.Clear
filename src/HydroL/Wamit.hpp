#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "HydroL/HydroL_Type.hpp"

WamitFileType InferWamitFileType(const std::string &path);
WamitData ReadWamitFile(const std::string &path, WamitFileType type = WamitFileType::UNKNOWN);
HydroLWamitData ReadHydroLWamitFiles(const HydroLInput &input);
