#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "AeroL/Airfoil.hpp"
#include "AeroL/AeroL_Type.hpp"
#include "IO/ModuleIO.hpp"
#include "IO/Serializer.hpp"
#include "IO/ZFile.hpp"
#include "IO/ZPath.hpp"
#include "StrL/IO/StrL_IO_Subs.hpp"

namespace aerol_io_detail
{
inline constexpr const char *kYamlRoot = "Qahse.AeroL";

inline std::vector<std::string> ReadStringListAfter(const Serializer &reader,
                                                    const std::string &key,
                                                    int count)
{
	const auto lines = reader.RawLines();
	const int index = module_io::FindLineContaining(lines, key);
	if (index < 0)
		return {};

	std::vector<std::string> values;
	for (std::size_t i = static_cast<std::size_t>(index + 1); i < lines.size(); ++i)
	{
		const auto text = ZString::Trim(lines[i]);
		if (module_io::IsSectionBoundary(text))
			break;
		if (module_io::IsCommentOrEmpty(text))
			continue;
		auto tokens = module_io::TokenizeLoose(text);
		if (tokens.empty())
			continue;
		values.push_back(tokens.front());
		if (count > 0 && values.size() >= static_cast<std::size_t>(count))
			break;
	}
	return values;
}

class AeroLInputSerializer : public Serializer
{
public:
	AeroLInput data;

	AeroLInputSerializer()
	{
		SetValueFirst(true);
		SetYamlRoot(kYamlRoot);
	}

	explicit AeroLInputSerializer(const AeroLInput &input)
		: data(input)
	{
		SetValueFirst(true);
		SetYamlRoot(kYamlRoot);
	}

protected:
	void SerializeFields() override
	{
		Fields(
			Field("ApOfMb", data.apOfMb),
			Field("RotorType", data.rotorType),
			Field("HubRad", data.hubRadius));
		ReadOrWriteAny({"BladeNum", "Bldnum", "BldNum"}, data.bladeNum);
		ReadOrWriteAny({"CutInWindSpeed", "CutInWSpeed"}, data.cutInWindSpeed);
		ReadOrWriteAny({"CutOutWindSpeed", "CutOutWSpeed"}, data.cutOutWindSpeed);
		Fields(
			Field("RatedPower", data.ratedPower),
			Field("RatedRotorSpeed", data.ratedRotorSpeed));
		ReadOrWriteAny({"AirDens", "AirDensity"}, data.airDensity);
		Fields(
			Field("KinVisc", data.kinVisc),
			Field("SpdSound", data.speedOfSound));
		ReadOrWriteAny({"NumAFfiles", "NumAFiles"}, data.airfoils.count);
		Fields(
			Field("InterpOrd", data.airfoils.interpolationOrder),
			Field("BladeAeroStructFile", data.bladeAeroStructFile),
			Field("UnsteadyAero", data.unsteadyAero),
			Field("TwoPointLiftDrag", data.twoPointLiftDrag),
			Field("Himmelskamp", data.himmelskamp),
			Field("TowerShadow", data.towerShadow),
			Field("TowerDrag", data.towerDrag));
		ReadOrWriteAny({"DynStallType", "AFAeroMod"}, data.dynamicStallType);
		Fields(
			Field("TfOye", data.tfOye),
			Field("AmGb", data.amGb),
			Field("TfAte", data.tfAte),
			Field("TpAte", data.tpAte),
			Field("WakeType", data.wakeType),
			Field("WakeIntType", data.wakeIntType),
			Field("WakeRollup", data.wakeRollup),
			Field("TrailingVort", data.trailingVort),
			Field("ShedVort", data.shedVort),
			Field("ConvectionType", data.convectionType),
			Field("WakeRelaxation", data.wakeRelaxation),
			Field("FirstWakeRow", data.firstWakeRow),
			Field("MaxWakeSize", data.maxWakeSize),
			Field("MaxWakeDist", data.maxWakeDist),
			Field("WakeReduction", data.wakeReduction),
			Field("WakeLengthType", data.wakeLengthType),
			Field("ConversionLength", data.conversionLength),
			Field("NearWakeLength", data.nearWakeLength),
			Field("Zone1Length", data.zone1Length),
			Field("Zone2Length", data.zone2Length),
			Field("Zone3Length", data.zone3Length),
			Field("Zone1Factor", data.zone1Factor),
			Field("Zone2Factor", data.zone2Factor),
			Field("Zone3Factor", data.zone3Factor),
			Field("Zone1FactorS", data.zone1FactorS),
			Field("Zone2FactorS", data.zone2FactorS),
			Field("Zone3FactorS", data.zone3FactorS),
			Field("BoundCoreRadius", data.boundCoreRadius),
			Field("WakeCoreRadius", data.wakeCoreRadius),
			Field("VortexViscosity", data.vortexViscosity),
			Field("VortexStrain", data.vortexStrain),
			Field("MaxStrain", data.maxStrain),
			Field("GammaEpsilon", data.gammaEpsilon),
			Field("GammaIterations", data.gammaIterations),
			Field("PolarDisc", data.polarDisc),
			Field("BemTipLoss", data.bemTipLoss),
			Field("BemSpeedUp", data.bemSpeedUp),
			Field("MinLambda", data.minLambda),
			Field("MaxLambda", data.maxLambda),
			Field("LambdaStep", data.lambdaStep),
			Field("MinPitch", data.minPitch),
			Field("MaxPitch", data.maxPitch),
			Field("PitchStep", data.pitchStep),
			Field("CpResultFilePath", data.cpResultFilePath),
			Field("MinWindSpeed", data.minWindSpeed),
			Field("MaxWindSpeed", data.maxWindSpeed),
			Field("WindSpeedStep", data.windSpeedStep),
			Field("OrigPitch", data.origPitch),
			Field("OmegaMin", data.omegaMin),
			Field("GenEfficiency", data.genEfficiency),
			Field("PitchUp", data.pitchUp),
			Field("PitchDown", data.pitchDown));
		ReadOrWriteAny({"IfPitch", "ifpitch"}, data.ifPitch);
		ReadOrWriteAny({"FixedPitch", "Fixed_pitch"}, data.fixedPitch);
		ReadOrWriteAny({"FixedRotationalSpeed", "Fixed_rotationalspeed"}, data.fixedRotationalSpeed);
		Fields(
			Field("PowerCurveResultFilePath", data.powerCurveResultFilePath),
			Field("SumPrint", data.output.sumPrint),
			Field("AfSpanput", data.output.afSpanput),
			Field("SumPath", data.output.sumPath));
		ReadOrWriteAny({"NBlOuts", "NumBladeOutNodes"}, data.output.blade.count);
		ReadOrWriteAny({"NTwOuts", "NumTowerOutNodes"}, data.output.tower.count);
	}
};

inline void ResolvePaths(AeroLInput &input)
{
	const std::string base = input.inputPath.string();
	for (auto &file : input.airfoils.files)
		module_io::ResolveIfSet(base, file);
	module_io::ResolveIfSetWithExtension(base, input.bladeAeroStructFile, ".str");
	module_io::ResolveIfSet(base, input.cpResultFilePath);
	module_io::ResolveIfSet(base, input.powerCurveResultFilePath);
}

inline void Validate(const AeroLInput &input)
{
	if (input.airfoils.count > 0 &&
	    input.airfoils.files.size() != static_cast<std::size_t>(input.airfoils.count))
		throw std::runtime_error("AeroL NumAFfiles does not match AFNames row count");
	const bool hasEmbeddedAirfoils = !input.airfoilData.empty();
	for (const auto &file : input.airfoils.files)
		if (!hasEmbeddedAirfoils && !module_io::FileExistsOrEmpty(file))
			throw std::runtime_error("AeroL airfoil file does not exist: " + file);
	const bool hasEmbeddedBlade = module_io::YamlHasKey(input.inputPath.string(), "Qahse.BladeAeroStruct");
	if (!hasEmbeddedBlade && !module_io::FileExistsOrEmpty(input.bladeAeroStructFile))
		throw std::runtime_error("AeroL BladeAeroStructFile does not exist: " + input.bladeAeroStructFile);
}

inline std::optional<BladeAeroStructInput> TryReadBladeAeroStructFromPath(const std::string &path)
{
	if (path.empty() || !std::filesystem::is_regular_file(path))
		return std::nullopt;
	std::error_code ec;
	if (std::filesystem::file_size(path, ec) == 0 || ec)
		return std::nullopt;
	try
	{
		return ReadBladeAeroStructInput(path);
	}
	catch (...)
	{
		return std::nullopt;
	}
}

inline std::vector<std::vector<double>> PolarRows(const std::vector<AirfoilPolarPoint> &polar)
{
	std::vector<std::vector<double>> rows;
	rows.reserve(polar.size());
	for (const auto &point : polar)
		rows.push_back({point.alphaDeg, point.cl, point.cd, point.cm});
	return rows;
}

inline std::vector<AirfoilPolarPoint> PolarFromMatrix(const Eigen::MatrixXd &matrix)
{
	std::vector<AirfoilPolarPoint> polar;
	polar.reserve(static_cast<std::size_t>(matrix.rows()));
	for (Eigen::Index r = 0; r < matrix.rows(); ++r)
	{
		if (matrix.cols() >= 4)
			polar.push_back({matrix(r, 0), matrix(r, 1), matrix(r, 2), matrix(r, 3)});
	}
	return polar;
}

inline std::vector<std::vector<double>> CoordinateRows(const std::vector<AirfoilCoordinate> &coordinates)
{
	std::vector<std::vector<double>> rows;
	rows.reserve(coordinates.size());
	for (const auto &coord : coordinates)
		rows.push_back({coord.x, coord.y});
	return rows;
}

inline std::vector<AirfoilCoordinate> CoordinatesFromMatrix(const Eigen::MatrixXd &matrix)
{
	std::vector<AirfoilCoordinate> coordinates;
	coordinates.reserve(static_cast<std::size_t>(matrix.rows()));
	for (Eigen::Index r = 0; r < matrix.rows(); ++r)
	{
		if (matrix.cols() >= 2)
			coordinates.push_back({matrix(r, 0), matrix(r, 1)});
	}
	return coordinates;
}

inline void AddEmbeddedAirfoilData(Serializer &writer, const std::vector<AirfoilData> &airfoils)
{
	if (airfoils.empty())
		return;
	writer.AddNode("AirfoilDataCount", static_cast<int>(airfoils.size()));
	for (std::size_t i = 0; i < airfoils.size(); ++i)
	{
		const auto &airfoil = airfoils[i];
		const std::string root = "AirfoilData." + std::to_string(i);
		writer.AddNode(root + ".InputPath", airfoil.inputPath.string());
		writer.AddNode(root + ".PolarName", airfoil.polarName);
		writer.AddNode(root + ".AirfoilName", airfoil.airfoilName);
		writer.AddNode(root + ".Thickness", airfoil.thickness);
		writer.AddNode(root + ".ReynoldsNumber", airfoil.reynoldsNumber);
		writer.AddNode(root + ".ReynoldsNumbers", airfoil.reynoldsNumbers);
		writer.AddNode(root + ".PitchMomentCenter", airfoil.pitchMomentCenter);
		writer.AddNode(root + ".GeometryFile", airfoil.geometryFile);
		writer.AddNode(root + ".InterpolationOrder", airfoil.interpolationOrder);
		writer.AddNode(root + ".DeclaredPolarCount", airfoil.declaredPolarCount);
		module_io::AddNumericTableYamlNodes(writer, root + ".PolarRows",
			{"Alpha_deg", "Cl", "Cd", "Cm"}, PolarRows(airfoil.polar), 5);
		writer.AddNode(root + ".PolarSetCount", static_cast<int>(airfoil.polarSets.size()));
		for (std::size_t set = 0; set < airfoil.polarSets.size(); ++set)
			module_io::AddNumericTableYamlNodes(writer, root + ".PolarSets." + std::to_string(set),
				{"Alpha_deg", "Cl", "Cd", "Cm"}, PolarRows(airfoil.polarSets[set]), 6);

		writer.AddNode(root + ".Geometry.InputPath", airfoil.geometry.inputPath.string());
		writer.AddNode(root + ".Geometry.Name", airfoil.geometry.name);
		writer.AddNode(root + ".Geometry.DeclaredCoordinateCount", airfoil.geometry.declaredCoordinateCount);
		writer.AddNode(root + ".Geometry.HasExplicitReference", airfoil.geometry.hasExplicitReference);
		writer.AddNode(root + ".Geometry.Reference", std::vector<double>{airfoil.geometry.reference.x, airfoil.geometry.reference.y});
		module_io::AddNumericTableYamlNodes(writer, root + ".Geometry.Coordinates",
			{"X", "Y"}, CoordinateRows(airfoil.geometry.coordinates), 6);
	}
}

inline std::vector<AirfoilData> ReadEmbeddedAirfoilData(Serializer &reader, const std::string &path)
{
	const int count = reader.Read<int>("AirfoilDataCount", 0);
	std::vector<AirfoilData> airfoils;
	airfoils.reserve(static_cast<std::size_t>(std::max(count, 0)));
	for (int i = 0; i < count; ++i)
	{
		const std::string root = "AirfoilData." + std::to_string(i);
		AirfoilData airfoil;
		airfoil.inputPath = reader.Read<std::string>(root + ".InputPath", "");
		airfoil.polarName = reader.Read<std::string>(root + ".PolarName", "");
		airfoil.airfoilName = reader.Read<std::string>(root + ".AirfoilName", "");
		airfoil.thickness = reader.Read<double>(root + ".Thickness", 0.0);
		airfoil.reynoldsNumber = reader.Read<double>(root + ".ReynoldsNumber", 0.0);
		airfoil.reynoldsNumbers = module_io::ReadYamlDoubleArray(path, kYamlRoot, root + ".ReynoldsNumbers");
		airfoil.pitchMomentCenter = reader.Read<double>(root + ".PitchMomentCenter", 0.0);
		airfoil.geometryFile = reader.Read<std::string>(root + ".GeometryFile", "");
		airfoil.interpolationOrder = reader.Read<int>(root + ".InterpolationOrder", 1);
		airfoil.declaredPolarCount = reader.Read<int>(root + ".DeclaredPolarCount", 0);
		airfoil.polar = PolarFromMatrix(reader.ReadMatrix(root + ".PolarRows"));

		const int setCount = reader.Read<int>(root + ".PolarSetCount", 0);
		airfoil.polarSets.reserve(static_cast<std::size_t>(std::max(setCount, 0)));
		for (int set = 0; set < setCount; ++set)
		{
			auto rows = PolarFromMatrix(reader.ReadMatrix(root + ".PolarSets." + std::to_string(set)));
			if (!rows.empty())
				airfoil.polarSets.push_back(std::move(rows));
		}
		if (airfoil.polarSets.empty() && !airfoil.polar.empty())
			airfoil.polarSets.push_back(airfoil.polar);

		airfoil.geometry.inputPath = reader.Read<std::string>(root + ".Geometry.InputPath", "");
		airfoil.geometry.name = reader.Read<std::string>(root + ".Geometry.Name", "");
		airfoil.geometry.declaredCoordinateCount = reader.Read<int>(root + ".Geometry.DeclaredCoordinateCount", 0);
		airfoil.geometry.hasExplicitReference = reader.Read<bool>(root + ".Geometry.HasExplicitReference", false);
		const auto reference = module_io::ReadYamlDoubleArray(path, kYamlRoot, root + ".Geometry.Reference");
		if (reference.size() >= 2)
			airfoil.geometry.reference = {reference[0], reference[1]};
		airfoil.geometry.coordinates = CoordinatesFromMatrix(reader.ReadMatrix(root + ".Geometry.Coordinates"));

		BuildAirfoilLookupTables(airfoil);
		airfoils.push_back(std::move(airfoil));
	}
	return airfoils;
}

inline AeroLInput ReadAeroLInput(const std::string &path)
{
	if (!ZFile::Exists(path))
		throw std::runtime_error("Cannot open AeroL input file: " + path);

	AeroLInputSerializer reader;
	reader.ReadFile(path);

	AeroLInput input = reader.data;
	input.inputPath = std::filesystem::absolute(path).lexically_normal();
	if (reader.IsYaml())
		input.airfoils.files = module_io::ReadYamlStringArray(path, kYamlRoot, "AFNames");
	else
		input.airfoils.files = ReadStringListAfter(reader, "AFNames", input.airfoils.count);
	if (input.airfoils.count <= 0)
		input.airfoils.count = static_cast<int>(input.airfoils.files.size());
	input.iagParams = module_io::ReadDoubleList(reader, path, kYamlRoot, {"IagParams"});
	module_io::ReadOutputConfig(reader, path, kYamlRoot, input.output);
	if (reader.IsYaml())
		input.airfoilData = ReadEmbeddedAirfoilData(reader, path);
	ResolvePaths(input);
	Validate(input);
	if (reader.IsYaml() && module_io::YamlHasKey(path, strl_io_detail::kBladeYamlRoot))
		input.bladeAeroStruct = ReadBladeAeroStructInput(path);
	else
		input.bladeAeroStruct = TryReadBladeAeroStructFromPath(input.bladeAeroStructFile);
	return input;
}

inline void AddBladeAeroStructYamlRoot(const AeroLInput &input, const std::string &path)
{
	std::optional<BladeAeroStructInput> blade = input.bladeAeroStruct;
	if (!blade)
		blade = TryReadBladeAeroStructFromPath(input.bladeAeroStructFile);
	if (!blade)
		return;

	const auto outputPath = std::filesystem::path(path);
	const auto tempPath = outputPath.parent_path() /
	                      (outputPath.filename().string() + ".BladeAeroStruct.tmp.yml");
	WriteBladeAeroStructInputYaml(*blade, tempPath.string());
	YML yaml(path, false);
	yaml.AddYAML(YML(tempPath.string(), false));
	yaml.save(path);
	std::error_code ec;
	std::filesystem::remove(tempPath, ec);
}

inline void WriteAeroLInputYaml(const AeroLInput &input, const std::string &path)
{
	AeroLInputSerializer writer(input);
	writer.WriteYamlFile(path);
	writer.AddNode("AFNames", input.airfoils.files);
	writer.AddNode("IagParams", input.iagParams);
	AddEmbeddedAirfoilData(writer, input.airfoilData);
	module_io::AddOutputYamlNodes(writer, input.output);
	writer.SaveYamlFile(path);
	AddBladeAeroStructYamlRoot(input, path);
}

inline void WriteAeroLInput(const AeroLInput &input,
                            const std::string &path,
                            const std::string &templatePath = "")
{
	if (Serializer::IsYamlPath(path))
	{
		WriteAeroLInputYaml(input, path);
		return;
	}

	AeroLInputSerializer writer(input);
	if (!templatePath.empty() && ZFile::Exists(templatePath))
		writer.LoadTextFile(templatePath);
	writer.WriteTextFile(path);
}

inline void ConvertAeroLInput(const std::string &inputPath,
                              const std::string &outputPath,
                              const std::string &templatePath = "")
{
	WriteAeroLInput(ReadAeroLInput(inputPath), outputPath, templatePath);
}
} // namespace aerol_io_detail

using aerol_io_detail::ConvertAeroLInput;
using aerol_io_detail::ReadAeroLInput;
using aerol_io_detail::WriteAeroLInput;
