#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "../SimWind_Type.hpp"
#include "SimWind_UserData_IO.hpp"
#include "../../IO/LocaleString.hpp"
#include "../../IO/Serializer.hpp"
#include "../../IO/ZFile.hpp"
#include "../../IO/ZPath.hpp"
#include "../../IO/ZString.hpp"

namespace simwind_io_detail
{
inline constexpr const char *kYamlRoot = "Qahse.SimWind";

inline void RequireFile(const std::string &path)
{
	if (!ZFile::Exists(path))
		throw std::runtime_error(std::string(L_IO_FileNotFound) + ": " + path);
}

inline void ResolveIfSet(const std::string &baseFilePath, std::string &path)
{
	if (!path.empty())
		path = ZPath::ResolvePath(baseFilePath, path);
}

inline void ResolvePaths(SimWindInput &input, const std::string &baseFilePath)
{
	ResolveIfSet(baseFilePath, input.userShearFile);
	ResolveIfSet(baseFilePath, input.userTurbFile);
	ResolveIfSet(baseFilePath, input.wndFilePath);
	ResolveIfSet(baseFilePath, input.batchExcelPath);

	if (!input.savePath.empty())
	{
		input.savePath = ZPath::ResolvePath(baseFilePath, input.savePath);
		std::filesystem::create_directories(input.savePath);
	}

	if (!input.batchOutputDir.empty())
		input.batchOutputDir = ZPath::ResolvePath(baseFilePath, input.batchOutputDir);
	else if (input.mode == Mode::BATCH)
		input.batchOutputDir = (std::filesystem::path(baseFilePath).parent_path() / "batch_output").string();
}

inline std::string CohModelToText(CohModel value)
{
	switch (value)
	{
	case CohModel::IEC: return "IEC";
	case CohModel::GENERAL: return "GENERAL";
	case CohModel::NONE: return "NONE";
	case CohModel::API: return "API";
	case CohModel::DEFAULT_COH: return "default";
	default: return "default";
	}
}

inline CohModel ParseCohModel(std::string token, CohModel defaultValue = CohModel::DEFAULT_COH)
{
	token = ZString::Trim(token);
	if (token.empty())
		return defaultValue;

	std::string upper = token;
	std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char ch) {
		return static_cast<char>(std::toupper(ch));
	});
	if (upper == "DEFAULT" || upper == "DEFAULT_COH")
		return CohModel::DEFAULT_COH;
	if (upper == "IEC" || token == "0")
		return CohModel::IEC;
	if (upper == "GENERAL" || token == "1")
		return CohModel::GENERAL;
	if (upper == "NONE" || token == "3")
		return CohModel::NONE;
	if (upper == "API" || token == "4")
		return CohModel::API;

	try { return ZString::StringToEnum<CohModel>(token, true); }
	catch (...) { return defaultValue; }
}

class SimWindInputSerializer : public Serializer
{
public:
	SimWindInput data;

	SimWindInputSerializer()
	{
		SetValueFirst(true);
		SetYamlRoot(kYamlRoot);
	}

	explicit SimWindInputSerializer(const SimWindInput &input)
		: data(input)
	{
		SetValueFirst(true);
		SetYamlRoot(kYamlRoot);
	}

protected:
	void SerializeFields() override
	{
		Fields(
			Field("Mode", data.mode),
			Field("TurbModel", data.turbModel),
			Field("WindModel", data.windModel),
			Field("CalWu", data.calWu),
			Field("CalWv", data.calWv),
			Field("CalWw", data.calWw),
			Field("WrBlwnd", data.wrBlwnd),
			Field("WrTrbts", data.wrTrbts),
			Field("WrTrwnd", data.wrTrwnd),
			Field("IECStandard", data.iecEdition),
			Field("IECClass", data.turbineClass),
			Field("TurbulenceClass", data.turbClass),
			Field("VRef", data.vRef),
			Field("RotorDiameter", data.rotorDiameter),
			Field("MeanWindSpeed", data.meanWindSpeed),
			Field("HubHt", data.hubHeight),
			Field("RefHt", data.refHeight),
			Field("ShearType", data.shearType),
			Field("WindProfileType", data.windProfileType),
			Field("PLExp", data.shearExp),
			Field("Z0", data.roughness),
			Field("Roughness", data.roughness));

		ReadOrWriteAny({"HFlowAng", "HorAngle"}, data.horAngle);
		ReadOrWriteAny({"VFlowAng", "VertAngle"}, data.vertAngle);

		Fields(
			Field("UserShearFile", data.userShearFile),
			Field("TurbIntensity", data.turbIntensity),
			Field("RandSeed", data.turbSeed),
			Field("NumPointY", data.gridPtsY),
			Field("NumPointZ", data.gridPtsZ),
			Field("LenWidthY", data.fieldDimY),
			Field("LenHeightZ", data.fieldDimZ),
			Field("WindDuration", data.simTime),
			Field("TimeStep", data.timeStep),
			Field("CycleWind", data.cycleWind),
			Field("GenMethod", data.genMethod),
			Field("UseFFT", data.useFFT),
			Field("InterpMethod", data.interpMethod),
			Field("UserTurbFile", data.userTurbFile),
			Field("UseIECSimmga", data.useIECSimmga),
			Field("ScaleIEC", data.scaleIEC),
			Field("ETMc", data.etmC),
			Field("UsableTime", data.usableTime),
			Field("AnalysisTime", data.analysisTime),
			Field("Rich_No", data.richardson),
			Field("UStar", data.uStar),
			Field("ZL", data.zOverL),
			Field("ZI", data.mixingLayerDepth),
			Field("PC_UW", data.reynoldsUW),
			Field("PC_UV", data.reynoldsUV),
			Field("PC_VW", data.reynoldsVW),
			Field("VxLu", data.vkLu),
			Field("VxLv", data.vkLv),
			Field("VxLw", data.vkLw),
			Field("VyLu", data.vyLu),
			Field("VyLv", data.vyLv),
			Field("VyLw", data.vyLw),
			Field("VzLu", data.vzLu),
			Field("VzLv", data.vzLv),
			Field("VzLw", data.vzLw),
			Field("Latitude", data.latitude),
			Field("TI_u", data.tiU),
			Field("TI_v", data.tiV),
			Field("TI_w", data.tiW),
			Field("MannAlphaEps", data.mannAlphaEps),
			Field("MannScalelength", data.mannLength),
			Field("MannGamma", data.mannGamma),
			Field("MannMaxL", data.mannMaxL),
			Field("MannNx", data.mannNx),
			Field("MannNy", data.mannNy),
			Field("MannNz", data.mannNz));

		CohField("CohMod1", data.cohMod1);
		CohField("CohMod2", data.cohMod2);
		CohField("CohMod3", data.cohMod3);

		Fields(
			Field("CohDecayU", data.cohDecayU),
			Field("CohDecayV", data.cohDecayV),
			Field("CohDecayW", data.cohDecayW),
			Field("CohScaleB", data.cohScaleB),
			Field("CohExp", data.cohExp),
			Field("AllowCohApprox", data.allowCohApprox),
			Field("EWMType", data.ewmType),
			Field("GustPeriod", data.gustPeriod),
			Field("EventStart", data.eventStart),
			Field("EventSign", data.eventSign),
			Field("EcdVcog", data.ecdVcog),
			Field("TurWindFile", data.wndFilePath),
			Field("WndFormat", data.wndFormat),
			Field("WrWndPath", data.savePath),
			Field("WrWndName", data.saveName),
			Field("SumPrint", data.sumPrint),
			Field("BatchExcel", data.batchExcelPath),
			Field("BatchSheet", data.batchSheetName),
			Field("BatchOutputDir", data.batchOutputDir),
			Field("BatchThreads", data.batchThreads),
			Field("BatchLauncher", data.batchLauncher),
			Field("BatchValidateOnly", data.batchValidateOnly));
	}

private:
	void CohField(const char *key, CohModel &value)
	{
		std::string text = CohModelToText(value);
		ReadOrWrite(key, text);
		if (IsReadMode())
			value = ParseCohModel(text, value);
	}
};
} // namespace simwind_io_detail

inline SimWindInput ReadSimWindInput(const std::string &path)
{
	simwind_io_detail::RequireFile(path);

	simwind_io_detail::SimWindInputSerializer serializer;
	serializer.ReadFile(path);
	simwind_io_detail::ResolvePaths(serializer.data, path);
	return serializer.data;
}

inline void WriteSimWindInput(const SimWindInput &input,
                              const std::string &path,
                              const std::string &templatePath = "")
{
	simwind_io_detail::SimWindInputSerializer serializer(input);
	if (!Serializer::IsYamlPath(path) && !templatePath.empty() && ZFile::Exists(templatePath))
		serializer.LoadTextFile(templatePath);
	serializer.data = input;
	serializer.WriteFile(path);
}

inline void ConvertSimWindInput(const std::string &inputPath,
                                const std::string &outputPath,
                                const std::string &templatePath = "")
{
	WriteSimWindInput(ReadSimWindInput(inputPath), outputPath, templatePath);
}
