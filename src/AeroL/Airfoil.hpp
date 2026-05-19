#pragma once

#include <string>
#include <vector>

#include "AeroL/AeroL_Type.hpp"

AirfoilGeometry ReadAirfoilGeometryFile(const std::string &path);
AirfoilData ReadAirfoilFile(const std::string &path);
std::vector<AirfoilData> ReadAeroLAirfoilFiles(const AeroLInput &input);
void BuildAirfoilLookupTables(AirfoilData &airfoil);
AirfoilCoefficients EvaluateAirfoilCoefficients(const AirfoilData &airfoil,
                                                double alphaDeg,
                                                std::size_t polarSetIndex = 0);
