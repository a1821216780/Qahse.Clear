#pragma once

#include <array>
#include <string>
#include <vector>

enum class WaveFreqSpectrum
{
	NONE = 0,
	REGULAR = 1,
	JONSWAP = 2,
	TORSETHAUGEN = 3,
	OCHI_HUBBLE = 4,
	USER_SPECTRUM = 5,
	USER_TIMESERIES = 6
};

enum class WaveDirSpectrum
{
	UNIDIRECTIONAL = 0,
	COSINE_SPREAD = 1
};

enum class WaveDiscretization
{
	EQUAL_ENERGY = 0,
	EQUAL_FREQUENCY = 1
};

enum class WaveStretching
{
	NONE = 0,
	VERTICAL = 1,
	EXTRAPOLATION = 2,
	WHEELER = 3
};

enum class WaveMode
{
	GENERATE = 0,
	IMPORT = 1,
	BATCH = 2
};

struct WaveTrain
{
	double amplitude = 0.0;
	double phase = 0.0;
	double omega = 0.0;
	double wavenumber = 0.0;
	double direction = 0.0;
	double cosDir = 1.0;
	double sinDir = 0.0;
	double A_omega = 0.0;
	double A_omega2 = 0.0;
};

struct WaveSamplePoint
{
	std::string name;
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

struct WaveKinematics
{
	double eta = 0.0;
	std::array<double, 3> vel{0.0, 0.0, 0.0};
	std::array<double, 3> acc{0.0, 0.0, 0.0};
	double dynP = 0.0;
};

struct WaveEnvironmentSample
{
	WaveKinematics kinematics;
	double freeSurfaceZ = 0.0;
	double immersionDepth = 0.0;
	bool submerged = false;
};

struct WaveSpectrumResult
{
	std::vector<WaveTrain> waveTrains;
	double significantHeight = 0.0;
	double peakPeriod = 0.0;
	double peakFrequency = 0.0;
	double zeroMoment = 0.0;
	int numComponents = 0;
	double fMin = 0.0;
	double fMax = 0.0;
	double spectralArea = 0.0;
	std::string cacheFilePath;
	std::string metadataFilePath;
	std::string componentsFilePath;
	std::string timeSeriesFilePath;
	std::string summaryFilePath;
	std::string kinematicsDirectory;
	std::vector<std::string> warnings;
};

struct WaveLInput
{
	WaveMode mode = WaveMode::GENERATE;

	WaveFreqSpectrum freqSpectrum = WaveFreqSpectrum::JONSWAP;
	WaveDirSpectrum dirSpectrum = WaveDirSpectrum::UNIDIRECTIONAL;
	WaveDiscretization discretization = WaveDiscretization::EQUAL_ENERGY;
	WaveStretching stretching = WaveStretching::EXTRAPOLATION;

	double Hs = 3.0;
	double Tp = 10.0;
	double timeOffset = 0.0;
	double dirMean = 0.0;
	double dirMax = 30.0;
	double dirSpreadExp = 2.0;

	double fCutIn = 0.0;
	double fCutOut = 0.0;
	double dfMax = 0.0;
	int numFreqBins = 200;
	int numDirBins = 24;
	int randomSeed = 0;

	double gamma = 0.0;
	double sigma1 = 0.0;
	double sigma2 = 0.0;
	bool autoGamma = true;
	bool autoSigma = true;
	bool autoFreqRange = true;

	double Hs1 = 2.0;
	double Hs2 = 1.5;
	double f1 = 0.1;
	double f2 = 0.2;
	double lambda1 = 2.0;
	double lambda2 = 2.0;
	bool autoOchi = false;

	bool doublePeak = false;
	double regularPhase = 0.0;

	double timeStep = 0.1;
	double simDuration = 600.0;
	double waterDepth = 100.0;

	double mcfDiameter = 0.0;

	bool outputComponents = true;
	bool outputTimeSeries = true;
	bool outputKinematicsGrid = false;
	std::string cachePath;
	std::string metadataPath;
	std::string componentsPath;
	std::string timeSeriesPath;
	std::string kinematicsPath;
	std::string summaryPath;

	int gridNX = 1;
	int gridNY = 1;
	int gridNZ = 10;
	double gridDX = 0.0;
	double gridDY = 0.0;
	double gridDZ = 0.0;

	std::string importedSpectrumPath;
	std::string importedTimeSeriesPath;
	std::string importedComponentsPath;
	std::string importedCachePath;
};

struct WaveUserSpectrumData
{
	std::vector<double> frequencies;
	std::vector<double> spectralDensity;
};

struct WaveTimeSeriesData
{
	std::vector<double> times;
	std::vector<double> elevations;
};
