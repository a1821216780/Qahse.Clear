#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include "IO/ModuleIO.hpp"
#include "IO/Serializer.hpp"
#include "IO/ZFile.hpp"
#include "SimL/SimL_Type.hpp"

namespace siml_io_detail
{
inline constexpr const char *kYamlRoot = "Qahse.SimL";

class SimLInputSerializer : public Serializer
{
public:
	SimLInput data;

	SimLInputSerializer()
	{
		SetValueFirst(true);
		SetYamlRoot(kYamlRoot);
	}

	explicit SimLInputSerializer(const SimLInput &input)
		: data(input)
	{
		SetValueFirst(true);
		SetYamlRoot(kYamlRoot);
	}

protected:
	void SerializeFields() override
	{
		Fields(
			Field("TMax", data.tMax),
			Field("DT", data.dt),
			Field("AfShowlog", data.afShowLog),
			Field("Dtput", data.dtPut),
			Field("SimulateType", data.simulateType));
		ReadOrWriteAny({"RamupTime", "RampUpTime"}, data.rampUpTime);
		Fields(
			Field("WTType", data.wtType),
			Field("Solver", data.solver),
			Field("Linearization", data.linearization),
			Field("Gravity", data.gravity));
		ReadOrWriteAny({"AirDens", "AirDensity"}, data.airDensity);
		ReadOrWriteAny({"WtrDens", "WaterDensity"}, data.waterDensity);
		Fields(
			Field("KinVisc", data.kinVisc),
			Field("SpdSound", data.speedOfSound),
			Field("StrFile", data.strFile),
			Field("WindFile", data.windFile),
			Field("AeroFile", data.aeroFile),
			Field("ControlFile", data.controlFile),
			Field("HydroLFile", data.hydroLFile),
			Field("SubFEMLFile", data.subFEMLFile),
			Field("MlinLFile", data.mlinLFile),
			Field("AfVTK", data.vtk.enabled),
			Field("DT_VTK", data.vtk.dt),
			Field("VTK_type", data.vtk.type),
			Field("VTK_SideNum", data.vtk.sideNum),
			Field("DT_Out", data.output.dtOut),
			Field("TStart", data.output.tStart),
			Field("OutType", data.output.outType),
			Field("SumPath", data.output.sumPath));
	}
};

inline void ResolvePaths(SimLInput &input)
{
	const std::string base = input.inputPath.string();
	module_io::ResolveIfSet(base, input.strFile);
	module_io::ResolveIfSet(base, input.windFile);
	module_io::ResolveIfSet(base, input.aeroFile);
	module_io::ResolveIfSet(base, input.controlFile);
	module_io::ResolveIfSet(base, input.hydroLFile);
	module_io::ResolveIfSet(base, input.subFEMLFile);
	module_io::ResolveIfSet(base, input.mlinLFile);
	module_io::ResolveOutputPath(base, input.output);
}

inline void RequirePath(const std::string &path, const std::string &key)
{
	if (!module_io::FileExistsOrEmpty(path))
		throw std::runtime_error("SimL " + key + " does not exist: " + path);
}

inline void Validate(const SimLInput &input)
{
	RequirePath(input.strFile, "StrFile");
	RequirePath(input.windFile, "WindFile");
	RequirePath(input.aeroFile, "AeroFile");
	RequirePath(input.controlFile, "ControlFile");
	if (input.wtType == 2)
		RequirePath(input.hydroLFile, "HydroLFile");
	if (input.wtType == 1)
		RequirePath(input.subFEMLFile, "SubFEMLFile");
	if (input.linearization)
		RequirePath(input.mlinLFile, "MlinLFile");
}

inline SimLInput ReadSimLInput(const std::string &path)
{
	if (!ZFile::Exists(path))
		throw std::runtime_error("Cannot open SimL input file: " + path);

	SimLInputSerializer reader;
	reader.ReadFile(path);
	SimLInput input = reader.data;
	input.inputPath = std::filesystem::absolute(path).lexically_normal();
	module_io::ReadOutputConfig(reader, path, kYamlRoot, input.output);
	ResolvePaths(input);
	Validate(input);
	return input;
}

inline void WriteSimLInputYaml(const SimLInput &input, const std::string &path)
{
	SimLInputSerializer writer(input);
	writer.WriteYamlFile(path);
	module_io::AddOutputYamlNodes(writer, input.output);
	writer.SaveYamlFile(path);
}

inline void WriteSimLInput(const SimLInput &input,
                           const std::string &path,
                           const std::string &templatePath = "")
{
	if (Serializer::IsYamlPath(path))
	{
		WriteSimLInputYaml(input, path);
		return;
	}
	SimLInputSerializer writer(input);
	if (!templatePath.empty() && ZFile::Exists(templatePath))
		writer.LoadTextFile(templatePath);
	writer.WriteTextFile(path);
}

inline void ConvertSimLInput(const std::string &inputPath,
                             const std::string &outputPath,
                             const std::string &templatePath = "")
{
	WriteSimLInput(ReadSimLInput(inputPath), outputPath, templatePath);
}
} // namespace siml_io_detail

using siml_io_detail::ConvertSimLInput;
using siml_io_detail::ReadSimLInput;
using siml_io_detail::WriteSimLInput;
