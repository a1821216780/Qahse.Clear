#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <Eigen/Dense>
#include "../../src/Math/MathHelper.hpp"

TEST(MathHelper, RandomSeed_Reproducibility)
{
    MathHelper::RandomSeed rng1(42);
    MathHelper::RandomSeed rng2(42);

    for (int i = 0; i < 100; ++i)
    {
        EXPECT_DOUBLE_EQ(rng1.Randomd(), rng2.Randomd());
    }
}

TEST(MathHelper, RandomSeed_RandomdRange)
{
    MathHelper::RandomSeed rng(123);
    const double low = -5.0;
    const double high = 10.0;

    for (int i = 0; i < 1000; ++i)
    {
        double val = rng.Randomd(low, high);
        EXPECT_GE(val, low);
        EXPECT_LT(val, high);
    }
}

TEST(MathHelper, RandomSeed_RandomiRange)
{
    MathHelper::RandomSeed rng(456);
    const int minVal = -10;
    const int maxVal = 20;

    for (int i = 0; i < 1000; ++i)
    {
        int val = rng.Randomi(1, minVal, maxVal)[0];
        EXPECT_GE(val, minVal);
        EXPECT_LE(val, maxVal);
    }
}

TEST(MathHelper, RandomSeed_RandomdArray)
{
    MathHelper::RandomSeed rng(789);
    const int len = 50;
    auto arr = rng.Randomd(len);

    EXPECT_EQ(static_cast<int>(arr.size()), len);

    for (const auto &v : arr)
    {
        EXPECT_GE(v, 0.0);
        EXPECT_LT(v, 1.0);
    }
}

TEST(MathHelper, RandomSeed_RandomiArray)
{
    MathHelper::RandomSeed rng(321);
    const int len = 50;
    auto arr = rng.Randomi(len);

    EXPECT_EQ(static_cast<int>(arr.size()), len);

    for (const auto &v : arr)
    {
        EXPECT_GE(v, 0);
        EXPECT_LE(v, 2147483647);
    }
}

TEST(MathHelper, RandomSeed_RandomiMatrix)
{
    MathHelper::RandomSeed rng(654);
    const int rows = 3;
    const int cols = 4;
    const int minVal = -5;
    const int maxVal = 15;
    auto mat = rng.Randomi(rows, cols, minVal, maxVal);

    EXPECT_EQ(mat.rows(), rows);
    EXPECT_EQ(mat.cols(), cols);

    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            EXPECT_GE(mat(i, j), minVal);
            EXPECT_LE(mat(i, j), maxVal);
        }
    }
}

TEST(MathHelper, RandomSeed_RandomdMatrix)
{
    MathHelper::RandomSeed rng(987);
    const int rows = 4;
    const int cols = 5;
    const double minVal = -1.0;
    const double maxVal = 3.0;
    auto mat = rng.Randomd(rows, cols, minVal, maxVal);

    EXPECT_EQ(mat.rows(), rows);
    EXPECT_EQ(mat.cols(), cols);

    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < cols; ++j)
        {
            EXPECT_GE(mat(i, j), minVal);
            EXPECT_LT(mat(i, j), maxVal);
        }
    }
}

TEST(MathHelper, StaticRandomd)
{
    for (int i = 0; i < 500; ++i)
    {
        double val = MathHelper::Randomd();
        EXPECT_GE(val, 0.0);
        EXPECT_LT(val, 1.0);
    }
}

TEST(MathHelper, StaticRandomi)
{
    for (int i = 0; i < 500; ++i)
    {
        int val = MathHelper::Randomi();
        EXPECT_GE(val, 0);
        EXPECT_LE(val, 100);
    }
}

TEST(MathHelper, StaticRandomiRange)
{
    using SingleInt_fn = int(*)(int, int);
    SingleInt_fn fn = &MathHelper::Randomi;
    for (int i = 0; i < 500; ++i)
    {
        int val = fn(10, 50);
        EXPECT_GE(val, 10);
        EXPECT_LE(val, 50);
    }
}

TEST(MathHelper, Range)
{
    auto vi = MathHelper::Range(5, 4);
    ASSERT_EQ(vi.size(), 4);
    EXPECT_EQ(vi(0), 5);
    EXPECT_EQ(vi(1), 6);
    EXPECT_EQ(vi(2), 7);
    EXPECT_EQ(vi(3), 8);

    auto v0 = MathHelper::Range(5);
    ASSERT_EQ(v0.size(), 5);
    EXPECT_DOUBLE_EQ(v0(0), 0.0);
    EXPECT_DOUBLE_EQ(v0(4), 4.0);

    auto vStep = MathHelper::Range(0, 10, 2);
    ASSERT_EQ(vStep.size(), 6);
    EXPECT_EQ(vStep(0), 0);
    EXPECT_EQ(vStep(1), 2);
    EXPECT_EQ(vStep(5), 10);

    auto vdStep = MathHelper::Range(0.0, 10.0, 2.5);
    ASSERT_EQ(vdStep.size(), 5);
    EXPECT_DOUBLE_EQ(vdStep(0), 0.0);
    EXPECT_DOUBLE_EQ(vdStep(4), 10.0);

    auto vfStep = MathHelper::Range(0.0f, 5.0f, 1.0f);
    ASSERT_EQ(vfStep.size(), 6);
    EXPECT_FLOAT_EQ(vfStep(0), 0.0f);
    EXPECT_FLOAT_EQ(vfStep(5), 5.0f);

    auto vfStepLen = MathHelper::Range(1.0f, 0.5f, 5);
    ASSERT_EQ(vfStepLen.size(), 5);
    EXPECT_FLOAT_EQ(vfStepLen(0), 1.0f);
    EXPECT_FLOAT_EQ(vfStepLen(4), 3.0f);

    auto vdLen = MathHelper::Range(0.0, 10.0, 5);
    ASSERT_EQ(vdLen.size(), 5);
    EXPECT_DOUBLE_EQ(vdLen(0), 0.0);
    EXPECT_DOUBLE_EQ(vdLen(4), 10.0);
}

TEST(MathHelper, linspace)
{
    auto vd = MathHelper::linspace(0.0, 10.0, 5);
    ASSERT_EQ(vd.size(), 5);
    EXPECT_DOUBLE_EQ(vd(0), 0.0);
    EXPECT_DOUBLE_EQ(vd(4), 10.0);
    EXPECT_NEAR(vd(2), 5.0, 1e-12);

    auto vf = MathHelper::linspace(0.0f, 5.0f, 6);
    ASSERT_EQ(vf.size(), 6);
    EXPECT_FLOAT_EQ(vf(0), 0.0f);
    EXPECT_FLOAT_EQ(vf(5), 5.0f);

    auto vSingle = MathHelper::linspace(3.0, 7.0, 1);
    ASSERT_EQ(vSingle.size(), 1);
    EXPECT_DOUBLE_EQ(vSingle(0), 3.0);
}

TEST(MathHelper, zeros)
{
    auto zT = MathHelper::zeros(7.0, 2, 3);
    EXPECT_EQ(zT.rows(), 2);
    EXPECT_EQ(zT.cols(), 3);
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 3; ++j)
            EXPECT_DOUBLE_EQ(zT(i, j), 7.0);

    auto z = MathHelper::zeros(3, 4);
    EXPECT_EQ(z.rows(), 3);
    EXPECT_EQ(z.cols(), 4);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j)
            EXPECT_DOUBLE_EQ(z(i, j), 0.0);

    auto zVal = MathHelper::zeros(2, 2, 1.5);
    EXPECT_EQ(zVal.rows(), 2);
    EXPECT_EQ(zVal.cols(), 2);
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            EXPECT_DOUBLE_EQ(zVal(i, j), 1.5);

    auto z3d = MathHelper::zeros(0.0, 2, 3, 4);
    EXPECT_EQ(static_cast<int>(z3d.size()), 2);
    for (int i = 0; i < 2; ++i)
    {
        EXPECT_EQ(z3d[i].rows(), 3);
        EXPECT_EQ(z3d[i].cols(), 4);
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c)
                EXPECT_DOUBLE_EQ(z3d[i](r, c), 0.0);
    }

    auto z4d = MathHelper::zeros(1.0, 2, 3, 4, 5);
    EXPECT_EQ(static_cast<int>(z4d.size()), 2);
    EXPECT_EQ(static_cast<int>(z4d[0].size()), 3);
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 3; ++j)
        {
            EXPECT_EQ(z4d[i][j].rows(), 4);
            EXPECT_EQ(z4d[i][j].cols(), 5);
            for (int r = 0; r < 4; ++r)
                for (int c = 0; c < 5; ++c)
                    EXPECT_DOUBLE_EQ(z4d[i][j](r, c), 1.0);
        }
}

TEST(MathHelper, FunAT)
{
    Eigen::VectorXd x(4);
    x << 1.0, 2.0, 3.0, 4.0;

    auto sq = MathHelper::FunAT([](double v) { return v * v; }, x);
    ASSERT_EQ(sq.size(), 4);
    EXPECT_DOUBLE_EQ(sq(0), 1.0);
    EXPECT_DOUBLE_EQ(sq(1), 4.0);
    EXPECT_DOUBLE_EQ(sq(2), 9.0);
    EXPECT_DOUBLE_EQ(sq(3), 16.0);

    auto sinX = MathHelper::FunAT(static_cast<double(*)(double)>(std::sin), x);
    ASSERT_EQ(sinX.size(), 4);
    EXPECT_NEAR(sinX(0), std::sin(1.0), 1e-12);
    EXPECT_NEAR(sinX(1), std::sin(2.0), 1e-12);
    EXPECT_NEAR(sinX(2), std::sin(3.0), 1e-12);
    EXPECT_NEAR(sinX(3), std::sin(4.0), 1e-12);
}

TEST(MathHelper, FunATExpr)
{
    Eigen::VectorXd x(3);
    x << 0.0, 1.0, 2.0;

    auto doubled = MathHelper::FunATExpr([](double v) { return v * 2.0; }, x);
    ASSERT_EQ(doubled.size(), 3);
    EXPECT_DOUBLE_EQ(doubled(0), 0.0);
    EXPECT_DOUBLE_EQ(doubled(1), 2.0);
    EXPECT_DOUBLE_EQ(doubled(2), 4.0);
}

TEST(MathHelper, Pow1)
{
    EXPECT_DOUBLE_EQ(MathHelper::Pow1(2, 10), 1024.0);
    EXPECT_NEAR(MathHelper::Pow1(3, -2), 1.0 / 9.0, 1e-10);
    EXPECT_DOUBLE_EQ(MathHelper::Pow1(5, 0), 1.0);
    EXPECT_DOUBLE_EQ(MathHelper::Pow1(7, 1), 7.0);
    EXPECT_DOUBLE_EQ(MathHelper::Pow1(0, 5), 0.0);
}

TEST(MathHelper, FindIndex)
{
    Eigen::VectorXd asc(5);
    asc << 1.0, 3.0, 5.0, 7.0, 9.0;

    EXPECT_EQ(MathHelper::FindIndex(asc, 4.0, false), 1);
    EXPECT_EQ(MathHelper::FindIndex(asc, 1.0, false), 0);
    EXPECT_EQ(MathHelper::FindIndex(asc, 7.0, false), 2);
    EXPECT_EQ(MathHelper::FindIndex(asc, 9.0, false), 3);

    Eigen::VectorXd desc(4);
    desc << 9.0, 6.0, 3.0, 0.0;

    EXPECT_EQ(MathHelper::FindIndex(desc, 4.0, false), 1);
    EXPECT_EQ(MathHelper::FindIndex(desc, 9.0, false), 0);
    EXPECT_EQ(MathHelper::FindIndex(desc, 0.5, false), 2);

    EXPECT_EQ(MathHelper::FindIndex(asc, -1.0, false), 0);
    EXPECT_EQ(MathHelper::FindIndex(asc, 10.0, false), -1);

    Eigen::VectorXi ascI(4);
    ascI << 0, 2, 4, 6;
    EXPECT_EQ(MathHelper::FindIndex(ascI, 3.0, false), 1);
}

TEST(MathHelper, Zero2TwoPi)
{
    EXPECT_NEAR(MathHelper::Zero2TwoPi(-M_PI / 2), 3.0 * M_PI / 2, 1e-12);
    EXPECT_NEAR(MathHelper::Zero2TwoPi(3.0 * M_PI), M_PI, 1e-12);
    EXPECT_NEAR(MathHelper::Zero2TwoPi(M_PI / 4), M_PI / 4, 1e-12);
    EXPECT_NEAR(MathHelper::Zero2TwoPi(0.0), 0.0, 1e-12);
    EXPECT_NEAR(MathHelper::Zero2TwoPi(2.0 * M_PI), 0.0, 1e-12);
}

TEST(MathHelper, MirrorRound)
{
    EXPECT_NEAR(MathHelper::MirrorRound(3.14159, 2), 3.14, 1e-10);
    EXPECT_NEAR(MathHelper::MirrorRound(2.718, 0), 3.0, 1e-10);
    EXPECT_NEAR(MathHelper::MirrorRound(123.456, 1), 123.5, 1e-10);
}

TEST(MathHelper, ToStdVector)
{
    Eigen::VectorXd v(3);
    v << 1.0, 2.0, 3.0;
    auto sv = MathHelper::ToStdVector(v);

    ASSERT_EQ(sv.size(), 3u);
    EXPECT_DOUBLE_EQ(sv[0], 1.0);
    EXPECT_DOUBLE_EQ(sv[1], 2.0);
    EXPECT_DOUBLE_EQ(sv[2], 3.0);
}

TEST(MathHelper, FromStdVector)
{
    std::vector<double> sv = {1.0, 2.0, 3.0};
    auto v = MathHelper::FromStdVector(sv);

    ASSERT_EQ(v.size(), 3);
    EXPECT_DOUBLE_EQ(v(0), 1.0);
    EXPECT_DOUBLE_EQ(v(1), 2.0);
    EXPECT_DOUBLE_EQ(v(2), 3.0);
}

TEST(MathHelper, ToStdVectorInt)
{
    Eigen::VectorXi vi(4);
    vi << 10, 20, 30, 40;
    auto sv = MathHelper::ToStdVector(vi);

    ASSERT_EQ(sv.size(), 4u);
    EXPECT_EQ(sv[0], 10);
    EXPECT_EQ(sv[1], 20);
    EXPECT_EQ(sv[2], 30);
    EXPECT_EQ(sv[3], 40);

    auto viBack = MathHelper::FromStdVector(sv);
    ASSERT_EQ(viBack.size(), 4);
    EXPECT_EQ(viBack(0), 10);
    EXPECT_EQ(viBack(1), 20);
    EXPECT_EQ(viBack(2), 30);
    EXPECT_EQ(viBack(3), 40);
}