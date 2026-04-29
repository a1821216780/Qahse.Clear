#include "WindL/SimWind.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <fftw/fftw3.h>

#include "WindL/IO/WindL_IO_Subs.hpp"

namespace
{
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kRad = kPi / 180.0;
constexpr double kTiny = 1.0e-12;
constexpr double kHugeDecay = 1.0e9;
constexpr double kOmega = 7.2921159e-5;

struct ReynoldsStressSetup
{
	std::array<double, 3> target{0.0, 0.0, 0.0};
	std::array<bool, 3> skip{true, true, true};
	bool active = false;
};

struct MeteorologyClosure
{
	double richardson = 0.0;
	double zL = 0.0;
	double moninLength = std::numeric_limits<double>::infinity();
	double uStar = 0.0;
	double uStarDiab = 0.0;
	double mixingLayerDepth = 0.0;
	double coriolis = 0.0;
	std::array<double, 3> defaultGeneralCohDecay{0.0, 0.0, 0.0};
	std::array<double, 3> defaultGeneralCohB{0.0, 0.0, 0.0};
	ReynoldsStressSetup reynoldsStress;
};

struct SimWindConfig
{
	WindLInput input;
	int ny = 0;
	int nz = 0;
	int nPoints = 0;
	int nSteps = 0;
	int nFreq = 0;
	double dt = 0.0;
	double duration = 0.0;
	double df = 0.0;
	double dy = 0.0;
	double dz = 0.0;
	double dx = 0.0;
	double gridWidth = 0.0;
	double gridHeight = 0.0;
	double zBottom = 0.0;
	double hubHeight = 0.0;
	double uHub = 0.0;
	double refHeight = 0.0;
	double lambda = 0.0;
	double lc = 0.0;
	double effectiveRotorDiameter = 0.0;
	double mannLength = 0.0;
	double mannGamma = 0.0;
	double mannMaxL = 0.0;
	int mannFftPoints = 0;
	int mannGridY = 0;
	int mannGridZ = 0;
	int scaleIEC = 0;
	std::array<double, 3> sigma{0.0, 0.0, 0.0};
	std::array<double, 3> integralScale{0.0, 0.0, 0.0};
	std::array<double, 3> lateralScale{0.0, 0.0, 0.0};
	std::array<double, 3> verticalScale{0.0, 0.0, 0.0};
	std::array<double, 3> cohDecay{0.0, 0.0, 0.0};
	std::array<double, 3> cohB{0.0, 0.0, 0.0};
	std::array<CohModel, 3> cohModel{CohModel::DEFAULT_COH, CohModel::DEFAULT_COH, CohModel::DEFAULT_COH};
	std::array<bool, 3> useKronecker{false, false, false};
	std::array<int, 3> kroneckerFreqLimit{0, 0, 0};
	std::array<bool, 3> hasExplicitCohDecay{false, false, false};
	bool hasExplicitCohB = false;
	std::vector<double> yCoords;
	std::vector<double> zCoords;
	std::vector<double> meanUByZ;
	std::vector<double> y;
	std::vector<double> z;
	std::vector<double> meanU;
	std::vector<double> directionByZ;
	std::array<std::vector<double>, 3> sigmaByZ;
	std::array<std::vector<double>, 3> lengthScaleByZ;
	std::array<std::vector<double>, 3> lateralScaleByZ;
	std::array<std::vector<double>, 3> verticalScaleByZ;
	std::filesystem::path outputBase;
	UserSpectraData userSpectra;
	UserShearData userShear;
	bool hasUserSpectra = false;
	bool hasUserShear = false;
	bool hasUserDirectionProfile = false;
	bool hasUserSigmaProfile = false;
	bool hasUserLengthScaleProfile = false;
	bool hasSigmaProfileByZ = false;
	bool hasLengthScaleProfileByZ = false;
	bool hasImprovedVkProfile = false;
	double estimatedPeakMemoryGiB = 0.0;
	double estimatedCholeskyFlops = 0.0;
	int strictCoherenceComponents = 0;
	MeteorologyClosure met;
	SimWindProgressCallback progress;
	std::vector<std::string> warnings;
};

struct WindField
{
	int nSteps = 0;
	int nPoints = 0;
	std::array<std::vector<double>, 3> component;

	double &At(int comp, int step, int point)
	{
		return component[static_cast<std::size_t>(comp)][static_cast<std::size_t>(step) * nPoints + point];
	}

	double At(int comp, int step, int point) const
	{
		return component[static_cast<std::size_t>(comp)][static_cast<std::size_t>(step) * nPoints + point];
	}
};

struct FftwBatchPlan1D
{
	int n = 0;
	int nTransforms = 0;
	fftw_complex *data = nullptr;
	fftw_plan plan = nullptr;

	FftwBatchPlan1D(int n, int nTransforms)
	    : n(n), nTransforms(nTransforms)
	{
		const std::size_t total = static_cast<std::size_t>(n) * static_cast<std::size_t>(nTransforms);
		data = static_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * total));
		if (data == nullptr)
			throw std::runtime_error("FFTW failed to allocate SimWind batch IFFT buffers.");

		std::memset(data, 0, sizeof(fftw_complex) * total);
		int dims[1] = {n};
		plan = fftw_plan_many_dft(1,
		                          dims,
		                          nTransforms,
		                          data,
		                          nullptr,
		                          nTransforms,
		                          1,
		                          data,
		                          nullptr,
		                          nTransforms,
		                          1,
		                          FFTW_BACKWARD,
		                          FFTW_ESTIMATE);
		if (plan == nullptr)
			throw std::runtime_error("FFTW failed to create SimWind batch IFFT plan.");
	}

	~FftwBatchPlan1D()
	{
		if (plan != nullptr)
			fftw_destroy_plan(plan);
		if (data != nullptr)
			fftw_free(data);
	}

	FftwBatchPlan1D(const FftwBatchPlan1D &) = delete;
	FftwBatchPlan1D &operator=(const FftwBatchPlan1D &) = delete;

	void ZeroAll()
	{
		std::memset(data, 0, sizeof(fftw_complex) * static_cast<std::size_t>(n) * static_cast<std::size_t>(nTransforms));
	}

	void Execute()
	{
		fftw_execute(plan);
	}

	void SetSpectrum(int step, int transform, const std::complex<double> &value)
	{
		const std::size_t index = static_cast<std::size_t>(step) * static_cast<std::size_t>(nTransforms) + static_cast<std::size_t>(transform);
		data[index][0] = value.real();
		data[index][1] = value.imag();
	}

	double OutputReal(int step, int transform) const
	{
		const std::size_t index = static_cast<std::size_t>(step) * static_cast<std::size_t>(nTransforms) + static_cast<std::size_t>(transform);
		return data[index][0];
	}
};

struct FftwComplexVolume
{
	int nx = 0;
	int ny = 0;
	int nz = 0;
	fftw_complex *data = nullptr;

	FftwComplexVolume(int nx, int ny, int nz)
	    : nx(nx), ny(ny), nz(nz)
	{
		const std::size_t total = static_cast<std::size_t>(nx) * ny * nz;
		data = static_cast<fftw_complex *>(fftw_malloc(sizeof(fftw_complex) * total));
		if (data == nullptr)
			throw std::runtime_error("FFTW failed to allocate SimWind 3D Mann buffers.");
		std::memset(data, 0, sizeof(fftw_complex) * total);
	}

	~FftwComplexVolume()
	{
		if (data != nullptr)
			fftw_free(data);
	}

	FftwComplexVolume(const FftwComplexVolume &) = delete;
	FftwComplexVolume &operator=(const FftwComplexVolume &) = delete;

	std::size_t Index(int ix, int iy, int iz) const
	{
		return (static_cast<std::size_t>(ix) * ny + static_cast<std::size_t>(iy)) * nz + static_cast<std::size_t>(iz);
	}

	void Set(int ix, int iy, int iz, const std::complex<double> &value)
	{
		const std::size_t index = Index(ix, iy, iz);
		data[index][0] = value.real();
		data[index][1] = value.imag();
	}

	double Real(int ix, int iy, int iz) const
	{
		return data[Index(ix, iy, iz)][0];
	}

	void ExecuteBackward()
	{
		fftw_plan plan = fftw_plan_dft_3d(nx, ny, nz, data, data, FFTW_BACKWARD, FFTW_ESTIMATE);
		if (plan == nullptr)
			throw std::runtime_error("FFTW failed to create SimWind 3D Mann IFFT plan.");
		fftw_execute(plan);
		fftw_destroy_plan(plan);
	}
};

template <typename T>
void WriteScalar(std::ofstream &out, T value)
{
	out.write(reinterpret_cast<const char *>(&value), sizeof(T));
	if (!out)
		throw std::runtime_error("Failed while writing SimWind binary output.");
}

int GridIndex(const SimWindConfig &cfg, int iz, int iy)
{
	return iz * cfg.ny + iy;
}

double ClampPositive(double value, double fallback)
{
	return value > 0.0 ? value : fallback;
}

void Report(const SimWindConfig &cfg, const std::string &message)
{
	if (cfg.progress)
		cfg.progress(message);
}

bool HasProfileColumn(const std::vector<double> &heights, const std::vector<double> &values)
{
	return !heights.empty() && heights.size() == values.size();
}

double UserStdScale(const UserShearData &data, int comp)
{
	switch (comp)
	{
	case 0: return data.stdScale1 > 0.0 ? data.stdScale1 : 1.0;
	case 1: return data.stdScale2 > 0.0 ? data.stdScale2 : 1.0;
	default: return data.stdScale3 > 0.0 ? data.stdScale3 : 1.0;
	}
}

double InterpolateProfile(const std::vector<double> &heights, const std::vector<double> &values, double z);
double NormalizeDirectionDegrees(double value)
{
	double wrapped = std::fmod(value, 360.0);
	if (wrapped < 0.0)
		wrapped += 360.0;
	return wrapped;
}

double InterpolateWrappedDirection(const std::vector<double> &heights,
                                   const std::vector<double> &directions,
                                   double z)
{
	if (heights.empty() || directions.empty())
		return 0.0;
	const std::size_t n = std::min(heights.size(), directions.size());
	if (n == 1 || z <= heights.front())
		return NormalizeDirectionDegrees(directions.front());
	if (z >= heights[n - 1])
		return NormalizeDirectionDegrees(directions[n - 1]);

	const auto upper = std::upper_bound(heights.begin(), heights.begin() + static_cast<std::ptrdiff_t>(n), z);
	const std::size_t i1 = static_cast<std::size_t>(std::distance(heights.begin(), upper));
	const std::size_t i0 = i1 - 1;
	const double span = std::max(heights[i1] - heights[i0], kTiny);
	const double a = (z - heights[i0]) / span;
	double d0 = NormalizeDirectionDegrees(directions[i0]);
	double d1 = NormalizeDirectionDegrees(directions[i1]);
	double delta = d1 - d0;
	if (delta > 180.0)
		delta -= 360.0;
	else if (delta < -180.0)
		delta += 360.0;
	return NormalizeDirectionDegrees(d0 + a * delta);
}

void AppendWarning(std::vector<std::string> &warnings, const std::string &text)
{
	if (std::find(warnings.begin(), warnings.end(), text) == warnings.end())
		warnings.push_back(text);
}

std::vector<std::string> &MutableWarnings(const SimWindConfig &cfg)
{
	return const_cast<std::vector<std::string> &>(cfg.warnings);
}

std::string FormatGiB(double value)
{
	std::ostringstream out;
	out << std::fixed << std::setprecision(2) << value << " GiB";
	return out.str();
}

std::string FormatScientific(double value)
{
	std::ostringstream out;
	out << std::scientific << std::setprecision(3) << value;
	return out.str();
}

std::string FormatSeconds(double value)
{
	std::ostringstream out;
	out << std::fixed << std::setprecision(2) << value;
	return out.str();
}

std::string FormatDuration(double seconds)
{
	if (!std::isfinite(seconds) || seconds < 0.0)
		return "unknown";

	const auto total = static_cast<long long>(std::llround(seconds));
	const long long days = total / 86400;
	const long long hours = (total % 86400) / 3600;
	const long long minutes = (total % 3600) / 60;
	const long long secs = total % 60;

	std::ostringstream out;
	if (days > 0)
		out << days << "d ";
	if (days > 0 || hours > 0)
		out << hours << "h ";
	if (days > 0 || hours > 0 || minutes > 0)
		out << minutes << "m ";
	out << secs << "s";
	return out.str();
}

std::string InitialRuntimeEstimate(double flops)
{
	if (flops <= 0.0)
		return "0s";

	const double slow = flops / 1.0e11;
	const double fast = flops / 1.0e12;
	return FormatDuration(fast) + " to " + FormatDuration(slow) + " for Cholesky work only";
}

int EvenStepCount(double duration, double dt)
{
	return std::max(2, static_cast<int>(std::ceil(duration / dt)));
}

double FractionalTI(double value)
{
	if (value <= 0.0)
		return 0.0;
	return value > 1.0 ? value / 100.0 : value;
}

double TurbulenceClassIref(TurbulenceClass value)
{
	switch (value)
	{
	case TurbulenceClass::Class_A: return 0.16;
	case TurbulenceClass::Class_B: return 0.14;
	case TurbulenceClass::Class_C: return 0.12;
	default: return 0.14;
	}
}

double DefaultVRef(TurbineClass value)
{
	switch (value)
	{
	case TurbineClass::Class_I: return 50.0;
	case TurbineClass::Class_II: return 42.5;
	case TurbineClass::Class_III: return 37.5;
	case TurbineClass::Class_S: return 50.0;
	default: return 50.0;
	}
}

double ExtremeWindSpeed50(const SimWindConfig &cfg)
{
	return 1.4 * ClampPositive(cfg.input.vRef, DefaultVRef(cfg.input.turbineClass));
}

double ExtremeWindSpeed1(const SimWindConfig &cfg)
{
	return 0.8 * ExtremeWindSpeed50(cfg);
}

double ReferenceSigma1Ntm(const SimWindConfig &cfg)
{
	const double iref = TurbulenceClassIref(cfg.input.turbClass);
	if (cfg.input.iecEdition == IecStandard::ED2)
	{
		const double slope = cfg.input.turbClass == TurbulenceClass::Class_A ? 2.0 : 3.0;
		const double ti15 = cfg.input.turbClass == TurbulenceClass::Class_A ? 0.18 : 0.16;
		return ti15 * ((15.0 + slope * cfg.uHub) / (slope + 1.0));
	}
	return iref * (0.75 * cfg.uHub + 5.6);
}

double ExtremeOperatingGustAmplitude(const SimWindConfig &cfg)
{
	const double sigma1 = ReferenceSigma1Ntm(cfg);
	const double denom = 1.0 + 0.1 * cfg.effectiveRotorDiameter / std::max(cfg.lambda, kTiny);
	const double a = 1.35 * (ExtremeWindSpeed1(cfg) - cfg.uHub);
	const double b = 3.3 * sigma1 / denom;
	return std::min(a, b);
}

double ExtremeDirectionChangeDegrees(const SimWindConfig &cfg)
{
	const double sigma1 = ReferenceSigma1Ntm(cfg);
	const double denom = cfg.uHub * (1.0 + 0.1 * cfg.effectiveRotorDiameter / std::max(cfg.lambda, kTiny));
	const double theta = 4.0 * std::atan(sigma1 / std::max(denom, kTiny)) / kRad;
	return std::min(theta, 180.0);
}

bool IsVonKarman(TurbModel value)
{
	return value == TurbModel::IEC_VKAIMAL ||
	       value == TurbModel::B_VKAL ||
	       value == TurbModel::B_IVKAL ||
	       value == TurbModel::USRVKM;
}

bool IsMann(TurbModel value)
{
	return value == TurbModel::B_MANN;
}

bool IsImprovedVonKarman(TurbModel value)
{
	return value == TurbModel::B_IVKAL;
}

using Matrix3 = std::array<std::array<double, 3>, 3>;

struct ImprovedVkProfilePoint
{
	bool valid = false;
	double sigma[3]{0.0, 0.0, 0.0};
	double xScale[3]{0.0, 0.0, 0.0};
	double yScale[3]{0.0, 0.0, 0.0};
	double zScale[3]{0.0, 0.0, 0.0};
	double a = 1.0;
	double boundaryLayerHeight = 0.0;
	double frictionVelocity = 0.0;
};

Matrix3 ZeroMatrix3()
{
	return {{{0.0, 0.0, 0.0},
	         {0.0, 0.0, 0.0},
	         {0.0, 0.0, 0.0}}};
}

double TargetReynoldsStress(const ReynoldsStressSetup &setup, int compA, int compB)
{
	if ((compA == 0 && compB == 2) || (compA == 2 && compB == 0))
		return setup.target[0];
	if ((compA == 0 && compB == 1) || (compA == 1 && compB == 0))
		return setup.target[1];
	if ((compA == 1 && compB == 2) || (compA == 2 && compB == 1))
		return setup.target[2];
	return 0.0;
}

bool HasTargetReynoldsStress(const ReynoldsStressSetup &setup, int compA, int compB)
{
	if ((compA == 0 && compB == 2) || (compA == 2 && compB == 0))
		return !setup.skip[0];
	if ((compA == 0 && compB == 1) || (compA == 1 && compB == 0))
		return !setup.skip[1];
	if ((compA == 1 && compB == 2) || (compA == 2 && compB == 1))
		return !setup.skip[2];
	return false;
}

bool IsEwmWindModel(WindModel model)
{
	return model == WindModel::EWM1 || model == WindModel::EWM50;
}

bool IsSteadyEwmWindModel(const WindLInput &input)
{
	return IsEwmWindModel(input.windModel) && input.ewmType == EWMType::Steady;
}

bool IsIecSpectralModel(TurbModel value)
{
	return value == TurbModel::IEC_KAIMAL || value == TurbModel::IEC_VKAIMAL;
}

bool IsSyntheticStochasticModel(const WindLInput &input);

double StabilityPsiM(double zOverL)
{
	if (zOverL >= 0.0)
		return -5.0 * std::min(zOverL, 1.0);

	const double tmp = std::pow(1.0 - 15.0 * zOverL, 0.25);
	double psiM = -std::log(0.125 * std::pow(1.0 + tmp, 2.0) * (1.0 + tmp * tmp)) +
	              2.0 * std::atan(tmp) - 0.5 * kPi;
	return -psiM;
}

double MoninLengthFromZL(double hubHeight, double zL)
{
	if (std::abs(zL) <= kTiny)
		return std::numeric_limits<double>::infinity();
	return hubHeight / zL;
}

double ZOverLAtHeight(double height, double moninLength)
{
	if (!std::isfinite(moninLength) || std::abs(moninLength) <= kTiny)
		return 0.0;
	return height / moninLength;
}

double DeriveZLFromRichardson(double richardson)
{
	if (richardson <= 0.0)
		return richardson;
	if (richardson < 0.16667)
		return std::min(richardson / (1.0 - 5.0 * richardson), 1.0);
	return 1.0;
}

double DiabaticLogWindSpeed(double height,
                            double refHeight,
                            double refSpeed,
                            double roughness,
                            double moninLength)
{
	if (height <= 0.0 || refHeight <= 0.0 || roughness <= 0.0)
		return 0.0;
	const double z0 = std::clamp(roughness, 1.0e-5, refHeight * 0.95);
	const double psiHt = StabilityPsiM(ZOverLAtHeight(height, moninLength));
	const double psiRef = StabilityPsiM(ZOverLAtHeight(refHeight, moninLength));
	const double denom = std::log(refHeight / z0) - psiRef;
	if (std::abs(denom) <= kTiny)
		return 0.0;
	return refSpeed * (std::log(height / z0) - psiHt) / denom;
}

double UstarDiabatic(double uRef, double refHeight, double roughness, double zLAtRef)
{
	const double z0 = std::clamp(roughness, 1.0e-5, refHeight * 0.95);
	const double psiRef = StabilityPsiM(zLAtRef);
	const double denom = std::log(refHeight / z0) - psiRef;
	if (uRef <= 0.0 || refHeight <= 0.0 || std::abs(denom) <= kTiny)
		return 0.0;
	return 0.4 * uRef / denom;
}

double DefaultMixingLayerDepth(double uStar,
                               double uStarDiab,
                               double uRef,
                               double refHeight,
                               double roughness,
                               double coriolis)
{
	const double z0 = std::clamp(roughness, 1.0e-5, refHeight * 0.95);
	if (uStar < uStarDiab || std::abs(coriolis) <= 1.0e-8)
		return (0.04 * uRef) / (1.0e-4 * std::max(std::log10(refHeight / z0), 1.0e-3));
	return uStar / (6.0 * std::abs(coriolis));
}

bool HasImprovedVkAtmosphericInputs(const WindLInput &input)
{
	return input.roughness > 0.0 && std::abs(input.latitude) > 1.0e-3;
}

double ImprovedVkFrictionVelocity(double meanU, double z, double roughness, double coriolis)
{
	const double zEval = std::max(z, roughness * 1.01);
	const double denom = std::log(std::max(zEval / std::max(roughness, 1.0e-5), 1.000001));
	if (meanU <= 0.0 || denom <= kTiny)
		return 0.0;
	const double value = (0.4 * meanU - 34.5 * std::abs(coriolis) * zEval) / denom;
	return std::max(value, 0.0);
}

ImprovedVkProfilePoint ComputeImprovedVkPoint(const SimWindConfig &cfg, double z, double meanU)
{
	ImprovedVkProfilePoint point;
	if (!HasImprovedVkAtmosphericInputs(cfg.input))
		return point;

	const double z0 = std::max(cfg.input.roughness, 1.0e-5);
	const double zEval = std::max(z, z0 * 1.01);
	const double coriolis = 2.0 * kOmega * std::sin(std::abs(cfg.input.latitude) * kRad);
	const double coriolisAbs = std::abs(coriolis);
	if (coriolisAbs <= 1.0e-8 || meanU <= 0.1)
		return point;

	const double uStar = ImprovedVkFrictionVelocity(meanU, zEval, z0, coriolisAbs);
	if (uStar <= kTiny)
		return point;

	const double boundaryLayerHeight = uStar / (6.0 * coriolisAbs);
	if (boundaryLayerHeight <= zEval + kTiny)
		return point;

	const double eta = std::clamp(1.0 - 6.0 * coriolisAbs * zEval / uStar, 0.05, 1.0);
	const double p = std::pow(eta, 16.0);
	const double logZ = std::log(std::max(zEval / z0, 1.000001));
	const double R = std::max(uStar / (coriolisAbs * z0), 1.000001);
	const double sigmaU = uStar * 7.5 * eta * std::pow(std::max(0.538 + 0.09 * logZ, 1.0e-6), p) /
	                      std::max(1.0 + 0.156 * std::log(R), 1.0e-6);
	if (sigmaU <= kTiny)
		return point;

	const double zh = std::clamp(zEval / boundaryLayerHeight, 1.0e-6, 0.999);
	const double cosTerm = std::pow(std::cos(0.5 * kPi * zh), 4.0);
	const double iu = sigmaU / meanU;
	const double iv = iu * (1.0 - 0.22 * cosTerm);
	const double iw = iu * (1.0 - 0.45 * cosTerm);

	const double k0 = 0.39 / std::pow(R, 0.11);
	const double b = 24.0 * std::pow(R, 0.155);
	const double n = 1.24 * std::pow(R, 0.008);
	const double kz = 0.19 - (0.19 - k0) * std::exp(-b * std::pow(zh, n));
	const double a = 0.535 + 2.76 * std::pow(std::max(0.138 - 0.115 * std::pow(1.0 + 0.315 * std::pow(1.0 - zh, 6.0), 2.0 / 3.0), 0.0), 0.68);
	const double factorA = 0.115 * std::pow(1.0 + 0.315 * std::pow(1.0 - zh, 6.0), 2.0 / 3.0);
	const double luX = factorA > 0.0 && kz > 0.0
	                       ? std::pow(factorA, 1.5) * (sigmaU / uStar) * zEval /
	                             std::max(2.5 * std::pow(kz, 1.5) * std::pow(1.0 - zh, 2.0) * (1.0 + 5.75 * zh), 1.0e-6)
	                       : 0.0;
	if (luX <= kTiny || !std::isfinite(luX))
		return point;

	const double luY = 0.5 * luX * (1.0 - 0.68 * std::exp(-35.0 * zh));
	const double luZ = 0.5 * luX * (1.0 - 0.46 * std::exp(-35.0 * std::pow(zh, 1.7)));
	const double ratioV = std::pow(std::max(iv / std::max(iu, 1.0e-9), 1.0e-6), 3.0);
	const double ratioW = std::pow(std::max(iw / std::max(iu, 1.0e-9), 1.0e-6), 3.0);

	point.valid = true;
	point.boundaryLayerHeight = boundaryLayerHeight;
	point.frictionVelocity = uStar;
	point.a = std::max(a, 1.0e-3);
	point.sigma[0] = cfg.input.tiU > 0.0 ? cfg.input.tiU * 0.01 * meanU : sigmaU;
	point.sigma[1] = cfg.input.tiV > 0.0 ? cfg.input.tiV * 0.01 * meanU : iv * meanU;
	point.sigma[2] = cfg.input.tiW > 0.0 ? cfg.input.tiW * 0.01 * meanU : iw * meanU;
	point.xScale[0] = cfg.input.vkLu > 0.0 ? cfg.input.vkLu : luX;
	point.yScale[0] = cfg.input.vyLu > 0.0 ? cfg.input.vyLu : std::max(luY, kTiny);
	point.zScale[0] = cfg.input.vzLu > 0.0 ? cfg.input.vzLu : std::max(luZ, kTiny);
	point.xScale[1] = cfg.input.vkLv > 0.0 ? cfg.input.vkLv : std::max(0.5 * luX * ratioV, kTiny);
	point.yScale[1] = cfg.input.vyLv > 0.0 ? cfg.input.vyLv : std::max(2.0 * luY * ratioV, kTiny);
	point.zScale[1] = cfg.input.vzLv > 0.0 ? cfg.input.vzLv : std::max(luZ * ratioV, kTiny);
	point.xScale[2] = cfg.input.vkLw > 0.0 ? cfg.input.vkLw : std::max(0.5 * luX * ratioW, kTiny);
	point.yScale[2] = cfg.input.vyLw > 0.0 ? cfg.input.vyLw : std::max(luY * ratioW, kTiny);
	point.zScale[2] = cfg.input.vzLw > 0.0 ? cfg.input.vzLw : std::max(2.0 * luZ * ratioW, kTiny);
	return point;
}

double DefaultIecProfileExponent(const SimWindConfig &cfg)
{
	if (IsSteadyEwmWindModel(cfg.input))
		return 0.11;
	if (cfg.input.iecEdition == IecStandard::ED3 || cfg.input.iecEdition == IecStandard::ED4)
		return 0.14;
	return 0.2;
}

bool ComponentEnabled(const WindLInput &input, int comp)
{
	if (comp == 0)
		return input.calWu;
	if (comp == 1)
		return input.calWv;
	return input.calWw;
}

bool IsDeterministicEventWindModel(WindModel model)
{
	return model == WindModel::EOG ||
	       model == WindModel::EDC ||
	       model == WindModel::ECD ||
	       model == WindModel::EWS;
}

bool IsUniformWindModel(WindModel model)
{
	return model == WindModel::UNIFORM;
}

double SteadyEwmHubSpeed(const SimWindConfig &cfg)
{
	return cfg.input.windModel == WindModel::EWM1 ? ExtremeWindSpeed1(cfg) : ExtremeWindSpeed50(cfg);
}

bool UsesSpectralTurbulenceGeneration(const WindLInput &input)
{
	return !IsDeterministicEventWindModel(input.windModel) &&
	       !IsUniformWindModel(input.windModel) &&
	       input.turbModel != TurbModel::USER_WIND_SPEED;
}

bool IsSyntheticStochasticModel(const WindLInput &input)
{
	return UsesSpectralTurbulenceGeneration(input);
}

bool HasExplicitReynoldsTarget(double value)
{
	return std::abs(value) > kTiny;
}

bool HasAnyMeteorologyHints(const WindLInput &input)
{
	return std::abs(input.richardson) > kTiny ||
	       input.uStar > 0.0 ||
	       std::abs(input.zOverL) > kTiny ||
	       input.mixingLayerDepth > 0.0 ||
	       HasExplicitReynoldsTarget(input.reynoldsUW) ||
	       HasExplicitReynoldsTarget(input.reynoldsUV) ||
	       HasExplicitReynoldsTarget(input.reynoldsVW);
}

bool UsesStrictCoherence(const SimWindConfig &cfg, int comp)
{
	if (!UsesSpectralTurbulenceGeneration(cfg.input))
		return false;
	if (!ComponentEnabled(cfg.input, comp))
		return false;

	const CohModel model = cfg.cohModel[static_cast<std::size_t>(comp)];
	if (model == CohModel::NONE)
		return false;
	if (model == CohModel::API)
		return comp == 0;

	return cfg.cohDecay[static_cast<std::size_t>(comp)] < kHugeDecay * 0.5;
}

bool SupportsKroneckerApproximation(const SimWindConfig &cfg, int comp)
{
	if (!UsesStrictCoherence(cfg, comp))
		return false;
	if (cfg.ny <= 1 || cfg.nz <= 1)
		return false;
	if (!cfg.input.allowCohApprox)
		return false;
	if (cfg.cohModel[static_cast<std::size_t>(comp)] == CohModel::API)
		return false;
	if (cfg.cohModel[static_cast<std::size_t>(comp)] == CohModel::GENERAL && cfg.input.cohExp > 0.0)
		return false;
	return true;
}

bool ShouldUseKroneckerApproximation(const SimWindConfig &cfg, int comp)
{
	if (!SupportsKroneckerApproximation(cfg, comp))
		return false;

	const double nFreq = static_cast<double>(std::max(cfg.nFreq - 1, 0));
	const double nPoints = static_cast<double>(cfg.nPoints);
	const double denseFlops = nFreq * nPoints * nPoints * nPoints / 3.0;
	return cfg.nPoints >= 256 || denseFlops >= 2.0e8;
}

int EstimateKroneckerFreqLimit(const SimWindConfig &cfg, int comp)
{
	if (!ShouldUseKroneckerApproximation(cfg, comp))
		return 0;

	double minDist = std::numeric_limits<double>::max();
	for (int iy = 1; iy < cfg.ny; ++iy)
	{
		const double delta = std::abs(cfg.yCoords[static_cast<std::size_t>(iy)] - cfg.yCoords[static_cast<std::size_t>(iy - 1)]);
		if (delta > 0.01)
			minDist = std::min(minDist, delta);
	}
	for (int iz = 1; iz < cfg.nz; ++iz)
	{
		const double delta = std::abs(cfg.zCoords[static_cast<std::size_t>(iz)] - cfg.zCoords[static_cast<std::size_t>(iz - 1)]);
		if (delta > 0.01)
			minDist = std::min(minDist, delta);
	}

	if (!std::isfinite(minDist) || minDist <= 0.0)
		return std::max(cfg.nFreq - 1, 0);

	const double decay = std::max(cfg.cohDecay[static_cast<std::size_t>(comp)], kTiny);
	const double representativeU = cfg.meanUByZ.empty()
	                                   ? std::max(cfg.uHub, 0.1)
	                                   : std::max(cfg.meanUByZ[static_cast<std::size_t>(cfg.nz / 2)], 0.1);
	const double cohEpsilon = 1.0e-3;
	const double fThresh = representativeU * std::log(1.0 / cohEpsilon) / (decay * std::max(minDist, 0.1));
	return std::clamp(static_cast<int>(fThresh / cfg.df), 1, std::max(cfg.nFreq - 1, 1));
}

double InterpolateProfile(const std::vector<double> &x, const std::vector<double> &y, double xi)
{
	if (x.empty() || y.empty())
		return 0.0;
	const std::size_t n = std::min(x.size(), y.size());
	if (n == 1 || xi <= x.front())
		return y.front();
	if (xi >= x[n - 1])
		return y[n - 1];

	const auto upper = std::upper_bound(x.begin(), x.begin() + static_cast<std::ptrdiff_t>(n), xi);
	const std::size_t i1 = static_cast<std::size_t>(std::distance(x.begin(), upper));
	const std::size_t i0 = i1 - 1;
	const double span = std::max(x[i1] - x[i0], kTiny);
	const double a = (xi - x[i0]) / span;
	return y[i0] * (1.0 - a) + y[i1] * a;
}

double LinearInterpolate(const std::vector<double> &x, const std::vector<double> &y, double xi)
{
	return InterpolateProfile(x, y, xi);
}

void ValidateInput(const WindLInput &input)
{
	if (input.mode != Mode::GENERATE)
		throw std::runtime_error("SimWind only supports Mode=GENERATE.");
	if (input.gridPtsY <= 0 || input.gridPtsZ <= 0)
		throw std::runtime_error("SimWind requires NumPointY and NumPointZ to be positive.");
	if (input.fieldDimY <= 0.0 || input.fieldDimZ <= 0.0)
		throw std::runtime_error("SimWind requires LenWidthY and LenHeightZ to be positive.");
	if (input.simTime <= 0.0 && input.analysisTime <= 0.0)
		throw std::runtime_error("SimWind requires WindDuration or AnalysisTime to be positive.");
	if (input.timeStep <= 0.0)
		throw std::runtime_error("SimWind requires TimeStep to be positive.");
	if (input.meanWindSpeed <= 0.0)
		throw std::runtime_error("SimWind requires MeanWindSpeed to be positive.");
	if (input.hubHeight <= 0.0)
		throw std::runtime_error("SimWind requires HubHt to be positive.");
	if (!input.calWu && !input.calWv && !input.calWw)
		throw std::runtime_error("At least one of CalWu, CalWv, or CalWw must be true.");
	if (input.cohMod2 == CohModel::API || input.cohMod3 == CohModel::API)
		throw std::runtime_error("API coherence model is valid only for the u component (CohMod1).");
	if (input.turbModel == TurbModel::USER_SPECTRA && input.userTurbFile.empty())
		throw std::runtime_error("USER_SPECTRA requires TurbFilePath/UserTurbFile.");
	if (input.turbModel == TurbModel::USER_WIND_SPEED && input.userTurbFile.empty())
		throw std::runtime_error("USER_WIND_SPEED requires TurbFilePath/UserTurbFile.");
	if (input.turbModel == TurbModel::USRVKM && input.userShearFile.empty())
		throw std::runtime_error("USRVKM requires UserShearFile.");
}

void ApplyIecDefaults(SimWindConfig &cfg)
{
	const auto &in = cfg.input;
	const bool ed2 = in.iecEdition == IecStandard::ED2;
	const bool deterministicEvent = IsDeterministicEventWindModel(in.windModel);
	const bool uniformWind = IsUniformWindModel(in.windModel);

	if (ed2)
	{
		cfg.lambda = cfg.hubHeight < 30.0 ? 0.7 * cfg.hubHeight : 21.0;
		cfg.lc = 3.5 * cfg.lambda;
		cfg.cohDecay = {8.8, kHugeDecay, kHugeDecay};
	}
	else
	{
		cfg.lambda = cfg.hubHeight < 60.0 ? 0.7 * cfg.hubHeight : 42.0;
		cfg.lc = 8.1 * cfg.lambda;
		cfg.cohDecay = {12.0, kHugeDecay, kHugeDecay};
	}
	cfg.cohB = {0.12 / std::max(cfg.lc, kTiny), 0.0, 0.0};

	const double tiOverride = FractionalTI(in.turbIntensity);
	if (tiOverride > 0.0)
	{
		cfg.sigma[0] = tiOverride * cfg.uHub;
	}
	else if (in.tiU > 0.0)
	{
		cfg.sigma[0] = FractionalTI(in.tiU) * cfg.uHub;
	}
	else
	{
		const double iref = TurbulenceClassIref(in.turbClass);
		if (ed2)
		{
			const double slope = in.turbClass == TurbulenceClass::Class_A ? 2.0 : 3.0;
			const double ti15 = in.turbClass == TurbulenceClass::Class_A ? 0.18 : 0.16;
			cfg.sigma[0] = ti15 * ((15.0 + slope * cfg.uHub) / (slope + 1.0));
		}
		else if (in.windModel == WindModel::ETM)
		{
			const double vRef = ClampPositive(in.vRef, DefaultVRef(in.turbineClass));
			const double vAve = 0.2 * vRef;
			const double c = ClampPositive(in.etmC, 2.0);
			cfg.sigma[0] = c * iref * (0.072 * (vAve / c + 3.0) * (cfg.uHub / c - 4.0) + 10.0);
		}
		else if (IsEwmWindModel(in.windModel))
		{
			cfg.sigma[0] = in.ewmType == EWMType::Turbulent ? 0.11 * ExtremeWindSpeed50(cfg) : 0.0;
		}
		else
		{
			cfg.sigma[0] = iref * (0.75 * cfg.uHub + 5.6);
		}
	}

	if (in.turbModel == TurbModel::IEC_VKAIMAL)
	{
		cfg.sigma[1] = in.tiV > 0.0 ? FractionalTI(in.tiV) * cfg.uHub : cfg.sigma[0];
		cfg.sigma[2] = in.tiW > 0.0 ? FractionalTI(in.tiW) * cfg.uHub : cfg.sigma[0];
		cfg.integralScale = {3.5 * cfg.lambda, 3.5 * cfg.lambda, 3.5 * cfg.lambda};
	}
	else if (in.turbModel == TurbModel::B_VKAL)
	{
		cfg.sigma[1] = in.tiV > 0.0 ? FractionalTI(in.tiV) * cfg.uHub : 0.8 * cfg.sigma[0];
		cfg.sigma[2] = in.tiW > 0.0 ? FractionalTI(in.tiW) * cfg.uHub : 0.5 * cfg.sigma[0];
		cfg.integralScale = {3.5 * cfg.lambda, 3.5 * cfg.lambda, 3.5 * cfg.lambda};
	}
	else if (in.turbModel == TurbModel::B_IVKAL)
	{
		cfg.sigma[1] = in.tiV > 0.0 ? FractionalTI(in.tiV) * cfg.uHub : 0.8 * cfg.sigma[0];
		cfg.sigma[2] = in.tiW > 0.0 ? FractionalTI(in.tiW) * cfg.uHub : 0.5 * cfg.sigma[0];
		cfg.integralScale = {3.5 * cfg.lambda, 3.5 * cfg.lambda, 3.5 * cfg.lambda};
	}
	else
	{
		cfg.sigma[1] = in.tiV > 0.0 ? FractionalTI(in.tiV) * cfg.uHub : 0.8 * cfg.sigma[0];
		cfg.sigma[2] = in.tiW > 0.0 ? FractionalTI(in.tiW) * cfg.uHub : 0.5 * cfg.sigma[0];
		cfg.integralScale = {8.1 * cfg.lambda, 2.7 * cfg.lambda, 0.66 * cfg.lambda};
	}

	if (in.vkLu > 0.0) cfg.integralScale[0] = in.vkLu;
	if (in.vkLv > 0.0) cfg.integralScale[1] = in.vkLv;
	if (in.vkLw > 0.0) cfg.integralScale[2] = in.vkLw;
	cfg.lateralScale = {0.3 * cfg.integralScale[0], 0.3 * cfg.integralScale[1], 0.3 * cfg.integralScale[2]};
	cfg.verticalScale = cfg.lateralScale;
	if (in.vyLu > 0.0) cfg.lateralScale[0] = in.vyLu;
	if (in.vyLv > 0.0) cfg.lateralScale[1] = in.vyLv;
	if (in.vyLw > 0.0) cfg.lateralScale[2] = in.vyLw;
	if (in.vzLu > 0.0) cfg.verticalScale[0] = in.vzLu;
	if (in.vzLv > 0.0) cfg.verticalScale[1] = in.vzLv;
	if (in.vzLw > 0.0) cfg.verticalScale[2] = in.vzLw;
	if (in.cohDecayU > 0.0) cfg.cohDecay[0] = in.cohDecayU;
	if (in.cohDecayV > 0.0) cfg.cohDecay[1] = in.cohDecayV;
	if (in.cohDecayW > 0.0) cfg.cohDecay[2] = in.cohDecayW;
	if (in.cohScaleB > 0.0) cfg.cohB = {in.cohScaleB, in.cohScaleB, in.cohScaleB};

	if (deterministicEvent || uniformWind)
	{
		cfg.sigma = {0.0, 0.0, 0.0};
		cfg.cohDecay = {kHugeDecay, kHugeDecay, kHugeDecay};
		cfg.cohB = {0.0, 0.0, 0.0};
	}
}

std::array<CohModel, 3> ResolveCoherenceModels(const WindLInput &input)
{
	std::array<CohModel, 3> models{input.cohMod1, input.cohMod2, input.cohMod3};
	for (int comp = 0; comp < 3; ++comp)
	{
		auto &model = models[static_cast<std::size_t>(comp)];
		if (model != CohModel::DEFAULT_COH)
			continue;

		switch (input.turbModel)
		{
		case TurbModel::IEC_KAIMAL:
		case TurbModel::IEC_VKAIMAL:
		case TurbModel::USRVKM:
			model = comp == 0 ? CohModel::IEC : CohModel::NONE;
			break;
		case TurbModel::USER_SPECTRA:
			model = comp == 0 ? CohModel::GENERAL : CohModel::NONE;
			break;
		default:
			model = CohModel::GENERAL;
			break;
		}
	}
	return models;
}

void LoadUserShearIfNeeded(SimWindConfig &cfg)
{
	const auto &in = cfg.input;
	if (!(in.shearType == ShearType::USER ||
	      in.windProfileType == WindProfileType::USER ||
	      in.turbModel == TurbModel::USRVKM))
		return;
	if (in.userShearFile.empty())
		return;
	cfg.userShear = ReadUserShear(in.userShearFile);
	cfg.hasUserShear = !cfg.userShear.heights.empty();
}

void AppendModelConsumptionNotes(SimWindConfig &cfg)
{
	if (cfg.input.turbModel == TurbModel::USER_WIND_SPEED && HasAnyMeteorologyHints(cfg.input))
	{
		AppendWarning(cfg.warnings,
		              "USER_WIND_SPEED imports the supplied time series without inferred stability, coherence, or Reynolds-stress corrections.");
	}

	if (!IsSyntheticStochasticModel(cfg.input) &&
	    (HasExplicitReynoldsTarget(cfg.input.reynoldsUW) ||
	     HasExplicitReynoldsTarget(cfg.input.reynoldsUV) ||
	     HasExplicitReynoldsTarget(cfg.input.reynoldsVW)))
	{
		AppendWarning(cfg.warnings,
		              "Reynolds-stress targets are only applied to synthetic stochastic turbulence models; the current wind model skips this step.");
	}
}

double ProfileValueAtHub(const SimWindConfig &cfg, const std::vector<double> &valuesByZ)
{
	if (cfg.zCoords.empty() || valuesByZ.empty())
		return 0.0;
	return LinearInterpolate(cfg.zCoords, valuesByZ, cfg.hubHeight);
}

double LocalSigmaAtZ(const SimWindConfig &cfg, int comp, int iz)
{
	if (cfg.hasSigmaProfileByZ &&
	    iz >= 0 &&
	    static_cast<std::size_t>(iz) < cfg.sigmaByZ[static_cast<std::size_t>(comp)].size())
		return std::max(cfg.sigmaByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)], 0.0);
	return cfg.sigma[static_cast<std::size_t>(comp)];
}

double LocalLengthScaleAtZ(const SimWindConfig &cfg, int comp, int iz)
{
	if (cfg.hasLengthScaleProfileByZ &&
	    iz >= 0 &&
	    static_cast<std::size_t>(iz) < cfg.lengthScaleByZ[static_cast<std::size_t>(comp)].size())
		return std::max(cfg.lengthScaleByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)], kTiny);
	return std::max(cfg.integralScale[static_cast<std::size_t>(comp)], kTiny);
}

void ApplyUserProfileTurbulenceOverrides(SimWindConfig &cfg)
{
	if (cfg.hasSigmaProfileByZ)
	{
		for (int comp = 0; comp < 3; ++comp)
			cfg.sigma[static_cast<std::size_t>(comp)] = ProfileValueAtHub(cfg, cfg.sigmaByZ[static_cast<std::size_t>(comp)]);
	}

	if (cfg.hasLengthScaleProfileByZ)
	{
		for (int comp = 0; comp < 3; ++comp)
			cfg.integralScale[static_cast<std::size_t>(comp)] = ProfileValueAtHub(cfg, cfg.lengthScaleByZ[static_cast<std::size_t>(comp)]);
	}
}

void NormalizeAndValidateUserSpectra(UserSpectraData &data)
{
	const std::size_t n = data.frequencies.size();
	if (n < 3 || data.uPsd.size() != n || data.vPsd.size() != n || data.wPsd.size() != n)
		throw std::runtime_error("USER_SPECTRA requires at least 3 rows with Frequency/uPSD/vPSD/wPSD.");
	if (data.specScale1 <= 0.0 || data.specScale2 <= 0.0 || data.specScale3 <= 0.0)
		throw std::runtime_error("USER_SPECTRA requires SpecScale1/2/3 to be positive.");

	struct UserSpectraRow
	{
		double f = 0.0;
		double u = 0.0;
		double v = 0.0;
		double w = 0.0;
	};

	std::vector<UserSpectraRow> rows(n);
	for (std::size_t i = 0; i < n; ++i)
	{
		if (data.uPsd[i] <= 0.0 || data.vPsd[i] <= 0.0 || data.wPsd[i] <= 0.0)
			throw std::runtime_error("USER_SPECTRA requires strictly positive u/v/w PSD values.");
		rows[i] = {data.frequencies[i], data.uPsd[i], data.vPsd[i], data.wPsd[i]};
	}

	std::sort(rows.begin(), rows.end(), [](const UserSpectraRow &a, const UserSpectraRow &b) { return a.f < b.f; });
	for (std::size_t i = 1; i < rows.size(); ++i)
	{
		if (rows[i].f <= rows[i - 1].f + kTiny)
			throw std::runtime_error("USER_SPECTRA requires unique frequencies in strictly ascending order.");
	}

	for (std::size_t i = 0; i < rows.size(); ++i)
	{
		data.frequencies[i] = rows[i].f;
		data.uPsd[i] = rows[i].u;
		data.vPsd[i] = rows[i].v;
		data.wPsd[i] = rows[i].w;
	}
	data.numFrequencies = static_cast<int>(rows.size());
}

void ResolveMeteorologyClosure(SimWindConfig &cfg)
{
	cfg.met.richardson = cfg.input.richardson;
	cfg.met.zL = std::abs(cfg.input.zOverL) > kTiny
	                 ? cfg.input.zOverL
	                 : DeriveZLFromRichardson(cfg.input.richardson);
	cfg.met.moninLength = MoninLengthFromZL(cfg.hubHeight, cfg.met.zL);
	cfg.met.coriolis = 2.0 * kOmega * std::sin(std::abs(cfg.input.latitude) * kRad);

	const double zLAtRef = ZOverLAtHeight(cfg.refHeight, cfg.met.moninLength);
	cfg.met.uStarDiab = UstarDiabatic(cfg.input.meanWindSpeed,
	                                  cfg.refHeight,
	                                  cfg.input.roughness,
	                                  zLAtRef);
	cfg.met.uStar = cfg.input.uStar > 0.0 ? cfg.input.uStar : cfg.met.uStarDiab;

	if (cfg.input.mixingLayerDepth > 0.0)
		cfg.met.mixingLayerDepth = cfg.input.mixingLayerDepth;
	else
		cfg.met.mixingLayerDepth = DefaultMixingLayerDepth(cfg.met.uStar,
		                                                   cfg.met.uStarDiab,
		                                                   cfg.input.meanWindSpeed,
		                                                   cfg.refHeight,
		                                                   cfg.input.roughness,
		                                                   cfg.met.coriolis);

	if (IsSyntheticStochasticModel(cfg.input) && cfg.met.zL < 0.0 &&
	    (!std::isfinite(cfg.met.mixingLayerDepth) || cfg.met.mixingLayerDepth <= 0.0))
	{
		throw std::runtime_error("Unstable synthetic turbulence generation requires a positive mixing-layer depth (ZI).");
	}

	const double stabilityFactor = std::clamp(1.0 + 0.35 * cfg.met.zL, 0.65, 1.50);
	const double baseDecay = std::max(cfg.uHub, 0.1) * stabilityFactor;
	cfg.met.defaultGeneralCohDecay = {baseDecay, 0.75 * baseDecay, 0.75 * baseDecay};

	const double mixingScale = std::max({cfg.lc,
	                                     cfg.hubHeight,
	                                     cfg.met.mixingLayerDepth > 0.0 ? 0.25 * cfg.met.mixingLayerDepth : 0.0,
	                                     1.0});
	const double baseB = 0.12 / mixingScale;
	cfg.met.defaultGeneralCohB = {baseB, baseB, baseB};

	const bool explicitUW = HasExplicitReynoldsTarget(cfg.input.reynoldsUW);
	const bool explicitUV = HasExplicitReynoldsTarget(cfg.input.reynoldsUV);
	const bool explicitVW = HasExplicitReynoldsTarget(cfg.input.reynoldsVW);

	cfg.met.reynoldsStress.skip = {true, true, true};
	cfg.met.reynoldsStress.target = {0.0, 0.0, 0.0};
	if (IsSyntheticStochasticModel(cfg.input))
	{
		cfg.met.reynoldsStress.target[0] = explicitUW ? cfg.input.reynoldsUW : -(cfg.met.uStar * cfg.met.uStar);
		cfg.met.reynoldsStress.skip[0] = !(explicitUW || cfg.met.uStar > kTiny);
		cfg.met.reynoldsStress.target[1] = explicitUV ? cfg.input.reynoldsUV : 0.0;
		cfg.met.reynoldsStress.target[2] = explicitVW ? cfg.input.reynoldsVW : 0.0;
		cfg.met.reynoldsStress.skip[1] = !explicitUV;
		cfg.met.reynoldsStress.skip[2] = !explicitVW;
	}
	cfg.met.reynoldsStress.active = !cfg.met.reynoldsStress.skip[0] ||
	                                !cfg.met.reynoldsStress.skip[1] ||
	                                !cfg.met.reynoldsStress.skip[2];
}

void ApplyMeteorologyCoherenceDefaults(SimWindConfig &cfg)
{
	for (int comp = 0; comp < 3; ++comp)
	{
		const std::size_t cs = static_cast<std::size_t>(comp);
		const CohModel model = cfg.cohModel[cs];
		if (model == CohModel::NONE)
		{
			cfg.cohDecay[cs] = kHugeDecay;
			cfg.cohB[cs] = 0.0;
			continue;
		}
		if (model == CohModel::GENERAL)
		{
			if (!cfg.hasExplicitCohDecay[cs])
				cfg.cohDecay[cs] = cfg.met.defaultGeneralCohDecay[cs];
			if (!cfg.hasExplicitCohB)
				cfg.cohB[cs] = cfg.met.defaultGeneralCohB[cs];
		}
	}
}

void ValidateResolvedUserInputs(const SimWindConfig &cfg)
{
	if (cfg.input.turbModel == TurbModel::USRVKM)
	{
		if (!cfg.hasUserShear)
			throw std::runtime_error("USRVKM requires a readable UserShearFile with profile rows.");
		if (!HasProfileColumn(cfg.userShear.heights, cfg.userShear.windSpeeds) ||
		    !HasProfileColumn(cfg.userShear.heights, cfg.userShear.windDirections) ||
		    !HasProfileColumn(cfg.userShear.heights, cfg.userShear.standardDeviations) ||
		    !HasProfileColumn(cfg.userShear.heights, cfg.userShear.lengthScales))
		{
			throw std::runtime_error("USRVKM requires UserShearFile columns Height/U/WindDir/Sigma/L with matching row counts.");
		}
	}
}

void BuildGridAndProfiles(SimWindConfig &cfg)
{
	const auto &in = cfg.input;
	cfg.yCoords.resize(static_cast<std::size_t>(cfg.ny));
	cfg.zCoords.resize(static_cast<std::size_t>(cfg.nz));
	cfg.meanUByZ.resize(static_cast<std::size_t>(cfg.nz));
	cfg.directionByZ.assign(static_cast<std::size_t>(cfg.nz), 0.0);
	for (auto &values : cfg.sigmaByZ)
		values.assign(static_cast<std::size_t>(cfg.nz), 0.0);
	for (auto &values : cfg.lengthScaleByZ)
		values.assign(static_cast<std::size_t>(cfg.nz), 0.0);
	for (auto &values : cfg.lateralScaleByZ)
		values.assign(static_cast<std::size_t>(cfg.nz), 0.0);
	for (auto &values : cfg.verticalScaleByZ)
		values.assign(static_cast<std::size_t>(cfg.nz), 0.0);
	cfg.y.resize(static_cast<std::size_t>(cfg.nPoints));
	cfg.z.resize(static_cast<std::size_t>(cfg.nPoints));
	cfg.meanU.resize(static_cast<std::size_t>(cfg.nPoints));

	for (int iy = 0; iy < cfg.ny; ++iy)
		cfg.yCoords[static_cast<std::size_t>(iy)] = -0.5 * cfg.gridWidth + cfg.dy * iy;

	for (int iz = 0; iz < cfg.nz; ++iz)
	{
		const double z = cfg.zBottom + cfg.dz * iz;
		cfg.zCoords[static_cast<std::size_t>(iz)] = z;
		for (int iy = 0; iy < cfg.ny; ++iy)
		{
			const int p = GridIndex(cfg, iz, iy);
			cfg.y[static_cast<std::size_t>(p)] = cfg.yCoords[static_cast<std::size_t>(iy)];
			cfg.z[static_cast<std::size_t>(p)] = z;
		}
	}

	LoadUserShearIfNeeded(cfg);
	cfg.hasUserDirectionProfile = cfg.hasUserShear && HasProfileColumn(cfg.userShear.heights, cfg.userShear.windDirections);
	cfg.hasUserSigmaProfile = cfg.hasUserShear && HasProfileColumn(cfg.userShear.heights, cfg.userShear.standardDeviations);
	cfg.hasUserLengthScaleProfile = cfg.hasUserShear && HasProfileColumn(cfg.userShear.heights, cfg.userShear.lengthScales);
	cfg.hasSigmaProfileByZ = cfg.hasUserSigmaProfile;
	cfg.hasLengthScaleProfileByZ = cfg.hasUserLengthScaleProfile;
	cfg.hasImprovedVkProfile = false;

	if (cfg.hasUserShear)
	{
		if (!cfg.userShear.windDirections.empty() && !cfg.hasUserDirectionProfile)
			AppendWarning(cfg.warnings, "UserShear wind-direction column size does not match the height column; direction profile is ignored.");
		if (!cfg.userShear.standardDeviations.empty() && !cfg.hasUserSigmaProfile)
			AppendWarning(cfg.warnings, "UserShear standard-deviation column size does not match the height column; sigma profile is ignored.");
		if (!cfg.userShear.lengthScales.empty() && !cfg.hasUserLengthScaleProfile)
			AppendWarning(cfg.warnings, "UserShear length-scale column size does not match the height column; length-scale profile is ignored.");
	}

	const double luBase = std::max(cfg.integralScale[0], kTiny);
	const std::array<double, 3> lengthRatios{
	    1.0,
	    std::max(cfg.integralScale[1], kTiny) / luBase,
	    std::max(cfg.integralScale[2], kTiny) / luBase};

	for (int iz = 0; iz < cfg.nz; ++iz)
	{
		const double z = std::max(cfg.zCoords[static_cast<std::size_t>(iz)], 0.05);
		double u = cfg.uHub;

		if (cfg.hasUserShear)
		{
			u = LinearInterpolate(cfg.userShear.heights, cfg.userShear.windSpeeds, z);
		}
		else if (IsUniformWindModel(in.windModel))
		{
			u = cfg.uHub;
		}
		else if (IsSteadyEwmWindModel(in))
		{
			u = cfg.uHub * std::pow(z / std::max(cfg.hubHeight, kTiny), 0.11);
		}
		else if (in.shearType == ShearType::LOG || in.windProfileType == WindProfileType::LOG)
		{
			u = DiabaticLogWindSpeed(z,
			                         cfg.refHeight,
			                         cfg.input.meanWindSpeed,
			                         in.roughness,
			                         cfg.met.moninLength);
		}
		else if (in.windProfileType == WindProfileType::IEC)
		{
			u = cfg.uHub * std::pow(z / std::max(cfg.refHeight, kTiny), DefaultIecProfileExponent(cfg));
		}
		else
		{
			u = cfg.uHub * std::pow(z / std::max(cfg.refHeight, kTiny), in.shearExp);
		}

		const double uClamped = std::max(0.0, u);
		cfg.meanUByZ[static_cast<std::size_t>(iz)] = uClamped;
		if (cfg.hasUserDirectionProfile)
			cfg.directionByZ[static_cast<std::size_t>(iz)] =
			    InterpolateWrappedDirection(cfg.userShear.heights, cfg.userShear.windDirections, z);
		if (cfg.hasUserSigmaProfile)
		{
			const double sigmaBase = std::max(0.0, InterpolateProfile(cfg.userShear.heights, cfg.userShear.standardDeviations, z));
			for (int comp = 0; comp < 3; ++comp)
				cfg.sigmaByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)] = sigmaBase * UserStdScale(cfg.userShear, comp);
		}
		if (cfg.hasUserLengthScaleProfile)
		{
			const double lengthBase = std::max(kTiny, InterpolateProfile(cfg.userShear.heights, cfg.userShear.lengthScales, z));
			for (int comp = 0; comp < 3; ++comp)
				cfg.lengthScaleByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)] =
				    std::max(lengthBase * lengthRatios[static_cast<std::size_t>(comp)], kTiny);
		}
		if (IsImprovedVonKarman(in.turbModel))
		{
			const ImprovedVkProfilePoint ivk = ComputeImprovedVkPoint(cfg, z, uClamped > 0.1 ? uClamped : std::max(cfg.uHub, 0.1));
			if (ivk.valid)
			{
				cfg.hasImprovedVkProfile = true;
				cfg.hasSigmaProfileByZ = true;
				cfg.hasLengthScaleProfileByZ = true;
				for (int comp = 0; comp < 3; ++comp)
				{
					cfg.sigmaByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)] = ivk.sigma[comp];
					cfg.lengthScaleByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)] = ivk.xScale[comp];
					cfg.lateralScaleByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)] = ivk.yScale[comp];
					cfg.verticalScaleByZ[static_cast<std::size_t>(comp)][static_cast<std::size_t>(iz)] = ivk.zScale[comp];
				}
			}
		}
		for (int iy = 0; iy < cfg.ny; ++iy)
		{
			const int p = GridIndex(cfg, iz, iy);
			cfg.meanU[static_cast<std::size_t>(p)] = uClamped;
		}
	}

	if (IsImprovedVonKarman(in.turbModel) && !cfg.hasImprovedVkProfile)
	{
		AppendWarning(cfg.warnings,
		              "B_IVKAL requires valid Latitude and Roughness to derive the Bladed 4.11 improved von Karman profile; missing atmospheric inputs fall back to the Bladed von Karman sigma/length defaults.");
	}

	if (cfg.hasImprovedVkProfile)
	{
		for (int comp = 0; comp < 3; ++comp)
		{
			cfg.lateralScale[static_cast<std::size_t>(comp)] =
			    ProfileValueAtHub(cfg, cfg.lateralScaleByZ[static_cast<std::size_t>(comp)]);
			cfg.verticalScale[static_cast<std::size_t>(comp)] =
			    ProfileValueAtHub(cfg, cfg.verticalScaleByZ[static_cast<std::size_t>(comp)]);
		}
	}
}

void EstimateGenerationCost(SimWindConfig &cfg)
{
	cfg.strictCoherenceComponents = 0;
	cfg.estimatedCholeskyFlops = 0.0;
	if (IsMann(cfg.input.turbModel))
	{
		const double nMann = static_cast<double>(std::max(cfg.mannFftPoints, 1)) *
		                     static_cast<double>(std::max(cfg.mannGridY, 1)) *
		                     static_cast<double>(std::max(cfg.mannGridZ, 1));
		const double mannBytes = 3.0 * nMann * sizeof(fftw_complex);
		const double fieldBytes = 3.0 * static_cast<double>(cfg.nSteps) *
		                          static_cast<double>(cfg.nPoints) * sizeof(double);
		cfg.estimatedPeakMemoryGiB = (mannBytes + fieldBytes) / (1024.0 * 1024.0 * 1024.0);
		return;
	}

	double strictMatrixBytes = 0.0;
	for (int comp = 0; comp < 3; ++comp)
	{
		if (UsesStrictCoherence(cfg, comp))
		{
			++cfg.strictCoherenceComponents;
			if (cfg.useKronecker[static_cast<std::size_t>(comp)])
			{
				const double lowFreq = static_cast<double>(std::max(cfg.kroneckerFreqLimit[static_cast<std::size_t>(comp)], 0));
				cfg.estimatedCholeskyFlops += lowFreq *
				                              (std::pow(static_cast<double>(cfg.ny), 3.0) +
				                               std::pow(static_cast<double>(cfg.nz), 3.0)) /
				                              3.0;
				strictMatrixBytes += 3.0 * static_cast<double>((cfg.ny * cfg.ny) + (cfg.nz * cfg.nz)) * sizeof(double);
			}
			else
			{
				const double nPoints = static_cast<double>(cfg.nPoints);
				const double nFreq = static_cast<double>(std::max(cfg.nFreq - 1, 0));
				cfg.estimatedCholeskyFlops += nFreq * nPoints * nPoints * nPoints / 3.0;
				strictMatrixBytes += 3.0 * nPoints * nPoints * sizeof(double);
			}
		}
	}

	const double nPoints = static_cast<double>(cfg.nPoints);
	const double spectrumBytes = static_cast<double>(cfg.nSteps) * nPoints * sizeof(fftw_complex);
	const double fieldBytes = 3.0 * static_cast<double>(cfg.nSteps) * nPoints * sizeof(double);
	cfg.estimatedPeakMemoryGiB = (strictMatrixBytes + spectrumBytes + fieldBytes) / (1024.0 * 1024.0 * 1024.0);
}

SimWindConfig BuildConfig(const WindLInput &input)
{
	ValidateInput(input);

	SimWindConfig cfg;
	cfg.input = input;
	cfg.ny = input.gridPtsY;
	cfg.nz = input.gridPtsZ;
	cfg.nPoints = cfg.ny * cfg.nz;
	cfg.dt = input.timeStep;
	cfg.duration = input.analysisTime > 0.0 ? input.analysisTime : input.simTime;
	const int requestedSteps = std::max(2, static_cast<int>(std::ceil(cfg.duration / cfg.dt)));
	cfg.nSteps = EvenStepCount(cfg.duration, cfg.dt);
	cfg.duration = cfg.nSteps * cfg.dt;
	cfg.nFreq = cfg.nSteps / 2;
	cfg.df = 1.0 / cfg.duration;
	cfg.gridWidth = input.fieldDimY;
	cfg.gridHeight = input.fieldDimZ;
	cfg.dy = cfg.ny > 1 ? cfg.gridWidth / (cfg.ny - 1) : 0.0;
	cfg.dz = cfg.nz > 1 ? cfg.gridHeight / (cfg.nz - 1) : 0.0;
	cfg.hubHeight = input.hubHeight;
	cfg.uHub = input.meanWindSpeed;
	if (IsSteadyEwmWindModel(input))
		cfg.uHub = SteadyEwmHubSpeed(cfg);
	cfg.refHeight = input.refHeight > 0.0 ? input.refHeight : input.hubHeight;
	cfg.effectiveRotorDiameter = input.rotorDiameter > 0.0 ? input.rotorDiameter : input.fieldDimZ;
	cfg.zBottom = input.hubHeight + 0.5 * cfg.effectiveRotorDiameter - input.fieldDimZ;
	cfg.dx = cfg.uHub * cfg.dt;
	cfg.scaleIEC = input.scaleIEC >= 0 ? input.scaleIEC : (input.useIECSimmga ? 2 : 0);
	cfg.cohModel = ResolveCoherenceModels(input);
	cfg.hasExplicitCohDecay = {input.cohDecayU > 0.0, input.cohDecayV > 0.0, input.cohDecayW > 0.0};
	cfg.hasExplicitCohB = input.cohScaleB > 0.0;

	if (cfg.zBottom <= 0.0)
		cfg.warnings.push_back("Grid bottom is at or below ground; low grid heights are clamped in profile calculations.");

	ApplyIecDefaults(cfg);

	cfg.mannLength = input.mannLength > 0.0 ? input.mannLength : 0.8 * std::max(cfg.lambda, kTiny);
	cfg.mannGamma = input.mannGamma > 0.0 ? input.mannGamma : 3.9;
	cfg.mannMaxL = input.mannMaxL > 0.0 ? input.mannMaxL : 8.0 * cfg.mannLength;
	cfg.mannFftPoints = input.mannNx > 0 ? input.mannNx : cfg.nSteps;
	cfg.mannGridY = input.mannNy > 0 ? std::max(input.mannNy, cfg.ny) : cfg.ny;
	cfg.mannGridZ = input.mannNz > 0 ? std::max(input.mannNz, cfg.nz) : cfg.nz;
	if (IsMann(input.turbModel))
		cfg.integralScale = {cfg.mannLength, cfg.mannLength, cfg.mannLength};
	if (input.turbModel == TurbModel::USER_SPECTRA)
	{
		cfg.userSpectra = ReadUserSpectra(input.userTurbFile);
		cfg.hasUserSpectra = !cfg.userSpectra.frequencies.empty();
		NormalizeAndValidateUserSpectra(cfg.userSpectra);
	}

	ResolveMeteorologyClosure(cfg);
	ApplyMeteorologyCoherenceDefaults(cfg);
	BuildGridAndProfiles(cfg);
	ValidateResolvedUserInputs(cfg);
	ApplyUserProfileTurbulenceOverrides(cfg);
	AppendModelConsumptionNotes(cfg);
	for (int comp = 0; comp < 3; ++comp)
	{
		if (IsMann(input.turbModel))
		{
			cfg.useKronecker[static_cast<std::size_t>(comp)] = false;
			cfg.kroneckerFreqLimit[static_cast<std::size_t>(comp)] = 0;
		}
		else
		{
			cfg.useKronecker[static_cast<std::size_t>(comp)] = ShouldUseKroneckerApproximation(cfg, comp);
			cfg.kroneckerFreqLimit[static_cast<std::size_t>(comp)] = EstimateKroneckerFreqLimit(cfg, comp);
		}
	}
	EstimateGenerationCost(cfg);
	if (IsMann(input.turbModel) && cfg.mannFftPoints < cfg.nSteps)
		cfg.warnings.push_back("MannNx is smaller than the output time-step count; the synthesized Mann box is sampled periodically in time.");
	if (IsMann(input.turbModel) && cfg.scaleIEC < 1)
		cfg.warnings.push_back("B_MANN uses MannAlphaEps for absolute energy because ScaleIEC=0; set ScaleIEC=1 or 2 to match IEC target sigma exactly.");
	for (int comp = 0; comp < 3; ++comp)
	{
		if (cfg.useKronecker[static_cast<std::size_t>(comp)])
		{
			const char axis = comp == 0 ? 'u' : (comp == 1 ? 'v' : 'w');
			std::ostringstream note;
			note << "Component " << axis
			     << " uses the legacy WindL Kronecker coherence acceleration for the first "
			     << cfg.kroneckerFreqLimit[static_cast<std::size_t>(comp)]
			     << " positive frequencies, then switches to diagonal high-frequency synthesis. Set AllowCohApprox=false to force the exact strict-coherence path.";
			cfg.warnings.push_back(note.str());
		}
	}
	if (cfg.estimatedCholeskyFlops > 5.0e13 || cfg.estimatedPeakMemoryGiB > 12.0)
	{
		std::ostringstream warning;
		warning << "Large strict-coherence estimate: peak memory "
		        << FormatGiB(cfg.estimatedPeakMemoryGiB)
		        << ", Cholesky FLOPs "
		        << FormatScientific(cfg.estimatedCholeskyFlops)
		        << ".";
		cfg.warnings.push_back(warning.str());
		if (!cfg.input.allowCohApprox)
			AppendWarning(cfg.warnings, "AllowCohApprox=false requests the exact strict-coherence path; large grids may run much longer than the approximate path.");
	}

	const std::filesystem::path outputDir = input.savePath.empty() ? std::filesystem::current_path() : std::filesystem::path(input.savePath);
	std::filesystem::create_directories(outputDir);
	const std::string baseName = input.saveName.empty() ? "SimWind" : input.saveName;
	cfg.outputBase = outputDir / baseName;

	return cfg;
}

double UserSpectraScale(const UserSpectraData &data, int comp)
{
	if (comp == 0)
		return data.specScale1;
	if (comp == 1)
		return data.specScale2;
	return data.specScale3;
}

double InterpolateUserPsdWithEndpointHold(const std::vector<double> &freqs,
                                          const std::vector<double> &values,
                                          double freq)
{
	if (freqs.empty() || values.empty())
		return 0.0;
	const std::size_t n = std::min(freqs.size(), values.size());
	if (n == 1)
		return std::max(values.front(), 0.0);
	if (freq <= freqs.front())
		return std::max(values.front(), 0.0);
	if (freq >= freqs[n - 1])
		return std::max(values[n - 1], 0.0);
	return std::max(InterpolateProfile(freqs, values, freq), 0.0);
}

double SpectrumWithParameters(const SimWindConfig &cfg,
                              int comp,
                              double freq,
                              double uMean,
                              double sigma,
                              double integralScale,
                              double height)
{
	if (freq <= 0.0 || !ComponentEnabled(cfg.input, comp))
		return 0.0;

	const double meanU = std::max(uMean, 0.1);

	if (cfg.input.turbModel == TurbModel::USER_SPECTRA && cfg.hasUserSpectra)
	{
		const auto &data = cfg.userSpectra;
		const std::vector<double> *psd = &data.uPsd;
		if (comp == 1)
			psd = &data.vPsd;
		else if (comp == 2)
			psd = &data.wPsd;
		return UserSpectraScale(data, comp) * InterpolateUserPsdWithEndpointHold(data.frequencies, *psd, freq);
	}

	if (IsImprovedVonKarman(cfg.input.turbModel))
	{
		const ImprovedVkProfilePoint ivk = ComputeImprovedVkPoint(cfg, height, meanU);
		if (!ivk.valid)
		{
			const double lOverU = std::max(integralScale, kTiny) / meanU;
			const double flu2 = std::pow(freq * lOverU, 2.0);
			const double tmp = 1.0 + 71.0 * flu2;
			const double sigmaL = 2.0 * sigma * sigma * lOverU;
			if (comp == 0)
				return 2.0 * sigmaL / std::pow(tmp, 5.0 / 6.0);
			return sigmaL * (1.0 + 189.0 * flu2) / std::pow(tmp, 11.0 / 6.0);
		}

		const double xScale = std::max(ivk.xScale[comp], kTiny);
		const double nTilde = std::max(freq * xScale / meanU, 1.0e-9);
		const double a = std::max(ivk.a, 1.0e-6);
		const double nOverA = nTilde / a;
		const double beta1 = std::clamp(2.357 * a - 0.761, 0.0, 1.0);
		const double beta2 = std::clamp(1.0 - beta1, 0.0, 1.0);

		if (comp == 0)
		{
			const double f1 = std::pow(1.0 + 0.455 * std::exp(-0.76 * nOverA), -0.8);
			const double term1 = beta1 * 2.987 * nOverA /
			                     std::pow(1.0 + std::pow(2.0 * kPi * nOverA, 2.0), 5.0 / 6.0);
			const double term2 = beta2 * 1.294 * nOverA /
			                     std::max(f1 * std::pow(1.0 + std::pow(kPi * nOverA, 2.0), 5.0 / 6.0), 1.0e-12);
			return sigma * sigma * (term1 + term2) / freq;
		}

		const double f2 = std::pow(1.0 + 2.88 * std::exp(-0.218 * nOverA), -0.9);
		const double term1 = beta1 * 2.987 * (1.0 + (8.0 / 3.0) * std::pow(4.0 * kPi * nOverA, 2.0)) * nOverA /
		                     std::pow(1.0 + std::pow(4.0 * kPi * nOverA, 2.0), 11.0 / 6.0);
		const double term2 = beta2 * 1.294 * nOverA /
		                     std::max(f2 * std::pow(1.0 + std::pow(2.0 * kPi * nOverA, 2.0), 5.0 / 6.0), 1.0e-12);
		return sigma * sigma * (term1 + term2) / freq;
	}

	if ((IsVonKarman(cfg.input.turbModel) && !IsImprovedVonKarman(cfg.input.turbModel)) || IsMann(cfg.input.turbModel))
	{
		const double lOverU = std::max(integralScale, kTiny) / meanU;
		const double flu2 = std::pow(freq * lOverU, 2.0);
		const double tmp = 1.0 + 71.0 * flu2;
		const double sigmaL = 2.0 * sigma * sigma * lOverU;
		if (comp == 0)
			return 2.0 * sigmaL / std::pow(tmp, 5.0 / 6.0);
		return sigmaL * (1.0 + 189.0 * flu2) / std::pow(tmp, 11.0 / 6.0);
	}

	const double lOverU = std::max(integralScale, kTiny) / meanU;
	const double sigmaLU = 4.0 * sigma * sigma * lOverU;
	return sigmaLU / std::pow(1.0 + 6.0 * lOverU * freq, 5.0 / 3.0);
}

double SpectrumAtMeanU(const SimWindConfig &cfg, int comp, double freq, double uMean)
{
	return SpectrumWithParameters(cfg,
	                              comp,
	                              freq,
	                              uMean,
	                              cfg.sigma[static_cast<std::size_t>(comp)],
	                              cfg.integralScale[static_cast<std::size_t>(comp)],
	                              cfg.hubHeight);
}

double SpectrumAt(const SimWindConfig &cfg, int comp, double freq)
{
	return SpectrumAtMeanU(cfg, comp, freq, cfg.uHub);
}

double CoherenceAtOffsets(const SimWindConfig &cfg,
                          int comp,
                          double freq,
                          double dy,
                          double dz,
                          double meanU,
                          double z1,
                          double z2)
{
	const CohModel model = cfg.cohModel[static_cast<std::size_t>(comp)];
	const double distance = std::sqrt(dy * dy + dz * dz);

	if (distance <= kTiny)
		return 1.0;
	if (model == CohModel::NONE)
		return 0.0;

	if (model == CohModel::API)
	{
		if (comp != 0)
			return 0.0;

		const double zGeom = std::sqrt(std::max(z1, 0.1) * std::max(z2, 0.1)) / 10.0;
		const double ay = 45.0 * std::pow(std::max(freq, 0.0), 0.92) *
		                  std::pow(std::abs(dy), 1.0) *
		                  std::pow(std::max(zGeom, 1.0e-3), -0.40);
		const double az = 13.0 * std::pow(std::max(freq, 0.0), 0.85) *
		                  std::pow(std::abs(dz), 1.25) *
		                  std::pow(std::max(zGeom, 1.0e-3), -0.50);
		return std::exp(-std::sqrt(ay * ay + az * az) / std::max(cfg.input.meanWindSpeed, 0.1));
	}

	const double decay = cfg.cohDecay[static_cast<std::size_t>(comp)];
	const double b = cfg.cohB[static_cast<std::size_t>(comp)];
	const double expValue = cfg.input.cohExp > 0.0 ? cfg.input.cohExp : 0.0;
	if (decay >= kHugeDecay * 0.5)
		return 0.0;

	const double meanSpeed = std::max(meanU, 0.1);
	const double distU = distance / meanSpeed;
	if (model == CohModel::IEC)
		return std::exp(-decay * std::sqrt(std::pow(freq * distU, 2.0) + std::pow(b * distance, 2.0)));

	const double localZ = 0.5 * (std::max(z1, 0.0) + std::max(z2, 0.0));
	const double distExp = expValue > 0.0 && localZ > 0.0 ? -std::pow(distance / localZ, expValue) : -1.0;
	return std::exp(decay * distExp * std::sqrt(std::pow(freq * distU, 2.0) + std::pow(b * distance, 2.0)));
}

void FillSpectralMatrix(const SimWindConfig &cfg,
                        int comp,
                        double freq,
                        const std::vector<double> &psdByPoint,
                        std::vector<double> &matrix)
{
	const int n = cfg.nPoints;
	std::fill(matrix.begin(), matrix.end(), 0.0);
	for (int i = 0; i < n; ++i)
		matrix[static_cast<std::size_t>(i) * n + i] = std::max(psdByPoint[static_cast<std::size_t>(i)], 0.0);

	if (cfg.cohDecay[static_cast<std::size_t>(comp)] >= kHugeDecay * 0.5)
		return;

	for (int i = 0; i < n; ++i)
	{
		for (int j = 0; j < i; ++j)
		{
			const double dy = cfg.y[static_cast<std::size_t>(i)] - cfg.y[static_cast<std::size_t>(j)];
			const double dz = cfg.z[static_cast<std::size_t>(i)] - cfg.z[static_cast<std::size_t>(j)];
			const double uMean = std::max(0.5 * (cfg.meanU[static_cast<std::size_t>(i)] + cfg.meanU[static_cast<std::size_t>(j)]), 0.1);
			const double coherence = CoherenceAtOffsets(cfg,
			                                           comp,
			                                           freq,
			                                           dy,
			                                           dz,
			                                           uMean,
			                                           cfg.z[static_cast<std::size_t>(i)],
			                                           cfg.z[static_cast<std::size_t>(j)]);
			const double value = std::sqrt(std::max(psdByPoint[static_cast<std::size_t>(i)], 0.0) *
			                               std::max(psdByPoint[static_cast<std::size_t>(j)], 0.0)) *
			                     coherence;
			matrix[static_cast<std::size_t>(i) * n + j] = value;
			matrix[static_cast<std::size_t>(j) * n + i] = value;
		}
	}
}

bool TryCholesky(const std::vector<double> &matrix, int n, std::vector<double> &lower, const SimWindConfig *cfg = nullptr)
{
	std::fill(lower.begin(), lower.end(), 0.0);
	const bool reportRows = cfg != nullptr && n >= 200;
	const int reportEvery = std::max(1, n / 20);
	const auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < n; ++i)
	{
		for (int j = 0; j <= i; ++j)
		{
			double sum = matrix[static_cast<std::size_t>(i) * n + j];
			for (int k = 0; k < j; ++k)
				sum -= lower[static_cast<std::size_t>(i) * n + k] * lower[static_cast<std::size_t>(j) * n + k];

			if (i == j)
			{
				if (sum <= 0.0 || !std::isfinite(sum))
					return false;
				lower[static_cast<std::size_t>(i) * n + j] = std::sqrt(sum);
			}
			else
			{
				const double diag = lower[static_cast<std::size_t>(j) * n + j];
				if (std::abs(diag) <= kTiny)
					return false;
				lower[static_cast<std::size_t>(i) * n + j] = sum / diag;
			}
		}

		if (reportRows && (i == 0 || (i + 1) % reportEvery == 0 || i + 1 == n))
		{
			const int completed = i + 1;
			const int percent = static_cast<int>(std::round(100.0 * completed / n));
			const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
			const double rowFraction = static_cast<double>(completed) / static_cast<double>(n);
			const double progress = std::max(std::pow(rowFraction, 3.0), 1.0e-9);
			const double remaining = progress > 0.0 ? elapsed * (1.0 / progress - 1.0) : 0.0;
			Report(*cfg,
			       "        Cholesky row progress " + std::to_string(percent) +
			           "%, elapsed " + FormatDuration(elapsed) +
			           ", ETA " + FormatDuration(remaining) + ".");
		}
	}
	return true;
}

std::vector<double> StrictCholeskyL(std::vector<double> matrix, int n, const SimWindConfig *cfg = nullptr)
{
	std::vector<double> lower(static_cast<std::size_t>(n) * n, 0.0);
	for (int attempt = 0; attempt < 8; ++attempt)
	{
		if (TryCholesky(matrix, n, lower, attempt == 0 ? cfg : nullptr))
			return lower;

		const double jitter = std::pow(10.0, -10 + attempt);
		for (int i = 0; i < n; ++i)
			matrix[static_cast<std::size_t>(i) * n + i] += jitter;
	}

	throw std::runtime_error("Strict coherence matrix is not positive definite.");
}

void MultiplyLower(const std::vector<double> &lower,
                   const std::vector<std::complex<double>> &phase,
                   int n,
                   std::vector<std::complex<double>> &result)
{
	result.assign(static_cast<std::size_t>(n), std::complex<double>(0.0, 0.0));
	for (int i = 0; i < n; ++i)
	{
		std::complex<double> sum(0.0, 0.0);
		for (int j = 0; j <= i; ++j)
			sum += lower[static_cast<std::size_t>(i) * n + j] * phase[static_cast<std::size_t>(j)];
		result[static_cast<std::size_t>(i)] = sum;
	}
}

void ApplyRightLowerTranspose(const std::vector<double> &lower,
                              int rows,
                              int cols,
                              std::vector<std::complex<double>> &data,
                              std::vector<std::complex<double>> &scratch)
{
	scratch.assign(data.size(), std::complex<double>(0.0, 0.0));
	for (int col = 0; col < cols; ++col)
	{
		for (int row = 0; row < rows; ++row)
		{
			std::complex<double> sum(0.0, 0.0);
			for (int k = 0; k <= col; ++k)
				sum += data[static_cast<std::size_t>(row + k * rows)] *
				       lower[static_cast<std::size_t>(col) * cols + k];
			scratch[static_cast<std::size_t>(row + col * rows)] = sum;
		}
	}
	data.swap(scratch);
}

void ApplyLeftLower(const std::vector<double> &lower,
                    int rows,
                    int cols,
                    std::vector<std::complex<double>> &data,
                    std::vector<std::complex<double>> &scratch)
{
	scratch.assign(data.size(), std::complex<double>(0.0, 0.0));
	for (int col = 0; col < cols; ++col)
	{
		for (int row = 0; row < rows; ++row)
		{
			std::complex<double> sum(0.0, 0.0);
			for (int k = 0; k <= row; ++k)
				sum += lower[static_cast<std::size_t>(row) * rows + k] *
				       data[static_cast<std::size_t>(k + col * rows)];
			scratch[static_cast<std::size_t>(row + col * rows)] = sum;
		}
	}
	data.swap(scratch);
}

double ComponentSigma(const std::vector<double> &values)
{
	if (values.empty())
		return 0.0;
	const double mean = std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
	double sum2 = 0.0;
	for (double value : values)
		sum2 += (value - mean) * (value - mean);
	return std::sqrt(sum2 / static_cast<double>(values.size()));
}

void ScaleZeroMeanComponent(std::vector<double> &values, double targetSigma)
{
	if (targetSigma <= 0.0 || values.empty())
		return;

	const double mean = std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
	for (double &value : values)
		value -= mean;

	const double actual = ComponentSigma(values);
	if (actual > kTiny)
	{
		const double scale = targetSigma / actual;
		for (double &value : values)
			value *= scale;
	}
}

int HubPointIndex(const SimWindConfig &cfg)
{
	int hubIy = 0;
	double bestY = std::numeric_limits<double>::max();
	for (int iy = 0; iy < cfg.ny; ++iy)
	{
		const double delta = std::abs(cfg.yCoords[static_cast<std::size_t>(iy)]);
		if (delta < bestY)
		{
			bestY = delta;
			hubIy = iy;
		}
	}

	int hubIz = 0;
	double bestZ = std::numeric_limits<double>::max();
	for (int iz = 0; iz < cfg.nz; ++iz)
	{
		const double delta = std::abs(cfg.zCoords[static_cast<std::size_t>(iz)] - cfg.hubHeight);
		if (delta < bestZ)
		{
			bestZ = delta;
			hubIz = iz;
		}
	}

	return GridIndex(cfg, hubIz, hubIy);
}

double HubPointSigma(const SimWindConfig &cfg, const std::vector<double> &values)
{
	if (values.empty() || cfg.nSteps <= 0 || cfg.nPoints <= 0)
		return 0.0;

	const int point = HubPointIndex(cfg);
	double mean = 0.0;
	for (int t = 0; t < cfg.nSteps; ++t)
		mean += values[static_cast<std::size_t>(t) * cfg.nPoints + point];
	mean /= static_cast<double>(cfg.nSteps);

	double sum2 = 0.0;
	for (int t = 0; t < cfg.nSteps; ++t)
	{
		const double delta = values[static_cast<std::size_t>(t) * cfg.nPoints + point] - mean;
		sum2 += delta * delta;
	}
	return std::sqrt(sum2 / static_cast<double>(cfg.nSteps));
}

void ScaleComponentToHubSigma(const SimWindConfig &cfg, std::vector<double> &values, double targetSigma)
{
	if (targetSigma <= 0.0 || values.empty())
		return;

	const double actual = HubPointSigma(cfg, values);
	if (actual <= kTiny)
		return;

	const double scale = targetSigma / actual;
	for (double &value : values)
		value *= scale;
}

void ScaleComponentPerPointSigma(const SimWindConfig &cfg, std::vector<double> &values, double targetSigma)
{
	if (targetSigma <= 0.0 || values.empty() || cfg.nSteps <= 0 || cfg.nPoints <= 0)
		return;

	for (int point = 0; point < cfg.nPoints; ++point)
	{
		double mean = 0.0;
		for (int t = 0; t < cfg.nSteps; ++t)
			mean += values[static_cast<std::size_t>(t) * cfg.nPoints + point];
		mean /= static_cast<double>(cfg.nSteps);

		double sum2 = 0.0;
		for (int t = 0; t < cfg.nSteps; ++t)
		{
			const double delta = values[static_cast<std::size_t>(t) * cfg.nPoints + point] - mean;
			sum2 += delta * delta;
		}

		const double actual = std::sqrt(sum2 / static_cast<double>(cfg.nSteps));
		if (actual <= kTiny)
			continue;

		const double scale = targetSigma / actual;
		for (int t = 0; t < cfg.nSteps; ++t)
			values[static_cast<std::size_t>(t) * cfg.nPoints + point] *= scale;
	}
}

void ApplyScaleIecForComponent(const SimWindConfig &cfg, WindField &field, int comp)
{
	if (cfg.scaleIEC < 1)
		return;

	auto &values = field.component[static_cast<std::size_t>(comp)];
	const double targetSigma = cfg.sigma[static_cast<std::size_t>(comp)];
	if (cfg.scaleIEC == 1 || comp > 0)
		ScaleComponentToHubSigma(cfg, values, targetSigma);
	else
		ScaleComponentPerPointSigma(cfg, values, targetSigma);
}

WindField AllocateField(const SimWindConfig &cfg)
{
	WindField field;
	field.nSteps = cfg.nSteps;
	field.nPoints = cfg.nPoints;
	const std::size_t total = static_cast<std::size_t>(cfg.nSteps) * cfg.nPoints;
	for (auto &component : field.component)
		component.assign(total, 0.0);
	return field;
}

struct UserWindSpatialWeight
{
	int index = 0;
	double weight = 0.0;
};

std::vector<UserWindSpatialWeight> BuildUserWindSpatialWeights(const UserWindSpeedData &data, double targetY, double targetZ)
{
	std::vector<std::pair<double, int>> distances;
	distances.reserve(data.points.size());
	for (int src = 0; src < static_cast<int>(data.points.size()); ++src)
	{
		const double dy = targetY - data.points[static_cast<std::size_t>(src)].y;
		const double dz = targetZ - data.points[static_cast<std::size_t>(src)].z;
		const double d2 = dy * dy + dz * dz;
		if (d2 <= 1.0e-12)
			return {{src, 1.0}};
		distances.emplace_back(d2, src);
	}

	std::sort(distances.begin(), distances.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
	const std::size_t keep = std::min<std::size_t>(4, distances.size());
	std::vector<UserWindSpatialWeight> weights;
	weights.reserve(keep);

	double weightSum = 0.0;
	for (std::size_t i = 0; i < keep; ++i)
	{
		const double w = 1.0 / std::max(distances[i].first, 1.0e-12);
		weights.push_back({distances[i].second, w});
		weightSum += w;
	}

	if (weightSum <= kTiny)
		return {};
	for (auto &item : weights)
		item.weight /= weightSum;
	return weights;
}

double InterpolateUserWindTimeSeries(const std::vector<double> &time,
                                     const std::vector<double> &values,
                                     double targetTime)
{
	if (time.empty() || values.empty())
		return 0.0;
	const std::size_t n = std::min(time.size(), values.size());
	if (n == 1 || targetTime <= time.front())
		return values.front();
	if (targetTime >= time[n - 1])
		return values[n - 1];

	const auto upper = std::lower_bound(time.begin(), time.begin() + static_cast<std::ptrdiff_t>(n), targetTime);
	const std::size_t i1 = static_cast<std::size_t>(std::distance(time.begin(), upper));
	if (i1 == 0)
		return values.front();
	const std::size_t i0 = i1 - 1;
	const double t0 = time[i0];
	const double t1 = time[i1];
	const double span = std::max(t1 - t0, kTiny);
	const double a = (targetTime - t0) / span;
	return values[i0] * (1.0 - a) + values[i1] * a;
}

WindField LoadUserWindSpeedField(const SimWindConfig &cfg)
{
	const UserWindSpeedData data = ReadUserWindSpeed(cfg.input.userTurbFile);
	if (data.time.empty() || data.components.empty())
		throw std::runtime_error("USER_WIND_SPEED requires a non-empty user wind speed file.");

	WindField field = AllocateField(cfg);
	if (!std::is_sorted(data.time.begin(), data.time.end()))
		throw std::runtime_error("USER_WIND_SPEED requires time samples sorted in ascending order.");

	std::vector<std::vector<UserWindSpatialWeight>> weightsByPoint(static_cast<std::size_t>(cfg.nPoints));

	for (int p = 0; p < cfg.nPoints; ++p)
	{
		weightsByPoint[static_cast<std::size_t>(p)] =
		    BuildUserWindSpatialWeights(data, cfg.y[static_cast<std::size_t>(p)], cfg.z[static_cast<std::size_t>(p)]);
		if (weightsByPoint[static_cast<std::size_t>(p)].empty())
			throw std::runtime_error("USER_WIND_SPEED could not build spatial interpolation weights.");
	}

	for (int p = 0; p < cfg.nPoints; ++p)
	{
		const auto &weights = weightsByPoint[static_cast<std::size_t>(p)];
		for (int t = 0; t < cfg.nSteps; ++t)
		{
			const double targetTime = t * cfg.dt;
			for (int comp = 0; comp < 3; ++comp)
			{
				double value = 0.0;
				for (const auto &item : weights)
				{
					const auto ps = static_cast<std::size_t>(item.index);
					const auto cs = static_cast<std::size_t>(comp);
					if (ps >= data.components.size() || cs >= data.components[ps].size())
						continue;
					value += item.weight * InterpolateUserWindTimeSeries(data.time, data.components[ps][cs], targetTime);
				}
				field.At(comp, t, p) = value;
			}
		}
	}

	return field;
}

void FillPsdByHeight(const SimWindConfig &cfg,
                     int comp,
                     double freq,
                     std::vector<double> &psdByZ,
                     std::vector<double> &sqrtPsdByZ)
{
	psdByZ.resize(static_cast<std::size_t>(cfg.nz));
	sqrtPsdByZ.resize(static_cast<std::size_t>(cfg.nz));
	for (int iz = 0; iz < cfg.nz; ++iz)
	{
		const double psd = std::max(SpectrumWithParameters(cfg,
		                                                   comp,
		                                                   freq,
		                                                   cfg.meanUByZ[static_cast<std::size_t>(iz)],
		                                                   LocalSigmaAtZ(cfg, comp, iz),
		                                                   LocalLengthScaleAtZ(cfg, comp, iz),
		                                                   cfg.zCoords[static_cast<std::size_t>(iz)]),
		                            0.0);
		psdByZ[static_cast<std::size_t>(iz)] = psd;
		sqrtPsdByZ[static_cast<std::size_t>(iz)] = std::sqrt(psd);
	}
}

void GenerateSpectralComponent(const SimWindConfig &cfg, int comp, std::mt19937_64 &rng, WindField &field)
{
	if (!ComponentEnabled(cfg.input, comp) || cfg.sigma[static_cast<std::size_t>(comp)] <= 0.0)
	{
		Report(cfg, "  component " + std::to_string(comp + 1) + "/3 skipped.");
		return;
	}

	const bool strictCoherence = UsesStrictCoherence(cfg, comp);
	const bool useKronecker = strictCoherence && cfg.useKronecker[static_cast<std::size_t>(comp)];
	const int kroneckerFreqLimit = useKronecker ? cfg.kroneckerFreqLimit[static_cast<std::size_t>(comp)] : 0;
	Report(cfg,
	       "    " + std::string(comp == 0 ? "u" : (comp == 1 ? "v" : "w")) + "-component matrices (" +
	           (strictCoherence ? (useKronecker ? "legacy Kronecker strict coherence" : "strict coherence")
	                            : "uncorrelated spatial phases") +
	           ").");
	if (useKronecker)
	{
		Report(cfg,
		       "      Kronecker factorization active for " + std::to_string(kroneckerFreqLimit) +
		           " of " + std::to_string(std::max(cfg.nFreq - 1, 0)) +
		           " positive frequencies; higher frequencies use diagonal synthesis.");
	}

	FftwBatchPlan1D spectrum(cfg.nSteps, cfg.nPoints);
	spectrum.ZeroAll();
	std::uniform_real_distribution<double> phaseDist(0.0, 2.0 * kPi);
	std::vector<std::complex<double>> randomPhase(static_cast<std::size_t>(cfg.nPoints));
	std::vector<std::complex<double>> correlated;
	std::vector<std::complex<double>> scratch;
	std::vector<double> psdByZ;
	std::vector<double> sqrtPsdByZ;
	std::vector<double> psdByPoint;
	std::vector<double> denseSpectral;
	std::vector<double> sy;
	std::vector<double> sz;
	if (strictCoherence && !useKronecker)
		denseSpectral.assign(static_cast<std::size_t>(cfg.nPoints) * cfg.nPoints, 0.0);
	if (useKronecker)
	{
		sy.assign(static_cast<std::size_t>(cfg.ny) * cfg.ny, 0.0);
		sz.assign(static_cast<std::size_t>(cfg.nz) * cfg.nz, 0.0);
	}

	const int reportEvery = std::max(1, cfg.nFreq / 10);
	const auto freqStart = std::chrono::steady_clock::now();
	Report(cfg, "      frequency progress 0% (ETA unknown until first block completes).");

	for (int k = 1; k < cfg.nFreq; ++k)
	{
		const double freq = k * cfg.df;
		FillPsdByHeight(cfg, comp, freq, psdByZ, sqrtPsdByZ);
		const bool hasEnergy = std::any_of(psdByZ.begin(), psdByZ.end(), [](double value) { return value > 0.0; });
		if (!hasEnergy)
			continue;

		for (int p = 0; p < cfg.nPoints; ++p)
		{
			const double phi = phaseDist(rng);
			randomPhase[static_cast<std::size_t>(p)] = std::complex<double>(std::cos(phi), std::sin(phi));
		}

		const double amplitude = cfg.nSteps * std::sqrt(cfg.df / 2.0);
		if (strictCoherence && useKronecker && k <= kroneckerFreqLimit)
		{
			std::fill(sy.begin(), sy.end(), 0.0);
			std::fill(sz.begin(), sz.end(), 0.0);

			for (int iz = 0; iz < cfg.nz; ++iz)
			{
				sz[static_cast<std::size_t>(iz) * cfg.nz + iz] = psdByZ[static_cast<std::size_t>(iz)];
				for (int kz = 0; kz < iz; ++kz)
				{
					const double dz = std::abs(cfg.zCoords[static_cast<std::size_t>(iz)] - cfg.zCoords[static_cast<std::size_t>(kz)]);
					const double meanU = 0.5 * (cfg.meanUByZ[static_cast<std::size_t>(iz)] + cfg.meanUByZ[static_cast<std::size_t>(kz)]);
					const double coherence = CoherenceAtOffsets(cfg,
					                                           comp,
					                                           freq,
					                                           0.0,
					                                           dz,
					                                           meanU,
					                                           cfg.zCoords[static_cast<std::size_t>(iz)],
					                                           cfg.zCoords[static_cast<std::size_t>(kz)]);
					const double value = sqrtPsdByZ[static_cast<std::size_t>(iz)] *
					                     sqrtPsdByZ[static_cast<std::size_t>(kz)] * coherence;
					sz[static_cast<std::size_t>(iz) * cfg.nz + kz] = value;
					sz[static_cast<std::size_t>(kz) * cfg.nz + iz] = value;
				}
			}

			const double representativeU = cfg.meanUByZ.empty()
			                                   ? std::max(cfg.uHub, 0.1)
			                                   : std::max(cfg.meanUByZ[static_cast<std::size_t>(cfg.nz / 2)], 0.1);
			for (int iy = 0; iy < cfg.ny; ++iy)
			{
				sy[static_cast<std::size_t>(iy) * cfg.ny + iy] = 1.0;
				for (int jy = 0; jy < iy; ++jy)
				{
					const double dy = std::abs(cfg.yCoords[static_cast<std::size_t>(iy)] - cfg.yCoords[static_cast<std::size_t>(jy)]);
					const double coherence = CoherenceAtOffsets(cfg,
					                                           comp,
					                                           freq,
					                                           dy,
					                                           0.0,
					                                           representativeU,
					                                           cfg.hubHeight,
					                                           cfg.hubHeight);
					sy[static_cast<std::size_t>(iy) * cfg.ny + jy] = coherence;
					sy[static_cast<std::size_t>(jy) * cfg.ny + iy] = coherence;
				}
			}

			correlated = randomPhase;
			const auto lz = StrictCholeskyL(sz, cfg.nz);
			const auto ly = StrictCholeskyL(sy, cfg.ny);
			ApplyRightLowerTranspose(lz, cfg.ny, cfg.nz, correlated, scratch);
			ApplyLeftLower(ly, cfg.ny, cfg.nz, correlated, scratch);
			for (int p = 0; p < cfg.nPoints; ++p)
			{
				const std::complex<double> value = amplitude * correlated[static_cast<std::size_t>(p)];
				spectrum.SetSpectrum(k, p, value);
				spectrum.SetSpectrum(cfg.nSteps - k, p, std::conj(value));
			}
		}
		else if (strictCoherence && !useKronecker)
		{
			psdByPoint.resize(static_cast<std::size_t>(cfg.nPoints));
			for (int iz = 0; iz < cfg.nz; ++iz)
			{
				for (int iy = 0; iy < cfg.ny; ++iy)
					psdByPoint[static_cast<std::size_t>(GridIndex(cfg, iz, iy))] = psdByZ[static_cast<std::size_t>(iz)];
			}
			FillSpectralMatrix(cfg, comp, freq, psdByPoint, denseSpectral);
			const auto l = StrictCholeskyL(denseSpectral, cfg.nPoints, &cfg);
			MultiplyLower(l, randomPhase, cfg.nPoints, correlated);
			for (int p = 0; p < cfg.nPoints; ++p)
			{
				const std::complex<double> value = amplitude * correlated[static_cast<std::size_t>(p)];
				spectrum.SetSpectrum(k, p, value);
				spectrum.SetSpectrum(cfg.nSteps - k, p, std::conj(value));
			}
		}
		else
		{
			for (int iz = 0; iz < cfg.nz; ++iz)
			{
				const double localAmplitude = amplitude * sqrtPsdByZ[static_cast<std::size_t>(iz)];
				for (int iy = 0; iy < cfg.ny; ++iy)
				{
					const int p = GridIndex(cfg, iz, iy);
					const std::complex<double> value = localAmplitude * randomPhase[static_cast<std::size_t>(p)];
					spectrum.SetSpectrum(k, p, value);
					spectrum.SetSpectrum(cfg.nSteps - k, p, std::conj(value));
				}
			}
		}

		if (k == 1 || k % reportEvery == 0 || k == cfg.nFreq - 1)
		{
			const int totalFreq = std::max(cfg.nFreq - 1, 1);
			const int percent = static_cast<int>(std::round(100.0 * k / totalFreq));
			const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - freqStart).count();
			const double perFreq = elapsed / static_cast<double>(k);
			const double remaining = perFreq * static_cast<double>(totalFreq - k);
			Report(cfg,
			       "      frequency progress " + std::to_string(percent) +
			           "%, elapsed " + FormatDuration(elapsed) +
			           ", ETA " + FormatDuration(remaining) + ".");
		}
	}

	Report(cfg, "      FFTW batch inverse transform for all grid points.");
	const auto ifftStart = std::chrono::steady_clock::now();
	spectrum.Execute();
	const double ifftElapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - ifftStart).count();
	Report(cfg, "      FFTW batch inverse transform complete in " + FormatDuration(ifftElapsed) + ".");

	const double invN = 1.0 / static_cast<double>(cfg.nSteps);
	const int copyReportEvery = std::max(1, cfg.nSteps / 10);
	const auto copyStart = std::chrono::steady_clock::now();
	for (int t = 0; t < cfg.nSteps; ++t)
	{
		for (int p = 0; p < cfg.nPoints; ++p)
			field.At(comp, t, p) = invN * spectrum.OutputReal(t, p);

		const int completed = t + 1;
		if (completed == 1 || completed % copyReportEvery == 0 || completed == cfg.nSteps)
		{
			const int percent = static_cast<int>(std::round(100.0 * completed / std::max(cfg.nSteps, 1)));
			const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - copyStart).count();
			const double perStep = elapsed / static_cast<double>(completed);
			const double remaining = perStep * static_cast<double>(cfg.nSteps - completed);
			Report(cfg,
			       "      copy-out progress " + std::to_string(percent) +
			           "%, elapsed " + FormatDuration(elapsed) +
			           ", ETA " + FormatDuration(remaining) + ".");
		}
	}

	ApplyScaleIecForComponent(cfg, field, comp);
	Report(cfg, "    component complete.");
}

double DftWaveNumber(int index, int count, double length)
{
	const int signedIndex = index <= count / 2 ? index : index - count;
	return 2.0 * kPi * static_cast<double>(signedIndex) / std::max(length, kTiny);
}

bool MannLowerCholesky(const SimWindConfig &cfg,
                       double k1,
                       double k2,
                       double k3,
                       std::array<double, 6> &lower)
{
	lower.fill(0.0);

	const double gamma = cfg.mannGamma;
	const double length = std::max(cfg.mannLength, kTiny);
	const double alphaEps = cfg.input.mannAlphaEps > 0.0 ? cfg.input.mannAlphaEps : 0.05;
	const double k03 = k3 + gamma * k1;
	const double k0sq = k1 * k1 + k2 * k2 + k03 * k03;
	if (k0sq <= 1.0e-30)
		return false;

	const double k0 = std::sqrt(k0sq);
	const double k0L = k0 * length;
	const double k0L2 = k0L * k0L;
	const double energy = alphaEps * std::pow(length, 5.0 / 3.0) *
	                      k0L2 * k0L2 / std::pow(1.0 + k0L2, 17.0 / 6.0);
	const double c0 = energy / (4.0 * kPi * k0sq * k0sq);
	if (c0 <= 0.0 || !std::isfinite(c0))
		return false;

	const double phi00Iso = c0 * (k2 * k2 + k03 * k03);
	const double phi11Iso = c0 * (k1 * k1 + k03 * k03);
	const double phi22Iso = c0 * (k1 * k1 + k2 * k2);
	const double phi01Iso = -c0 * k1 * k2;
	const double phi02Iso = -c0 * k1 * k03;
	const double phi12Iso = -c0 * k2 * k03;

	double a00 = phi00Iso + 2.0 * gamma * phi02Iso + gamma * gamma * phi22Iso;
	double a01 = phi01Iso + gamma * phi12Iso;
	double a02 = phi02Iso + gamma * phi22Iso;
	double a11 = phi11Iso;
	double a12 = phi12Iso;
	double a22 = phi22Iso;

	const double baseJitter = std::max((std::abs(a00) + std::abs(a11) + std::abs(a22)) * 1.0e-12, 1.0e-30);
	for (int attempt = 0; attempt < 8; ++attempt)
	{
		const double jitter = baseJitter * std::pow(10.0, attempt);
		const double b00 = a00 + jitter;
		const double b11 = a11 + jitter;
		const double b22 = a22 + jitter;
		if (b00 <= 0.0)
			continue;

		const double l00 = std::sqrt(b00);
		const double l10 = a01 / l00;
		const double l20 = a02 / l00;
		const double d11 = b11 - l10 * l10;
		if (d11 <= 0.0)
			continue;

		const double l11 = std::sqrt(d11);
		const double l21 = (a12 - l20 * l10) / l11;
		const double d22 = b22 - l20 * l20 - l21 * l21;
		if (d22 <= 0.0)
			continue;

		lower = {l00, l10, l11, l20, l21, std::sqrt(d22)};
		return true;
	}

	return false;
}

int NearestMannIndex(double coord, double minCoord, double width, int count)
{
	if (count <= 1 || width <= kTiny)
		return 0;
	const double position = (coord - minCoord) / width * static_cast<double>(count - 1);
	return std::clamp(static_cast<int>(std::lround(position)), 0, count - 1);
}

WindField GenerateMannWindField(const SimWindConfig &cfg)
{
	const int nx = std::max(2, cfg.mannFftPoints);
	const int ny = std::max(1, cfg.mannGridY);
	const int nz = std::max(1, cfg.mannGridZ);
	const double lx = std::max(cfg.uHub * cfg.duration, cfg.uHub * cfg.dt * static_cast<double>(nx));
	const double ly = std::max(cfg.gridWidth, kTiny);
	const double lz = std::max(cfg.gridHeight, kTiny);
	const double dkVolume = (2.0 * kPi / lx) * (2.0 * kPi / ly) * (2.0 * kPi / lz);
	const double fftScale = std::sqrt(std::max(dkVolume, 0.0)) * static_cast<double>(nx) * ny * nz;

	Report(cfg,
	       " Calculating Mann 3D spectral tensor field: " + std::to_string(nx) + " x " +
	           std::to_string(ny) + " x " + std::to_string(nz) + ".");

	FftwComplexVolume uHat(nx, ny, nz);
	FftwComplexVolume vHat(nx, ny, nz);
	FftwComplexVolume wHat(nx, ny, nz);
	std::mt19937_64 rng(static_cast<std::uint64_t>(cfg.input.turbSeed));
	std::normal_distribution<double> normal(0.0, 1.0);
	const double invSqrt2 = 1.0 / std::sqrt(2.0);
	const int reportEvery = std::max(1, nx / 10);
	const auto start = std::chrono::steady_clock::now();

	for (int ix = 0; ix < nx; ++ix)
	{
		const int nix = (nx - ix) % nx;
		const double k1 = DftWaveNumber(ix, nx, lx);
		for (int iy = 0; iy < ny; ++iy)
		{
			const int niy = (ny - iy) % ny;
			const double k2 = DftWaveNumber(iy, ny, ly);
			for (int iz = 0; iz < nz; ++iz)
			{
				const int niz = (nz - iz) % nz;
				const std::size_t index = uHat.Index(ix, iy, iz);
				const std::size_t mirror = uHat.Index(nix, niy, niz);
				if (index > mirror)
					continue;

				std::array<double, 6> lower{};
				const double k3 = DftWaveNumber(iz, nz, lz);
				if (!MannLowerCholesky(cfg, k1, k2, k3, lower))
					continue;

				const bool selfConjugate = index == mirror;
				const auto gaussian = [&]() -> std::complex<double>
				{
					if (selfConjugate)
						return {normal(rng), 0.0};
					return {normal(rng) * invSqrt2, normal(rng) * invSqrt2};
				};

				const auto z0 = gaussian();
				const auto z1 = gaussian();
				const auto z2 = gaussian();
				const std::complex<double> u = fftScale * lower[0] * z0;
				const std::complex<double> v = fftScale * (lower[1] * z0 + lower[2] * z1);
				const std::complex<double> w = fftScale * (lower[3] * z0 + lower[4] * z1 + lower[5] * z2);
				uHat.Set(ix, iy, iz, u);
				vHat.Set(ix, iy, iz, v);
				wHat.Set(ix, iy, iz, w);
				if (!selfConjugate)
				{
					uHat.Set(nix, niy, niz, std::conj(u));
					vHat.Set(nix, niy, niz, std::conj(v));
					wHat.Set(nix, niy, niz, std::conj(w));
				}
			}
		}

		if (ix == 0 || (ix + 1) % reportEvery == 0 || ix + 1 == nx)
		{
			const int completed = ix + 1;
			const int percent = static_cast<int>(std::round(100.0 * completed / std::max(nx, 1)));
			const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
			const double perStep = elapsed / static_cast<double>(completed);
			const double remaining = perStep * static_cast<double>(nx - completed);
			Report(cfg,
			       "      Mann tensor progress " + std::to_string(percent) +
			           "%, elapsed " + FormatDuration(elapsed) +
			           ", ETA " + FormatDuration(remaining) + ".");
		}
	}

	Report(cfg, "      Mann 3D FFTW inverse transform.");
	uHat.ExecuteBackward();
	vHat.ExecuteBackward();
	wHat.ExecuteBackward();

	WindField field = AllocateField(cfg);
	const double invN = 1.0 / (static_cast<double>(nx) * ny * nz);
	const double yMin = -0.5 * cfg.gridWidth;
	const double zMin = cfg.zBottom;
	for (int iz = 0; iz < cfg.nz; ++iz)
	{
		const int miz = NearestMannIndex(cfg.zCoords[static_cast<std::size_t>(iz)], zMin, cfg.gridHeight, nz);
		for (int iy = 0; iy < cfg.ny; ++iy)
		{
			const int miy = NearestMannIndex(cfg.yCoords[static_cast<std::size_t>(iy)], yMin, cfg.gridWidth, ny);
			const int point = GridIndex(cfg, iz, iy);
			for (int t = 0; t < cfg.nSteps; ++t)
			{
				const int mix = t % nx;
				field.At(0, t, point) = invN * uHat.Real(mix, miy, miz);
				field.At(1, t, point) = invN * vHat.Real(mix, miy, miz);
				field.At(2, t, point) = invN * wHat.Real(mix, miy, miz);
			}
		}
	}

	for (int comp = 0; comp < 3; ++comp)
	{
		if (!ComponentEnabled(cfg.input, comp))
		{
			std::fill(field.component[static_cast<std::size_t>(comp)].begin(),
			          field.component[static_cast<std::size_t>(comp)].end(),
			          0.0);
			continue;
		}
		ApplyScaleIecForComponent(cfg, field, comp);
	}

	return field;
}

struct HubCovariances
{
	double uu = 0.0;
	double uv = 0.0;
	double uw = 0.0;
	double vw = 0.0;
	double ww = 0.0;
};

HubCovariances ComputeHubCovariances(const SimWindConfig &cfg, const WindField &field)
{
	HubCovariances cov;
	if (cfg.nSteps <= 0 || cfg.nPoints <= 0)
		return cov;

	const int point = HubPointIndex(cfg);
	for (int t = 0; t < cfg.nSteps; ++t)
	{
		const double u = field.At(0, t, point);
		const double v = field.At(1, t, point);
		const double w = field.At(2, t, point);
		cov.uu += u * u;
		cov.uv += u * v;
		cov.uw += u * w;
		cov.vw += v * w;
		cov.ww += w * w;
	}

	const double inv = 1.0 / static_cast<double>(cfg.nSteps);
	cov.uu *= inv;
	cov.uv *= inv;
	cov.uw *= inv;
	cov.vw *= inv;
	cov.ww *= inv;
	return cov;
}

bool CholeskyLower(const Matrix3 &matrix, int n, Matrix3 &lower)
{
	lower = ZeroMatrix3();
	const double trace = matrix[0][0] + matrix[1][1] + matrix[2][2];
	const double baseJitter = std::max(std::abs(trace) * 1.0e-12, 1.0e-12);

	for (int attempt = 0; attempt < 8; ++attempt)
	{
		const double jitter = baseJitter * std::pow(10.0, attempt);
		bool ok = true;
		for (int i = 0; i < n && ok; ++i)
		{
			for (int j = 0; j <= i; ++j)
			{
				double sum = matrix[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
				if (i == j)
					sum += jitter;
				for (int k = 0; k < j; ++k)
					sum -= lower[static_cast<std::size_t>(i)][static_cast<std::size_t>(k)] *
					       lower[static_cast<std::size_t>(j)][static_cast<std::size_t>(k)];
				if (i == j)
				{
					if (sum <= 0.0)
					{
						ok = false;
						break;
					}
					lower[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = std::sqrt(sum);
				}
				else
				{
					const double pivot = lower[static_cast<std::size_t>(j)][static_cast<std::size_t>(j)];
					if (std::abs(pivot) <= kTiny)
					{
						ok = false;
						break;
					}
					lower[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = sum / pivot;
				}
			}
		}
		if (ok)
			return true;
	}

	return false;
}

Matrix3 MultiplyMatrix(const Matrix3 &a, const Matrix3 &b, int n)
{
	Matrix3 out = ZeroMatrix3();
	for (int i = 0; i < n; ++i)
	{
		for (int j = 0; j < n; ++j)
		{
			double sum = 0.0;
			for (int k = 0; k < n; ++k)
				sum += a[static_cast<std::size_t>(i)][static_cast<std::size_t>(k)] *
				       b[static_cast<std::size_t>(k)][static_cast<std::size_t>(j)];
			out[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = sum;
		}
	}
	return out;
}

Matrix3 InvertLowerTriangular(const Matrix3 &lower, int n)
{
	Matrix3 inv = ZeroMatrix3();
	for (int i = 0; i < n; ++i)
	{
		inv[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)] =
		    1.0 / std::max(lower[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)], kTiny);
		for (int j = 0; j < i; ++j)
		{
			double sum = 0.0;
			for (int k = j; k < i; ++k)
				sum += lower[static_cast<std::size_t>(i)][static_cast<std::size_t>(k)] *
				       inv[static_cast<std::size_t>(k)][static_cast<std::size_t>(j)];
			inv[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] =
			    -sum / std::max(lower[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)], kTiny);
		}
	}
	return inv;
}

bool SoftenedTargetCovariance(Matrix3 &target, int n)
{
	const Matrix3 original = target;
	double lo = 0.0;
	double hi = 1.0;
	Matrix3 best = target;
	bool found = false;
	for (int iter = 0; iter < 32; ++iter)
	{
		const double scale = 0.5 * (lo + hi);
		Matrix3 trial = original;
		for (int i = 0; i < n; ++i)
		{
			for (int j = 0; j < i; ++j)
			{
				trial[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] *= scale;
				trial[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)] =
				    trial[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
			}
		}
		Matrix3 lower{};
		if (CholeskyLower(trial, n, lower))
		{
			best = trial;
			lo = scale;
			found = true;
		}
		else
		{
			hi = scale;
		}
	}
	if (found)
		target = best;
	return found;
}

void ApplyReynoldsStressScaling(const SimWindConfig &cfg, WindField &field)
{
	if (!cfg.met.reynoldsStress.active)
		return;

	bool anySoftened = false;
	bool anySkipped = false;

	for (int point = 0; point < cfg.nPoints; ++point)
	{
		std::array<double, 3> mean{0.0, 0.0, 0.0};
		for (int comp = 0; comp < 3; ++comp)
		{
			if (!ComponentEnabled(cfg.input, comp))
				continue;
			for (int t = 0; t < cfg.nSteps; ++t)
				mean[static_cast<std::size_t>(comp)] += field.At(comp, t, point);
			mean[static_cast<std::size_t>(comp)] /= static_cast<double>(cfg.nSteps);
		}

		std::vector<int> active;
		active.reserve(3);
		for (int comp = 0; comp < 3; ++comp)
		{
			if (ComponentEnabled(cfg.input, comp))
				active.push_back(comp);
		}
		if (active.empty())
			continue;

		const int dim = static_cast<int>(active.size());
		Matrix3 current = ZeroMatrix3();
		for (int t = 0; t < cfg.nSteps; ++t)
		{
			std::array<double, 3> sample{0.0, 0.0, 0.0};
			for (int i = 0; i < dim; ++i)
			{
				const int comp = active[static_cast<std::size_t>(i)];
				sample[static_cast<std::size_t>(i)] = field.At(comp, t, point) - mean[static_cast<std::size_t>(comp)];
			}
			for (int i = 0; i < dim; ++i)
			{
				for (int j = 0; j <= i; ++j)
				{
					current[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] +=
					    sample[static_cast<std::size_t>(i)] * sample[static_cast<std::size_t>(j)];
				}
			}
		}
		for (int i = 0; i < dim; ++i)
		{
			for (int j = 0; j <= i; ++j)
			{
				current[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] /= static_cast<double>(cfg.nSteps);
				current[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)] =
				    current[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
			}
		}

		Matrix3 currentLower{};
		if (!CholeskyLower(current, dim, currentLower))
		{
			anySkipped = true;
			continue;
		}

		const int iz = point / cfg.ny;
		Matrix3 target = ZeroMatrix3();
		for (int i = 0; i < dim; ++i)
		{
			const int compI = active[static_cast<std::size_t>(i)];
			const double sigmaTarget = cfg.scaleIEC >= 1
			                               ? LocalSigmaAtZ(cfg, compI, iz)
			                               : std::sqrt(std::max(current[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)], 0.0));
			target[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)] = sigmaTarget * sigmaTarget;
			for (int j = 0; j < i; ++j)
			{
				const int compJ = active[static_cast<std::size_t>(j)];
				double offdiag = current[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
				if (HasTargetReynoldsStress(cfg.met.reynoldsStress, compI, compJ))
					offdiag = TargetReynoldsStress(cfg.met.reynoldsStress, compI, compJ);
				const double sigmaI = std::sqrt(std::max(target[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)], 0.0));
				const double sigmaJ = std::sqrt(std::max(target[static_cast<std::size_t>(j)][static_cast<std::size_t>(j)], 0.0));
				const double maxCov = 0.995 * sigmaI * sigmaJ;
				offdiag = std::clamp(offdiag, -maxCov, maxCov);
				target[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = offdiag;
				target[static_cast<std::size_t>(j)][static_cast<std::size_t>(i)] = offdiag;
			}
		}

		Matrix3 targetLower{};
		if (!CholeskyLower(target, dim, targetLower))
		{
			if (!SoftenedTargetCovariance(target, dim) || !CholeskyLower(target, dim, targetLower))
			{
				anySkipped = true;
				continue;
			}
			anySoftened = true;
		}

		const Matrix3 invCurrentLower = InvertLowerTriangular(currentLower, dim);
		const Matrix3 transform = MultiplyMatrix(targetLower, invCurrentLower, dim);

		for (int t = 0; t < cfg.nSteps; ++t)
		{
			std::array<double, 3> centered{0.0, 0.0, 0.0};
			std::array<double, 3> mapped{0.0, 0.0, 0.0};
			for (int i = 0; i < dim; ++i)
			{
				const int comp = active[static_cast<std::size_t>(i)];
				centered[static_cast<std::size_t>(i)] = field.At(comp, t, point) - mean[static_cast<std::size_t>(comp)];
			}
			for (int i = 0; i < dim; ++i)
			{
				for (int j = 0; j < dim; ++j)
				{
					mapped[static_cast<std::size_t>(i)] +=
					    transform[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] *
					    centered[static_cast<std::size_t>(j)];
				}
			}
			for (int i = 0; i < dim; ++i)
			{
				const int comp = active[static_cast<std::size_t>(i)];
				field.At(comp, t, point) = mapped[static_cast<std::size_t>(i)];
			}
		}
	}

	if (anySoftened)
	{
		AppendWarning(MutableWarnings(cfg),
		              "Reynolds-stress target covariance was softened at some points to remain positive-definite while preserving the final component sigmas.");
	}
	if (anySkipped)
	{
		AppendWarning(MutableWarnings(cfg),
		              "Reynolds-stress scaling skipped at some points because the local covariance matrix was singular or numerically degenerate.");
	}
}

void ApplyMeanAndEvents(const SimWindConfig &cfg, WindField &field)
{
	const double hAngleBase = cfg.input.horAngle * kRad;
	const double vAngle = cfg.input.vertAngle * kRad;
	const bool deterministicEvent = IsDeterministicEventWindModel(cfg.input.windModel);
	const bool uniformWind = IsUniformWindModel(cfg.input.windModel);

	for (int t = 0; t < cfg.nSteps; ++t)
	{
		for (int p = 0; p < cfg.nPoints; ++p)
		{
			const double meanU = cfg.meanU[static_cast<std::size_t>(p)];
			const int iz = p / cfg.ny;
			const double localHAngle = hAngleBase +
			                           (cfg.hasUserDirectionProfile &&
			                                    static_cast<std::size_t>(iz) < cfg.directionByZ.size()
			                                ? cfg.directionByZ[static_cast<std::size_t>(iz)] * kRad
			                                : 0.0);
			if (deterministicEvent || uniformWind)
			{
				field.At(0, t, p) = meanU * std::cos(localHAngle) * std::cos(vAngle);
				field.At(1, t, p) = meanU * std::sin(localHAngle);
				field.At(2, t, p) = meanU * std::sin(vAngle);
			}
			else
			{
				field.At(0, t, p) += meanU * std::cos(localHAngle) * std::cos(vAngle);
				field.At(1, t, p) += meanU * std::sin(localHAngle);
				field.At(2, t, p) += meanU * std::sin(vAngle);
			}
		}
	}

	if (uniformWind || !deterministicEvent)
		return;

	const double period = ClampPositive(cfg.input.gustPeriod, std::min(10.5, cfg.duration));
	const double start = cfg.input.eventStart > 0.0 ? cfg.input.eventStart : std::max(0.0, 0.5 * (cfg.duration - period));
	const double sign = cfg.input.eventSign == EventSign::NEGATIVE ? -1.0 : 1.0;
	const double sigma1 = ReferenceSigma1Ntm(cfg);
	const double rotorDiameter = std::max(cfg.effectiveRotorDiameter, kTiny);
	const double gustAmplitude = ExtremeOperatingGustAmplitude(cfg);
	const double thetaE = ExtremeDirectionChangeDegrees(cfg) * kRad;
	const double vCog = cfg.input.ecdVcog > 0.0 ? cfg.input.ecdVcog : 15.0;
	const double thetaCg = (cfg.uHub < 4.0 ? 180.0 : 720.0 / std::max(cfg.uHub, kTiny)) * kRad * sign;
	const double shearDenom = 1.0 + 0.1 * rotorDiameter / std::max(cfg.lambda, kTiny);

	for (int t = 0; t < cfg.nSteps; ++t)
	{
		const double time = t * cfg.dt;
		const double localTime = time - start;
		const bool inEvent = localTime >= 0.0 && localTime <= period;

		for (int p = 0; p < cfg.nPoints; ++p)
		{
			const double meanU = cfg.meanU[static_cast<std::size_t>(p)];
			const double y = cfg.y[static_cast<std::size_t>(p)];
			const double z = cfg.z[static_cast<std::size_t>(p)];

			if (cfg.input.windModel == WindModel::EOG)
			{
				double dU = 0.0;
				if (inEvent)
				{
					dU = -sign * 0.37 * gustAmplitude *
					     std::sin(3.0 * kPi * localTime / period) *
					     (1.0 - std::cos(2.0 * kPi * localTime / period));
				}
				field.At(0, t, p) = meanU + dU;
				field.At(1, t, p) = 0.0;
				field.At(2, t, p) = 0.0;
			}
			else if (cfg.input.windModel == WindModel::EDC)
			{
				double theta = 0.0;
				if (inEvent)
					theta = 0.5 * thetaE * sign * (1.0 - std::cos(kPi * localTime / period));
				else if (localTime > period)
					theta = thetaE * sign;

				field.At(0, t, p) = meanU * std::cos(theta);
				field.At(1, t, p) = meanU * std::sin(theta);
				field.At(2, t, p) = 0.0;
			}
			else if (cfg.input.windModel == WindModel::ECD)
			{
				double dU = 0.0;
				double theta = 0.0;
				if (inEvent)
				{
					dU = 0.5 * vCog * (1.0 - std::cos(kPi * localTime / period));
					theta = 0.5 * thetaCg * (1.0 - std::cos(kPi * localTime / period));
				}
				else if (localTime > period)
				{
					dU = vCog;
					theta = thetaCg;
				}

				const double totalU = meanU + dU;
				field.At(0, t, p) = totalU * std::cos(theta);
				field.At(1, t, p) = totalU * std::sin(theta);
				field.At(2, t, p) = 0.0;
			}
			else if (cfg.input.windModel == WindModel::EWS)
			{
				double dU = 0.0;
				if (inEvent)
				{
					const double envelope = 0.5 * (1.0 - std::cos(2.0 * kPi * localTime / period));
					const double vertShear = sign * 2.0 * sigma1 / shearDenom *
					                         (z - cfg.hubHeight) / rotorDiameter * envelope;
					const double latShear = sign * 2.0 * sigma1 / shearDenom *
					                        y / rotorDiameter * envelope;
					dU = vertShear + latShear;
				}
				field.At(0, t, p) = meanU + dU;
				field.At(1, t, p) = 0.0;
				field.At(2, t, p) = 0.0;
			}
		}
	}
}

WindField GenerateWindField(const SimWindConfig &cfg)
{
	if (cfg.input.turbModel == TurbModel::USER_WIND_SPEED)
	{
		Report(cfg, " Loading user wind-speed time series.");
		return LoadUserWindSpeedField(cfg);
	}

	if (IsUniformWindModel(cfg.input.windModel))
	{
		Report(cfg, " WindModel=UNIFORM; generating mean profile only without turbulence.");
		WindField field = AllocateField(cfg);
		Report(cfg, " Applying mean wind profile and IEC event shape.");
		ApplyMeanAndEvents(cfg, field);
		return field;
	}

	if (IsDeterministicEventWindModel(cfg.input.windModel))
	{
		Report(cfg, " WindModel is a deterministic IEC event; generating event field without spectral turbulence.");
		WindField field = AllocateField(cfg);
		Report(cfg, " Applying mean wind profile and IEC event shape.");
		ApplyMeanAndEvents(cfg, field);
		return field;
	}

	if (IsMann(cfg.input.turbModel))
	{
		WindField field = GenerateMannWindField(cfg);
		Report(cfg, " Generating Mann time series for all points.");
		if (cfg.met.reynoldsStress.active)
		{
			Report(cfg, " Applying Reynolds-stress scaling before mean-wind addition.");
			ApplyReynoldsStressScaling(cfg, field);
		}
		Report(cfg, " Applying mean wind profile and IEC event shape.");
		ApplyMeanAndEvents(cfg, field);
		return field;
	}

	Report(cfg, " Calculating the spectral and transfer function matrices:");
	WindField field = AllocateField(cfg);
	std::mt19937_64 rng(static_cast<std::uint64_t>(cfg.input.turbSeed));
	for (int comp = 0; comp < 3; ++comp)
		GenerateSpectralComponent(cfg, comp, rng, field);
	if (cfg.met.reynoldsStress.active)
	{
		Report(cfg, " Applying Reynolds-stress scaling before mean-wind addition.");
		ApplyReynoldsStressScaling(cfg, field);
	}
	Report(cfg, " Generating time series for all points.");
	Report(cfg, " Applying mean wind profile and IEC event shape.");
	ApplyMeanAndEvents(cfg, field);
	return field;
}

SimWindComponentStats ComputeStats(const std::vector<double> &values, double uHub)
{
	SimWindComponentStats stats;
	if (values.empty())
		return stats;

	stats.mean = std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
	double sum2 = 0.0;
	for (double value : values)
		sum2 += (value - stats.mean) * (value - stats.mean);
	stats.sigma = std::sqrt(sum2 / static_cast<double>(values.size()));
	stats.turbulenceIntensity = uHub > 0.0 ? stats.sigma / uHub : 0.0;
	return stats;
}

std::array<SimWindComponentStats, 3> ComputeFieldStats(const SimWindConfig &cfg, const WindField &field)
{
	std::array<SimWindComponentStats, 3> stats{};
	for (int comp = 0; comp < 3; ++comp)
		stats[static_cast<std::size_t>(comp)] = ComputeStats(field.component[static_cast<std::size_t>(comp)], cfg.uHub);
	return stats;
}

std::pair<float, float> ScalingForComponent(const std::vector<double> &values)
{
	const auto [minIt, maxIt] = std::minmax_element(values.begin(), values.end());
	if (minIt == values.end() || std::abs(*maxIt - *minIt) <= kTiny)
	{
		const double value = minIt == values.end() ? 0.0 : *minIt;
		return {1.0f, static_cast<float>(-value)};
	}
	const double slope = 65535.0 / (*maxIt - *minIt);
	const double offset = -32768.0 - slope * (*minIt);
	return {static_cast<float>(slope), static_cast<float>(offset)};
}

std::int16_t EncodeInt16(double value)
{
	const long rounded = std::lround(std::clamp(value, -32768.0, 32767.0));
	return static_cast<std::int16_t>(rounded);
}

int BladedModelId(TurbModel model)
{
	switch (model)
	{
	case TurbModel::IEC_KAIMAL: return 5;
	case TurbModel::IEC_VKAIMAL: return 3;
	case TurbModel::B_MANN: return 8;
	case TurbModel::B_KAL: return 7;
	case TurbModel::B_VKAL: return 3;
	case TurbModel::B_IVKAL: return 4;
	case TurbModel::USRVKM: return 3;
	default: return 4;
	}
}

double LengthScaleOrDefault(double value, double fallback)
{
	return value > 0.0 ? value : std::max(fallback, kTiny);
}

float BladedTiPercent(const std::array<SimWindComponentStats, 3> &stats, const SimWindConfig &cfg, int comp)
{
	const double ti = std::max(stats[static_cast<std::size_t>(comp)].sigma / std::max(cfg.uHub, kTiny), 1.0e-6);
	return static_cast<float>(100.0 * ti);
}

void WriteBts(const SimWindConfig &cfg, const WindField &field, const std::filesystem::path &path)
{
	std::ofstream out(path, std::ios::binary);
	if (!out)
		throw std::runtime_error("Cannot open BTS output: " + path.string());

	const auto [uScl, uOff] = ScalingForComponent(field.component[0]);
	const auto [vScl, vOff] = ScalingForComponent(field.component[1]);
	const auto [wScl, wOff] = ScalingForComponent(field.component[2]);
	const std::string desc = "Qahse WindL SimWind";

	WriteScalar<std::int16_t>(out, static_cast<std::int16_t>(cfg.input.cycleWind ? 8 : 7));
	WriteScalar<std::int32_t>(out, cfg.nz);
	WriteScalar<std::int32_t>(out, cfg.ny);
	WriteScalar<std::int32_t>(out, 0);
	WriteScalar<std::int32_t>(out, cfg.nSteps);
	WriteScalar<float>(out, static_cast<float>(cfg.dz));
	WriteScalar<float>(out, static_cast<float>(cfg.dy));
	WriteScalar<float>(out, static_cast<float>(cfg.dt));
	WriteScalar<float>(out, static_cast<float>(cfg.uHub));
	WriteScalar<float>(out, static_cast<float>(cfg.hubHeight));
	WriteScalar<float>(out, static_cast<float>(cfg.zBottom));
	WriteScalar<float>(out, uScl);
	WriteScalar<float>(out, uOff);
	WriteScalar<float>(out, vScl);
	WriteScalar<float>(out, vOff);
	WriteScalar<float>(out, wScl);
	WriteScalar<float>(out, wOff);
	WriteScalar<std::int32_t>(out, static_cast<std::int32_t>(desc.size()));
	out.write(desc.data(), static_cast<std::streamsize>(desc.size()));

	for (int t = 0; t < cfg.nSteps; ++t)
	{
		for (int iz = 0; iz < cfg.nz; ++iz)
		{
			for (int iy = 0; iy < cfg.ny; ++iy)
			{
				const int p = GridIndex(cfg, iz, iy);
				WriteScalar<std::int16_t>(out, EncodeInt16(uScl * field.At(0, t, p) + uOff));
				WriteScalar<std::int16_t>(out, EncodeInt16(vScl * field.At(1, t, p) + vOff));
				WriteScalar<std::int16_t>(out, EncodeInt16(wScl * field.At(2, t, p) + wOff));
			}
		}
	}
}

void WriteBladedWnd(const SimWindConfig &cfg,
                    const WindField &field,
                    const std::array<SimWindComponentStats, 3> &stats,
                    const std::filesystem::path &path)
{
	std::ofstream out(path, std::ios::binary);
	if (!out)
		throw std::runtime_error("Cannot open Bladed WND output: " + path.string());

	const int modelId = BladedModelId(cfg.input.turbModel);
	const int componentCount = 3;

	const float zLu = static_cast<float>(LengthScaleOrDefault(cfg.input.vzLu, cfg.verticalScale[0]));
	const float yLu = static_cast<float>(LengthScaleOrDefault(cfg.input.vyLu, cfg.lateralScale[0]));
	const float xLu = static_cast<float>(LengthScaleOrDefault(cfg.input.vkLu, cfg.integralScale[0]));
	const float zLv = static_cast<float>(LengthScaleOrDefault(cfg.input.vzLv, cfg.verticalScale[1]));
	const float yLv = static_cast<float>(LengthScaleOrDefault(cfg.input.vyLv, cfg.lateralScale[1]));
	const float xLv = static_cast<float>(LengthScaleOrDefault(cfg.input.vkLv, cfg.integralScale[1]));
	const float zLw = static_cast<float>(LengthScaleOrDefault(cfg.input.vzLw, cfg.verticalScale[2]));
	const float yLw = static_cast<float>(LengthScaleOrDefault(cfg.input.vyLw, cfg.lateralScale[2]));
	const float xLw = static_cast<float>(LengthScaleOrDefault(cfg.input.vkLw, cfg.integralScale[2]));
	const float maxFreq = static_cast<float>(0.5 / cfg.dt);
	const double cohScale = cfg.cohB[0] > 0.0 ? 1.0 / cfg.cohB[0] : cfg.lc;

	WriteScalar<std::int16_t>(out, static_cast<std::int16_t>(-99));
	WriteScalar<std::int16_t>(out, static_cast<std::int16_t>(modelId));

	std::streampos headerOffsetPos = std::streampos(-1);
	if (modelId >= 7)
	{
		headerOffsetPos = out.tellp();
		WriteScalar<std::int32_t>(out, static_cast<std::int32_t>(0));
		WriteScalar<std::int32_t>(out, static_cast<std::int32_t>(componentCount));
	}

	if (modelId == 4)
	{
		WriteScalar<std::int32_t>(out, static_cast<std::int32_t>(componentCount));
		WriteScalar<float>(out, static_cast<float>(cfg.input.latitude));
		WriteScalar<float>(out, static_cast<float>(cfg.input.roughness));
		WriteScalar<float>(out, static_cast<float>(cfg.refHeight));
		WriteScalar<float>(out, BladedTiPercent(stats, cfg, 0));
		WriteScalar<float>(out, BladedTiPercent(stats, cfg, 1));
		WriteScalar<float>(out, BladedTiPercent(stats, cfg, 2));
	}

	WriteScalar<float>(out, static_cast<float>(cfg.dz));
	WriteScalar<float>(out, static_cast<float>(cfg.dy));
	WriteScalar<float>(out, static_cast<float>(cfg.dx));
	WriteScalar<std::int32_t>(out, cfg.nSteps / 2);
	WriteScalar<float>(out, static_cast<float>(cfg.uHub));
	WriteScalar<float>(out, zLu);
	WriteScalar<float>(out, yLu);
	WriteScalar<float>(out, xLu);
	WriteScalar<float>(out, maxFreq);
	WriteScalar<std::int32_t>(out, cfg.input.turbSeed);
	WriteScalar<std::int32_t>(out, cfg.nz);
	WriteScalar<std::int32_t>(out, cfg.ny);

	WriteScalar<float>(out, zLv);
	WriteScalar<float>(out, yLv);
	WriteScalar<float>(out, xLv);
	WriteScalar<float>(out, zLw);
	WriteScalar<float>(out, yLw);
	WriteScalar<float>(out, xLw);

	if (modelId == 7)
	{
		WriteScalar<float>(out, static_cast<float>(cfg.cohDecay[0]));
		WriteScalar<float>(out, static_cast<float>(cohScale));
	}
	else if (modelId == 8)
	{
		WriteScalar<float>(out, static_cast<float>(cfg.mannGamma));
		WriteScalar<float>(out, static_cast<float>(cfg.mannLength));
		WriteScalar<float>(out, static_cast<float>(std::max(stats[1].sigma, kTiny) / std::max(stats[0].sigma, kTiny)));
		WriteScalar<float>(out, static_cast<float>(std::max(stats[2].sigma, kTiny) / std::max(stats[0].sigma, kTiny)));
		WriteScalar<float>(out, static_cast<float>(cfg.mannMaxL > 0.0 ? cfg.mannMaxL : 8.0 * std::max(cfg.mannLength, 0.0)));
		WriteScalar<float>(out, 0.0f);
		WriteScalar<float>(out, 0.0f);
		WriteScalar<std::int32_t>(out, 0);
		WriteScalar<std::int32_t>(out, cfg.nSteps);
		WriteScalar<std::int32_t>(out, 0);
		WriteScalar<std::int32_t>(out, 0);
		WriteScalar<std::int32_t>(out, 0);
		WriteScalar<float>(out, 0.0f);
		WriteScalar<float>(out, 0.0f);
	}

	if (modelId >= 7)
	{
		const std::streampos endPos = out.tellp();
		const auto headerBytes = static_cast<std::int32_t>(endPos - headerOffsetPos - static_cast<std::streamoff>(4));
		out.seekp(headerOffsetPos);
		WriteScalar<std::int32_t>(out, headerBytes);
		out.seekp(endPos);
	}

	const std::array<double, 3> means{stats[0].mean, stats[1].mean, stats[2].mean};
	const std::array<double, 3> sigmas{
	    stats[0].sigma > kTiny ? stats[0].sigma : 1.0,
	    stats[1].sigma > kTiny ? stats[1].sigma : 1.0,
	    stats[2].sigma > kTiny ? stats[2].sigma : 1.0};

	for (int t = 0; t < cfg.nSteps; ++t)
	{
		for (int iz = 0; iz < cfg.nz; ++iz)
		{
			for (int iy = cfg.ny - 1; iy >= 0; --iy)
			{
				const int p = GridIndex(cfg, iz, iy);
				for (int comp = 0; comp < componentCount; ++comp)
				{
					const double normalized = 1000.0 * (field.At(comp, t, p) - means[static_cast<std::size_t>(comp)]) /
					                          sigmas[static_cast<std::size_t>(comp)];
					WriteScalar<std::int16_t>(out, EncodeInt16(normalized));
				}
			}
		}
	}
}

void WriteTurbSimWnd(const SimWindConfig &cfg,
                     const WindField &field,
                     const std::array<SimWindComponentStats, 3> &stats,
                     const std::filesystem::path &path)
{
	std::ofstream out(path, std::ios::binary);
	if (!out)
		throw std::runtime_error("Cannot open TurbSim-compatible WND output: " + path.string());

	double maxUDev = 0.0;
	for (double value : field.component[0])
		maxUDev = std::max(maxUDev, std::abs(value - cfg.uHub));
	const double uSig = std::max(stats[0].sigma, 0.05 * maxUDev);
	const double tiU = std::max(uSig / cfg.uHub, 1.0e-6);
	const double tiV = std::max(stats[1].sigma / cfg.uHub, 1.0e-6);
	const double tiW = std::max(stats[2].sigma / cfg.uHub, 1.0e-6);
	const double uC1 = 1000.0 / (cfg.uHub * tiU);
	const double uC2 = 1000.0 / tiU;
	const double vC = 1000.0 / (cfg.uHub * tiV);
	const double wC = 1000.0 / (cfg.uHub * tiW);

	WriteScalar<std::int16_t>(out, static_cast<std::int16_t>(-99));
	WriteScalar<std::int16_t>(out, static_cast<std::int16_t>(4));
	WriteScalar<std::int32_t>(out, static_cast<std::int32_t>(3));
	WriteScalar<float>(out, static_cast<float>(cfg.input.latitude));
	WriteScalar<float>(out, static_cast<float>(cfg.input.roughness));
	WriteScalar<float>(out, static_cast<float>(cfg.zBottom + 0.5 * cfg.gridHeight));
	WriteScalar<float>(out, static_cast<float>(100.0 * tiU));
	WriteScalar<float>(out, static_cast<float>(100.0 * tiV));
	WriteScalar<float>(out, static_cast<float>(100.0 * tiW));
	WriteScalar<float>(out, static_cast<float>(cfg.dz));
	WriteScalar<float>(out, static_cast<float>(cfg.dy));
	WriteScalar<float>(out, static_cast<float>(cfg.dx));
	WriteScalar<std::int32_t>(out, cfg.nSteps / 2);
	WriteScalar<float>(out, static_cast<float>(cfg.uHub));
	WriteScalar<float>(out, 0.0f);
	WriteScalar<float>(out, 0.0f);
	WriteScalar<float>(out, 0.0f);
	WriteScalar<std::int32_t>(out, 0);
	WriteScalar<std::int32_t>(out, cfg.input.turbSeed);
	WriteScalar<std::int32_t>(out, cfg.nz);
	WriteScalar<std::int32_t>(out, cfg.ny);
	for (int i = 0; i < 6; ++i)
		WriteScalar<std::int32_t>(out, 0);

	for (int t = 0; t < cfg.nSteps; ++t)
	{
		for (int iz = 0; iz < cfg.nz; ++iz)
		{
			for (int iy = 0; iy < cfg.ny; ++iy)
			{
				const int p = GridIndex(cfg, iz, iy);
				WriteScalar<std::int16_t>(out, EncodeInt16(uC1 * field.At(0, t, p) - uC2));
				WriteScalar<std::int16_t>(out, EncodeInt16(vC * field.At(1, t, p)));
				WriteScalar<std::int16_t>(out, EncodeInt16(wC * field.At(2, t, p)));
			}
		}
	}
}

void WriteSummary(const SimWindConfig &cfg,
                  const std::array<SimWindComponentStats, 3> &stats,
                  const SimWindResult &result,
                  const std::filesystem::path &path)
{
	std::ofstream out(path);
	if (!out)
		throw std::runtime_error("Cannot open SUM output: " + path.string());

	out << "Qahse WindL SimWind Summary\n";
	out << "===========================\n\n";
	out << std::setprecision(10);
	out << "Input\n";
	out << "  TurbModel: " << static_cast<int>(cfg.input.turbModel) << "\n";
	out << "  WindModel: " << static_cast<int>(cfg.input.windModel) << "\n";
	out << "  RandSeed: " << cfg.input.turbSeed << "\n";
	out << "  MeanWindSpeed: " << cfg.uHub << " m/s\n";
	out << "  HubHt: " << cfg.hubHeight << " m\n\n";

	out << "Grid\n";
	out << "  NumPointY: " << cfg.ny << "\n";
	out << "  NumPointZ: " << cfg.nz << "\n";
	out << "  LenWidthY: " << cfg.gridWidth << " m\n";
	out << "  LenHeightZ: " << cfg.gridHeight << " m\n";
	out << "  Zbottom: " << cfg.zBottom << " m\n";
	out << "  TimeStep: " << cfg.dt << " s\n";
	out << "  NumSteps: " << cfg.nSteps << "\n\n";

	out << "Generation Cost Estimate\n";
	out << "  StrictCoherenceComponents: " << cfg.strictCoherenceComponents << "\n";
	out << "  EstimatedPeakMemoryGiB: " << cfg.estimatedPeakMemoryGiB << "\n";
	out << "  EstimatedCholeskyFLOPs: " << cfg.estimatedCholeskyFlops << "\n\n";

	out << "Derived IEC Parameters\n";
	out << "  Lambda: " << cfg.lambda << " m\n";
	out << "  LC: " << cfg.lc << " m\n";
	out << "  SigmaU/SigmaV/SigmaW: " << cfg.sigma[0] << ", " << cfg.sigma[1] << ", " << cfg.sigma[2] << " m/s\n";
	out << "  IntegralScaleU/V/W: " << cfg.integralScale[0] << ", " << cfg.integralScale[1] << ", " << cfg.integralScale[2] << " m\n";
	out << "  ScaleIEC: " << cfg.scaleIEC << "\n";
	out << "  AllowCohApprox: " << (cfg.input.allowCohApprox ? "true" : "false") << "\n";
	out << "  UserDirectionProfile: " << (cfg.hasUserDirectionProfile ? "true" : "false") << "\n";
	out << "  UserSigmaProfile: " << (cfg.hasUserSigmaProfile ? "true" : "false") << "\n";
	out << "  UserLengthScaleProfile: " << (cfg.hasUserLengthScaleProfile ? "true" : "false") << "\n";
	out << "  ImprovedVKProfile: " << (cfg.hasImprovedVkProfile ? "true" : "false") << "\n";
	out << "  ZL: " << cfg.met.zL << "\n";
	out << "  L: " << cfg.met.moninLength << " m\n";
	out << "  UStar: " << cfg.met.uStar << " m/s\n";
	out << "  UStarDiab: " << cfg.met.uStarDiab << " m/s\n";
	out << "  ZI: " << cfg.met.mixingLayerDepth << " m\n";
	out << "  ReynoldsStressActive: " << (cfg.met.reynoldsStress.active ? "true" : "false") << "\n\n";

	out << "Bladed Export Parameters\n";
	out << "  Record2ModelID: " << BladedModelId(cfg.input.turbModel) << "\n";
	out << "  ExportLateralScaleU/V/W: " << cfg.lateralScale[0] << ", " << cfg.lateralScale[1] << ", " << cfg.lateralScale[2] << " m\n";
	out << "  ExportVerticalScaleU/V/W: " << cfg.verticalScale[0] << ", " << cfg.verticalScale[1] << ", " << cfg.verticalScale[2] << " m\n";
	if (IsMann(cfg.input.turbModel))
	{
		out << "  MannGamma: " << cfg.mannGamma << "\n";
		out << "  MannLength: " << cfg.mannLength << " m\n";
		out << "  MannMaxL: " << cfg.mannMaxL << " m\n";
	}
	out << "\n";

	out << "Output Files\n";
	if (!result.btsPath.empty()) out << "  BTS: " << result.btsPath << "\n";
	if (!result.bladedWndPath.empty()) out << "  Bladed WND: " << result.bladedWndPath << "\n";
	if (!result.turbsimWndPath.empty()) out << "  TurbSim-compatible WND: " << result.turbsimWndPath << "\n";
	if (!result.wndPath.empty()) out << "  WND alias: " << result.wndPath << "\n";
	out << "  SUM: " << path.string() << "\n\n";

	out << "Statistics\n";
	static const char *names[3] = {"u", "v", "w"};
	for (int comp = 0; comp < 3; ++comp)
	{
		out << "  " << names[comp] << ": mean=" << stats[static_cast<std::size_t>(comp)].mean
		    << " sigma=" << stats[static_cast<std::size_t>(comp)].sigma
		    << " TI=" << 100.0 * stats[static_cast<std::size_t>(comp)].turbulenceIntensity << "%\n";
	}

	if (!result.warnings.empty())
	{
		out << "\nWarnings\n";
		for (const auto &warning : result.warnings)
			out << "  - " << warning << "\n";
	}
}
} // namespace

SimWindResult SimWind::Generate(const WindLInput &input, SimWindProgressCallback progress)
{
	const auto startTime = std::chrono::steady_clock::now();
	if (progress)
		progress(" Reading and normalizing the SimWind input file.");
	SimWindConfig cfg = BuildConfig(input);
	cfg.progress = std::move(progress);
	Report(cfg,
	       " Grid/time prepared: " + std::to_string(cfg.ny) + " x " + std::to_string(cfg.nz) +
	           ", steps=" + std::to_string(cfg.nSteps) +
	           ", strict-coherence components=" + std::to_string(cfg.strictCoherenceComponents) +
	           ", estimated peak memory=" + FormatGiB(cfg.estimatedPeakMemoryGiB) +
	           ", estimated Cholesky FLOPs=" + FormatScientific(cfg.estimatedCholeskyFlops) + ".");
	if (cfg.estimatedCholeskyFlops > 5.0e13 || cfg.estimatedPeakMemoryGiB > 12.0)
	{
		Report(cfg, " Large strict-coherence run requested; continuing and reporting elapsed time/ETA from measured progress.");
		Report(cfg, " Initial runtime estimate: " + InitialRuntimeEstimate(cfg.estimatedCholeskyFlops) + ".");
	}
	WindField field = GenerateWindField(cfg);

	SimWindResult result;
	result.gridPtsY = cfg.ny;
	result.gridPtsZ = cfg.nz;
	result.timeSteps = cfg.nSteps;
	result.timeStep = cfg.dt;
	result.hubHeight = cfg.hubHeight;
	result.meanWindSpeed = cfg.uHub;
	result.estimatedPeakMemoryGiB = cfg.estimatedPeakMemoryGiB;
	result.estimatedCholeskyFlops = cfg.estimatedCholeskyFlops;
	result.warnings = cfg.warnings;
	Report(cfg, " Computing generated-field statistics.");
	result.stats = ComputeFieldStats(cfg, field);

	Report(cfg, " Writing requested wind output files.");
	if (input.wrTrbts)
	{
		auto path = cfg.outputBase;
		result.btsPath = path.replace_extension(".bts").string();
		Report(cfg, " Generating AeroDyn/TurbSim binary full-field file \"" + result.btsPath + "\".");
		WriteBts(cfg, field, result.btsPath);
	}

	if (input.wrBlwnd)
	{
		auto path = cfg.outputBase;
		result.bladedWndPath = path.replace_extension(".wnd").string();
		Report(cfg, " Generating Bladed binary full-field file \"" + result.bladedWndPath + "\".");
		WriteBladedWnd(cfg, field, result.stats, result.bladedWndPath);
		result.wndPath = result.bladedWndPath;
	}

	if (input.wrTrwnd)
	{
		auto path = cfg.outputBase;
		result.turbsimWndPath = path.replace_extension(input.wrBlwnd ? ".ts.wnd" : ".wnd").string();
		Report(cfg, " Generating TurbSim-compatible Bladed-style file \"" + result.turbsimWndPath + "\".");
		WriteTurbSimWnd(cfg, field, result.stats, result.turbsimWndPath);
		if (result.wndPath.empty())
			result.wndPath = result.turbsimWndPath;
	}

	{
		auto path = cfg.outputBase;
		result.sumPath = path.replace_extension(".sum").string();
		Report(cfg, " Writing statistics to summary file \"" + result.sumPath + "\".");
		WriteSummary(cfg, result.stats, result, result.sumPath);
	}

	const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
	Report(cfg, " Processing complete. " + FormatSeconds(elapsed) + " CPU seconds used.");
	return result;
}

SimWindResult SimWind::GenerateFromFile(const std::string &qwdPath, SimWindProgressCallback progress)
{
	return Generate(ReadWindLInput(qwdPath), std::move(progress));
}
