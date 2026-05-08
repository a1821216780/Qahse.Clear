#pragma once

#include <functional>
#include <string>
#include <vector>

#include "WaveL/WaveKinematics.hpp"
#include "WaveL/WaveL_Type.hpp"

using WaveProgressCallback = std::function<void(const std::string &)>;

class WaveSpectrum
{
public:
	explicit WaveSpectrum(const WaveLInput &input = WaveLInput{}, std::string sourcePath = {});

	void Generate(WaveProgressCallback progress = {});
	void ImportComponents(const std::string &path, WaveProgressCallback progress = {});
	void ImportCache(const std::string &path, WaveProgressCallback progress = {});

	WaveKinematics GetKinematics(double x, double y, double z, double time) const;
	double GetElevation(double x, double y, double time) const;
	double GetFreeSurfaceZ(double x, double y, double time) const;
	bool IsSubmerged(double x, double y, double z, double time, double tolerance = 0.0) const;
	std::vector<WaveKinematics> SampleKinematics(const std::vector<WaveSamplePoint> &points, double time) const;

	const WaveLInput &GetInput() const { return input_; }
	const WaveSpectrumResult &GetResult() const { return result_; }
	const std::vector<WaveTrain> &GetWaveTrains() const { return result_.waveTrains; }
	double GetHs() const { return result_.significantHeight; }
	double GetTp() const { return result_.peakPeriod; }
	double GetFp() const { return result_.peakFrequency; }
	double GetDepth() const { return input_.waterDepth; }

	static void ValidateInputOnly(const WaveLInput &input);
	static WaveSpectrum Generate(const WaveLInput &input, WaveProgressCallback progress = {});
	static WaveSpectrum Import(const WaveLInput &input, WaveProgressCallback progress = {});
	static WaveSpectrum GenerateFromFile(const std::string &qoePath, WaveProgressCallback progress = {});
	static WaveSpectrum ImportFromFile(const std::string &qoePath, WaveProgressCallback progress = {});

	static double EvaluateJonswapSpectrum(double f, double Hs, double Tp, bool autoGamma, bool autoSigma, double gamma, double sigma1, double sigma2);
	static double EvaluateTorsethaugenSpectrum(double f, double Hs, double Tp, bool autoGamma, bool autoSigma, double gamma, double sigma1, double sigma2, bool doublePeak);
	static double EvaluateOchiHubbleSpectrum(double f, double Hs1, double Hs2, double f1, double f2, double lambda1, double lambda2);
	static double EvaluateDirectionalWeight(double directionRad, double dirMeanDeg, double dirMaxDeg, double spreadExp);
	static double SolveWaveNumber(double omega, double waterDepth);

private:
	WaveLInput input_;
	WaveSpectrumResult result_;
	std::string sourcePath_;
	wavel_detail::WaveFieldCache cache_;
	bool cacheBuilt_ = false;
	bool importedExternalKinematics_ = false;

	void ValidateInput() const;
	void ResolveDerivedPaths();
	void GenerateRegular();
	void GenerateSpectrumDriven(WaveProgressCallback progress);
	void GenerateFromUserSpectrum(WaveProgressCallback progress);
	void GenerateFromUserTimeSeries(WaveProgressCallback progress);
	void ApplyDirectionalDistribution();
	void UpdateResultStatistics();
	void BuildPrimaryCache();
	void WriteCacheFiles();
	void WriteOutputs(WaveProgressCallback progress);
};
