#include <gtest/gtest.h>
#include <cmath>
#include <Eigen/Dense>
#include "../../src/Math/Vec3.h"
#include "../../src/Math/Vec3f.h"
#include "../../src/Math/Quaternion.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST(Vec3, DefaultConstructor)
{
    Vec3 v;
    EXPECT_DOUBLE_EQ(v.x, 0.0);
    EXPECT_DOUBLE_EQ(v.y, 0.0);
    EXPECT_DOUBLE_EQ(v.z, 0.0);
}

TEST(Vec3, ParameterizedConstructor)
{
    Vec3 v(1, 2, 3);
    EXPECT_DOUBLE_EQ(v.x, 1.0);
    EXPECT_DOUBLE_EQ(v.y, 2.0);
    EXPECT_DOUBLE_EQ(v.z, 3.0);
}

TEST(Vec3, EigenConstructor)
{
    Eigen::Vector3d ev(4.0, 5.0, 6.0);
    Vec3 v(ev);
    EXPECT_DOUBLE_EQ(v.x, 4.0);
    EXPECT_DOUBLE_EQ(v.y, 5.0);
    EXPECT_DOUBLE_EQ(v.z, 6.0);
}

TEST(Vec3, EigenMap)
{
    Vec3 v(7.0, 8.0, 9.0);
    Eigen::Map<Eigen::Vector3d> m = v.eigen();
    EXPECT_DOUBLE_EQ(m[0], 7.0);
    EXPECT_DOUBLE_EQ(m[1], 8.0);
    EXPECT_DOUBLE_EQ(m[2], 9.0);

    const Vec3 cv(10.0, 11.0, 12.0);
    Eigen::Map<const Eigen::Vector3d> cm = cv.eigen();
    EXPECT_DOUBLE_EQ(cm[0], 10.0);
    EXPECT_DOUBLE_EQ(cm[1], 11.0);
    EXPECT_DOUBLE_EQ(cm[2], 12.0);
}

TEST(Vec3, Equality)
{
    Vec3 a(1.0, 2.0, 3.0);
    Vec3 b(1.0, 2.0, 3.0);
    Vec3 c(1.0, 2.0, 4.0);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(Vec3, AddSubtract)
{
    Vec3 a(1.0, 2.0, 3.0);
    Vec3 b(4.0, 5.0, 6.0);

    Vec3 sum = a + b;
    EXPECT_DOUBLE_EQ(sum.x, 5.0);
    EXPECT_DOUBLE_EQ(sum.y, 7.0);
    EXPECT_DOUBLE_EQ(sum.z, 9.0);

    Vec3 diff = b - a;
    EXPECT_DOUBLE_EQ(diff.x, 3.0);
    EXPECT_DOUBLE_EQ(diff.y, 3.0);
    EXPECT_DOUBLE_EQ(diff.z, 3.0);

    Vec3 a2(1.0, 2.0, 3.0);
    a2 += b;
    EXPECT_DOUBLE_EQ(a2.x, 5.0);
    EXPECT_DOUBLE_EQ(a2.y, 7.0);
    EXPECT_DOUBLE_EQ(a2.z, 9.0);

    Vec3 b2(4.0, 5.0, 6.0);
    b2 -= a;
    EXPECT_DOUBLE_EQ(b2.x, 3.0);
    EXPECT_DOUBLE_EQ(b2.y, 3.0);
    EXPECT_DOUBLE_EQ(b2.z, 3.0);
}

TEST(Vec3, ScalarMultiplyDivide)
{
    Vec3 v(2.0, 4.0, 6.0);

    Vec3 mul = v * 3.0;
    EXPECT_DOUBLE_EQ(mul.x, 6.0);
    EXPECT_DOUBLE_EQ(mul.y, 12.0);
    EXPECT_DOUBLE_EQ(mul.z, 18.0);

    Vec3 div = v / 2.0;
    EXPECT_DOUBLE_EQ(div.x, 1.0);
    EXPECT_DOUBLE_EQ(div.y, 2.0);
    EXPECT_DOUBLE_EQ(div.z, 3.0);

    Vec3 v2(2.0, 4.0, 6.0);
    v2 *= 3.0;
    EXPECT_DOUBLE_EQ(v2.x, 6.0);
    EXPECT_DOUBLE_EQ(v2.y, 12.0);
    EXPECT_DOUBLE_EQ(v2.z, 18.0);
}

TEST(Vec3, CrossProduct)
{
    Vec3 a(1.0, 0.0, 0.0);
    Vec3 b(0.0, 1.0, 0.0);
    Vec3 c = a * b;
    EXPECT_NEAR(c.x, 0.0, 1e-10);
    EXPECT_NEAR(c.y, 0.0, 1e-10);
    EXPECT_NEAR(c.z, 1.0, 1e-10);
}

TEST(Vec3, SetAndCopy)
{
    Vec3 v;
    v.Set(10.0, 20.0, 30.0);
    EXPECT_DOUBLE_EQ(v.x, 10.0);
    EXPECT_DOUBLE_EQ(v.y, 20.0);
    EXPECT_DOUBLE_EQ(v.z, 30.0);

    Vec3 src(5.0, 6.0, 7.0);
    v.Set(src);
    EXPECT_DOUBLE_EQ(v.x, 5.0);
    EXPECT_DOUBLE_EQ(v.y, 6.0);
    EXPECT_DOUBLE_EQ(v.z, 7.0);

    Vec3 dst;
    dst.Copy(src);
    EXPECT_DOUBLE_EQ(dst.x, 5.0);
    EXPECT_DOUBLE_EQ(dst.y, 6.0);
    EXPECT_DOUBLE_EQ(dst.z, 7.0);
}

TEST(Vec3, Normalize)
{
    Vec3 v(3.0, 4.0, 0.0);
    v.Normalize();
    EXPECT_NEAR(v.VAbs(), 1.0, 1e-10);
    EXPECT_NEAR(v.x, 0.6, 1e-10);
    EXPECT_NEAR(v.y, 0.8, 1e-10);
    EXPECT_NEAR(v.z, 0.0, 1e-10);
}

TEST(Vec3, VAbs)
{
    Vec3 v(3.0, 4.0, 0.0);
    EXPECT_NEAR(v.VAbs(), 5.0, 1e-10);
}

TEST(Vec3, DotProduct)
{
    Vec3 a(1.0, 2.0, 3.0);
    Vec3 b(4.0, 5.0, 6.0);
    EXPECT_NEAR(a.dot(b), 32.0, 1e-10);
}

TEST(Vec3, CrossMethod)
{
    Vec3 a(1.0, 0.0, 0.0);
    Vec3 b(0.0, 1.0, 0.0);

    Vec3 c1 = a.cross(b);
    EXPECT_NEAR(c1.x, 0.0, 1e-10);
    EXPECT_NEAR(c1.y, 0.0, 1e-10);
    EXPECT_NEAR(c1.z, 1.0, 1e-10);

    Vec3 c2 = Vec3::cross(a, b);
    EXPECT_NEAR(c2.x, 0.0, 1e-10);
    EXPECT_NEAR(c2.y, 0.0, 1e-10);
    EXPECT_NEAR(c2.z, 1.0, 1e-10);
}

TEST(Vec3, IsSame)
{
    Vec3 a(1.0, 2.0, 3.0);
    Vec3 b(1.0, 2.0, 3.0);
    Vec3 c(1.0, 2.0, 4.0);
    EXPECT_TRUE(a.IsSame(b));
    EXPECT_FALSE(a.IsSame(c));
}

TEST(Vec3, Translate)
{
    Vec3 v(1.0, 2.0, 3.0);
    Vec3 t(10.0, 20.0, 30.0);
    v.Translate(t);
    EXPECT_DOUBLE_EQ(v.x, 11.0);
    EXPECT_DOUBLE_EQ(v.y, 22.0);
    EXPECT_DOUBLE_EQ(v.z, 33.0);
}

TEST(Vec3, RotateAroundOrigin)
{
    Vec3 v(1.0, 0.0, 0.0);
    Vec3 axis(0.0, 0.0, 1.0);
    v.Rotate(axis, 90.0);
    EXPECT_NEAR(v.x, 0.0, 1e-10);
    EXPECT_NEAR(v.y, 1.0, 1e-10);
    EXPECT_NEAR(v.z, 0.0, 1e-10);
}

TEST(Vec3, RotateAroundPoint)
{
    Vec3 v(2.0, 0.0, 0.0);
    Vec3 O(1.0, 0.0, 0.0);
    Vec3 axis(0.0, 0.0, 1.0);
    v.Rotate(O, axis, 90.0);
    EXPECT_NEAR(v.x, 1.0, 1e-10);
    EXPECT_NEAR(v.y, 1.0, 1e-10);
    EXPECT_NEAR(v.z, 0.0, 1e-10);
}

TEST(Vec3, RotateZAroundPoint)
{
    Vec3 v(2.0, 0.0, 5.0);
    Vec3 O(1.0, 0.0, 5.0);
    v.RotateZ(O, 90.0);
    EXPECT_NEAR(v.x, 1.0, 1e-10);
    EXPECT_NEAR(v.y, 1.0, 1e-10);
    EXPECT_NEAR(v.z, 5.0, 1e-10);
}

TEST(Vec3, RotZRadians)
{
    Vec3 v(1.0, 0.0, 0.0);
    v.RotZ(M_PI / 2.0);
    EXPECT_NEAR(v.x, 0.0, 1e-10);
    EXPECT_NEAR(v.y, 1.0, 1e-10);
    EXPECT_NEAR(v.z, 0.0, 1e-10);
}

TEST(Vec3, RotXRadians)
{
    Vec3 v(0.0, 1.0, 0.0);
    v.RotX(M_PI / 2.0);
    EXPECT_NEAR(v.x, 0.0, 1e-10);
    EXPECT_NEAR(v.y, 0.0, 1e-10);
    EXPECT_NEAR(v.z, 1.0, 1e-10);
}

TEST(Vec3, RotYRadians)
{
    Vec3 v(1.0, 0.0, 0.0);
    v.RotY(M_PI / 2.0);
    EXPECT_NEAR(v.x, 0.0, 1e-10);
    EXPECT_NEAR(v.y, 0.0, 1e-10);
    EXPECT_NEAR(v.z, -1.0, 1e-10);
}

TEST(Vec3, RotateYDegrees)
{
    Vec3 v(1.0, 0.0, 0.0);
    v.RotateY(90.0);
    EXPECT_NEAR(v.x, 0.0, 1e-10);
    EXPECT_NEAR(v.y, 0.0, 1e-10);
    EXPECT_NEAR(v.z, -1.0, 1e-10);
}

TEST(Vec3f, DefaultConstructor)
{
    Vec3f v;
    EXPECT_FLOAT_EQ(v.x, 0.0f);
    EXPECT_FLOAT_EQ(v.y, 0.0f);
    EXPECT_FLOAT_EQ(v.z, 0.0f);
}

TEST(Vec3f, ParameterizedConstructor)
{
    Vec3f v(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
}

TEST(Vec3f, EigenConstructor)
{
    Eigen::Vector3f ev(4.0f, 5.0f, 6.0f);
    Vec3f v(ev);
    EXPECT_FLOAT_EQ(v.x, 4.0f);
    EXPECT_FLOAT_EQ(v.y, 5.0f);
    EXPECT_FLOAT_EQ(v.z, 6.0f);
}

TEST(Vec3f, Equality)
{
    Vec3f a(1.0f, 2.0f, 3.0f);
    Vec3f b(1.0f, 2.0f, 3.0f);
    Vec3f c(1.0f, 2.0f, 4.0f);
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
}

TEST(Vec3f, AddSubtract)
{
    Vec3f a(1.0f, 2.0f, 3.0f);
    Vec3f b(4.0f, 5.0f, 6.0f);

    Vec3f sum = a + b;
    EXPECT_FLOAT_EQ(sum.x, 5.0f);
    EXPECT_FLOAT_EQ(sum.y, 7.0f);
    EXPECT_FLOAT_EQ(sum.z, 9.0f);

    Vec3f diff = b - a;
    EXPECT_FLOAT_EQ(diff.x, 3.0f);
    EXPECT_FLOAT_EQ(diff.y, 3.0f);
    EXPECT_FLOAT_EQ(diff.z, 3.0f);

    Vec3f a2(1.0f, 2.0f, 3.0f);
    a2 += b;
    EXPECT_FLOAT_EQ(a2.x, 5.0f);
    EXPECT_FLOAT_EQ(a2.y, 7.0f);
    EXPECT_FLOAT_EQ(a2.z, 9.0f);

    Vec3f b2(4.0f, 5.0f, 6.0f);
    b2 -= a;
    EXPECT_FLOAT_EQ(b2.x, 3.0f);
    EXPECT_FLOAT_EQ(b2.y, 3.0f);
    EXPECT_FLOAT_EQ(b2.z, 3.0f);
}

TEST(Vec3f, CrossTypeAssignment)
{
    Vec3 vd(7.0, 8.0, 9.0);
    Vec3f vf;
    vf = vd;
    EXPECT_FLOAT_EQ(vf.x, 7.0f);
    EXPECT_FLOAT_EQ(vf.y, 8.0f);
    EXPECT_FLOAT_EQ(vf.z, 9.0f);
}

TEST(Vec3f, ScalarMultiplyDivide)
{
    Vec3f v(2.0f, 4.0f, 6.0f);

    Vec3f mul = v * 3.0f;
    EXPECT_FLOAT_EQ(mul.x, 6.0f);
    EXPECT_FLOAT_EQ(mul.y, 12.0f);
    EXPECT_FLOAT_EQ(mul.z, 18.0f);

    Vec3f div = v / 2.0f;
    EXPECT_FLOAT_EQ(div.x, 1.0f);
    EXPECT_FLOAT_EQ(div.y, 2.0f);
    EXPECT_FLOAT_EQ(div.z, 3.0f);
}

TEST(Vec3f, VAbs)
{
    Vec3f v(3.0f, 4.0f, 0.0f);
    EXPECT_NEAR(v.VAbs(), 5.0f, 1e-5f);
}

TEST(Vec3f, DotProduct)
{
    Vec3f a(1.0f, 2.0f, 3.0f);
    Vec3f b(4.0f, 5.0f, 6.0f);
    EXPECT_NEAR(a.dot(b), 32.0f, 1e-5f);
}

TEST(Vec3f, Normalize)
{
    Vec3f v(3.0f, 4.0f, 0.0f);
    v.Normalize();
    EXPECT_NEAR(v.VAbs(), 1.0f, 1e-5f);
}

TEST(Vec3f, Translate)
{
    Vec3f v(1.0f, 2.0f, 3.0f);
    Vec3f t(10.0f, 20.0f, 30.0f);
    v.Translate(t);
    EXPECT_FLOAT_EQ(v.x, 11.0f);
    EXPECT_FLOAT_EQ(v.y, 22.0f);
    EXPECT_FLOAT_EQ(v.z, 33.0f);
}

TEST(Vec3f, CrossProduct)
{
    Vec3f a(1.0f, 0.0f, 0.0f);
    Vec3f b(0.0f, 1.0f, 0.0f);
    Vec3f c = a * b;
    EXPECT_NEAR(c.x, 0.0f, 1e-5f);
    EXPECT_NEAR(c.y, 0.0f, 1e-5f);
    EXPECT_NEAR(c.z, 1.0f, 1e-5f);
}

TEST(Vec3f, SetAndCopy)
{
    Vec3f v;
    v.Set(10.0f, 20.0f, 30.0f);
    EXPECT_FLOAT_EQ(v.x, 10.0f);
    EXPECT_FLOAT_EQ(v.y, 20.0f);
    EXPECT_FLOAT_EQ(v.z, 30.0f);

    Vec3f src(5.0f, 6.0f, 7.0f);
    v.Set(src);
    EXPECT_FLOAT_EQ(v.x, 5.0f);
    EXPECT_FLOAT_EQ(v.y, 6.0f);
    EXPECT_FLOAT_EQ(v.z, 7.0f);

    Vec3f dst;
    dst.Copy(src);
    EXPECT_FLOAT_EQ(dst.x, 5.0f);
    EXPECT_FLOAT_EQ(dst.y, 6.0f);
    EXPECT_FLOAT_EQ(dst.z, 7.0f);
}

TEST(Quaternion, DefaultConstructor)
{
    Quaternion q;
    double x = 0.0, y = 0.0, z = 0.0;
    q.conj(x, y, z);
    EXPECT_NEAR(x, 0.0, 1e-10);
    EXPECT_NEAR(y, 0.0, 1e-10);
    EXPECT_NEAR(z, 0.0, 1e-10);
}

TEST(Quaternion, SetAndConjVec3)
{
    Quaternion q;
    q.set(90.0, Vec3(0.0, 0.0, 1.0));
    Vec3 v(1.0, 0.0, 0.0);
    q.conj(v);
    EXPECT_NEAR(v.x, 0.0, 1e-10);
    EXPECT_NEAR(v.y, 1.0, 1e-10);
    EXPECT_NEAR(v.z, 0.0, 1e-10);
}

TEST(Quaternion, SetAndConjVec3f)
{
    Quaternion q;
    q.set(90.0, Vec3f(0.0f, 0.0f, 1.0f));
    Vec3f v(1.0f, 0.0f, 0.0f);
    q.conj(v);
    EXPECT_NEAR(v.x, 0.0f, 1e-5f);
    EXPECT_NEAR(v.y, 1.0f, 1e-5f);
    EXPECT_NEAR(v.z, 0.0f, 1e-5f);
}

TEST(Quaternion, SetAndConjDoubles)
{
    Quaternion q;
    q.set(90.0, Vec3(0.0, 0.0, 1.0));
    double x = 1.0, y = 0.0, z = 0.0;
    q.conj(x, y, z);
    EXPECT_NEAR(x, 0.0, 1e-10);
    EXPECT_NEAR(y, 1.0, 1e-10);
    EXPECT_NEAR(z, 0.0, 1e-10);
}

TEST(Quaternion, 90DegreeRotationAroundZ)
{
    Quaternion q;
    q.set(90.0, Vec3(0.0, 0.0, 1.0));
    Vec3 v(1.0, 0.0, 0.0);
    q.conj(v);
    EXPECT_NEAR(v.x, 0.0, 1e-10);
    EXPECT_NEAR(v.y, 1.0, 1e-10);
    EXPECT_NEAR(v.z, 0.0, 1e-10);
}
