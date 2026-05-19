#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "AeroL/AeroL_Type.hpp"
#include "IO/ModuleIO.hpp"
#include "IO/Serializer.hpp"
#include "IO/ZFile.hpp"
#include "IO/ZPath.hpp"

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
	for (const auto &file : input.airfoils.files)
		if (!module_io::FileExistsOrEmpty(file))
			throw std::runtime_error("AeroL airfoil file does not exist: " + file);
	if (!module_io::FileExistsOrEmpty(input.bladeAeroStructFile))
		throw std::runtime_error("AeroL BladeAeroStructFile does not exist: " + input.bladeAeroStructFile);
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
	ResolvePaths(input);
	Validate(input);
	return input;
}

inline void WriteAeroLInputYaml(const AeroLInput &input, const std::string &path)
{
	AeroLInputSerializer writer(input);
	writer.WriteYamlFile(path);
	writer.AddNode("AFNames", input.airfoils.files);
	writer.AddNode("IagParams", input.iagParams);
	module_io::AddOutputYamlNodes(writer, input.output);
	writer.SaveYamlFile(path);
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
