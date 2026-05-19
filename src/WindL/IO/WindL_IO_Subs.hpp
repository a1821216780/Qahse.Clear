#pragma once

#include <Eigen/Dense>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "../WindL_Type.hpp"
#include "../../IO/ModuleIO.hpp"
#include "../../IO/Serializer.hpp"
#include "../../IO/ZFile.hpp"
#include "../../IO/ZPath.hpp"

namespace windl_io_detail
{
	inline constexpr const char *kYamlRoot = "Qahse.WindL";

	inline WindLWindType ParseWindTypeCode(int value)
	{
		switch (value)
		{
		case 1:
			return WindLWindType::STEADY;
		case 2:
			return WindLWindType::USER_DEFINED;
		case 3:
			return WindLWindType::TURBSIM_WND;
		case 4:
			return WindLWindType::BLADED_WND;
		case 5:
			return WindLWindType::TURBSIM_BTS;
		case 6:
		case 8:
			throw std::runtime_error("WindL no longer supports WindType=" + std::to_string(value) +
									 "; use SimWind for wind-file generation.");
		default:
			throw std::runtime_error("Unsupported WindL WindType=" + std::to_string(value));
		}
	}

	inline std::vector<WindLTimeSpeed> TimeSpeedFromMatrix(const Eigen::MatrixXd &matrix)
	{
		std::vector<WindLTimeSpeed> series;
		series.reserve(static_cast<std::size_t>(matrix.rows()));
		for (Eigen::Index row = 0; row < matrix.rows(); ++row)
		{
			if (matrix.cols() >= 2)
				series.push_back({matrix(row, 0), matrix(row, 1)});
		}
		return series;
	}

	inline std::vector<std::vector<double>> TimeSpeedRows(const std::vector<WindLTimeSpeed> &series)
	{
		std::vector<std::vector<double>> rows;
		rows.reserve(series.size());
		for (const auto &item : series)
			rows.push_back({item.time, item.speed});
		return rows;
	}

	inline void ResolveIfSet(const std::string &baseFilePath, std::string &path)
	{
		if (!path.empty())
			path = ZPath::ResolvePath(baseFilePath, path);
	}

	inline void ResolvePaths(WindLInput &input)
	{
		const std::string base = input.inputPath.string();
		ResolveIfSet(base, input.turWindFilePath);
		ResolveIfSet(base, input.bldWindFilePath);
		ResolveIfSet(base, input.iecWindFilePath);
	}

	class WindLInputSerializer : public Serializer
	{
	public:
		WindLInput data;
		int windTypeCode = static_cast<int>(WindLWindType::STEADY);
		int windSpeedNum = 0;

		WindLInputSerializer()
		{
			SetValueFirst(true);
			SetYamlRoot(kYamlRoot);
		}

		explicit WindLInputSerializer(const WindLInput &input)
			: data(input),
			  windTypeCode(static_cast<int>(input.windType)),
			  windSpeedNum(static_cast<int>(input.windSpeedList.size()))
		{
			SetValueFirst(true);
			SetYamlRoot(kYamlRoot);
		}

		void FinalizeRead()
		{
			data.windType = ParseWindTypeCode(windTypeCode);
		}

		void SyncWriteFields()
		{
			windTypeCode = static_cast<int>(data.windType);
			windSpeedNum = static_cast<int>(data.windSpeedList.size());
		}

	protected:
		void SerializeFields() override
		{
			if (!IsReadMode())
				SyncWriteFields();

			Fields(
				Field("WindType", windTypeCode),
				Field("HWindSpeed", data.hWindSpeed),
				Field("RefHt", data.refHeight),
				Field("PLExp", data.plExp),
				Field("gridY_min", data.gridYMin),
				Field("gridY_max", data.gridYMax),
				Field("gridY_step", data.gridYStep),
				Field("gridZ_min", data.gridZMin),
				Field("gridZ_max", data.gridZMax),
				Field("gridZ_step", data.gridZStep),
				Field("WindSpeedNum", windSpeedNum));

			ReadOrWriteAny({"CycleWind", "CreadW"}, data.cycleWind);
			ReadOrWriteAny({"TurWindFilePath", "TurWindFile"}, data.turWindFilePath);
			ReadOrWriteAny({"BldWindFilePath", "BldWindFile"}, data.bldWindFilePath);
			ReadOrWriteAny({"IECWindFilePath", "IECWindFile"}, data.iecWindFilePath);
		}
	};

	inline WindLInput ReadWindLInputFile(const std::string &path)
	{
		if (!ZFile::Exists(path))
			throw std::runtime_error("Cannot open WindL input file: " + path);

		WindLInputSerializer reader;
		if (Serializer::IsYamlPath(path))
			reader.ReadYamlFile(path);
		else
			reader.ReadTextFile(path);
		reader.FinalizeRead();

		WindLInput input = reader.data;
		input.inputPath = std::filesystem::absolute(path).lexically_normal();

		const Eigen::MatrixXd windSpeedMatrix = reader.ReadMatrixAfter(
			"WindSpeedList", "WindSpeedNum", reader.windSpeedNum, 2);
		input.windSpeedList = TimeSpeedFromMatrix(windSpeedMatrix);

		ResolvePaths(input);
		return input;
	}

	inline void WriteWindLInputYaml(const WindLInput &input, const std::string &path)
	{
		WindLInputSerializer writer(input);
		writer.WriteYamlFile(path);
		module_io::AddNumericTableYamlNodes(writer, "WindSpeedList",
			{"Time_s", "WindSpeed_mps"}, TimeSpeedRows(input.windSpeedList), 3);
		writer.SaveYamlFile(path);
	}

	inline void ConvertWindLInputToYaml(const std::string &inputPath, const std::string &outputPath)
	{
		WriteWindLInputYaml(ReadWindLInputFile(inputPath), outputPath);
	}
} // namespace windl_io_detail
