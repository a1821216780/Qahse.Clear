#include "SimL/SimL.hpp"

#include <cmath>
#include <filesystem>
#include <sstream>
#include <stdexcept>

#include "AeroL/Airfoil.hpp"
#include "AeroL/IO/AeroL_IO_Subs.hpp"
#include "ControL/IO/ControL_IO_Subs.hpp"
#include "HydroL/IO/HydroL_IO_Subs.hpp"
#include "HydroL/Wamit.hpp"
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
	modules.aeroL = ReadAeroLInput(input.aeroFile);
	modules.strL = ReadStrLInput(input.strFile);
	modules.controL = ReadControLInput(input.controlFile);
	modules.windL = windl_io_detail::ReadWindLInputFile(input.windFile);

	modules.aeroL.airfoilData = ReadAeroLAirfoilFiles(modules.aeroL);
	modules.bladeAeroStruct = ReadBladeAeroStructInput(modules.aeroL.bladeAeroStructFile);
	if (!modules.strL.towerFile.empty())
		modules.towerStruct = ReadTowerStructInput(modules.strL.towerFile);

	if (input.wtType == 2 && !input.hydroLFile.empty())
	{
		modules.hydroL = ReadHydroLInput(input.hydroLFile);
		if (!modules.hydroL->waveLFile.empty())
			modules.waveL = wavel_io_detail::ReadWaveLInputFile(modules.hydroL->waveLFile);

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
	return Load(ReadResolvedInputFile(path));
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
