#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

#include "IO/Serializer.hpp"
#include "IO/ZFile.hpp"
#include "IO/ZPath.hpp"
#include "WaveL/WaveL_Type.hpp"

namespace wavel_io_detail
{
	inline constexpr const char *kYamlRoot = "Qahse.WaveL";

	inline void ResolveIfSet(const std::string &baseFilePath, std::string &path)
	{
		if (!path.empty())
			path = ZPath::ResolvePath(baseFilePath, path);
	}

	inline void ResolvePaths(WaveLInput &input)
	{
		const std::string base = input.inputPath.string();
		ResolveIfSet(base, input.spectrumFilePath);
		ResolveIfSet(base, input.componentFilePath);
		ResolveIfSet(base, input.timeSeriesFilePath);
		if (!input.savePath.empty())
			input.savePath = ZPath::ResolvePath(base, input.savePath);
	}

	class WaveLInputSerializer : public Serializer
	{
	public:
		WaveLInput data;

		WaveLInputSerializer()
		{
			SetValueFirst(true);
			SetYamlRoot(kYamlRoot);
		}

		explicit WaveLInputSerializer(const WaveLInput &input)
			: data(input)
		{
			SetValueFirst(true);
			SetYamlRoot(kYamlRoot);
		}

	protected:
		void SerializeFields() override
		{
			Fields(
				Field("WaveType", data.waveType),
				Field("WaterDepth", data.waterDepth),
				Field("Gravity", data.gravity),
				Field("WaveStretching", data.waveStretching),
				Field("TimeOffset", data.timeOffset),
				Field("Hs", data.hs),
				Field("Tp", data.tp),
				Field("WaveDirMean", data.waveDirMean),
				Field("WaveDirMax", data.waveDirMax),
				Field("WaveDirSpread", data.waveDirSpread),
				Field("WaveFreqNum", data.waveFreqNum),
				Field("WaveDirNum", data.waveDirNum),
				Field("FreqDisc", data.freqDisc),
				Field("DFMax", data.dfMax),
				Field("AutoFreqRange", data.autoFreqRange),
				Field("FreqStart", data.freqStart),
				Field("FreqEnd", data.freqEnd),
				Field("Seed", data.seed),
				Field("AutoGamma", data.autoGamma),
				Field("Gamma", data.gamma),
				Field("AutoSigma", data.autoSigma),
				Field("Sigma1", data.sigma1),
				Field("Sigma2", data.sigma2),
				Field("TorsethaugenDoublePeak", data.torsethaugenDoublePeak),
				Field("AutoOchi", data.autoOchi),
				Field("OchiHs1", data.ochiHs1),
				Field("OchiHs2", data.ochiHs2),
				Field("OchiF1", data.ochiF1),
				Field("OchiF2", data.ochiF2),
				Field("OchiLambda1", data.ochiLambda1),
				Field("OchiLambda2", data.ochiLambda2),
				Field("SpectrumFilePath", data.spectrumFilePath),
				Field("ComponentFilePath", data.componentFilePath),
				Field("TimeSeriesFilePath", data.timeSeriesFilePath),
				Field("DFTCutIn", data.dftCutIn),
				Field("DFTCutOut", data.dftCutOut),
				Field("DFTSample", data.dftSample),
				Field("DFTThreshold", data.dftThreshold),
				Field("ConstCurrent", data.constCurrent),
				Field("ConstCurrentDir", data.constCurrentDir),
				Field("ShearCurrent", data.shearCurrent),
				Field("ShearCurrentDir", data.shearCurrentDir),
				Field("ShearCurrentDepth", data.shearCurrentDepth),
				Field("ProfileCurrent", data.profileCurrent),
				Field("ProfileCurrentDir", data.profileCurrentDir),
				Field("ProfileCurrentExponent", data.profileCurrentExponent),
				Field("SumPrint", data.sumPrint),
				Field("SaveComponents", data.saveComponents),
				Field("SavePath", data.savePath),
				Field("SaveName", data.saveName));
		}
	};

	inline WaveLInput ReadWaveLInputFile(const std::string &path)
	{
		if (!ZFile::Exists(path))
			throw std::runtime_error("Cannot open WaveL input file: " + path);

		WaveLInputSerializer reader;
		if (Serializer::IsYamlPath(path))
			reader.ReadYamlFile(path);
		else
			reader.ReadTextFile(path);

		WaveLInput input = reader.data;
		input.inputPath = std::filesystem::absolute(path).lexically_normal();
		ResolvePaths(input);
		return input;
	}

	inline void WriteWaveLInputYaml(const WaveLInput &input, const std::string &path)
	{
		WaveLInputSerializer writer(input);
		writer.WriteYamlFile(path);
	}
} // namespace wavel_io_detail
