#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "IO/ModuleIO.hpp"
#include "IO/Serializer.hpp"
#include "IO/ZFile.hpp"
#include "StrL/StrL_Type.hpp"

namespace strl_io_detail
{
inline constexpr const char *kYamlRoot = "Qahse.StrL";
inline constexpr const char *kBladeYamlRoot = "Qahse.BladeAeroStruct";
inline constexpr const char *kTowerYamlRoot = "Qahse.TowerStruct";

class StrLInputSerializer : public Serializer
{
public:
	StrLInput data;

	StrLInputSerializer()
	{
		SetValueFirst(true);
		SetYamlRoot(kYamlRoot);
	}

	explicit StrLInputSerializer(const StrLInput &input)
		: data(input)
	{
		SetValueFirst(true);
		SetYamlRoot(kYamlRoot);
	}

protected:
	void SerializeFields() override
	{
		Fields(
			Field("TowerNum", data.towerNum),
			Field("TowerYdeg", data.towerYdeg));
		ReadOrWriteAny({"NumBld", "BldNum", "BladeNum"}, data.numBld);
		ReadOrWriteAny({"HubRad", "HubRadius"}, data.hubRadius);
		ReadOrWriteAny({"OverHang", "RotorOverhang"}, data.rotorOverhang);
		Fields(
			Field("PreCone", data.preCone),
			Field("ShaftTilt", data.shaftTilt),
			Field("Twr2Shft", data.twr2Shft),
			Field("HubMass", data.hubMass),
			Field("HubIner", data.hubIner),
			Field("Gravity", data.gravity),
			Field("Azimuth", data.azimuth),
			Field("AzimB1Up", data.azimB1Up),
			Field("RotSpeed", data.rotSpeed),
			Field("NacYaw", data.nacYaw),
			Field("NACCAX", data.naccAx),
			Field("NACCAY", data.naccAy),
			Field("NACCAZ", data.naccAz),
			Field("NACCDX", data.naccDx),
			Field("NACCDY", data.naccDy),
			Field("NACCDZ", data.naccDz),
			Field("YawBrMass", data.yawBrMass),
			Field("NacMass", data.nacMass),
			Field("NacCmX", data.nacCmX),
			Field("NacCmY", data.nacCmY),
			Field("NacCmZ", data.nacCmZ),
			Field("NacYawIner", data.nacYawIner),
			Field("GearboxRatio", data.gearboxRatio),
			Field("GearboxEff", data.gearboxEff),
			Field("DrivetrainDof", data.drivetrainDof),
			Field("GenIner", data.genIner),
			Field("DTTorSpr", data.dtTorSpr),
			Field("DTTorDmp", data.dtTorDmp));
		ReadOrWriteAny({"BladeNum", "Bldnum", "NumBld"}, data.bladeNum);
		ReadOrWriteAny({"BladeAeroStructFile", "BladeFile"}, data.bladeAeroStructFile);
		Fields(
			Field("TowerHeight", data.towerHeight),
			Field("TowerFile", data.towerFile),
			Field("SubFile", data.subFile),
			Field("SumPrint", data.output.sumPrint),
			Field("AfSpanput", data.output.afSpanput),
			Field("SumPath", data.output.sumPath));
		ReadOrWriteAny({"NBlOuts", "NumBladeOutNodes"}, data.output.blade.count);
		ReadOrWriteAny({"NTwOuts", "NumTowerOutNodes"}, data.output.tower.count);
	}
};

inline void ReadLegacyBladeFiles(const Serializer &reader, StrLInput &input)
{
	for (const auto *key : {"BladeFile", "BladeFile1", "BladeFile2", "BladeFile3"})
	{
		auto value = module_io::TrimQuotes(reader.read(key));
		if (!value.empty())
			input.bladeFiles.push_back(value);
	}
	for (int i = 1; i <= 3; ++i)
	{
		auto value = module_io::TrimQuotes(reader.read("BladeFile(" + std::to_string(i) + ")"));
		if (!value.empty())
			input.bladeFiles.push_back(value);
	}

	std::sort(input.bladeFiles.begin(), input.bladeFiles.end());
	input.bladeFiles.erase(std::unique(input.bladeFiles.begin(), input.bladeFiles.end()), input.bladeFiles.end());
	if (input.bladeAeroStructFile.empty() && !input.bladeFiles.empty())
		input.bladeAeroStructFile = input.bladeFiles.front();
	if (input.bladeFiles.empty() && !input.bladeAeroStructFile.empty())
		input.bladeFiles.push_back(input.bladeAeroStructFile);
}

inline void ResolvePaths(StrLInput &input)
{
	const std::string base = input.inputPath.string();
	module_io::ResolveIfSetWithExtension(base, input.bladeAeroStructFile, ".str");
	for (auto &file : input.bladeFiles)
		module_io::ResolveIfSetWithExtension(base, file, ".str");
	module_io::ResolveIfSetWithExtension(base, input.towerFile, ".str");
	module_io::ResolveIfSetWithExtension(base, input.subFile, ".str");
	module_io::ResolveOutputPath(base, input.output);
}

inline void Validate(const StrLInput &input)
{
	if (!module_io::FileExistsOrEmpty(input.bladeAeroStructFile))
		throw std::runtime_error("StrL BladeAeroStructFile does not exist: " + input.bladeAeroStructFile);
	if (!module_io::FileExistsOrEmpty(input.towerFile))
		throw std::runtime_error("StrL TowerFile does not exist: " + input.towerFile);
	// SubFile is intentionally optional in StrL v1. HydroL owns foundation and hydrodynamics.
}

inline StrLInput ReadStrLInput(const std::string &path)
{
	if (!ZFile::Exists(path))
		throw std::runtime_error("Cannot open StrL input file: " + path);

	StrLInputSerializer reader;
	reader.ReadFile(path);
	StrLInput input = reader.data;
	input.inputPath = std::filesystem::absolute(path).lexically_normal();
	if (reader.IsYaml())
		input.bladeFiles = module_io::ReadYamlStringArray(path, kYamlRoot, "BladeFiles");
	else
		ReadLegacyBladeFiles(reader, input);
	module_io::ReadOutputConfig(reader, path, kYamlRoot, input.output);
	ResolvePaths(input);
	Validate(input);
	return input;
}

class BladeAeroStructSerializer : public Serializer
{
public:
	BladeAeroStructInput data;

	BladeAeroStructSerializer()
	{
		SetValueFirst(true);
		SetYamlRoot(kBladeYamlRoot);
	}

	explicit BladeAeroStructSerializer(const BladeAeroStructInput &input)
		: data(input)
	{
		SetValueFirst(true);
		SetYamlRoot(kBladeYamlRoot);
	}

protected:
	void SerializeFields() override
	{
		Fields(
			Field("BladeRayleighDamp", data.rayleighDamp),
			Field("BladeStiffTuner", data.stiffTuner),
			Field("BladeMassTuner", data.massTuner),
			Field("BladeBeamType", data.beamType),
			Field("BladeDiscCount", data.discCount));
	}
};

inline BladeSection ToBladeSection(const std::vector<std::string> &row)
{
	BladeSection section;
	section.raw = row;
	auto number = [&](std::size_t index) {
		if (index >= row.size())
			return 0.0;
		try { return std::stod(row[index]); }
		catch (...) { return 0.0; }
	};
	auto parsePolarFileId = [&]() {
		if (row.size() <= 7)
			throw std::runtime_error("Blade aero-struct row is missing PolarFileID column");

		section.polarFileToken = module_io::TrimQuotes(row[7]);
		double value = 0.0;
		try
		{
			std::size_t parsed = 0;
			value = std::stod(section.polarFileToken, &parsed);
			if (parsed != section.polarFileToken.size())
				throw std::invalid_argument("trailing characters");
		}
		catch (...)
		{
			throw std::runtime_error(
				"Blade aero-struct PolarFileID must be a numeric 1-based AeroL AFNames index, got '" +
				section.polarFileToken +
				"'. Put the polar/airfoil file path in AeroL AFNames and use its index in the blade table.");
		}

		const double rounded = std::round(value);
		if (rounded < 1.0 || std::abs(value - rounded) > 1.0e-9)
		{
			throw std::runtime_error(
				"Blade aero-struct PolarFileID must be a positive integer AeroL AFNames index, got '" +
				section.polarFileToken + "'.");
		}
		return static_cast<int>(rounded);
	};
	section.radialPos = number(0);
	section.chord = number(1);
	section.twist = number(2);
	section.offsetY = number(3);
	section.offsetX = number(4);
	section.pitchAxisY = number(5);
	section.pitchAxisX = number(6);
	section.polarFileId = parsePolarFileId();
	section.relThickness = number(8);
	return section;
}

inline BladeAeroStructInput ReadBladeAeroStructInput(const std::string &path)
{
	if (!ZFile::Exists(path))
		throw std::runtime_error("Cannot open blade aero-struct file: " + path);

	BladeAeroStructSerializer reader;
	reader.ReadFile(path);
	BladeAeroStructInput input = reader.data;
	input.inputPath = std::filesystem::absolute(path).lexically_normal();
	input.rayleighDampAniso = module_io::ReadDoubleList(reader, path, kBladeYamlRoot, {"BladeRayleighDampAniso"});
	input.sectionRows = module_io::ReadRows(reader, path, kBladeYamlRoot, "SectionRows", 9);
	if (input.sectionRows.empty())
		input.sectionRows = module_io::ReadRows(reader, path, kBladeYamlRoot, "RadialPos", 9);
	input.sections.reserve(input.sectionRows.size());
	for (const auto &row : input.sectionRows)
		input.sections.push_back(ToBladeSection(row));
	if (input.sections.empty())
		throw std::runtime_error("Blade aero-struct file has no section rows: " + path);
	return input;
}

class TowerStructSerializer : public Serializer
{
public:
	TowerStructInput data;

	TowerStructSerializer()
	{
		SetValueFirst(true);
		SetYamlRoot(kTowerYamlRoot);
	}

	explicit TowerStructSerializer(const TowerStructInput &input)
		: data(input)
	{
		SetValueFirst(true);
		SetYamlRoot(kTowerYamlRoot);
	}

protected:
	void SerializeFields() override
	{
		Fields(
			Field("TowerRayleighDamp", data.rayleighDamp),
			Field("TowerStiffTuner", data.stiffTuner),
			Field("TowerMassTuner", data.massTuner),
			Field("TowerBeamType", data.beamType),
			Field("TowerDiscCount", data.discCount));
	}
};

inline TowerStructInput ReadTowerStructInput(const std::string &path)
{
	if (!ZFile::Exists(path))
		throw std::runtime_error("Cannot open tower struct file: " + path);

	TowerStructSerializer reader;
	reader.ReadFile(path);
	TowerStructInput input = reader.data;
	input.inputPath = std::filesystem::absolute(path).lexically_normal();
	input.sectionRows = module_io::ReadRows(reader, path, kTowerYamlRoot, "SectionRows", 5);
	if (input.sectionRows.empty())
		input.sectionRows = module_io::ReadRows(reader, path, kTowerYamlRoot, "SpanFrac", 5);
	if (input.sectionRows.empty())
		input.sectionRows = module_io::ReadRows(reader, path, kTowerYamlRoot, "TowerStructTable", 5);
	if (input.sectionRows.empty())
		throw std::runtime_error("Tower struct file has no section rows: " + path);
	return input;
}

inline std::vector<std::string> BladeSectionHeaders(const std::vector<std::vector<std::string>> &rows)
{
	const auto columns = module_io::MaxColumnCount(rows);
	std::vector<std::string> headers{
		"RadialPos_m", "Chord_m", "Twist_deg", "OffsetY_m", "OffsetX_m",
		"PitchAxisY", "PitchAxisX", "PolarFileID", "RelThickness_pct"};
	const std::vector<std::string> eulerBeamColumns{
		"MassDensity", "EIx", "EIy", "EA", "GJ", "GA", "StructPitch",
		"KsX", "KsY", "RgX", "RgY", "Xcm", "Ycm", "Xce", "Yce", "Xcs", "Ycs"};
	const std::vector<std::string> fpmColumns{
		"Xcb", "Ycb", "Pitch",
		"K11", "K12", "K13", "K14", "K15", "K16",
		"K22", "K23", "K24", "K25", "K26",
		"K33", "K34", "K35", "K36",
		"K44", "K45", "K46",
		"K55", "K56", "K66",
		"M11", "M12", "M13", "M14", "M15", "M16",
		"M22", "M23", "M24", "M25", "M26",
		"M33", "M34", "M35", "M36",
		"M44", "M45", "M46",
		"M55", "M56", "M66"};
	const auto &extra = columns > headers.size() + eulerBeamColumns.size() ? fpmColumns : eulerBeamColumns;
	headers.insert(headers.end(), extra.begin(), extra.end());
	return headers;
}

inline std::vector<std::string> TowerSectionHeaders(const std::vector<std::vector<std::string>> &rows)
{
	(void)rows;
	return {
		"SpanFrac", "MassDensity", "EIx", "EIy", "EA", "GJ", "GA", "StructPitch",
		"KsX", "KsY", "RgX", "RgY", "Xcm", "Ycm", "Xce", "Yce", "Xcs", "Ycs",
		"Diameter", "DragParam"};
}

inline void WriteStrLInputYaml(const StrLInput &input, const std::string &path)
{
	StrLInputSerializer writer(input);
	writer.WriteYamlFile(path);
	writer.AddNode("BladeFiles", input.bladeFiles);
	module_io::AddOutputYamlNodes(writer, input.output);
	writer.SaveYamlFile(path);
}

inline void WriteBladeAeroStructInputYaml(const BladeAeroStructInput &input, const std::string &path)
{
	BladeAeroStructSerializer writer(input);
	writer.WriteYamlFile(path);
	writer.AddNode("BladeRayleighDampAniso", input.rayleighDampAniso);
	module_io::AddStringTableYamlNodes(writer, "SectionRows", BladeSectionHeaders(input.sectionRows), input.sectionRows, 3);
	writer.SaveYamlFile(path);
}

inline void WriteTowerStructInputYaml(const TowerStructInput &input, const std::string &path)
{
	TowerStructSerializer writer(input);
	writer.WriteYamlFile(path);
	module_io::AddStringTableYamlNodes(writer, "SectionRows", TowerSectionHeaders(input.sectionRows), input.sectionRows, 3);
	writer.SaveYamlFile(path);
}

inline void WriteStrLInput(const StrLInput &input,
                           const std::string &path,
                           const std::string &templatePath = "")
{
	if (Serializer::IsYamlPath(path))
	{
		WriteStrLInputYaml(input, path);
		return;
	}
	StrLInputSerializer writer(input);
	if (!templatePath.empty() && ZFile::Exists(templatePath))
		writer.LoadTextFile(templatePath);
	writer.WriteTextFile(path);
}

inline void ConvertStrLInput(const std::string &inputPath,
                             const std::string &outputPath,
                             const std::string &templatePath = "")
{
	WriteStrLInput(ReadStrLInput(inputPath), outputPath, templatePath);
}
} // namespace strl_io_detail

using strl_io_detail::ConvertStrLInput;
using strl_io_detail::ReadBladeAeroStructInput;
using strl_io_detail::ReadStrLInput;
using strl_io_detail::ReadTowerStructInput;
using strl_io_detail::WriteBladeAeroStructInputYaml;
using strl_io_detail::WriteStrLInput;
using strl_io_detail::WriteTowerStructInputYaml;
