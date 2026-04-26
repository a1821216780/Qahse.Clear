#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <Eigen/Dense>
#include "../../src/Math/LinearAlgebraHelper.h"

using LA = LinearAlgebraHelper;
using VectorXd = Eigen::VectorXd;
using MatrixXd = Eigen::MatrixXd;
using VectorXf = Eigen::VectorXf;
using MatrixXf = Eigen::MatrixXf;
using Vector3d = Eigen::Vector3d;

TEST(LinearAlgebraHelper, LA_zeros_MatrixDims)
{
    auto m = LA::zeros(3, 4);
    EXPECT_EQ(m.rows(), 3);
    EXPECT_EQ(m.cols(), 4);
    EXPECT_EQ(m.lpNorm<1>(), 0.0);
}

TEST(LinearAlgebraHelper, LA_zeros_WithValue)
{
    auto arr = LA::zeros(2, 3, 4, 1.0);
    EXPECT_EQ(arr.size(), 2u);
    EXPECT_EQ(arr[0].size(), 3u);
    EXPECT_EQ(arr[0][0].size(), 4u);
    for (const auto &layer2 : arr)
        for (const auto &layer1 : layer2)
            for (double v : layer1)
                EXPECT_NEAR(v, 1.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_zeros_VectorSize)
{
    auto v = LA::zeros(5);
    EXPECT_EQ(v.size(), 5);
    EXPECT_EQ(v.lpNorm<1>(), 0.0);
}

TEST(LinearAlgebraHelper, LA_zeros_MatrixCopy)
{
    const MatrixXd ref = MatrixXd::Ones(3, 7);
    auto z = LA::zeros(ref);
    EXPECT_EQ(z.rows(), 3);
    EXPECT_EQ(z.cols(), 7);
    EXPECT_EQ(z.lpNorm<1>(), 0.0);
}

TEST(LinearAlgebraHelper, LA_zeros_VectorCopy)
{
    const VectorXd ref = VectorXd::Ones(4);
    auto z = LA::zeros(ref, true);
    EXPECT_EQ(z.size(), 4);
    EXPECT_EQ(z.lpNorm<1>(), 0.0);
}

TEST(LinearAlgebraHelper, LA_zeros_3D)
{
    auto mats = LA::zeros(2, 3, 5);
    EXPECT_EQ(mats.size(), 5u);
    for (const auto &m : mats)
    {
        EXPECT_EQ(m.rows(), 2);
        EXPECT_EQ(m.cols(), 3);
        EXPECT_EQ(m.lpNorm<1>(), 0.0);
    }
}

TEST(LinearAlgebraHelper, LA_fzeros_Matrix)
{
    auto m = LA::fzeros(3, 4);
    EXPECT_EQ(m.rows(), 3);
    EXPECT_EQ(m.cols(), 4);
    EXPECT_EQ(m.lpNorm<1>(), 0.0f);
}

TEST(LinearAlgebraHelper, LA_fzeros_Vector)
{
    auto v = LA::fzeros(5);
    EXPECT_EQ(v.size(), 5);
    EXPECT_EQ(v.lpNorm<1>(), 0.0f);
}

TEST(LinearAlgebraHelper, LA_Eye_Identity)
{
    auto I = LA::Eye(4);
    EXPECT_EQ(I.rows(), 4);
    EXPECT_EQ(I.cols(), 4);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            EXPECT_NEAR(I(i, j), (i == j) ? 1.0 : 0.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_Eye_WithValue)
{
    auto M = LA::Eye(3, 2.0);
    EXPECT_EQ(M.rows(), 3);
    EXPECT_EQ(M.cols(), 3);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            EXPECT_NEAR(M(i, j), (i == j) ? 2.0 : 0.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_Eye_Rect)
{
    auto E = LA::Eye(3, 5);
    EXPECT_EQ(E.rows(), 3);
    EXPECT_EQ(E.cols(), 5);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 5; ++j)
            EXPECT_NEAR(E(i, j), (i == j) ? 1.0 : 0.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_Eye_VectorDiag)
{
    VectorXd d(3);
    d << 2.0, 3.0, 4.0;
    auto D = LA::Eye(d);
    EXPECT_EQ(D.rows(), 3);
    EXPECT_EQ(D.cols(), 3);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            EXPECT_NEAR(D(i, j), (i == j) ? d[i] : 0.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_Diag)
{
    VectorXd d(3);
    d << 5.0, 6.0, 7.0;
    auto D = LA::Diag(d);
    EXPECT_EQ(D.rows(), 3);
    EXPECT_EQ(D.cols(), 3);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            EXPECT_NEAR(D(i, j), (i == j) ? d[i] : 0.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_ones)
{
    auto M = LA::ones(2, 3);
    EXPECT_EQ(M.rows(), 2);
    EXPECT_EQ(M.cols(), 3);
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 3; ++j)
            EXPECT_NEAR(M(i, j), 1.0, 1e-15);

    auto v = LA::ones(4);
    EXPECT_EQ(v.size(), 4);
    for (int i = 0; i < 4; ++i)
        EXPECT_NEAR(v[i], 1.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_Size_Matrix)
{
    MatrixXd A(3, 7);
    auto [rows, cols] = LA::Size(A);
    EXPECT_EQ(rows, 3);
    EXPECT_EQ(cols, 7);
}

TEST(LinearAlgebraHelper, LA_Size_Vector2D)
{
    std::vector<std::vector<double>> mat(4, std::vector<double>(6));
    auto [rows, cols] = LA::Size(mat);
    EXPECT_EQ(rows, 4);
    EXPECT_EQ(cols, 6);
}

TEST(LinearAlgebraHelper, LA_Size_MatrixDim)
{
    MatrixXd A(5, 8);
    EXPECT_EQ(LA::Size(A, 1), 5);
    EXPECT_EQ(LA::Size(A, 2), 8);
}

TEST(LinearAlgebraHelper, LA_EqualRealNos)
{
    EXPECT_TRUE(LA::EqualRealNos(1.0, 1.000001));
    EXPECT_FALSE(LA::EqualRealNos(1.0, 1.00002));
}

TEST(LinearAlgebraHelper, LA_Cumsum_Vector)
{
    std::vector<double> v = {1, 2, 3, 4};
    auto result = LA::Cumsum(v);
    ASSERT_EQ(result.size(), 4u);
    EXPECT_NEAR(result[0], 1.0, 1e-15);
    EXPECT_NEAR(result[1], 3.0, 1e-15);
    EXPECT_NEAR(result[2], 6.0, 1e-15);
    EXPECT_NEAR(result[3], 10.0, 1e-15);

    VectorXd ev(4);
    ev << 1, 2, 3, 4;
    auto er = LA::Cumsum(ev);
    EXPECT_NEAR(er[0], 1.0, 1e-15);
    EXPECT_NEAR(er[1], 3.0, 1e-15);
    EXPECT_NEAR(er[2], 6.0, 1e-15);
    EXPECT_NEAR(er[3], 10.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_ReCumsum)
{
    std::vector<double> v = {1, 3, 6, 10};
    auto result = LA::ReCumsum(v);
    ASSERT_EQ(result.size(), 4u);
    EXPECT_NEAR(result[0], 1.0, 1e-15);
    EXPECT_NEAR(result[1], 2.0, 1e-15);
    EXPECT_NEAR(result[2], 3.0, 1e-15);
    EXPECT_NEAR(result[3], 4.0, 1e-15);

    VectorXd ev(4);
    ev << 1, 3, 6, 10;
    auto er = LA::ReCumsum(ev);
    EXPECT_NEAR(er[0], 1.0, 1e-15);
    EXPECT_NEAR(er[1], 2.0, 1e-15);
    EXPECT_NEAR(er[2], 3.0, 1e-15);
    EXPECT_NEAR(er[3], 4.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_Diff)
{
    VectorXd v(4);
    v << 1, 3, 6, 10;
    auto d = LA::Diff(v);
    ASSERT_EQ(d.size(), 3);
    EXPECT_NEAR(d[0], 2.0, 1e-15);
    EXPECT_NEAR(d[1], 3.0, 1e-15);
    EXPECT_NEAR(d[2], 4.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_Norm_Vector)
{
    VectorXd v(3);
    v << 3, 4, 0;

    EXPECT_NEAR(LA::Norm(v, 2), 5.0, 1e-12);
    EXPECT_NEAR(LA::Norm(v), 5.0, 1e-12);
    EXPECT_NEAR(LA::Norm(v, 1), 7.0, 1e-12);
    EXPECT_NEAR(LA::Norm(v, 3), 4.0, 1e-12);
}

TEST(LinearAlgebraHelper, LA_Norm_Matrix)
{
    MatrixXd A(2, 2);
    A << 3, 0, 0, 4;

    EXPECT_NEAR(LA::Norm(A, 2), 4.0, 1e-12);
    EXPECT_NEAR(LA::Norm(A, "fro"), std::sqrt(25.0), 1e-12);
}

TEST(LinearAlgebraHelper, LA_Outer)
{
    VectorXd a(2);
    a << 1, 2;
    VectorXd b(3);
    b << 3, 4, 5;
    auto M = LA::Outer(a, b);
    ASSERT_EQ(M.rows(), 2);
    ASSERT_EQ(M.cols(), 3);
    EXPECT_NEAR(M(0, 0), 3.0, 1e-15);
    EXPECT_NEAR(M(0, 1), 4.0, 1e-15);
    EXPECT_NEAR(M(0, 2), 5.0, 1e-15);
    EXPECT_NEAR(M(1, 0), 6.0, 1e-15);
    EXPECT_NEAR(M(1, 1), 8.0, 1e-15);
    EXPECT_NEAR(M(1, 2), 10.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_Inter)
{
    VectorXd a(3);
    a << 1, 2, 3;
    VectorXd b(3);
    b << 4, 5, 6;
    EXPECT_NEAR(LA::Inter(a, b), 32.0, 1e-15);
}

// LA_Dot_Vectors: skipped - dot(VectorXd,VectorXd)->double not yet implemented
// LA_Cross_Vectors: skipped - cross(VectorXd,VectorXd)->VectorXd not yet implemented

TEST(LinearAlgebraHelper, LA_Hact_Vectors)
{
    std::vector<double> a = {1, 2};
    std::vector<double> b = {3, 4, 5};
    auto r = LA::Hact(a, b);
    ASSERT_EQ(r.size(), 5);
    EXPECT_NEAR(r[0], 1.0, 1e-15);
    EXPECT_NEAR(r[1], 2.0, 1e-15);
    EXPECT_NEAR(r[2], 3.0, 1e-15);
    EXPECT_NEAR(r[3], 4.0, 1e-15);
    EXPECT_NEAR(r[4], 5.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_Hact_ScalarVec)
{
    VectorXd b(3);
    b << 2, 3, 4;
    auto r1 = LA::Hact(1.0, b);
    ASSERT_EQ(r1.size(), 4);
    EXPECT_NEAR(r1[0], 1.0, 1e-15);
    EXPECT_NEAR(r1[1], 2.0, 1e-15);
    EXPECT_NEAR(r1[2], 3.0, 1e-15);
    EXPECT_NEAR(r1[3], 4.0, 1e-15);

    VectorXd a(3);
    a << 1, 2, 3;
    auto r2 = LA::Hact(a, 4.0);
    ASSERT_EQ(r2.size(), 4);
    EXPECT_NEAR(r2[0], 1.0, 1e-15);
    EXPECT_NEAR(r2[1], 2.0, 1e-15);
    EXPECT_NEAR(r2[2], 3.0, 1e-15);
    EXPECT_NEAR(r2[3], 4.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_Vact_Vectors)
{
    VectorXd v1(3);
    v1 << 1, 2, 3;
    VectorXd v2(3);
    v2 << 4, 5, 6;
    auto M = LA::Vact(std::vector<VectorXd>{v1, v2});
    ASSERT_EQ(M.rows(), 3);
    ASSERT_EQ(M.cols(), 2);
    EXPECT_NEAR(M(0, 0), 1.0, 1e-15);
    EXPECT_NEAR(M(1, 0), 2.0, 1e-15);
    EXPECT_NEAR(M(2, 0), 3.0, 1e-15);
    EXPECT_NEAR(M(0, 1), 4.0, 1e-15);
    EXPECT_NEAR(M(1, 1), 5.0, 1e-15);
    EXPECT_NEAR(M(2, 1), 6.0, 1e-15);
}

// LA_Reshape_VectorToMatrix: skipped - Reshape(vec, vec<int>, bool) not yet implemented
// LA_Reshape_MatrixToVector: skipped - Reshape(MatrixXd)->VectorXd not yet implemented

// LA_Conv_Vector: skipped - Conv(vector<double>, vector<double>) not yet implemented
// LA_ConjugateGradientSolver: skipped - not yet implemented
// LA_Triu: skipped - not yet implemented

TEST(LinearAlgebraHelper, LA_Trace)
{
    MatrixXd A(3, 3);
    A << 1, 2, 3,
         4, 5, 6,
         7, 8, 9;
    EXPECT_NEAR(LA::Trace(A), 15.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_Det)
{
    MatrixXd A(2, 2);
    A << 1, 2, 3, 4;
    EXPECT_NEAR(LA::Det(A), -2.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_SkewMatrix)
{
    VectorXd a(3);
    a << 1, 2, 3;
    auto S = LA::SkewMatrix(a);
    ASSERT_EQ(S.rows(), 3);
    ASSERT_EQ(S.cols(), 3);

    EXPECT_NEAR(S(0, 0), 0.0, 1e-15);
    EXPECT_NEAR(S(0, 1), -3.0, 1e-15);
    EXPECT_NEAR(S(0, 2), 2.0, 1e-15);
    EXPECT_NEAR(S(1, 0), 3.0, 1e-15);
    EXPECT_NEAR(S(1, 1), 0.0, 1e-15);
    EXPECT_NEAR(S(1, 2), -1.0, 1e-15);
    EXPECT_NEAR(S(2, 0), -2.0, 1e-15);
    EXPECT_NEAR(S(2, 1), 1.0, 1e-15);
    EXPECT_NEAR(S(2, 2), 0.0, 1e-15);

    EXPECT_NEAR((S + S.transpose()).lpNorm<1>(), 0.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_Repmat_Vector)
{
    VectorXd v(2);
    v << 1, 2;

    auto M1 = LA::Repmat(v, 3, 1);
    ASSERT_EQ(M1.rows(), 3);
    ASSERT_EQ(M1.cols(), 2);
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_NEAR(M1(i, 0), 1.0, 1e-15);
        EXPECT_NEAR(M1(i, 1), 2.0, 1e-15);
    }

    auto M2 = LA::Repmat(v, 3, 2);
    ASSERT_EQ(M2.rows(), 2);
    ASSERT_EQ(M2.cols(), 3);
    for (int j = 0; j < 3; ++j)
    {
        EXPECT_NEAR(M2(0, j), 1.0, 1e-15);
        EXPECT_NEAR(M2(1, j), 2.0, 1e-15);
    }
}

TEST(LinearAlgebraHelper, LA_meshgrid)
{
    VectorXd x(3);
    x << 1, 2, 3;
    VectorXd y(2);
    y << 10, 20;

    auto [xx, yy] = LA::meshgrid(x, y, false);
    ASSERT_EQ(xx.rows(), 3);
    ASSERT_EQ(xx.cols(), 2);
    ASSERT_EQ(yy.rows(), 3);
    ASSERT_EQ(yy.cols(), 2);

    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 2; ++j)
        {
            EXPECT_NEAR(xx(i, j), x[i], 1e-15);
            EXPECT_NEAR(yy(i, j), y[j], 1e-15);
        }

    auto [xxr, yyr] = LA::meshgrid(x, y, true);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 2; ++j)
        {
            EXPECT_NEAR(xxr(i, j), x[2 - i], 1e-15);
            EXPECT_NEAR(yyr(i, j), y[j], 1e-15);
        }
}

TEST(LinearAlgebraHelper, LA_Sort)
{
    std::vector<double> v = {5.0, 2.0, 8.0, 1.0};
    auto s = LA::Sort(v);
    ASSERT_EQ(s.size(), 4u);
    EXPECT_NEAR(s[0], 1.0, 1e-15);
    EXPECT_NEAR(s[1], 2.0, 1e-15);
    EXPECT_NEAR(s[2], 5.0, 1e-15);
    EXPECT_NEAR(s[3], 8.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_FindClosestIndexAndValue)
{
    std::vector<double> v = {1.0, 3.0, 5.0, 7.0};
    auto [idx, val] = LA::FindClosestIndexAndValue(v, 4.2);
    EXPECT_EQ(idx, 2);
    EXPECT_NEAR(val, 5.0, 1e-15);

    auto [idx2, val2] = LA::FindClosestIndexAndValue(v, 5.0);
    EXPECT_EQ(idx2, 2);
    EXPECT_NEAR(val2, 5.0, 1e-15);

    auto [idx3, val3] = LA::FindClosestIndexAndValue(v, 6.8);
    EXPECT_EQ(idx3, 3);
    EXPECT_NEAR(val3, 7.0, 1e-15);
}

TEST(LinearAlgebraHelper, LA_FindSortedFirst)
{
    VectorXd v(5);
    v << 1, 3, 5, 7, 9;
    EXPECT_EQ(LA::FindSortedFirst(v, 6), 3);
    EXPECT_EQ(LA::FindSortedFirst(v, 0), 0);
    EXPECT_EQ(LA::FindSortedFirst(v, 5), 2);
    EXPECT_EQ(LA::FindSortedFirst(v, 10), 5);
}

TEST(LinearAlgebraHelper, LA_AddSortedValue)
{
    VectorXd v(3);
    v << 1, 3, 5;
    auto r = LA::AddSortedValue(v, 4.0, false);
    ASSERT_EQ(r.size(), 4);
    EXPECT_NEAR(r[0], 1.0, 1e-15);
    EXPECT_NEAR(r[1], 3.0, 1e-15);
    EXPECT_NEAR(r[2], 4.0, 1e-15);
    EXPECT_NEAR(r[3], 5.0, 1e-15);

    VectorXd v2(3);
    v2 << 5, 1, 3;
    auto r2 = LA::AddSortedValue(v2, 2.0, true);
    ASSERT_EQ(r2.size(), 4);
    EXPECT_NEAR(r2[0], 1.0, 1e-15);
    EXPECT_NEAR(r2[1], 2.0, 1e-15);
    EXPECT_NEAR(r2[2], 3.0, 1e-15);
    EXPECT_NEAR(r2[3], 5.0, 1e-15);
}
