#include "WindL/WindL.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "SiMwind/IO/SimWind_IO_Subs.hpp"
#include "SiMwind/IO/LocaleString_SimWind.hpp"

namespace
{
std::string FormatSeconds(double value)
{
	std::ostringstream out;
	out << std::fixed << std::setprecision(2) << value;
	return out.str();
}

void ValidateImportInput(const SimWindInput &input)
{
	if (input.wndFilePath.empty())
		throw std::runtime_error("WindL import requires TurWindFile");

	const std::filesystem::path path(input.wndFilePath);
	if (!std::filesystem::is_regular_file(path))
		throw std::runtime_error("The import wind file does not exist: " + path.string());

	auto ext = path.extension().string();
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
	if (ext != ".bts" && ext != ".wnd")
		throw std::runtime_error("WindL import supports only .bts and .wnd wind files");
}

std::filesystem::path ImportSummaryBasePath(const SimWindInput &input)
{
	if (!input.savePath.empty() && !input.saveName.empty())
		return std::filesystem::path(input.savePath) / input.saveName;

	const std::filesystem::path source(input.wndFilePath);
	if (!source.empty())
		return source.parent_path() / (source.stem().string() + "_import");

	return std::filesystem::current_path() / "import_wind";
}

void WriteImportSummary(const SimWindInput &input, const ::WindField &field, const std::filesystem::path &path)
{
	std::ofstream out(path);
	if (!out)
		throw std::runtime_error(std::string(L_WIND_CannotOpenSUM) + ": " + path.string());

	out << L_SUM_Title << "\n";
	out << L_SUM_Separator << "\n\n";
	out << std::setprecision(10);
	out << "ImportSource: " << field.sourcePath.string() << "\n";
	out << "ImportFormat: ";
	switch (field.wndFormat)
	{
	case WndFormat::TURBSIM_BTS: out << "TURBSIM_BTS"; break;
	case WndFormat::TURBSIM_WND: out << "TURBSIM_WND"; break;
	case WndFormat::BLADED_WND: out << "BLADED_WND"; break;
	}
	out << "\n";
	out << "UsedCompanionSummary: " << (field.usedCompanionSummary ? "true" : "false") << "\n\n";

	out << L_SUM_Grid << "\n";
	out << "  NumPointY: " << field.ny << "\n";
	out << "  NumPointZ: " << field.nz << "\n";
	out << "  LenWidthY: " << field.fieldDimY << " m\n";
	out << "  LenHeightZ: " << field.fieldDimZ << " m\n";
	out << "  Zbottom: " << field.zBottom << " m\n";
	out << "  TimeStep: " << field.dt << " s\n";
	out << "  NumSteps: " << field.nSteps << "\n";
	out << "  HubHt: " << field.hubHeight << " m\n";
	out << "  MeanWindSpeed: " << field.meanWindSpeed << " m/s\n\n";

	out << L_SUM_InputKeywordStatus << "\n";
	out << "  CycleWind: sampling-time boundary behavior\n";
	out << "  InterpMethod: imported-field sampling interpolation method\n";
	out << "  TurWindFile: import source path\n";
	out << "  WndFormat: .wnd import decoder selector\n\n";

	out << L_SUM_Statistics << "\n";
	static const char *names[3] = {"u", "v", "w"};
	for (int comp = 0; comp < 3; ++comp)
	{
		out << "  " << names[comp] << ": mean=" << field.mean[static_cast<std::size_t>(comp)]
		    << " sigma=" << field.sigma[static_cast<std::size_t>(comp)]
		    << " TI=" << 100.0 * field.turbulenceIntensity[static_cast<std::size_t>(comp)] << "%\n";
	}

	if (!field.warnings.empty())
	{
		out << "\n" << L_SUM_Warnings << "\n";
		for (const auto &warning : field.warnings)
			out << "  - " << warning << "\n";
	}
}
} // namespace

void WindL::ValidateImportInputOnly(const SimWindInput &input)
{
	ValidateImportInput(input);
}

::WindField WindL::Import(const SimWindInput &input, WindLProgressCallback progress)
{
	ValidateImportInputOnly(input);
	if (progress)
		progress(std::string(" Importing wind file \"") + input.wndFilePath + "\".");

	::WindField field = ::WindField::ReadAny(input.wndFilePath, input.wndFormat, input);
	if (progress)
	{
		progress(std::string(" Imported format with grid ") + std::to_string(field.ny) + " x " + std::to_string(field.nz) +
		         ", steps=" + std::to_string(field.nSteps) + ", dt=" + FormatSeconds(field.dt) + ".");
	}

	if (input.sumPrint)
	{
		auto path = ImportSummaryBasePath(input);
		field.summaryPath = path.replace_extension(".sum").string();
		if (progress)
			progress(std::string(" Writing import summary \"") + field.summaryPath + "\".");
		WriteImportSummary(input, field, field.summaryPath);
	}

	return field;
}

::WindField WindL::ImportFromFile(const std::string &qwdPath, WindLProgressCallback progress)
{
	return Import(ReadSimWindInput(qwdPath), std::move(progress));
}
