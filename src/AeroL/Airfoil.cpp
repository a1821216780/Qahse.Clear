#include "AeroL/Airfoil.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <regex>
#include <stdexcept>
#include <string>

#include <libInterpolate/Interpolate.hpp>

#include "IO/ModuleIO.hpp"
#include "IO/ZFile.hpp"
#include "IO/ZPath.hpp"
#include "IO/ZString.hpp"

struct AirfoilLookupInterpolators
{
	_1D::LinearInterpolator<double> clLinear;
	_1D::LinearInterpolator<double> cdLinear;
	_1D::LinearInterpolator<double> cmLinear;
	_1D::CubicSplineInterpolator<double> clCubic;
	_1D::CubicSplineInterpolator<double> cdCubic;
	_1D::CubicSplineInterpolator<double> cmCubic;
	bool hasLinear = false;
	bool hasCubic = false;
};

namespace
{
constexpr double kAlphaEpsilon = 1.0e-12;

std::vector<std::string> ReadAllLines(const std::string &path)
{
	std::ifstream in(path);
	if (!in.is_open())
		throw std::runtime_error("Cannot open airfoil file: " + path);
	std::vector<std::string> lines;
	std::string line;
	while (std::getline(in, line))
		lines.push_back(line);
	return lines;
}

std::string StripComment(const std::string &line)
{
	return module_io::StripInlineComment(line);
}

bool StartsWithNumber(const std::string &line)
{
	const auto text = ZString::Trim(line);
	if (text.empty())
		return false;
	std::size_t i = 0;
	if (text[i] == '+' || text[i] == '-')
		++i;
	if (i >= text.size())
		return false;
	return std::isdigit(static_cast<unsigned char>(text[i])) != 0 || text[i] == '.';
}

double ParseNumberToken(std::string token)
{
	std::replace(token.begin(), token.end(), ',', '.');
	return std::stod(token);
}

std::vector<double> ExtractNumbers(const std::string &line)
{
	static const std::regex numberPattern(
		R"([+-]?(?:(?:\d+(?:[\.,]\d*)?)|(?:[\.,]\d+))(?:[Ee][+-]?\d+)?)");

	std::vector<double> values;
	const auto text = StripComment(line);
	for (std::sregex_iterator it(text.begin(), text.end(), numberPattern), end; it != end; ++it)
		values.push_back(ParseNumberToken(it->str()));
	return values;
}

double FirstNumberOr(const std::string &line, double fallback)
{
	const auto values = ExtractNumbers(line);
	return values.empty() ? fallback : values.front();
}

std::string FirstTextToken(const std::string &line)
{
	const auto tokens = module_io::TokenizeLoose(line);
	return tokens.empty() ? std::string{} : tokens.front();
}

std::string ResolveRelativeTo(const std::filesystem::path &baseFile, const std::string &path)
{
	if (path.empty())
		return {};
	return ZPath::ResolvePath(baseFile.string(), path);
}

bool ContainsKey(const std::string &line, const std::string &key)
{
	return ZString::ToUpper(line).find(ZString::ToUpper(key)) != std::string::npos;
}

std::string TextBeforeKey(const std::string &line, const std::string &key)
{
	const auto text = StripComment(line);
	const auto upper = ZString::ToUpper(text);
	const auto pos = upper.find(ZString::ToUpper(key));
	if (pos == std::string::npos)
		return {};
	return ZString::Trim(text.substr(0, pos));
}

bool TryBoolBeforeKey(const std::string &line, const std::string &key, bool fallback)
{
	const auto value = ZString::ToUpper(TextBeforeKey(line, key));
	if (value == "TRUE" || value == "T" || value == "YES")
		return true;
	if (value == "FALSE" || value == "F" || value == "NO")
		return false;
	const auto values = ExtractNumbers(value);
	if (!values.empty())
		return values.front() != 0.0;
	return fallback;
}

bool IsAirfoilGeometryPath(const std::string &path)
{
	const auto ext = ZString::ToUpper(std::filesystem::path(path).extension().string());
	return ext == ".AFL" || ext == ".TXT" || ext == ".DAT";
}

double SafeSlope(double x0, double y0, double x1, double y1)
{
	const double dx = x1 - x0;
	if (std::abs(dx) <= kAlphaEpsilon)
		return 0.0;
	return (y1 - y0) / dx;
}

std::vector<AirfoilPolarPoint> SortedUniquePolarRows(std::vector<AirfoilPolarPoint> rows)
{
	std::sort(rows.begin(), rows.end(), [](const auto &lhs, const auto &rhs) {
		return lhs.alphaDeg < rhs.alphaDeg;
	});

	std::vector<AirfoilPolarPoint> uniqueRows;
	uniqueRows.reserve(rows.size());
	for (const auto &row : rows)
	{
		if (!uniqueRows.empty() && std::abs(uniqueRows.back().alphaDeg - row.alphaDeg) <= kAlphaEpsilon)
			uniqueRows.back() = row;
		else
			uniqueRows.push_back(row);
	}
	return uniqueRows;
}

AirfoilLookupTable BuildLookupTable(const std::vector<AirfoilPolarPoint> &rows, int interpolationOrder)
{
	AirfoilLookupTable table;
	const auto sortedRows = SortedUniquePolarRows(rows);
	table.alphaDeg.reserve(sortedRows.size());
	table.cl.reserve(sortedRows.size());
	table.cd.reserve(sortedRows.size());
	table.cm.reserve(sortedRows.size());

	for (const auto &row : sortedRows)
	{
		table.alphaDeg.push_back(row.alphaDeg);
		table.cl.push_back(row.cl);
		table.cd.push_back(row.cd);
		table.cm.push_back(row.cm);
	}

	if (table.alphaDeg.size() < 2)
		return table;

	const std::size_t intervalCount = table.alphaDeg.size() - 1u;
	table.dClDAlpha.reserve(intervalCount);
	table.dCdDAlpha.reserve(intervalCount);
	table.dCmDAlpha.reserve(intervalCount);
	for (std::size_t i = 0; i < intervalCount; ++i)
	{
		table.dClDAlpha.push_back(SafeSlope(table.alphaDeg[i], table.cl[i],
		                                    table.alphaDeg[i + 1u], table.cl[i + 1u]));
		table.dCdDAlpha.push_back(SafeSlope(table.alphaDeg[i], table.cd[i],
		                                    table.alphaDeg[i + 1u], table.cd[i + 1u]));
		table.dCmDAlpha.push_back(SafeSlope(table.alphaDeg[i], table.cm[i],
		                                    table.alphaDeg[i + 1u], table.cm[i + 1u]));
	}

	table.interpolators = std::make_shared<AirfoilLookupInterpolators>();
	table.interpolators->clLinear.setData(table.alphaDeg, table.cl);
	table.interpolators->cdLinear.setData(table.alphaDeg, table.cd);
	table.interpolators->cmLinear.setData(table.alphaDeg, table.cm);
	table.interpolators->hasLinear = true;

	if (interpolationOrder == 2 && table.alphaDeg.size() >= 3)
	{
		table.interpolators->clCubic.setData(table.alphaDeg, table.cl);
		table.interpolators->cdCubic.setData(table.alphaDeg, table.cd);
		table.interpolators->cmCubic.setData(table.alphaDeg, table.cm);
		table.interpolators->hasCubic = true;
	}

	return table;
}

void BuildLookupTables(AirfoilData &airfoil)
{
	airfoil.lookup = BuildLookupTable(airfoil.polar, airfoil.interpolationOrder);
	airfoil.lookupSets.clear();
	airfoil.lookupSets.reserve(airfoil.polarSets.size());
	for (const auto &set : airfoil.polarSets)
		airfoil.lookupSets.push_back(BuildLookupTable(set, airfoil.interpolationOrder));
}

std::size_t FindInterval(const AirfoilLookupTable &table, double alphaDeg)
{
	const auto upper = std::upper_bound(table.alphaDeg.begin(), table.alphaDeg.end(), alphaDeg);
	if (upper == table.alphaDeg.begin())
		return 0u;
	const std::size_t index = static_cast<std::size_t>(std::distance(table.alphaDeg.begin(), upper) - 1);
	return std::min(index, table.alphaDeg.size() - 2u);
}

AirfoilCoefficients FirstRowCoefficients(const AirfoilLookupTable &table)
{
	AirfoilCoefficients coefficients;
	if (!table.alphaDeg.empty())
	{
		coefficients.cl = table.cl.front();
		coefficients.cd = table.cd.front();
		coefficients.cm = table.cm.front();
	}
	return coefficients;
}

std::vector<AirfoilPolarPoint> ReadPolarRows(const std::vector<std::string> &lines,
                                             std::size_t startLine,
                                             int expectedCount)
{
	std::vector<AirfoilPolarPoint> rows;
	rows.reserve(static_cast<std::size_t>(std::max(expectedCount, 0)));
	for (std::size_t i = startLine; i < lines.size(); ++i)
	{
		if (!StartsWithNumber(lines[i]))
			continue;
		const auto values = ExtractNumbers(lines[i]);
		if (values.size() < 4)
			continue;
		rows.push_back({values[0], values[1], values[2], values[3]});
		if (expectedCount > 0 && rows.size() >= static_cast<std::size_t>(expectedCount))
			break;
	}
	return rows;
}

std::vector<std::vector<AirfoilPolarPoint>> ReadQBladePolarSets(const std::vector<std::string> &lines,
                                                                std::size_t reynoldsCount,
                                                                bool isDecomposed)
{
	const std::size_t stride = isDecomposed ? 6u : 3u;
	const std::size_t minColumnCount = 1u + stride * reynoldsCount;
	std::vector<std::vector<AirfoilPolarPoint>> sets(reynoldsCount);

	for (const auto &line : lines)
	{
		if (!StartsWithNumber(line))
			continue;
		const auto values = ExtractNumbers(line);
		if (values.size() < minColumnCount)
			continue;
		for (std::size_t i = 0; i < reynoldsCount; ++i)
		{
			const std::size_t offset = 1u + stride * i;
			sets[i].push_back({values[0], values[offset], values[offset + 1u], values[offset + 2u]});
		}
	}

	sets.erase(std::remove_if(sets.begin(), sets.end(), [](const auto &set) { return set.empty(); }), sets.end());
	return sets;
}
} // namespace

AirfoilGeometry ReadAirfoilGeometryFile(const std::string &path)
{
	if (!ZFile::Exists(path))
		throw std::runtime_error("Cannot open airfoil geometry file: " + path);

	AirfoilGeometry geometry;
	geometry.inputPath = std::filesystem::absolute(path).lexically_normal();
	const auto lines = ReadAllLines(path);

	int numLine = -1;
	for (std::size_t i = 0; i < lines.size(); ++i)
	{
		const auto text = ZString::Trim(StripComment(lines[i]));
		if (geometry.name.empty() && !text.empty() && !StartsWithNumber(text))
			geometry.name = text;
		if (ContainsKey(lines[i], "NumCoords"))
		{
			geometry.declaredCoordinateCount = static_cast<int>(FirstNumberOr(lines[i], 0.0));
			numLine = static_cast<int>(i);
			break;
		}
	}

	std::vector<AirfoilCoordinate> rows;
	if (numLine >= 0)
	{
		if (geometry.declaredCoordinateCount <= 0)
			return geometry;
		rows.reserve(static_cast<std::size_t>(geometry.declaredCoordinateCount));
		for (std::size_t i = static_cast<std::size_t>(numLine + 1); i < lines.size(); ++i)
		{
			if (!StartsWithNumber(lines[i]))
				continue;
			const auto values = ExtractNumbers(lines[i]);
			if (values.size() < 2)
				continue;
			rows.push_back({values[0], values[1]});
			if (rows.size() >= static_cast<std::size_t>(geometry.declaredCoordinateCount))
				break;
		}

		if (rows.size() != static_cast<std::size_t>(geometry.declaredCoordinateCount))
			throw std::runtime_error("Airfoil geometry NumCoords does not match coordinate row count: " + path);

		geometry.hasExplicitReference = true;
		geometry.reference = rows.front();
		geometry.coordinates.assign(rows.begin() + 1, rows.end());
		return geometry;
	}

	int formatType = 0;
	int numUpper = 0;
	int coordinateIndex = 0;
	for (const auto &line : lines)
	{
		if (!StartsWithNumber(line))
			continue;
		const auto values = ExtractNumbers(line);
		if (values.size() < 2)
			continue;

		if (formatType == 0 && rows.empty() && values[0] > 2.0 && values[1] > 2.0)
		{
			formatType = 1;
			numUpper = static_cast<int>(values[0]);
			continue;
		}

		if (formatType == 0)
		{
			rows.push_back({values[0], values[1]});
		}
		else
		{
			if (coordinateIndex < numUpper)
				rows.insert(rows.begin(), {values[0], values[1]});
			else if (coordinateIndex > numUpper)
				rows.push_back({values[0], values[1]});
			++coordinateIndex;
		}
	}

	if (rows.empty())
		throw std::runtime_error("Airfoil geometry file has no coordinate rows: " + path);

	geometry.reference = {0.25, 0.0};
	geometry.declaredCoordinateCount = static_cast<int>(rows.size());
	geometry.coordinates = std::move(rows);
	return geometry;
}

AirfoilData ReadAirfoilFile(const std::string &path)
{
	if (!ZFile::Exists(path))
		throw std::runtime_error("Cannot open airfoil file: " + path);

	AirfoilData airfoil;
	airfoil.inputPath = std::filesystem::absolute(path).lexically_normal();
	const auto lines = ReadAllLines(path);

	int polarStartLine = -1;
	bool qbladeMultiRe = false;
	bool qbladeIsDecomposed = false;
	for (std::size_t i = 0; i < lines.size(); ++i)
	{
		const auto &line = lines[i];
		if (ContainsKey(line, "POLARNAME"))
		{
			qbladeMultiRe = true;
			airfoil.polarName = FirstTextToken(line);
		}
		else if (ContainsKey(line, "FOILNAME"))
		{
			qbladeMultiRe = true;
			airfoil.airfoilName = FirstTextToken(line);
			if (IsAirfoilGeometryPath(airfoil.airfoilName))
				airfoil.geometryFile = ResolveRelativeTo(airfoil.inputPath, airfoil.airfoilName);
		}
		else if (ContainsKey(line, "Thickness"))
			airfoil.thickness = FirstNumberOr(line, airfoil.thickness);
		else if (ContainsKey(line, "ISDECOMPOSED"))
		{
			qbladeMultiRe = true;
			qbladeIsDecomposed = TryBoolBeforeKey(line, "ISDECOMPOSED", qbladeIsDecomposed);
		}
		else if (ContainsKey(line, "ReyNum") || ContainsKey(line, "Reynolds"))
		{
			const auto values = ExtractNumbers(line);
			if (!values.empty())
				airfoil.reynoldsNumbers = values;
			airfoil.reynoldsNumber = FirstNumberOr(line, airfoil.reynoldsNumber);
		}
		else if (ContainsKey(line, "PMCentre") || ContainsKey(line, "Pitching Moment Centre"))
			airfoil.pitchMomentCenter = FirstNumberOr(line, airfoil.pitchMomentCenter);
		else if (ContainsKey(line, "Geometry"))
			airfoil.geometryFile = ResolveRelativeTo(airfoil.inputPath, FirstTextToken(line));
		else if (ContainsKey(line, "InterpOrd"))
			airfoil.interpolationOrder = static_cast<int>(FirstNumberOr(line, airfoil.interpolationOrder));
		else if (ContainsKey(line, "NumAlf"))
		{
			airfoil.declaredPolarCount = static_cast<int>(FirstNumberOr(line, 0.0));
			polarStartLine = static_cast<int>(i + 1);
		}
	}

	if (polarStartLine >= 0)
	{
		airfoil.polar = ReadPolarRows(lines, static_cast<std::size_t>(polarStartLine), airfoil.declaredPolarCount);
		if (airfoil.declaredPolarCount > 0 &&
		    airfoil.polar.size() != static_cast<std::size_t>(airfoil.declaredPolarCount))
			throw std::runtime_error("Airfoil NumAlf does not match polar row count: " + path);
		if (airfoil.polar.empty())
			throw std::runtime_error("Airfoil file has no polar rows: " + path);
		if (airfoil.reynoldsNumbers.empty() && airfoil.reynoldsNumber != 0.0)
			airfoil.reynoldsNumbers.push_back(airfoil.reynoldsNumber);
		airfoil.polarSets = {airfoil.polar};
	}
	else if (qbladeMultiRe)
	{
		if (airfoil.reynoldsNumbers.empty())
			throw std::runtime_error("QBlade polar file is missing REYNOLDS: " + path);
		airfoil.reynoldsNumber = airfoil.reynoldsNumbers.front();
		airfoil.polarSets = ReadQBladePolarSets(lines, airfoil.reynoldsNumbers.size(), qbladeIsDecomposed);
		if (airfoil.polarSets.empty())
			throw std::runtime_error("QBlade polar file has no polar rows: " + path);
		airfoil.polar = airfoil.polarSets.front();
		airfoil.declaredPolarCount = static_cast<int>(airfoil.polar.size());
	}
	else
	{
		throw std::runtime_error("Airfoil file is missing NumAlf or QBlade POLARNAME/REYNOLDS data: " + path);
	}

	if (!airfoil.geometryFile.empty() && ZFile::Exists(airfoil.geometryFile))
		airfoil.geometry = ReadAirfoilGeometryFile(airfoil.geometryFile);
	BuildLookupTables(airfoil);
	return airfoil;
}

std::vector<AirfoilData> ReadAeroLAirfoilFiles(const AeroLInput &input)
{
	std::vector<AirfoilData> airfoils;
	airfoils.reserve(input.airfoils.files.size());
	for (const auto &file : input.airfoils.files)
		airfoils.push_back(ReadAirfoilFile(file));
	return airfoils;
}

AirfoilCoefficients EvaluateAirfoilCoefficients(const AirfoilData &airfoil,
                                                double alphaDeg,
                                                std::size_t polarSetIndex)
{
	const AirfoilLookupTable *table = &airfoil.lookup;
	if (!airfoil.lookupSets.empty())
	{
		if (polarSetIndex >= airfoil.lookupSets.size())
			throw std::out_of_range("Airfoil polar set index is out of range");
		table = &airfoil.lookupSets[polarSetIndex];
	}

	if (table->alphaDeg.empty())
		throw std::runtime_error("Airfoil lookup table is empty: " + airfoil.inputPath.string());
	if (table->alphaDeg.size() == 1u)
		return FirstRowCoefficients(*table);

	// QBlade Polar360 returns the first tabulated values outside the 360-degree table.
	if (alphaDeg < table->alphaDeg.front() || alphaDeg > table->alphaDeg.back())
		return FirstRowCoefficients(*table);

	const std::size_t interval = FindInterval(*table, alphaDeg);
	AirfoilCoefficients coefficients;
	coefficients.dClDAlpha = table->dClDAlpha[interval];
	coefficients.dCdDAlpha = table->dCdDAlpha[interval];
	coefficients.dCmDAlpha = table->dCmDAlpha[interval];

	if (airfoil.interpolationOrder == 2 &&
	    table->interpolators &&
	    table->interpolators->hasCubic)
	{
		coefficients.cl = table->interpolators->clCubic(alphaDeg);
		coefficients.cd = table->interpolators->cdCubic(alphaDeg);
		coefficients.cm = table->interpolators->cmCubic(alphaDeg);
		coefficients.dClDAlpha = table->interpolators->clCubic.derivative(alphaDeg);
		coefficients.dCdDAlpha = table->interpolators->cdCubic.derivative(alphaDeg);
		coefficients.dCmDAlpha = table->interpolators->cmCubic.derivative(alphaDeg);
		return coefficients;
	}

	if (table->interpolators && table->interpolators->hasLinear)
	{
		coefficients.cl = table->interpolators->clLinear(alphaDeg);
		coefficients.cd = table->interpolators->cdLinear(alphaDeg);
		coefficients.cm = table->interpolators->cmLinear(alphaDeg);
		return coefficients;
	}

	const double alpha0 = table->alphaDeg[interval];
	coefficients.cl = table->cl[interval] + (alphaDeg - alpha0) * coefficients.dClDAlpha;
	coefficients.cd = table->cd[interval] + (alphaDeg - alpha0) * coefficients.dCdDAlpha;
	coefficients.cm = table->cm[interval] + (alphaDeg - alpha0) * coefficients.dCmDAlpha;
	return coefficients;
}
