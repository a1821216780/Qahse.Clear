#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "HydroL/HydroL_Type.hpp"
#include "IO/ModuleIO.hpp"
#include "IO/Serializer.hpp"
#include "IO/ZFile.hpp"

namespace hydrol_io_detail
{
inline constexpr const char *kYamlRoot = "Qahse.HydroL";

class HydroLInputSerializer : public Serializer
{
public:
	HydroLInput data;

	HydroLInputSerializer()
	{
		SetValueFirst(true);
		SetYamlRoot(kYamlRoot);
	}

	explicit HydroLInputSerializer(const HydroLInput &input)
		: data(input)
	{
		SetValueFirst(true);
		SetYamlRoot(kYamlRoot);
	}

protected:
	void SerializeFields() override
	{
		Fields(
			Field("WaterDepth", data.waterDepth),
			Field("WaterDensity", data.waterDensity),
			Field("IsFloating", data.isFloating),
			Field("AdvancedBuoyancy", data.advancedBuoyancy),
			Field("WaveKinEvalMorison", data.waveKinEvalMorison),
			Field("WaveKinEvalPotential", data.waveKinEvalPotential),
			Field("WaveKinTau", data.waveKinTau),
			Field("WaveLFile", data.waveLFile),
			Field("StaticBuoyancy", data.staticBuoyancy));
		ReadOrWriteAny({"PotentialRadFile", "POT_RAD_FILE"}, data.potentialRadFile);
		ReadOrWriteAny({"UseRadiation", "USE_RADIATION"}, data.useRadiation);
		ReadOrWriteAny({"UseRadAddedMass", "USE_RAD_ADDED_MASS"}, data.useRadAddedMass);
		ReadOrWriteAny({"DeltaFreqRadiation", "DELTA_FREQ_RADIATION"}, data.deltaFreqRadiation);
		ReadOrWriteAny({"TruncTimeRadiation", "TRUNC_TIME_RADIATION"}, data.truncTimeRadiation);
		ReadOrWriteAny({"PotentialExcFile", "POT_EXC_FILE"}, data.potentialExcFile);
		ReadOrWriteAny({"UseExcitation", "USE_EXCITATION"}, data.useExcitation);
		ReadOrWriteAny({"DeltaFreqExcitation", "DELTA_FREQ_EXCITATION"}, data.deltaFreqExcitation);
		ReadOrWriteAny({"DeltaDirExcitation", "DELTA_DIR_EXCITATION"}, data.deltaDirExcitation);
		ReadOrWriteAny({"TruncTimeExcitation", "TRUNC_TIME_EXCITATION"}, data.truncTimeExcitation);
		ReadOrWriteAny({"PotentialDiffFile", "POT_DIFF_FILE"}, data.potentialDiffFile);
		ReadOrWriteAny({"DiffEvalType", "DIFF_EVAL_TYPE"}, data.diffEvalType);
		ReadOrWriteAny({"PotentialSumFile", "POT_SUM_FILE"}, data.potentialSumFile);
		ReadOrWriteAny({"UseSumFreqs", "USE_SUM_FREQS"}, data.useSumFreqs);
		Fields(
			Field("SubDisplacedVolume", data.subDisplacedVolume),
			Field("BuoyancyTuner", data.buoyancyTuner),
			Field("StiffTuner", data.stiffTuner),
			Field("MassTuner", data.massTuner),
			Field("BeamType", data.beamType),
			Field("SumPrint", data.output.sumPrint),
			Field("AfSpanput", data.output.afSpanput),
			Field("SumPath", data.output.sumPath));
		ReadOrWriteAny({"NBlOuts", "NumBladeOutNodes"}, data.output.blade.count);
		ReadOrWriteAny({"NTwOuts", "NumTowerOutNodes"}, data.output.tower.count);
	}
};

inline void ResolvePotentialPath(const std::string &baseFile, std::string &path)
{
	if (path.empty())
		return;
	module_io::ResolveIfSet(baseFile, path);
	if (std::filesystem::exists(path))
		return;

	const auto candidate = std::filesystem::path(baseFile).parent_path() /
	                       "WAMIT" /
	                       std::filesystem::path(path).filename();
	if (std::filesystem::exists(candidate))
		path = candidate.lexically_normal().string();
}

inline void ResolvePaths(HydroLInput &input)
{
	const std::string base = input.inputPath.string();
	module_io::ResolveIfSet(base, input.waveLFile);
	ResolvePotentialPath(base, input.potentialRadFile);
	ResolvePotentialPath(base, input.potentialExcFile);
	ResolvePotentialPath(base, input.potentialDiffFile);
	ResolvePotentialPath(base, input.potentialSumFile);
	module_io::ResolveOutputPath(base, input.output);
}

inline void Validate(const HydroLInput &input)
{
	if (!module_io::FileExistsOrEmpty(input.waveLFile))
		throw std::runtime_error("HydroL WaveLFile does not exist: " + input.waveLFile);
	const bool hasEmbeddedRadiation = input.wamit && !input.wamit->radiation.radiation.empty();
	const bool hasEmbeddedExcitation = input.wamit && !input.wamit->excitation.excitation.empty();
	const bool hasEmbeddedDifference = input.wamit && !input.wamit->difference.qtf.empty();
	const bool hasEmbeddedSum = input.wamit && !input.wamit->sum.qtf.empty();
	if (input.useRadiation && !hasEmbeddedRadiation && !module_io::FileExistsOrEmpty(input.potentialRadFile))
		throw std::runtime_error("HydroL UseRadiation=true but PotentialRadFile is missing: " + input.potentialRadFile);
	if (input.useExcitation && !hasEmbeddedExcitation && !module_io::FileExistsOrEmpty(input.potentialExcFile))
		throw std::runtime_error("HydroL UseExcitation=true but PotentialExcFile is missing: " + input.potentialExcFile);
	if (input.diffEvalType != HydroLDiffEvalType::NONE && !hasEmbeddedDifference && !module_io::FileExistsOrEmpty(input.potentialDiffFile))
		throw std::runtime_error("HydroL DiffEvalType requires PotentialDiffFile: " + input.potentialDiffFile);
	if (input.useSumFreqs && !hasEmbeddedSum && !module_io::FileExistsOrEmpty(input.potentialSumFile))
		throw std::runtime_error("HydroL UseSumFreqs=true but PotentialSumFile is missing: " + input.potentialSumFile);
	if (!input.subMembers.empty() && input.hydroMemberCoeff.empty())
		throw std::runtime_error("HydroL SubMembers are defined but HydroMemberCoeff is empty");
}

inline std::vector<std::vector<double>> RadiationRows(const std::vector<WamitRadiationEntry> &entries)
{
	std::vector<std::vector<double>> rows;
	rows.reserve(entries.size());
	for (const auto &entry : entries)
		rows.push_back({entry.period, static_cast<double>(entry.row), static_cast<double>(entry.column),
		                entry.addedMass, entry.damping, entry.hasDamping ? 1.0 : 0.0});
	return rows;
}

inline std::vector<std::vector<double>> ExcitationRows(const std::vector<WamitExcitationEntry> &entries)
{
	std::vector<std::vector<double>> rows;
	rows.reserve(entries.size());
	for (const auto &entry : entries)
		rows.push_back({entry.period, entry.headingDeg, static_cast<double>(entry.dof), entry.magnitude,
		                entry.phaseDeg, entry.real, entry.imaginary});
	return rows;
}

inline std::vector<std::vector<double>> QtfRows(const std::vector<WamitQtfEntry> &entries)
{
	std::vector<std::vector<double>> rows;
	rows.reserve(entries.size());
	for (const auto &entry : entries)
		rows.push_back({entry.period1, entry.period2, entry.heading1Deg, entry.heading2Deg,
		                static_cast<double>(entry.dof), entry.magnitude, entry.phaseDeg,
		                entry.real, entry.imaginary});
	return rows;
}

inline std::vector<WamitRadiationEntry> RadiationFromMatrix(const Eigen::MatrixXd &matrix)
{
	std::vector<WamitRadiationEntry> entries;
	entries.reserve(static_cast<std::size_t>(matrix.rows()));
	for (Eigen::Index r = 0; r < matrix.rows(); ++r)
	{
		if (matrix.cols() >= 6)
			entries.push_back({matrix(r, 0), static_cast<int>(matrix(r, 1)), static_cast<int>(matrix(r, 2)),
			                   matrix(r, 3), matrix(r, 4), matrix(r, 5) != 0.0});
	}
	return entries;
}

inline std::vector<WamitExcitationEntry> ExcitationFromMatrix(const Eigen::MatrixXd &matrix)
{
	std::vector<WamitExcitationEntry> entries;
	entries.reserve(static_cast<std::size_t>(matrix.rows()));
	for (Eigen::Index r = 0; r < matrix.rows(); ++r)
	{
		if (matrix.cols() >= 7)
			entries.push_back({matrix(r, 0), matrix(r, 1), static_cast<int>(matrix(r, 2)),
			                   matrix(r, 3), matrix(r, 4), matrix(r, 5), matrix(r, 6)});
	}
	return entries;
}

inline std::vector<WamitQtfEntry> QtfFromMatrix(const Eigen::MatrixXd &matrix)
{
	std::vector<WamitQtfEntry> entries;
	entries.reserve(static_cast<std::size_t>(matrix.rows()));
	for (Eigen::Index r = 0; r < matrix.rows(); ++r)
	{
		if (matrix.cols() >= 9)
			entries.push_back({matrix(r, 0), matrix(r, 1), matrix(r, 2), matrix(r, 3),
			                   static_cast<int>(matrix(r, 4)), matrix(r, 5), matrix(r, 6),
			                   matrix(r, 7), matrix(r, 8)});
	}
	return entries;
}

inline bool HasWamitRows(const HydroLWamitData &data)
{
	return !data.radiation.radiation.empty() ||
	       !data.excitation.excitation.empty() ||
	       !data.difference.qtf.empty() ||
	       !data.sum.qtf.empty();
}

inline std::optional<HydroLWamitData> ReadEmbeddedWamitData(Serializer &reader)
{
	HydroLWamitData data;
	data.radiation.inputPath = reader.Read<std::string>("Wamit.RadiationInputPath", "");
	data.radiation.type = WamitFileType::RADIATION;
	data.radiation.radiation = RadiationFromMatrix(reader.ReadMatrix("Wamit.RadiationRows"));
	data.excitation.inputPath = reader.Read<std::string>("Wamit.ExcitationInputPath", "");
	data.excitation.type = WamitFileType::EXCITATION;
	data.excitation.excitation = ExcitationFromMatrix(reader.ReadMatrix("Wamit.ExcitationRows"));
	data.difference.inputPath = reader.Read<std::string>("Wamit.DifferenceInputPath", "");
	data.difference.type = WamitFileType::DIFFERENCE_QTF;
	data.difference.qtf = QtfFromMatrix(reader.ReadMatrix("Wamit.DifferenceRows"));
	data.sum.inputPath = reader.Read<std::string>("Wamit.SumInputPath", "");
	data.sum.type = WamitFileType::SUM_QTF;
	data.sum.qtf = QtfFromMatrix(reader.ReadMatrix("Wamit.SumRows"));
	if (!HasWamitRows(data))
		return std::nullopt;
	return data;
}

inline void AddEmbeddedWamitData(Serializer &writer, const std::optional<HydroLWamitData> &wamit)
{
	if (!wamit || !HasWamitRows(*wamit))
		return;
	writer.AddNode("Wamit.RadiationInputPath", wamit->radiation.inputPath.string());
	writer.AddNode("Wamit.RadiationRows", RadiationRows(wamit->radiation.radiation), 8);
	writer.AddNode("Wamit.ExcitationInputPath", wamit->excitation.inputPath.string());
	writer.AddNode("Wamit.ExcitationRows", ExcitationRows(wamit->excitation.excitation), 8);
	writer.AddNode("Wamit.DifferenceInputPath", wamit->difference.inputPath.string());
	writer.AddNode("Wamit.DifferenceRows", QtfRows(wamit->difference.qtf), 8);
	writer.AddNode("Wamit.SumInputPath", wamit->sum.inputPath.string());
	writer.AddNode("Wamit.SumRows", QtfRows(wamit->sum.qtf), 8);
}

inline HydroLInput ReadHydroLInput(const std::string &path)
{
	if (!ZFile::Exists(path))
		throw std::runtime_error("Cannot open HydroL input file: " + path);

	HydroLInputSerializer reader;
	reader.ReadFile(path);
	HydroLInput input = reader.data;
	input.inputPath = std::filesystem::absolute(path).lexically_normal();

	input.jointOffset = module_io::ReadMatrixBlock(reader, path, kYamlRoot, "JointOffset", 3);
	input.marineGrowth = module_io::ReadMatrixBlock(reader, path, kYamlRoot, "MarineGrowth", 3);
	input.tpInterfacePos = module_io::ReadMatrixBlock(reader, path, kYamlRoot, "TpInterfacePos", 3);
	input.refCogPos = module_io::ReadMatrixBlock(reader, path, kYamlRoot, "RefCogPos", 3);
	input.refHydroPos = module_io::ReadMatrixBlock(reader, path, kYamlRoot, "RefHydroPos", 3);
	input.subMassMatrix = module_io::ReadMatrixBlock(reader, path, kYamlRoot, "SubMassMatrix", 6);
	input.hydroQuadDampingMatrix = module_io::ReadMatrixBlock(reader, path, kYamlRoot, "HydroQuadDampingMatrix", 6);
	input.hydroStiffnessMatrix = module_io::ReadMatrixBlock(reader, path, kYamlRoot, "HydroStiffnessMatrix", 6);
	input.hydroDampingMatrix = module_io::ReadMatrixBlock(reader, path, kYamlRoot, "HydroDampingMatrix", 6);
	input.hydroAddedMassMatrix = module_io::ReadMatrixBlock(reader, path, kYamlRoot, "HydroAddedMassMatrix", 6);
	input.hydroConstForce = module_io::ReadMatrixBlock(reader, path, kYamlRoot, "HydroConstForce", 6);

	input.subJoints = module_io::ReadRows(reader, path, kYamlRoot, "SubJoints", 4);
	input.rigidSubElements = module_io::ReadRows(reader, path, kYamlRoot, "RigidSubElements", 3);
	input.rigidRectSubElements = module_io::ReadRows(reader, path, kYamlRoot, "RigidRectSubElements", 5);
	input.subElements = module_io::ReadRows(reader, path, kYamlRoot, "SubElements", 4);
	input.hydroJointCoeff = module_io::ReadRows(reader, path, kYamlRoot, "HydroJointCoeff", 5);
	input.hydroMemberCoeff = module_io::ReadRows(reader, path, kYamlRoot, "HydroMemberCoeff", 5);
	if (input.hydroMemberCoeff.empty() && !reader.IsYaml())
	{
		const auto lines = reader.RawLines();
		for (std::size_t i = 0; i < lines.size(); ++i)
		{
			const auto upper = ZString::ToUpper(lines[i]);
			if (upper.find("COEFFID") == std::string::npos ||
			    upper.find("CDNORMAL") == std::string::npos)
				continue;
			for (std::size_t r = i + 1; r < lines.size(); ++r)
			{
				const auto text = ZString::Trim(lines[r]);
				if (module_io::IsSectionBoundary(text))
					break;
				auto tokens = module_io::TokenizeLoose(text);
				if (tokens.size() < 5)
					continue;
				try { (void)std::stod(tokens.front()); }
				catch (...) { continue; }
				input.hydroMemberCoeff.push_back(std::move(tokens));
			}
			if (!input.hydroMemberCoeff.empty())
				break;
		}
	}
	input.subConstraints = module_io::ReadRows(reader, path, kYamlRoot, "SubConstraints", 6);
	input.subMembers = module_io::ReadRows(reader, path, kYamlRoot, "SubMembers", 10);
	input.moorElements = module_io::ReadRows(reader, path, kYamlRoot, "MoorElements", 5);
	input.moorMembers = module_io::ReadRows(reader, path, kYamlRoot, "MoorMembers", 9);

	module_io::ReadOutputConfig(reader, path, kYamlRoot, input.output);
	if (reader.IsYaml())
		input.wamit = ReadEmbeddedWamitData(reader);
	ResolvePaths(input);
	Validate(input);
	return input;
}

inline void AddRows(Serializer &writer,
                    const std::string &key,
                    const std::vector<std::vector<std::string>> &rows)
{
	if (!rows.empty())
		writer.AddNode(key, rows, 2);
}

inline void AddMatrix(Serializer &writer,
                      const std::string &key,
                      const Eigen::MatrixXd &matrix)
{
	if (matrix.size() != 0)
		writer.AddNode(key, matrix, 2);
}

inline void WriteHydroLInputYaml(const HydroLInput &input, const std::string &path)
{
	HydroLInputSerializer writer(input);
	writer.WriteYamlFile(path);
	AddMatrix(writer, "JointOffset", input.jointOffset);
	AddMatrix(writer, "MarineGrowth", input.marineGrowth);
	AddMatrix(writer, "TpInterfacePos", input.tpInterfacePos);
	AddMatrix(writer, "RefCogPos", input.refCogPos);
	AddMatrix(writer, "RefHydroPos", input.refHydroPos);
	AddMatrix(writer, "SubMassMatrix", input.subMassMatrix);
	AddMatrix(writer, "HydroQuadDampingMatrix", input.hydroQuadDampingMatrix);
	AddMatrix(writer, "HydroStiffnessMatrix", input.hydroStiffnessMatrix);
	AddMatrix(writer, "HydroDampingMatrix", input.hydroDampingMatrix);
	AddMatrix(writer, "HydroAddedMassMatrix", input.hydroAddedMassMatrix);
	AddMatrix(writer, "HydroConstForce", input.hydroConstForce);
	AddRows(writer, "SubJoints", input.subJoints);
	AddRows(writer, "RigidSubElements", input.rigidSubElements);
	AddRows(writer, "RigidRectSubElements", input.rigidRectSubElements);
	AddRows(writer, "SubElements", input.subElements);
	AddRows(writer, "HydroJointCoeff", input.hydroJointCoeff);
	AddRows(writer, "HydroMemberCoeff", input.hydroMemberCoeff);
	AddRows(writer, "SubConstraints", input.subConstraints);
	AddRows(writer, "SubMembers", input.subMembers);
	AddRows(writer, "MoorElements", input.moorElements);
	AddRows(writer, "MoorMembers", input.moorMembers);
	module_io::AddOutputYamlNodes(writer, input.output);
	writer.SaveYamlFile(path);
}

inline void WriteHydroLInput(const HydroLInput &input,
                             const std::string &path,
                             const std::string &templatePath = "")
{
	if (Serializer::IsYamlPath(path))
	{
		WriteHydroLInputYaml(input, path);
		return;
	}
	HydroLInputSerializer writer(input);
	if (!templatePath.empty() && ZFile::Exists(templatePath))
		writer.LoadTextFile(templatePath);
	writer.WriteTextFile(path);
}

inline void ConvertHydroLInput(const std::string &inputPath,
                               const std::string &outputPath,
                               const std::string &templatePath = "")
{
	WriteHydroLInput(ReadHydroLInput(inputPath), outputPath, templatePath);
}
} // namespace hydrol_io_detail

using hydrol_io_detail::ConvertHydroLInput;
using hydrol_io_detail::ReadHydroLInput;
using hydrol_io_detail::WriteHydroLInput;
