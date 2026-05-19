#include "HydroL/Wamit.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "IO/ZFile.hpp"
#include "IO/ZString.hpp"

namespace
{
std::vector<std::string> ReadAllLines(const std::string &path)
{
	std::ifstream in(path);
	if (!in.is_open())
		throw std::runtime_error("Cannot open WAMIT file: " + path);
	std::vector<std::string> lines;
	std::string line;
	while (std::getline(in, line))
		lines.push_back(line);
	return lines;
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
	std::replace(token.begin(), token.end(), 'D', 'E');
	std::replace(token.begin(), token.end(), 'd', 'e');
	return std::stod(token);
}

std::vector<double> ExtractNumbers(const std::string &line)
{
	std::vector<double> values;
	std::istringstream input(line);
	std::string token;
	while (input >> token)
	{
		try
		{
			values.push_back(ParseNumberToken(token));
		}
		catch (...)
		{
		}
	}
	return values;
}

bool HasExtension(const std::filesystem::path &path, const std::string &extension)
{
	return ZString::ToLower(path.extension().string()) == ZString::ToLower(extension);
}

bool HasSuffix(const std::filesystem::path &path, const std::string &suffix)
{
	return ZString::EndsWith(ZString::ToLower(path.filename().string()), ZString::ToLower(suffix));
}

void RequireNonEmpty(const WamitData &data, const std::string &path)
{
	if (data.type == WamitFileType::RADIATION && data.radiation.empty())
		throw std::runtime_error("WAMIT radiation file has no valid rows: " + path);
	if (data.type == WamitFileType::EXCITATION && data.excitation.empty())
		throw std::runtime_error("WAMIT excitation file has no valid rows: " + path);
	if ((data.type == WamitFileType::DIFFERENCE_QTF || data.type == WamitFileType::SUM_QTF) && data.qtf.empty())
		throw std::runtime_error("WAMIT QTF file has no valid rows: " + path);
}
} // namespace

WamitFileType InferWamitFileType(const std::string &path)
{
	const auto file = std::filesystem::path(path);
	if (HasExtension(file, ".1"))
		return WamitFileType::RADIATION;
	if (HasExtension(file, ".3"))
		return WamitFileType::EXCITATION;
	if (HasExtension(file, ".12d") || HasSuffix(file, ".12d"))
		return WamitFileType::DIFFERENCE_QTF;
	if (HasExtension(file, ".12s") || HasSuffix(file, ".12s"))
		return WamitFileType::SUM_QTF;
	return WamitFileType::UNKNOWN;
}

WamitData ReadWamitFile(const std::string &path, WamitFileType type)
{
	if (!ZFile::Exists(path))
		throw std::runtime_error("Cannot open WAMIT file: " + path);
	if (type == WamitFileType::UNKNOWN)
		type = InferWamitFileType(path);
	if (type == WamitFileType::UNKNOWN)
		throw std::runtime_error("Cannot infer WAMIT file type from path: " + path);

	WamitData data;
	data.inputPath = std::filesystem::absolute(path).lexically_normal();
	data.type = type;

	for (const auto &line : ReadAllLines(path))
	{
		if (!StartsWithNumber(line))
			continue;
		const auto values = ExtractNumbers(line);
		if (type == WamitFileType::RADIATION)
		{
			if (values.size() < 4)
				continue;
			WamitRadiationEntry entry;
			entry.period = values[0];
			entry.row = static_cast<int>(values[1]);
			entry.column = static_cast<int>(values[2]);
			entry.addedMass = values[3];
			if (values.size() >= 5)
			{
				entry.damping = values[4];
				entry.hasDamping = true;
			}
			data.radiation.push_back(entry);
		}
		else if (type == WamitFileType::EXCITATION)
		{
			if (values.size() < 7)
				continue;
			data.excitation.push_back({
				values[0],
				values[1],
				static_cast<int>(values[2]),
				values[3],
				values[4],
				values[5],
				values[6],
			});
		}
		else
		{
			if (values.size() < 9)
				continue;
			data.qtf.push_back({
				values[0],
				values[1],
				values[2],
				values[3],
				static_cast<int>(values[4]),
				values[5],
				values[6],
				values[7],
				values[8],
			});
		}
	}

	RequireNonEmpty(data, path);
	return data;
}

HydroLWamitData ReadHydroLWamitFiles(const HydroLInput &input)
{
	HydroLWamitData data;
	if (input.useRadiation || input.useRadAddedMass)
		data.radiation = ReadWamitFile(input.potentialRadFile, WamitFileType::RADIATION);
	if (input.useExcitation)
		data.excitation = ReadWamitFile(input.potentialExcFile, WamitFileType::EXCITATION);
	if (input.diffEvalType != HydroLDiffEvalType::NONE)
		data.difference = ReadWamitFile(input.potentialDiffFile, WamitFileType::DIFFERENCE_QTF);
	if (input.useSumFreqs)
		data.sum = ReadWamitFile(input.potentialSumFile, WamitFileType::SUM_QTF);
	return data;
}
