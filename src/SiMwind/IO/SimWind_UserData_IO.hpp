#pragma once

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../SimWind_Type.hpp"
#include "../../IO/LocaleString.hpp"
#include "../../IO/ModuleIO.hpp"
#include "../../IO/Serializer.hpp"
#include "../../IO/ZFile.hpp"
#include "../../IO/ZString.hpp"

namespace simwind_user_data_io_detail
{
inline constexpr const char *kYamlRoot = "Qahse.SimWind";

inline void RequireFile(const std::string &path)
{
	if (!ZFile::Exists(path))
		throw std::runtime_error(std::string(L_IO_FileNotFound) + ": " + path);
}

inline std::vector<std::vector<double>> PointRows(const UserWindSpeedData &data)
{
	std::vector<std::vector<double>> rows;
	rows.reserve(data.points.size());
	for (const auto &point : data.points)
		rows.push_back({point.y, point.z});
	return rows;
}

inline int EffectiveComponentCount(const UserWindSpeedData &data)
{
	if (data.nComp > 0)
		return data.nComp;
	if (!data.components.empty())
		return static_cast<int>(data.components.front().size());
	return 0;
}

inline int EffectivePointCount(const UserWindSpeedData &data)
{
	if (data.nPoints > 0)
		return data.nPoints;
	if (!data.points.empty())
		return static_cast<int>(data.points.size());
	return static_cast<int>(data.components.size());
}

inline std::vector<std::vector<double>> WindSpeedRows(const UserWindSpeedData &data)
{
	const int nPoints = EffectivePointCount(data);
	const int nComp = EffectiveComponentCount(data);
	std::vector<std::vector<double>> rows;
	rows.reserve(data.time.size());

	for (std::size_t t = 0; t < data.time.size(); ++t)
	{
		std::vector<double> row;
		row.reserve(static_cast<std::size_t>(1 + nPoints * nComp));
		row.push_back(data.time[t]);
		for (int p = 0; p < nPoints; ++p)
		{
			for (int c = 0; c < nComp; ++c)
			{
				double value = 0.0;
				const auto ps = static_cast<std::size_t>(p);
				const auto cs = static_cast<std::size_t>(c);
				if (ps < data.components.size() &&
				    cs < data.components[ps].size() &&
				    t < data.components[ps][cs].size())
				{
					value = data.components[ps][cs][t];
				}
				row.push_back(value);
			}
		}
		rows.push_back(std::move(row));
	}
	return rows;
}

inline std::vector<std::vector<double>> ReadPointRows(const std::vector<std::string> &lines, int nPoints)
{
	bool inPointBlock = false;
	std::vector<std::vector<double>> rows;
	rows.reserve(static_cast<std::size_t>(std::max(nPoints, 0)));

	for (const auto &line : lines)
	{
		const std::string text = ZString::Trim(line);
		if (!inPointBlock)
		{
			if (text.find("Pointyi") != std::string::npos && text.find("Pointzi") != std::string::npos)
				inPointBlock = true;
			continue;
		}

		if (text.find("Time Series") != std::string::npos)
			break;
		if (text.empty() || ZString::StartsWith(text, "--") || text.find('(') != std::string::npos)
			continue;

		std::vector<double> row;
		if (Serializer::TryParseNumberRow(text, row) && row.size() >= 2)
		{
			row.resize(2);
			rows.push_back(std::move(row));
			if (nPoints > 0 && rows.size() >= static_cast<std::size_t>(nPoints))
				break;
		}
	}
	return rows;
}

inline char ComponentName(int index)
{
	if (index == 0) return 'u';
	if (index == 1) return 'v';
	if (index == 2) return 'w';
	return static_cast<char>('a' + index);
}

inline std::vector<std::string> UserWindTimeSeriesHeaders(int nPoints, int nComp)
{
	std::vector<std::string> headers{"Time_s"};
	for (int p = 0; p < nPoints; ++p)
		for (int c = 0; c < nComp; ++c)
			headers.push_back("Point" + std::to_string(p + 1) + ComponentName(c));
	return headers;
}
} // namespace simwind_user_data_io_detail

inline UserShearData ReadUserShear(const std::string &path)
{
	using namespace simwind_user_data_io_detail;
	RequireFile(path);
	const Serializer reader = Serializer::OpenReader(path, kYamlRoot);

	UserShearData data;
	data.numHeights = reader.Read<int>("NumUSRz", 0);
	data.stdScale1 = reader.Read<double>("StdScale1", data.stdScale1);
	data.stdScale2 = reader.Read<double>("StdScale2", data.stdScale2);
	data.stdScale3 = reader.Read<double>("StdScale3", data.stdScale3);

	Serializer::LoadColumns(
		reader.ReadTableRows("Data", 5, static_cast<std::size_t>(std::max(data.numHeights, 0))),
		data.heights,
		data.windSpeeds,
		data.windDirections,
		data.standardDeviations,
		data.lengthScales);

	if (data.numHeights == 0)
		data.numHeights = static_cast<int>(data.heights.size());
	return data;
}

inline void WriteUserShear(const UserShearData &data, const std::string &path)
{
	using namespace simwind_user_data_io_detail;
	const auto rows = Serializer::ColumnsToRows(data.heights,
	                                            data.windSpeeds,
	                                            data.windDirections,
	                                            data.standardDeviations,
	                                            data.lengthScales);

	if (Serializer::IsYamlPath(path))
	{
		Serializer writer = Serializer::OpenYamlWriter(kYamlRoot);
		writer.AddNode("NumUSRz", data.numHeights > 0 ? data.numHeights : static_cast<int>(rows.size()));
		writer.AddNode("StdScale1", data.stdScale1);
		writer.AddNode("StdScale2", data.stdScale2);
		writer.AddNode("StdScale3", data.stdScale3);
		module_io::AddNumericTableYamlNodes(writer, "Data",
			{"Height_m", "WindSpeed_mps", "WindDirection_deg", "StandardDeviation_mps", "LengthScale_m"},
			rows, 3);
		writer.SaveYamlFile(path);
		return;
	}

	std::vector<std::string> lines;
	lines.reserve(rows.size() + 10);
	lines.push_back("-- Qahse.SimWind user shear table");
	lines.push_back(std::to_string(data.numHeights > 0 ? data.numHeights : static_cast<int>(rows.size())) +
	                " NumUSRz - Number of heights");
	lines.push_back(ZString::FormatDouble(data.stdScale1) + " StdScale1 - u standard deviation scale");
	lines.push_back(ZString::FormatDouble(data.stdScale2) + " StdScale2 - v standard deviation scale");
	lines.push_back(ZString::FormatDouble(data.stdScale3) + " StdScale3 - w standard deviation scale");
	lines.push_back("Height WindSpeed WindDirection StandardDeviation LengthScale");
	lines.push_back("!Begin");
	Serializer::AddRows(lines, rows);
	ZFile::WriteAllLines(path, lines);
}

inline void ConvertUserShear(const std::string &inputPath, const std::string &outputPath)
{
	WriteUserShear(ReadUserShear(inputPath), outputPath);
}

inline UserSpectraData ReadUserSpectra(const std::string &path)
{
	using namespace simwind_user_data_io_detail;
	RequireFile(path);
	const Serializer reader = Serializer::OpenReader(path, kYamlRoot);

	UserSpectraData data;
	data.numFrequencies = reader.Read<int>("NumUSRf", 0);
	data.specScale1 = reader.Read<double>("SpecScale1", data.specScale1);
	data.specScale2 = reader.Read<double>("SpecScale2", data.specScale2);
	data.specScale3 = reader.Read<double>("SpecScale3", data.specScale3);

	Serializer::LoadColumns(
		reader.ReadTableRows("Data", 4, static_cast<std::size_t>(std::max(data.numFrequencies, 0))),
		data.frequencies,
		data.uPsd,
		data.vPsd,
		data.wPsd);

	if (data.numFrequencies == 0)
		data.numFrequencies = static_cast<int>(data.frequencies.size());
	return data;
}

inline void WriteUserSpectra(const UserSpectraData &data, const std::string &path)
{
	using namespace simwind_user_data_io_detail;
	const auto rows = Serializer::ColumnsToRows(data.frequencies, data.uPsd, data.vPsd, data.wPsd);

	if (Serializer::IsYamlPath(path))
	{
		Serializer writer = Serializer::OpenYamlWriter(kYamlRoot);
		writer.AddNode("NumUSRf", data.numFrequencies > 0 ? data.numFrequencies : static_cast<int>(rows.size()));
		writer.AddNode("SpecScale1", data.specScale1);
		writer.AddNode("SpecScale2", data.specScale2);
		writer.AddNode("SpecScale3", data.specScale3);
		module_io::AddNumericTableYamlNodes(writer, "Data",
			{"Frequency_Hz", "uPSD", "vPSD", "wPSD"}, rows, 3);
		writer.SaveYamlFile(path);
		return;
	}

	std::vector<std::string> lines;
	lines.reserve(rows.size() + 5);
	lines.push_back("-- Qahse.SimWind user spectra table");
	lines.push_back(std::to_string(data.numFrequencies > 0 ? data.numFrequencies : static_cast<int>(rows.size())) +
	                " NumUSRf - Number of frequencies");
	lines.push_back(ZString::FormatDouble(data.specScale1) + " SpecScale1 - u spectrum scale");
	lines.push_back(ZString::FormatDouble(data.specScale2) + " SpecScale2 - v spectrum scale");
	lines.push_back(ZString::FormatDouble(data.specScale3) + " SpecScale3 - w spectrum scale");
	lines.push_back("Frequency uPSD vPSD wPSD");
	lines.push_back("!Begin");
	Serializer::AddRows(lines, rows);
	ZFile::WriteAllLines(path, lines);
}

inline void ConvertUserSpectra(const std::string &inputPath, const std::string &outputPath)
{
	WriteUserSpectra(ReadUserSpectra(inputPath), outputPath);
}

inline UserWindSpeedData ReadUserWindSpeed(const std::string &path)
{
	using namespace simwind_user_data_io_detail;
	RequireFile(path);
	const Serializer reader = Serializer::OpenReader(path, kYamlRoot);

	UserWindSpeedData data;
	std::vector<std::vector<double>> pointRows;
	std::vector<std::vector<double>> seriesRows;

	data.nComp = reader.Read<int>("nComp", 0);
	data.nPoints = reader.Read<int>("nPoints", 0);
	data.refPtID = reader.Read<int>("RefPtID", 0);

	if (reader.IsYaml())
	{
		pointRows = Serializer::RowsFromMatrix(reader.ReadMatrix("Points"));
		seriesRows = Serializer::RowsFromMatrix(reader.ReadMatrix("TimeSeries"));
	}
	else
	{
		pointRows = ReadPointRows(reader.RawLines(), data.nPoints);
		const int expectedCols = data.nPoints > 0 && data.nComp > 0 ? 1 + data.nPoints * data.nComp : 0;
		seriesRows = Serializer::ReadRowsAfterBegin(reader.RawLines(), static_cast<std::size_t>(expectedCols));
	}

	for (const auto &row : pointRows)
	{
		if (row.size() < 2)
			continue;
		WindPoint point;
		point.y = row[0];
		point.z = row[1];
		data.points.push_back(point);
	}
	if (data.nPoints == 0)
		data.nPoints = static_cast<int>(data.points.size());

	data.components.resize(static_cast<std::size_t>(std::max(data.nPoints, 0)));
	for (auto &pointComponents : data.components)
		pointComponents.resize(static_cast<std::size_t>(std::max(data.nComp, 0)));

	const int expectedCols = data.nPoints > 0 && data.nComp > 0 ? 1 + data.nPoints * data.nComp : 0;
	for (const auto &row : seriesRows)
	{
		if (expectedCols != 0 && static_cast<int>(row.size()) < expectedCols)
			continue;

		data.time.push_back(row[0]);
		int col = 1;
		for (int p = 0; p < data.nPoints; ++p)
		{
			for (int c = 0; c < data.nComp; ++c)
			{
				data.components[static_cast<std::size_t>(p)][static_cast<std::size_t>(c)].push_back(row[static_cast<std::size_t>(col)]);
				++col;
			}
		}
	}

	return data;
}

inline void WriteUserWindSpeed(const UserWindSpeedData &data, const std::string &path)
{
	using namespace simwind_user_data_io_detail;
	const int nComp = EffectiveComponentCount(data);
	const int nPoints = EffectivePointCount(data);
	const auto pointRows = PointRows(data);
	const auto seriesRows = WindSpeedRows(data);

	if (Serializer::IsYamlPath(path))
	{
		Serializer writer = Serializer::OpenYamlWriter(kYamlRoot);
		writer.AddNode("nComp", nComp);
		writer.AddNode("nPoints", nPoints);
		writer.AddNode("RefPtID", data.refPtID);
		module_io::AddNumericTableYamlNodes(writer, "Points", {"PointY_m", "PointZ_m"}, pointRows, 3);
		module_io::AddNumericTableYamlNodes(writer, "TimeSeries", UserWindTimeSeriesHeaders(nPoints, nComp), seriesRows, 3);
		writer.SaveYamlFile(path);
		return;
	}

	std::vector<std::string> lines;
	lines.reserve(seriesRows.size() + pointRows.size() + 12);
	lines.push_back("-- Qahse.SimWind user wind speed table");
	lines.push_back(std::to_string(nComp) + " nComp - Number of velocity components");
	lines.push_back(std::to_string(nPoints) + " nPoints - Number of points");
	lines.push_back(std::to_string(data.refPtID) + " RefPtID - Reference point id");
	lines.push_back("Pointyi Pointzi");
	lines.push_back("(m) (m)");
	Serializer::AddRows(lines, pointRows);
	lines.push_back("--------Time Series------------------------------------------------------------");

	std::ostringstream header;
	header << "Elapsed Time";
	for (int p = 0; p < nPoints; ++p)
	{
		for (int c = 0; c < nComp; ++c)
		{
			header << " Point" << std::setw(2) << std::setfill('0') << (p + 1) << ComponentName(c);
			header << std::setfill(' ');
		}
	}
	lines.push_back(header.str());
	lines.push_back("!Begin");
	Serializer::AddRows(lines, seriesRows);
	ZFile::WriteAllLines(path, lines);
}

inline void ConvertUserWindSpeed(const std::string &inputPath, const std::string &outputPath)
{
	WriteUserWindSpeed(ReadUserWindSpeed(inputPath), outputPath);
}
