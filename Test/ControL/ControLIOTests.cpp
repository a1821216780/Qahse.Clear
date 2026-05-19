#include "../ModuleIOTestHelpers.hpp"

#include <filesystem>
#include <stdexcept>

#include <gtest/gtest.h>

#include "ControL/IO/ControL_IO_Subs.hpp"

using namespace module_io_test;

namespace
{
std::filesystem::path SemisubControlFile()
{
	return SemisubRoot() / "ControL" / "Qahse_ControL_Main_NREL_5MW_OC4_Semisub.dat";
}
} // namespace

TEST(ControLIO, ReadSemisubAndYamlRoundTrip)
{
	const auto control = ReadControLInput(SemisubControlFile().string());
	EXPECT_EQ(control.pcMode, 1);
	EXPECT_TRUE(std::filesystem::is_regular_file(control.dllFileName));
	EXPECT_TRUE(std::filesystem::is_regular_file(control.dllInFile));

	const auto yaml = TestOutputDir() / "ControL" / "control.yaml";
	WriteControLInput(control, yaml.string());
	ExpectControlEqual(ReadControLInput(yaml.string()), control);
}

TEST(ControLIO, TextTemplateWriteUpdatesBoundFields)
{
	const auto dir = TestOutputDir() / "ControL" / "text_template";
	const auto dll = dir / "controller.dll";
	const auto dllInput = dir / "DISCON.IN";
	TouchFile(dll);
	TouchFile(dllInput);

	const auto inputPath = dir / "control.dat";
	WriteLines(inputPath, {
		"1 PCMode",
		QuotePath(dll) + " DLL_FileName",
		QuotePath(dllInput) + " DLL_InFile",
		"DISCON DLL_ProcName",
		"true SumPrint",
		"END",
	});

	auto control = ReadControLInput(inputPath.string());
	control.dllProcName = "QAHSE_DISCON";
	const auto outPath = dir / "control_written.dat";
	WriteControLInput(control, outPath.string(), inputPath.string());

	const auto written = ReadControLInput(outPath.string());
	EXPECT_EQ(written.pcMode, 1);
	EXPECT_EQ(written.dllProcName, "QAHSE_DISCON");
}

TEST(ControLIO, ValidationRejectsMissingDllWhenEnabled)
{
	const auto dir = TestOutputDir() / "ControL" / "validation";
	const auto dllInput = dir / "DISCON.IN";
	TouchFile(dllInput);

	const auto path = dir / "control_missing_dll.dat";
	WriteLines(path, {
		"1 PCMode",
		"missing.dll DLL_FileName",
		QuotePath(dllInput) + " DLL_InFile",
		"DISCON DLL_ProcName",
		"END",
	});
	EXPECT_THROW((void)ReadControLInput(path.string()), std::runtime_error);
}
