#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "AeroL/AeroL_Type.hpp"

struct AirfoilLookupInterpolators;

struct AirfoilPolarPoint
{
	double alphaDeg = 0.0;
	double cl = 0.0;
	double cd = 0.0;
	double cm = 0.0;
};

struct AirfoilCoefficients
{
	double cl = 0.0;
	double cd = 0.0;
	double cm = 0.0;
	double dClDAlpha = 0.0;
	double dCdDAlpha = 0.0;
	double dCmDAlpha = 0.0;
};

struct AirfoilLookupTable
{
	std::vector<double> alphaDeg;
	std::vector<double> cl;
	std::vector<double> cd;
	std::vector<double> cm;
	std::vector<double> dClDAlpha;
	std::vector<double> dCdDAlpha;
	std::vector<double> dCmDAlpha;
	std::shared_ptr<AirfoilLookupInterpolators> interpolators;
};

struct AirfoilCoordinate
{
	double x = 0.0;
	double y = 0.0;
};

struct AirfoilGeometry
{
	std::filesystem::path inputPath;
	std::string name;
	int declaredCoordinateCount = 0;
	bool hasExplicitReference = false;
	AirfoilCoordinate reference;
	std::vector<AirfoilCoordinate> coordinates;
};

struct AirfoilData
{
	std::filesystem::path inputPath;
	std::string polarName;
	std::string airfoilName;
	double thickness = 0.0;
	double reynoldsNumber = 0.0;
	std::vector<double> reynoldsNumbers;
	double pitchMomentCenter = 0.0;
	std::string geometryFile;
	int interpolationOrder = 1;
	int declaredPolarCount = 0;
	std::vector<AirfoilPolarPoint> polar;
	std::vector<std::vector<AirfoilPolarPoint>> polarSets;
	AirfoilLookupTable lookup;
	std::vector<AirfoilLookupTable> lookupSets;
	AirfoilGeometry geometry;
};

AirfoilGeometry ReadAirfoilGeometryFile(const std::string &path);
AirfoilData ReadAirfoilFile(const std::string &path);
std::vector<AirfoilData> ReadAeroLAirfoilFiles(const AeroLInput &input);
AirfoilCoefficients EvaluateAirfoilCoefficients(const AirfoilData &airfoil,
                                                double alphaDeg,
                                                std::size_t polarSetIndex = 0);
