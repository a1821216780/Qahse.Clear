#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

#include "IO/Yaml.hpp"
#include "WindL/IO/WindL_IO_Subs.hpp"
#include "WindL/WindL_Type.hpp"

// ============================================================================
// 测试辅助
// ============================================================================

namespace
{

bool HasWindLTestData(const std::filesystem::path &root)
{
	std::error_code ec;
	return std::filesystem::is_regular_file(root / "Test" / "WindL" / "Qahse_WindL_Main_DEMO.qwd", ec);
}

std::filesystem::path FindRepoRootFrom(std::filesystem::path dir)
{
	std::error_code ec;
	dir = std::filesystem::absolute(dir, ec);
	if (ec)
		return {};

	while (!dir.empty())
	{
		if (HasWindLTestData(dir))
			return dir;

		const auto parent = dir.parent_path();
		if (parent == dir)
			break;
		dir = parent;
	}

	return {};
}

std::filesystem::path RepoRoot()
{
	auto root = FindRepoRootFrom(std::filesystem::current_path());
	if (!root.empty())
		return root;

	root = FindRepoRootFrom(std::filesystem::path(__FILE__).parent_path());
	if (!root.empty())
		return root;

	return std::filesystem::current_path();
}

/// @brief 内置测试文件的搜索目录。
/// 优先使用 Test/WindL 下的独立副本，其次回退到 demo/WindL。
std::string FindTestFile(const char *filename)
{
	static const char *const kDirs[] = {
		"Test/WindL",
		"demo/WindL",
	};
	const auto root = RepoRoot();
	for (const auto *dir : kDirs)
	{
		std::filesystem::path p = root / dir / filename;
		std::error_code ec;
		if (std::filesystem::is_regular_file(p, ec))
			return p.string();
	}
	return filename; // 兜底
}

std::filesystem::path TestOutputDir()
{
	auto dir = RepoRoot() / "build" / "test" / "windl_io";
	std::filesystem::create_directories(dir);
	return dir;
}

void ExpectDoubleVectorEqual(const std::vector<double> &expected,
                             const std::vector<double> &actual,
                             const char *fieldName)
{
	ASSERT_EQ(actual.size(), expected.size()) << fieldName;
	for (std::size_t i = 0; i < expected.size(); ++i)
		EXPECT_DOUBLE_EQ(actual[i], expected[i]) << fieldName << "[" << i << "]";
}

void ExpectWindLInputEqual(const WindLInput &expected, const WindLInput &actual)
{
#define EXPECT_FIELD_EQ(field) EXPECT_EQ(actual.field, expected.field) << #field
#define EXPECT_FIELD_DOUBLE_EQ(field) EXPECT_DOUBLE_EQ(actual.field, expected.field) << #field
	EXPECT_FIELD_EQ(mode);
	EXPECT_FIELD_EQ(turbModel);
	EXPECT_FIELD_EQ(windModel);
	EXPECT_FIELD_EQ(calWu);
	EXPECT_FIELD_EQ(calWv);
	EXPECT_FIELD_EQ(calWw);
	EXPECT_FIELD_EQ(wrBlwnd);
	EXPECT_FIELD_EQ(wrTrbts);
	EXPECT_FIELD_EQ(wrTrwnd);
	EXPECT_FIELD_EQ(iecEdition);
	EXPECT_FIELD_EQ(turbineClass);
	EXPECT_FIELD_EQ(turbClass);
	EXPECT_FIELD_DOUBLE_EQ(vRef);
	EXPECT_FIELD_DOUBLE_EQ(rotorDiameter);
	EXPECT_FIELD_DOUBLE_EQ(meanWindSpeed);
	EXPECT_FIELD_DOUBLE_EQ(hubHeight);
	EXPECT_FIELD_DOUBLE_EQ(refHeight);
	EXPECT_FIELD_EQ(shearType);
	EXPECT_FIELD_DOUBLE_EQ(shearExp);
	EXPECT_FIELD_DOUBLE_EQ(roughness);
	EXPECT_FIELD_DOUBLE_EQ(horAngle);
	EXPECT_FIELD_DOUBLE_EQ(vertAngle);
	EXPECT_FIELD_EQ(userShearFile);
	EXPECT_FIELD_DOUBLE_EQ(turbIntensity);
	EXPECT_FIELD_EQ(turbSeed);
	EXPECT_FIELD_EQ(gridPtsY);
	EXPECT_FIELD_EQ(gridPtsZ);
	EXPECT_FIELD_DOUBLE_EQ(fieldDimY);
	EXPECT_FIELD_DOUBLE_EQ(fieldDimZ);
	EXPECT_FIELD_DOUBLE_EQ(simTime);
	EXPECT_FIELD_DOUBLE_EQ(timeStep);
	EXPECT_FIELD_EQ(cycleWind);
	EXPECT_FIELD_EQ(genMethod);
	EXPECT_FIELD_EQ(useFFT);
	EXPECT_FIELD_EQ(interpMethod);
	EXPECT_FIELD_EQ(userTurbFile);
	EXPECT_FIELD_EQ(useIECSimmga);
	EXPECT_FIELD_DOUBLE_EQ(vkLu);
	EXPECT_FIELD_DOUBLE_EQ(vkLv);
	EXPECT_FIELD_DOUBLE_EQ(vkLw);
	EXPECT_FIELD_DOUBLE_EQ(vyLu);
	EXPECT_FIELD_DOUBLE_EQ(vyLv);
	EXPECT_FIELD_DOUBLE_EQ(vyLw);
	EXPECT_FIELD_DOUBLE_EQ(vzLu);
	EXPECT_FIELD_DOUBLE_EQ(vzLv);
	EXPECT_FIELD_DOUBLE_EQ(vzLw);
	EXPECT_FIELD_DOUBLE_EQ(latitude);
	EXPECT_FIELD_DOUBLE_EQ(tiU);
	EXPECT_FIELD_DOUBLE_EQ(tiV);
	EXPECT_FIELD_DOUBLE_EQ(tiW);
	EXPECT_FIELD_DOUBLE_EQ(mannAlphaEps);
	EXPECT_FIELD_DOUBLE_EQ(mannLength);
	EXPECT_FIELD_DOUBLE_EQ(mannGamma);
	EXPECT_FIELD_DOUBLE_EQ(mannMaxL);
	EXPECT_FIELD_EQ(mannNx);
	EXPECT_FIELD_EQ(mannNy);
	EXPECT_FIELD_EQ(mannNz);
	EXPECT_FIELD_EQ(cohMod1);
	EXPECT_FIELD_EQ(cohMod2);
	EXPECT_FIELD_EQ(cohMod3);
	EXPECT_FIELD_DOUBLE_EQ(cohDecayU);
	EXPECT_FIELD_DOUBLE_EQ(cohDecayV);
	EXPECT_FIELD_DOUBLE_EQ(cohDecayW);
	EXPECT_FIELD_DOUBLE_EQ(cohScaleB);
	EXPECT_FIELD_DOUBLE_EQ(cohExp);
	EXPECT_FIELD_EQ(ewmType);
	EXPECT_FIELD_EQ(ewmReturn);
	EXPECT_FIELD_DOUBLE_EQ(gustPeriod);
	EXPECT_FIELD_DOUBLE_EQ(eventStart);
	EXPECT_FIELD_EQ(eventSign);
	EXPECT_FIELD_DOUBLE_EQ(ecdVcog);
	EXPECT_FIELD_EQ(wndFilePath);
	EXPECT_FIELD_EQ(wndFormat);
	EXPECT_FIELD_EQ(savePath);
	EXPECT_FIELD_EQ(saveName);
	EXPECT_FIELD_EQ(sumPrint);
	EXPECT_FIELD_EQ(batchExcelPath);
	EXPECT_FIELD_EQ(batchSheetName);
	EXPECT_FIELD_EQ(batchOutputDir);
	EXPECT_FIELD_EQ(batchThreads);
	EXPECT_FIELD_EQ(batchLauncher);
#undef EXPECT_FIELD_EQ
#undef EXPECT_FIELD_DOUBLE_EQ
}

void ExpectUserShearEqual(const UserShearData &expected, const UserShearData &actual)
{
	EXPECT_EQ(actual.numHeights, expected.numHeights);
	EXPECT_DOUBLE_EQ(actual.stdScale1, expected.stdScale1);
	EXPECT_DOUBLE_EQ(actual.stdScale2, expected.stdScale2);
	EXPECT_DOUBLE_EQ(actual.stdScale3, expected.stdScale3);
	ExpectDoubleVectorEqual(expected.heights, actual.heights, "heights");
	ExpectDoubleVectorEqual(expected.windSpeeds, actual.windSpeeds, "windSpeeds");
	ExpectDoubleVectorEqual(expected.windDirections, actual.windDirections, "windDirections");
	ExpectDoubleVectorEqual(expected.standardDeviations, actual.standardDeviations, "standardDeviations");
	ExpectDoubleVectorEqual(expected.lengthScales, actual.lengthScales, "lengthScales");
}

void ExpectUserSpectraEqual(const UserSpectraData &expected, const UserSpectraData &actual)
{
	EXPECT_EQ(actual.numFrequencies, expected.numFrequencies);
	ExpectDoubleVectorEqual(expected.frequencies, actual.frequencies, "frequencies");
	ExpectDoubleVectorEqual(expected.uPsd, actual.uPsd, "uPsd");
	ExpectDoubleVectorEqual(expected.vPsd, actual.vPsd, "vPsd");
	ExpectDoubleVectorEqual(expected.wPsd, actual.wPsd, "wPsd");
}

void ExpectUserWindSpeedEqual(const UserWindSpeedData &expected, const UserWindSpeedData &actual)
{
	EXPECT_EQ(actual.nComp, expected.nComp);
	EXPECT_EQ(actual.nPoints, expected.nPoints);
	EXPECT_EQ(actual.refPtID, expected.refPtID);

	ASSERT_EQ(actual.points.size(), expected.points.size());
	for (std::size_t i = 0; i < expected.points.size(); ++i)
	{
		EXPECT_DOUBLE_EQ(actual.points[i].y, expected.points[i].y) << "points[" << i << "].y";
		EXPECT_DOUBLE_EQ(actual.points[i].z, expected.points[i].z) << "points[" << i << "].z";
	}

	ExpectDoubleVectorEqual(expected.time, actual.time, "time");
	ASSERT_EQ(actual.components.size(), expected.components.size());
	for (std::size_t p = 0; p < expected.components.size(); ++p)
	{
		ASSERT_EQ(actual.components[p].size(), expected.components[p].size()) << "components[" << p << "]";
		for (std::size_t c = 0; c < expected.components[p].size(); ++c)
			ExpectDoubleVectorEqual(expected.components[p][c], actual.components[p][c], "components");
	}
}

} // namespace

// ============================================================================
// 1. 主输入文件 (.qwd) — 文本读取
// ============================================================================

TEST(WindLIO_QWD, ReadMainFile_BasicFields)
{
	auto path = FindTestFile("Qahse_WindL_Main_DEMO.qwd");
	WindLInput in = ReadWindLInput(path);

	// 模式
	EXPECT_EQ(in.mode, Mode::GENERATE);
	EXPECT_EQ(in.turbModel, TurbModel::B_KAL);
	EXPECT_EQ(in.windModel, WindModel::ETM);

	// 输出开关
	EXPECT_TRUE(in.wrBlwnd);
	EXPECT_TRUE(in.wrTrbts);
	EXPECT_TRUE(in.wrTrwnd);
	EXPECT_TRUE(in.calWu);
	EXPECT_TRUE(in.calWv);
	EXPECT_TRUE(in.calWw);

	// 网格
	EXPECT_EQ(in.turbSeed, 2318573);
	EXPECT_EQ(in.gridPtsY, 36);
	EXPECT_EQ(in.gridPtsZ, 36);
	EXPECT_DOUBLE_EQ(in.fieldDimY, 150.0);
	EXPECT_DOUBLE_EQ(in.fieldDimZ, 150.0);

	// 时域
	EXPECT_DOUBLE_EQ(in.simTime, 660.0);
	EXPECT_DOUBLE_EQ(in.timeStep, 0.05);

	// 气象
	EXPECT_DOUBLE_EQ(in.hubHeight, 84.0);
	EXPECT_DOUBLE_EQ(in.meanWindSpeed, 11.4);
	EXPECT_EQ(in.shearType, ShearType::PL);
}

TEST(WindLIO_QWD, ReadMainFile_IECParams)
{
	auto path = FindTestFile("Qahse_WindL_Main_DEMO.qwd");
	WindLInput in = ReadWindLInput(path);

	EXPECT_EQ(in.iecEdition, IecStandard::ED3);
	EXPECT_EQ(in.turbineClass, TurbineClass::Class_II);
	EXPECT_EQ(in.turbClass, TurbulenceClass::Class_B);
}

TEST(WindLIO_QWD, ReadMainFile_DefaultKeyword)
{
	auto path = FindTestFile("Qahse_WindL_Main_DEMO.qwd");
	WindLInput in = ReadWindLInput(path);

	// PLExp 值为 "default"，应保持 C++ 默认值 0.2
	EXPECT_DOUBLE_EQ(in.shearExp, 0.2);

	// Z0 值为 "default"
	EXPECT_DOUBLE_EQ(in.roughness, 0.01);

	// Mann 参数大多为 "default"
	EXPECT_DOUBLE_EQ(in.mannLength, 0.0);   // default → 保持 0.0
	EXPECT_DOUBLE_EQ(in.mannGamma, 0.0);
	EXPECT_DOUBLE_EQ(in.mannMaxL, 0.0);
}

TEST(WindLIO_QWD, ReadMainFile_CoherenceModels)
{
	auto path = FindTestFile("Qahse_WindL_Main_DEMO.qwd");
	WindLInput in = ReadWindLInput(path);

	// 所有相干模型都是 "default"，应返回 DEFAULT_COH
	EXPECT_EQ(in.cohMod1, CohModel::DEFAULT_COH);
	EXPECT_EQ(in.cohMod2, CohModel::DEFAULT_COH);
	EXPECT_EQ(in.cohMod3, CohModel::DEFAULT_COH);
}

TEST(WindLIO_QWD, ReadMainFile_Paths)
{
	auto path = FindTestFile("Qahse_WindL_Main_DEMO.qwd");
	WindLInput in = ReadWindLInput(path);

	// 输出路径
	EXPECT_NE(in.savePath.find("result"), std::string::npos);
	EXPECT_EQ(in.saveName, "Test_Demo_wind");
}

// ============================================================================
// 2. WindL 主输入 YAML 导出和互换
// ============================================================================

TEST(WindLIO_YAML, ExportAndLoadYaml)
{
	const auto dir = TestOutputDir();
	const auto yamlPath = dir / "Qahse_WindL_Main_DEMO.yml";

	const auto input = ReadWindLInput(FindTestFile("Qahse_WindL_Main_DEMO.qwd"));
	WriteWindLInput(input, yamlPath.string());

	YML yaml(yamlPath.string(), false);
	EXPECT_TRUE(yaml.ChickfindNodeByKey("Qahse.WindL"));
	EXPECT_EQ(yaml.read("Qahse.WindL.WrWndName"), "Test_Demo_wind");
	EXPECT_EQ(yaml.read("Qahse.WindL.TurbModel"), "B_KAL");
	EXPECT_EQ(YML::YmlToInt(yaml.read("Qahse.WindL.NumPointY")), 36);

	const auto fromYaml = ReadWindLInput(yamlPath.string());
	EXPECT_TRUE(fromYaml.wrTrwnd);
	EXPECT_EQ(fromYaml.saveName, "Test_Demo_wind");
	EXPECT_DOUBLE_EQ(fromYaml.timeStep, 0.05);
}

TEST(WindLIO_YAML, ConvertTextToYaml)
{
	const auto dir = TestOutputDir();
	const auto yamlPath = dir / "converted_from_text.yml";

	ConvertWindLInput(FindTestFile("Qahse_WindL_Main_DEMO.qwd"), yamlPath.string());

	const auto fromYaml = ReadWindLInput(yamlPath.string());
	EXPECT_EQ(fromYaml.mode, Mode::GENERATE);
	EXPECT_EQ(fromYaml.gridPtsY, 36);
	EXPECT_EQ(fromYaml.saveName, "Test_Demo_wind");
}

TEST(WindLIO_YAML, ConvertYamlToText)
{
	const auto dir = TestOutputDir();
	const auto yamlPath = dir / "converted_to_text.yml";
	const auto textPath = dir / "converted_to_text.qwd";

	ConvertWindLInput(FindTestFile("Qahse_WindL_Main_DEMO.qwd"), yamlPath.string());
	ConvertWindLInput(yamlPath.string(), textPath.string(), FindTestFile("Qahse_WindL_Main_DEMO.qwd"));

	const auto fromText = ReadWindLInput(textPath.string());

	EXPECT_TRUE(fromText.wrTrwnd);
	EXPECT_EQ(fromText.saveName, "Test_Demo_wind");
	EXPECT_DOUBLE_EQ(fromText.timeStep, 0.05);
}

TEST(WindLIO_YAML, ConvertAllInputTextFilesToYamlAndMatchText)
{
	const auto dir = TestOutputDir();

	const auto mainTextPath = FindTestFile("Qahse_WindL_Main_DEMO.qwd");
	const auto mainYamlPath = dir / "all_main_from_text.yml";
	const auto mainFromText = ReadWindLInput(mainTextPath);
	ConvertWindLInput(mainTextPath, mainYamlPath.string());
	EXPECT_TRUE(YML(mainYamlPath.string(), false).ChickfindNodeByKey("Qahse.WindL.Mode"));
	const auto mainFromYaml = ReadWindLInput(mainYamlPath.string());
	ExpectWindLInputEqual(mainFromText, mainFromYaml);

	const auto shearTextPath = FindTestFile("Qahse_WindL_User_Defined_Shear_DEMO.dat");
	const auto shearYamlPath = dir / "all_shear_from_text.yml";
	const auto shearFromText = ReadUserShear(shearTextPath);
	ConvertUserShear(shearTextPath, shearYamlPath.string());
	EXPECT_TRUE(YML(shearYamlPath.string(), false).ChickfindNodeByKey("Qahse.WindL.NumUSRz"));
	const auto shearFromYaml = ReadUserShear(shearYamlPath.string());
	ExpectUserShearEqual(shearFromText, shearFromYaml);

	const auto spectraTextPath = FindTestFile("Qahse_WindL_User_Defined_Spectra_DEMO.dat");
	const auto spectraYamlPath = dir / "all_spectra_from_text.yml";
	const auto spectraFromText = ReadUserSpectra(spectraTextPath);
	ConvertUserSpectra(spectraTextPath, spectraYamlPath.string());
	EXPECT_TRUE(YML(spectraYamlPath.string(), false).ChickfindNodeByKey("Qahse.WindL.NumUSRf"));
	const auto spectraFromYaml = ReadUserSpectra(spectraYamlPath.string());
	ExpectUserSpectraEqual(spectraFromText, spectraFromYaml);

	const auto windSpeedTextPath = FindTestFile("Qahse_WindL_User_Defined_Wind_Speed_DEMO.dat");
	const auto windSpeedYamlPath = dir / "all_wind_speed_from_text.yml";
	const auto windSpeedFromText = ReadUserWindSpeed(windSpeedTextPath);
	ConvertUserWindSpeed(windSpeedTextPath, windSpeedYamlPath.string());
	EXPECT_TRUE(YML(windSpeedYamlPath.string(), false).ChickfindNodeByKey("Qahse.WindL.nComp"));
	const auto windSpeedFromYaml = ReadUserWindSpeed(windSpeedYamlPath.string());
	ExpectUserWindSpeedEqual(windSpeedFromText, windSpeedFromYaml);
}

TEST(WindLIO_YAML, RoundTripAllInputYamlFilesToTextAndMatchOriginal)
{
	const auto dir = TestOutputDir();

	const auto mainTextPath = FindTestFile("Qahse_WindL_Main_DEMO.qwd");
	const auto mainYamlPath = dir / "all_main_roundtrip.yml";
	const auto mainRoundTripPath = dir / "all_main_roundtrip.qwd";
	const auto mainOriginal = ReadWindLInput(mainTextPath);
	ConvertWindLInput(mainTextPath, mainYamlPath.string());
	ConvertWindLInput(mainYamlPath.string(), mainRoundTripPath.string(), mainTextPath);
	ExpectWindLInputEqual(mainOriginal, ReadWindLInput(mainRoundTripPath.string()));

	const auto shearTextPath = FindTestFile("Qahse_WindL_User_Defined_Shear_DEMO.dat");
	const auto shearYamlPath = dir / "all_shear_roundtrip.yml";
	const auto shearRoundTripPath = dir / "all_shear_roundtrip.dat";
	const auto shearOriginal = ReadUserShear(shearTextPath);
	ConvertUserShear(shearTextPath, shearYamlPath.string());
	ConvertUserShear(shearYamlPath.string(), shearRoundTripPath.string());
	ExpectUserShearEqual(shearOriginal, ReadUserShear(shearRoundTripPath.string()));

	const auto spectraTextPath = FindTestFile("Qahse_WindL_User_Defined_Spectra_DEMO.dat");
	const auto spectraYamlPath = dir / "all_spectra_roundtrip.yml";
	const auto spectraRoundTripPath = dir / "all_spectra_roundtrip.dat";
	const auto spectraOriginal = ReadUserSpectra(spectraTextPath);
	ConvertUserSpectra(spectraTextPath, spectraYamlPath.string());
	ConvertUserSpectra(spectraYamlPath.string(), spectraRoundTripPath.string());
	ExpectUserSpectraEqual(spectraOriginal, ReadUserSpectra(spectraRoundTripPath.string()));

	const auto windSpeedTextPath = FindTestFile("Qahse_WindL_User_Defined_Wind_Speed_DEMO.dat");
	const auto windSpeedYamlPath = dir / "all_wind_speed_roundtrip.yml";
	const auto windSpeedRoundTripPath = dir / "all_wind_speed_roundtrip.dat";
	const auto windSpeedOriginal = ReadUserWindSpeed(windSpeedTextPath);
	ConvertUserWindSpeed(windSpeedTextPath, windSpeedYamlPath.string());
	ConvertUserWindSpeed(windSpeedYamlPath.string(), windSpeedRoundTripPath.string());
	ExpectUserWindSpeedEqual(windSpeedOriginal, ReadUserWindSpeed(windSpeedRoundTripPath.string()));
}

// ============================================================================
// 3. 用户自定义剪切廓线 (.dat)
// ============================================================================

TEST(WindLIO_DAT, ReadUserShear)
{
	auto path = FindTestFile("Qahse_WindL_User_Defined_Shear_DEMO.dat");
	UserShearData data = ReadUserShear(path);

	// 头部关键字
	EXPECT_EQ(data.numHeights, 5);
	EXPECT_DOUBLE_EQ(data.stdScale1, 1.092);
	EXPECT_DOUBLE_EQ(data.stdScale2, 1.0);
	EXPECT_DOUBLE_EQ(data.stdScale3, 0.534);

	// 数据行数
	ASSERT_EQ(data.heights.size(), 5u);
	ASSERT_EQ(data.windSpeeds.size(), 5u);
	ASSERT_EQ(data.windDirections.size(), 5u);
	ASSERT_EQ(data.standardDeviations.size(), 5u);
	ASSERT_EQ(data.lengthScales.size(), 5u);

	// 第 1 行数据
	EXPECT_DOUBLE_EQ(data.heights[0], 15.0);
	EXPECT_DOUBLE_EQ(data.windSpeeds[0], 3.0);
	EXPECT_DOUBLE_EQ(data.windDirections[0], 0.0);
	EXPECT_DOUBLE_EQ(data.standardDeviations[0], 0.1);
	EXPECT_DOUBLE_EQ(data.lengthScales[0], 3.0);

	// 第 5 行数据
	EXPECT_DOUBLE_EQ(data.heights[4], 55.0);
	EXPECT_DOUBLE_EQ(data.windSpeeds[4], 7.0);
	EXPECT_DOUBLE_EQ(data.standardDeviations[4], 0.5);
	EXPECT_DOUBLE_EQ(data.lengthScales[4], 13.0);
}

// ============================================================================
// 4. 用户自定义风谱 (.dat)
// ============================================================================

TEST(WindLIO_DAT, ReadUserSpectra)
{
	auto path = FindTestFile("Qahse_WindL_User_Defined_Spectra_DEMO.dat");
	UserSpectraData data = ReadUserSpectra(path);

	// 头部关键字
	EXPECT_EQ(data.numFrequencies, 20000);

	// 数据行数
	ASSERT_EQ(data.frequencies.size(), 20000u);
	ASSERT_EQ(data.uPsd.size(), 20000u);
	ASSERT_EQ(data.vPsd.size(), 20000u);
	ASSERT_EQ(data.wPsd.size(), 20000u);

	// 第 1 行数据
	EXPECT_DOUBLE_EQ(data.frequencies[0], 0.001);
	EXPECT_NEAR(data.uPsd[0], 364.644672, 1e-6);
	EXPECT_NEAR(data.vPsd[0], 92.196417, 1e-6);
	EXPECT_NEAR(data.wPsd[0], 9.432145, 1e-6);

	// 最后 1 行数据
	const auto last = data.frequencies.size() - 1;
	EXPECT_DOUBLE_EQ(data.frequencies[last], 20.000);
	EXPECT_NEAR(data.uPsd[last], 0.000615, 1e-6);
	EXPECT_NEAR(data.vPsd[last], 0.000818, 1e-6);
	EXPECT_NEAR(data.wPsd[last], 0.000814, 1e-6);

	// 单调性检查
	for (size_t i = 1; i < data.frequencies.size(); ++i)
		EXPECT_GT(data.frequencies[i], data.frequencies[i - 1]);

	// 所有 PSD 应为非负
	for (size_t i = 0; i < data.uPsd.size(); ++i)
	{
		EXPECT_GE(data.uPsd[i], 0.0);
		EXPECT_GE(data.vPsd[i], 0.0);
		EXPECT_GE(data.wPsd[i], 0.0);
	}
}

// ============================================================================
// 5. 用户自定义风速时间序列 (.dat)
// ============================================================================

TEST(WindLIO_DAT, ReadUserWindSpeed)
{
	auto path = FindTestFile("Qahse_WindL_User_Defined_Wind_Speed_DEMO.dat");
	UserWindSpeedData data = ReadUserWindSpeed(path);

	// 头部关键字
	EXPECT_EQ(data.nComp, 3);
	EXPECT_EQ(data.nPoints, 2);
	EXPECT_EQ(data.refPtID, 2);

	// 空间点坐标
	ASSERT_EQ(data.points.size(), 2u);
	EXPECT_DOUBLE_EQ(data.points[0].y, 0.0);
	EXPECT_DOUBLE_EQ(data.points[0].z, 30.0);
	EXPECT_DOUBLE_EQ(data.points[1].y, 0.0);
	EXPECT_DOUBLE_EQ(data.points[1].z, 76.0);

	// 时间序列行数
	// nPoints=2, timeStep=0.05, duration=600s → 12000 行
	ASSERT_EQ(data.time.size(), 12000u);

	// 第一时间步
	EXPECT_DOUBLE_EQ(data.time[0], 0.0);
	EXPECT_NEAR(data.components[0][0][0], 10.0239, 1e-4); // Point01u
	EXPECT_NEAR(data.components[0][1][0], -6.5673, 1e-4); // Point01v
	EXPECT_NEAR(data.components[0][2][0], 0.1700, 1e-4);  // Point01w
	EXPECT_NEAR(data.components[1][0][0], 10.7104, 1e-4); // Point02u
	EXPECT_NEAR(data.components[1][1][0], -4.3265, 1e-4); // Point02v
	EXPECT_NEAR(data.components[1][2][0], -0.2657, 1e-4); // Point02w

	// 最后时间步
	const auto last = data.time.size() - 1;
	EXPECT_DOUBLE_EQ(data.time[last], 599.95);
	EXPECT_NEAR(data.components[0][0][last], 20.6705, 1e-4); // Point01u
}

TEST(WindLIO_DAT, WindSpeedFile_TimeStepUniform)
{
	auto path = FindTestFile("Qahse_WindL_User_Defined_Wind_Speed_DEMO.dat");
	UserWindSpeedData data = ReadUserWindSpeed(path);

	// 验证时间步长均匀
	ASSERT_GE(data.time.size(), 2u);
	const double dt = data.time[1] - data.time[0];
	EXPECT_DOUBLE_EQ(dt, 0.05);

	for (size_t i = 2; i < data.time.size(); ++i)
		EXPECT_NEAR(data.time[i] - data.time[i - 1], dt, 1e-9);
}

// ============================================================================
// 6. 回归测试 — 跨格式 YAML 往返
// ============================================================================

TEST(WindLIO_Regression, TextRoundTrip)
{
	const auto dir = TestOutputDir();
	const auto roundTripPath = dir / "Qahse_WindL_Main_DEMO_roundtrip.qwd";

	const auto originalPath = FindTestFile("Qahse_WindL_Main_DEMO.qwd");
	const auto input = ReadWindLInput(originalPath);
	WriteWindLInput(input, roundTripPath.string(), originalPath);

	// 重新读取往返文件
	const auto roundTrip = ReadWindLInput(roundTripPath.string());

	// 关键值应该一致
	EXPECT_EQ(roundTrip.mode, input.mode);
	EXPECT_EQ(roundTrip.gridPtsY, input.gridPtsY);
	EXPECT_EQ(roundTrip.gridPtsZ, input.gridPtsZ);
	EXPECT_DOUBLE_EQ(roundTrip.timeStep, input.timeStep);
	EXPECT_DOUBLE_EQ(roundTrip.simTime, input.simTime);
	EXPECT_EQ(roundTrip.saveName, input.saveName);
}

TEST(WindLIO_Regression, YAMLTextRoundTrip)
{
	const auto dir = TestOutputDir();
	const auto yamlPath = dir / "regression.yml";
	const auto textPath  = dir / "regression.qwd";

	auto path = FindTestFile("Qahse_WindL_Main_DEMO.qwd");

	// .qwd → YAML
	const auto original = ReadWindLInput(path);
	WriteWindLInput(original, yamlPath.string());

	// YAML → .qwd
	ConvertWindLInput(yamlPath.string(), textPath.string(), path);

	// 再读取 → 应与 fromYaml 一致
	const auto final = ReadWindLInput(textPath.string());

	EXPECT_EQ(final.saveName, original.saveName);
	EXPECT_DOUBLE_EQ(final.timeStep, original.timeStep);
}

// ============================================================================
// 7. 边界情况
// ============================================================================

TEST(WindLIO_Boundary, NonExistentFile)
{
	EXPECT_THROW(ReadWindLInput("nonexistent_file.qwd"),
	             std::runtime_error);
	EXPECT_THROW(ReadUserShear("nonexistent_file.dat"),
	             std::runtime_error);
	EXPECT_THROW(ReadUserSpectra("nonexistent_file.dat"),
	             std::runtime_error);
	EXPECT_THROW(ReadUserWindSpeed("nonexistent_file.dat"),
	             std::runtime_error);
}

TEST(WindLIO_Boundary, EmptyShearFile)
{
	UserShearData empty;
	EXPECT_EQ(empty.numHeights, 0);
	EXPECT_TRUE(empty.heights.empty());
	EXPECT_DOUBLE_EQ(empty.stdScale1, 1.0);
	EXPECT_DOUBLE_EQ(empty.stdScale2, 1.0);
	EXPECT_DOUBLE_EQ(empty.stdScale3, 1.0);
}

TEST(WindLIO_Boundary, DefaultWindLInputValues)
{
	WindLInput in;
	// 枚举默认值
	EXPECT_EQ(in.mode, Mode::GENERATE);
	EXPECT_EQ(in.turbModel, TurbModel::IEC_KAIMAL);
	EXPECT_EQ(in.windModel, WindModel::NTM);
	EXPECT_EQ(in.shearType, ShearType::PL);
	// 数值默认值
	EXPECT_DOUBLE_EQ(in.shearExp, 0.2);
	EXPECT_DOUBLE_EQ(in.refHeight, -1.0);
	EXPECT_DOUBLE_EQ(in.roughness, 0.01);
	// 开关默认值
	EXPECT_TRUE(in.calWu);
	EXPECT_TRUE(in.calWv);
	EXPECT_TRUE(in.calWw);
}
