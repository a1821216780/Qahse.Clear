#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

enum class WaveType
{
	NONE = 0,
	REGULAR = 1,
	JONSWAP = 2,
	ISSC = 3,
	TORSETHAUGEN = 4,
	OCHI_HUBBLE = 5,
	USER_SPECTRUM = 6,
	COMPONENT_FILE = 7,
	ELEVATION_TIME_SERIES = 8
};

enum class WaveStretching
{
	VERTICAL = 0,
	WHEELER = 1,
	EXTRAPOLATION = 2,
	NO_STRETCHING = 3
};

enum class WaveFrequencyDiscretization
{
	EQUAL_FREQUENCY = 0,
	EQUAL_ENERGY = 1
};

struct WaveComponent
{
	double frequency = 0.0;
	double omega = 0.0;
	double amplitude = 0.0;
	double phase = 0.0;
	double direction = 0.0;
	double wavenumber = 0.0;
};

struct WaveLInput
{
	std::filesystem::path inputPath;
	WaveType waveType = WaveType::NONE;
	double waterDepth = 0.0;
	double gravity = 9.81;
	WaveStretching waveStretching = WaveStretching::WHEELER;
	double timeOffset = 0.0;

	double hs = 0.0;
	double tp = 0.0;
	double waveDirMean = 0.0;
	double waveDirMax = 0.0;
	double waveDirSpread = 1.0;
	int waveFreqNum = 0;
	int waveDirNum = 1;
	WaveFrequencyDiscretization freqDisc = WaveFrequencyDiscretization::EQUAL_ENERGY;
	double dfMax = 0.0;
	bool autoFreqRange = true;
	double freqStart = 0.0;
	double freqEnd = 0.0;
	int seed = 1;

	bool autoGamma = true;
	double gamma = 3.3;
	bool autoSigma = true;
	double sigma1 = 0.07;
	double sigma2 = 0.09;
	bool torsethaugenDoublePeak = true;

	bool autoOchi = true;
	double ochiHs1 = 0.0;
	double ochiHs2 = 0.0;
	double ochiF1 = 0.0;
	double ochiF2 = 0.0;
	double ochiLambda1 = 0.0;
	double ochiLambda2 = 0.0;

	std::string spectrumFilePath;
	std::string componentFilePath;
	std::string timeSeriesFilePath;
	double dftCutIn = 0.0;
	double dftCutOut = 0.0;
	double dftSample = 1.0;
	double dftThreshold = 0.0;

	double constCurrent = 0.0;
	double constCurrentDir = 0.0;
	double shearCurrent = 0.0;
	double shearCurrentDir = 0.0;
	double shearCurrentDepth = 0.0;
	double profileCurrent = 0.0;
	double profileCurrentDir = 0.0;
	double profileCurrentExponent = 1.0;

	bool sumPrint = false;
	bool saveComponents = false;
	std::string savePath;
	std::string saveName;
};

struct SeaState
{
	double elevation = 0.0;
	std::array<double, 3> waveVelocity{0.0, 0.0, 0.0};
	std::array<double, 3> waveAcceleration{0.0, 0.0, 0.0};
	std::array<double, 3> currentVelocity{0.0, 0.0, 0.0};
	std::array<double, 3> waterVelocity{0.0, 0.0, 0.0};
	double dynamicPressure = 0.0;
};

struct WaveLResult
{
	std::vector<WaveComponent> components;
	std::string componentPath;
	std::string summaryPath;
};
