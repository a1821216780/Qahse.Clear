#include "SimL/SimL.hpp"

#include <cmath>
#include <filesystem>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include "AeroL/Airfoil.hpp"
#include "AeroL/IO/AeroL_IO_Subs.hpp"
#include "ControL/IO/ControL_IO_Subs.hpp"
#include "HydroL/IO/HydroL_IO_Subs.hpp"
#include "HydroL/Wamit.hpp"
#include "IO/Yaml.hpp"
#include "SimL/IO/SimL_IO_Subs.hpp"
#include "StrL/IO/StrL_IO_Subs.hpp"
#include "WaveL/IO/WaveL_IO_Subs.hpp"
#include "WindL/IO/WindL_IO_Subs.hpp"

namespace
{
std::filesystem::path NormalizedPath(const std::string &path)
{
	return std::filesystem::absolute(std::filesystem::path(path)).lexically_normal();
}

bool SameResolvedPath(const std::string &lhs, const std::string &rhs)
{
	if (lhs.empty() || rhs.empty())
		return lhs.empty() && rhs.empty();
	return NormalizedPath(lhs) == NormalizedPath(rhs);
}

bool HasWamitRows(const HydroLWamitData &data)
{
	return !data.radiation.radiation.empty() ||
	       !data.excitation.excitation.empty() ||
	       !data.difference.qtf.empty() ||
	       !data.sum.qtf.empty();
}

bool HasRoot(const SimLInput &input, const std::string &root)
{
	return module_io::YamlHasKey(input.inputPath.string(), root);
}

std::string ModuleSource(const SimLInput &input, const std::string &root, const std::string &fallbackPath)
{
	return HasRoot(input, root) ? input.inputPath.string() : fallbackPath;
}

std::filesystem::path TempYamlDirFor(const std::string &path)
{
	const auto absolute = std::filesystem::absolute(std::filesystem::path(path)).lexically_normal().string();
	return std::filesystem::temp_directory_path() /
	       ("Qahse_SimL_" + std::to_string(std::hash<std::string>{}(absolute)));
}

SimLResolvedInput MakeSelfContainedPathView(SimLResolvedInput input, const std::string &path)
{
	const std::string selfPath = std::filesystem::absolute(std::filesystem::path(path)).lexically_normal().string();
	input.simL.inputPath = selfPath;
	input.simL.strFile = selfPath;
	input.simL.windFile = selfPath;
	input.simL.aeroFile = selfPath;
	input.simL.controlFile = selfPath;
	if (input.modules.hydroL)
		input.simL.hydroLFile = selfPath;

	input.modules.aeroL.bladeAeroStructFile = selfPath;
	input.modules.aeroL.bladeAeroStruct = input.modules.bladeAeroStruct;
	input.modules.strL.bladeAeroStructFile = selfPath;
	if (input.modules.towerStruct)
		input.modules.strL.towerFile = selfPath;
	if (input.modules.hydroL && input.modules.waveL)
		input.modules.hydroL->waveLFile = selfPath;
	if (input.modules.hydroL)
		input.modules.hydroL->wamit.reset();
	return input;
}

template <typename Writer>
void WriteAndMerge(YML &yaml,
                   const std::filesystem::path &dir,
                   const std::string &name,
                   Writer writer)
{
	const auto path = dir / name;
	writer(path.string());
	yaml.AddYAML(YML(path.string(), false));
}

void ValidateResolvedInput(const SimLResolvedInput &resolved)
{
	const auto &sim = resolved.simL;
	const auto &modules = resolved.modules;

	if (!SameResolvedPath(modules.aeroL.bladeAeroStructFile, modules.strL.bladeAeroStructFile))
	{
		throw std::runtime_error(
			"SimL AeroL BladeAeroStructFile and StrL BladeAeroStructFile refer to different files: " +
			modules.aeroL.bladeAeroStructFile + " vs " + modules.strL.bladeAeroStructFile);
	}

	if (!modules.aeroL.airfoilData.empty())
	{
		for (const auto &section : modules.bladeAeroStruct.sections)
		{
			if (section.polarFileId < 1 ||
			    section.polarFileId > static_cast<int>(modules.aeroL.airfoilData.size()))
			{
				throw std::runtime_error(
					"SimL blade PolarFileID is outside AeroL AFNames range: " +
					std::to_string(section.polarFileId));
			}
		}
	}

	if (sim.wtType == 2 && !modules.hydroL)
		throw std::runtime_error("SimL WTType=2 requires HydroL input to be loaded");

	if (modules.hydroL && modules.waveL &&
	    std::abs(modules.hydroL->waterDepth - modules.waveL->waterDepth) > 1.0e-9)
	{
		throw std::runtime_error(
			"SimL HydroL water depth does not match WaveL water depth: " +
			std::to_string(modules.hydroL->waterDepth) + " vs " +
			std::to_string(modules.waveL->waterDepth));
	}
}
} // namespace

SimLInput ReadSimLInputFile(const std::string &path)
{
	return ReadSimLInput(path);
}

SimLModuleInputs ResolveSimLModuleInputs(const SimLInput &input)
{
	SimLModuleInputs modules;
	modules.aeroL = ReadAeroLInput(ModuleSource(input, "Qahse.AeroL", input.aeroFile));
	modules.strL = ReadStrLInput(ModuleSource(input, "Qahse.StrL", input.strFile));
	modules.controL = ReadControLInput(ModuleSource(input, "Qahse.ControL", input.controlFile));
	modules.windL = windl_io_detail::ReadWindLInputFile(ModuleSource(input, "Qahse.WindL", input.windFile));

	if (modules.aeroL.airfoilData.empty())
		modules.aeroL.airfoilData = ReadAeroLAirfoilFiles(modules.aeroL);
	modules.bladeAeroStruct = modules.aeroL.bladeAeroStruct
		? *modules.aeroL.bladeAeroStruct
		: ReadBladeAeroStructInput(
			HasRoot(input, "Qahse.BladeAeroStruct") ? input.inputPath.string() : modules.aeroL.bladeAeroStructFile);
	if (!modules.strL.towerFile.empty() || HasRoot(input, "Qahse.TowerStruct"))
	{
		modules.towerStruct = ReadTowerStructInput(
			HasRoot(input, "Qahse.TowerStruct") ? input.inputPath.string() : modules.strL.towerFile);
	}

	if (input.wtType == 2 && (!input.hydroLFile.empty() || HasRoot(input, "Qahse.HydroL")))
	{
		modules.hydroL = ReadHydroLInput(ModuleSource(input, "Qahse.HydroL", input.hydroLFile));
		if (!modules.hydroL->waveLFile.empty() || HasRoot(input, "Qahse.WaveL"))
		{
			modules.waveL = wavel_io_detail::ReadWaveLInputFile(
				HasRoot(input, "Qahse.WaveL") ? input.inputPath.string() : modules.hydroL->waveLFile);
		}

		if (!modules.hydroL->wamit)
		{
			try
			{
				const auto wamit = ReadHydroLWamitFiles(*modules.hydroL);
				if (HasWamitRows(wamit))
					modules.hydroL->wamit = wamit;
			}
			catch (const std::exception &ex)
			{
				modules.warnings.push_back(std::string("HydroL WAMIT data was not preloaded: ") + ex.what());
			}
		}
	}

	return modules;
}

SimLResolvedInput ReadSimLResolvedInputFile(const std::string &path)
{
	SimLResolvedInput resolved;
	resolved.simL = ReadSimLInput(path);
	resolved.modules = ResolveSimLModuleInputs(resolved.simL);
	ValidateResolvedInput(resolved);
	return resolved;
}

void WriteSimLResolvedInputFile(const SimLResolvedInput &input, const std::string &path)
{
	auto resolved = MakeSelfContainedPathView(input, path);
	const auto tempDir = TempYamlDirFor(path);
	std::error_code ec;
	std::filesystem::remove_all(tempDir, ec);
	std::filesystem::create_directories(tempDir);

	YML yaml;
	WriteAndMerge(yaml, tempDir, "SimL.yml", [&](const std::string &tmp) {
		WriteSimLInput(resolved.simL, tmp);
	});
	WriteAndMerge(yaml, tempDir, "AeroL.yml", [&](const std::string &tmp) {
		WriteAeroLInput(resolved.modules.aeroL, tmp);
	});
	WriteAndMerge(yaml, tempDir, "StrL.yml", [&](const std::string &tmp) {
		WriteStrLInput(resolved.modules.strL, tmp);
	});
	WriteAndMerge(yaml, tempDir, "ControL.yml", [&](const std::string &tmp) {
		WriteControLInput(resolved.modules.controL, tmp);
	});
	WriteAndMerge(yaml, tempDir, "WindL.yml", [&](const std::string &tmp) {
		windl_io_detail::WriteWindLInputYaml(resolved.modules.windL, tmp);
	});
	WriteAndMerge(yaml, tempDir, "BladeAeroStruct.yml", [&](const std::string &tmp) {
		WriteBladeAeroStructInputYaml(resolved.modules.bladeAeroStruct, tmp);
	});
	if (resolved.modules.towerStruct)
	{
		WriteAndMerge(yaml, tempDir, "TowerStruct.yml", [&](const std::string &tmp) {
			WriteTowerStructInputYaml(*resolved.modules.towerStruct, tmp);
		});
	}
	if (resolved.modules.hydroL)
	{
		WriteAndMerge(yaml, tempDir, "HydroL.yml", [&](const std::string &tmp) {
			WriteHydroLInput(*resolved.modules.hydroL, tmp);
		});
	}
	if (resolved.modules.waveL)
	{
		WriteAndMerge(yaml, tempDir, "WaveL.yml", [&](const std::string &tmp) {
			wavel_io_detail::WriteWaveLInputYaml(*resolved.modules.waveL, tmp);
		});
	}

	yaml.save(path);
	std::filesystem::remove_all(tempDir, ec);
}

void ConvertSimLInputToSimFile(const std::string &inputPath, const std::string &outputPath)
{
	WriteSimLResolvedInputFile(ReadSimLResolvedInputFile(inputPath), outputPath);
}

SimLInput SimL::ReadInputFile(const std::string &path)
{
	return ReadSimLInput(path);
}

SimLResolvedInput SimL::ReadResolvedInputFile(const std::string &path)
{
	return ReadSimLResolvedInputFile(path);
}

void SimL::ValidateInputOnly(const SimLInput &input)
{
	if (input.dt <= 0.0)
		throw std::runtime_error("SimL DT must be positive");
	if (input.tMax <= 0.0)
		throw std::runtime_error("SimL TMax must be positive");
}

SimL SimL::Load(const SimLInput &input)
{
	ValidateInputOnly(input);
	SimLResolvedInput resolved;
	resolved.simL = input;
	resolved.modules = ResolveSimLModuleInputs(input);
	ValidateResolvedInput(resolved);
	return Load(resolved);
}

SimL SimL::Load(const SimLResolvedInput &input)
{
	ValidateInputOnly(input.simL);
	ValidateResolvedInput(input);
	SimL module;
	module.input_ = input;
	return module;
}

SimL SimL::LoadFromFile(const std::string &path)
{
	auto resolved = ReadResolvedInputFile(path);
	const auto ext = ZString::ToUpper(std::filesystem::path(path).extension().string());
	if (ext != ".SIM")
	{
		try
		{
			auto simPath = std::filesystem::path(path);
			simPath.replace_extension(".sim");
			WriteSimLResolvedInputFile(resolved, simPath.string());
		}
		catch (const std::exception &ex)
		{
			resolved.modules.warnings.push_back(std::string("SimL .sim export failed: ") + ex.what());
		}
	}
	return Load(resolved);
}

const SimLInput &SimL::Input() const
{
	return input_.simL;
}

const SimLModuleInputs &SimL::Modules() const
{
	return input_.modules;
}

const SimLResolvedInput &SimL::ResolvedInput() const
{
	return input_;
}

std::string SimL::Summary() const
{
	std::ostringstream out;
	out << "SimL: TMax=" << input_.simL.tMax
	    << ", DT=" << input_.simL.dt
	    << ", WTType=" << input_.simL.wtType
	    << ", airfoils=" << input_.modules.aeroL.airfoilData.size()
	    << ", hydro=" << (input_.modules.hydroL ? "loaded" : "none");
	return out.str();
}
