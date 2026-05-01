#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "../../src/IO/MSExcel.h"
#include "../../src/WindL/Batch/WindLBatch.hpp"
#include "../../src/WindL/IO/WindL_IO_Subs.hpp"

namespace
{
class ScopedBatchRoot
{
public:
	ScopedBatchRoot()
		: root_(std::filesystem::temp_directory_path() / "Qahse_Clear_WindLBatchTests")
	{
		std::filesystem::remove_all(root_);
		std::filesystem::create_directories(root_);
	}

	~ScopedBatchRoot()
	{
		std::error_code ec;
		std::filesystem::remove_all(root_, ec);
	}

	std::filesystem::path path(const std::string &relative) const
	{
		return root_ / relative;
	}

private:
	std::filesystem::path root_;
};

WindLInput BatchTemplateInput(const ScopedBatchRoot &root)
{
	WindLInput input;
	input.mode = Mode::BATCH;
	input.turbModel = TurbModel::IEC_KAIMAL;
	input.windModel = WindModel::NTM;
	input.turbSeed = 12345;
	input.gridPtsY = 2;
	input.gridPtsZ = 2;
	input.fieldDimY = 10.0;
	input.fieldDimZ = 10.0;
	input.simTime = 4.0;
	input.timeStep = 0.2;
	input.hubHeight = 80.0;
	input.meanWindSpeed = 10.0;
	input.savePath = root.path("single_case_output").string();
	input.saveName = "batch_seed";
	input.wrBlwnd = false;
	input.wrTrbts = true;
	input.wrTrwnd = false;
	input.sumPrint = true;
	input.batchSheetName = "Cases";
	input.batchOutputDir = root.path("batch_output").string();
	input.batchLauncher = "inproc";
	return input;
}

void WriteBatchWorkbook(const std::string &path,
                        const std::vector<std::string> &headers,
                        const std::vector<std::vector<std::string>> &rows)
{
	MSExcel excel(path, "write");
	for (std::size_t c = 0; c < headers.size(); ++c)
		excel.WCellValue<std::string>("Cases", headers[c], 0, static_cast<int>(c));
	for (std::size_t r = 0; r < rows.size(); ++r)
	{
		for (std::size_t c = 0; c < rows[r].size(); ++c)
			excel.WCellValue<std::string>("Cases", rows[r][c], static_cast<int>(r + 1), static_cast<int>(c));
	}
	excel.WCellValue<std::string>("README", "WindL batch template", 0, 0);
	excel.WCellValue<std::string>("Catalog", "Override.WindModel", 0, 0);
	excel.Close();
}
} // namespace

TEST(WindL_Batch, ValidateOnlyReportsInvalidCaseButKeepsValidCase)
{
	const ScopedBatchRoot root;
	auto input = BatchTemplateInput(root);
	input.batchValidateOnly = true;
	input.batchExcelPath = root.path("validate_cases.xlsx").string();

	WriteBatchWorkbook(
	    input.batchExcelPath,
	    {"CaseName", "Enabled", "OutputSubdir", "Override.WindModel", "Override.MeanWindSpeed", "Meta.WaveHs"},
	    {{"good_case", "true", "good_case", "EWM1", "14.0", "4.2"},
	     {"bad_case", "true", "bad_case", "NOT_A_MODEL", "14.0", "5.0"}});

	const auto qwdPath = root.path("batch_validate.qwd");
	WriteWindLInput(input, qwdPath.string());

	const auto result = WindLBatch::RunFromFile(qwdPath.string(), "");
	EXPECT_EQ(result.totalCases, 2);
	EXPECT_EQ(result.validated, 1);
	EXPECT_EQ(result.invalid, 1);
	EXPECT_TRUE(std::filesystem::is_regular_file(result.manifestPath));
	EXPECT_TRUE(std::filesystem::is_regular_file(result.csvPath));
	EXPECT_TRUE(std::filesystem::is_regular_file(result.summaryPath));
}

TEST(WindL_Batch, InProcBatchGeneratesValidCaseWhenAnotherCaseIsInvalid)
{
	const ScopedBatchRoot root;
	auto input = BatchTemplateInput(root);
	input.batchValidateOnly = false;
	input.batchExcelPath = root.path("inproc_cases.xlsx").string();

	WriteBatchWorkbook(
	    input.batchExcelPath,
	    {"CaseName", "Enabled", "OutputSubdir", "Override.WindModel", "Override.MeanWindSpeed", "Override.WrTrbts"},
	    {{"ok_case", "true", "ok_case", "NTM", "12.0", "true"},
	     {"bad_case", "true", "bad_case", "BAD_ENUM", "12.0", "true"}});

	const auto qwdPath = root.path("batch_inproc.qwd");
	WriteWindLInput(input, qwdPath.string());

	const auto result = WindLBatch::RunFromFile(qwdPath.string(), "");
	EXPECT_EQ(result.totalCases, 2);
	EXPECT_EQ(result.succeeded, 1);
	EXPECT_EQ(result.invalid, 1);

	const auto btsPath = root.path("batch_output/ok_case/ok_case.bts");
	const auto sumPath = root.path("batch_output/ok_case/ok_case.sum");
	EXPECT_TRUE(std::filesystem::is_regular_file(btsPath));
	EXPECT_TRUE(std::filesystem::is_regular_file(sumPath));
}

TEST(WindL_Batch, LegacyCmdLauncherMapsToSubprocessDuringValidateOnly)
{
	const ScopedBatchRoot root;
	auto input = BatchTemplateInput(root);
	input.batchValidateOnly = true;
	input.batchLauncher = "cmd";
	input.batchExcelPath = root.path("cmd_cases.xlsx").string();

	WriteBatchWorkbook(
	    input.batchExcelPath,
	    {"CaseName", "Enabled", "OutputSubdir", "Override.WindModel"},
	    {{"cmd_case", "true", "cmd_case", "NTM"}});

	const auto qwdPath = root.path("batch_cmd.qwd");
	WriteWindLInput(input, qwdPath.string());

	const auto result = WindLBatch::RunFromFile(qwdPath.string(), "");
	EXPECT_EQ(result.validated, 1);
	EXPECT_EQ(result.invalid, 0);
}

TEST(WindL_Batch, UnsupportedOverrideColumnFailsWorkbookValidation)
{
	const ScopedBatchRoot root;
	auto input = BatchTemplateInput(root);
	input.batchValidateOnly = true;
	input.batchExcelPath = root.path("bad_header.xlsx").string();

	WriteBatchWorkbook(
	    input.batchExcelPath,
	    {"CaseName", "Override.DoesNotExist"},
	    {{"bad_header_case", "123"}});

	const auto qwdPath = root.path("batch_bad_header.qwd");
	WriteWindLInput(input, qwdPath.string());

	EXPECT_THROW((void)WindLBatch::RunFromFile(qwdPath.string(), ""), std::runtime_error);
}
