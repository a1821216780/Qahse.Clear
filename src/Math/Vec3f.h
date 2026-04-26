//**********************************************************************************************************************************
// 许可证说明
// 版权所有(C) 2021, 2025  赵子祯
//
// 根据 Boost 软件许可证 - 版本 1.0 - 2003年8月17日
// 您不得使用此文件，除非符合许可证。
// 您可以在以下网址获得许可证副本
//
//     http://www.hawtc.cn/licenses.txt
//
// 该软件按“原样”提供，不提供任何明示或暗示的保证，包括但不限于适销性、特定用途的适用性、所有权和非侵权。在任何情况下，版权持有人或任何分
// 发软件的人都不对任何索赔、损害或其他责任负责，无论是在合同诉讼、侵权诉讼或其他诉讼中，还是在软件使用或其他交易中产生的。
//
//**********************************************************************************************************************************

// ───────────────────────────────── File Info ─────────────────────────────────
//
// 该文件实现了 Vec3f 类，表示三维空间中的单精度浮点向量，并提供了与 Eigen 库的无缝集成。Vec3f 类支持基本的向量运算、归一化、点积和旋转等功能，适用于各种数学计算和图形应用。
//
// ──────────────────────────────────────────────────────────────────────────────



#pragma once

#include <cmath>
#include <cstdint>
#include <cstddef>
#include "Vec3.h"

class Vec3i
{
public:
    int16_t x;
    int16_t y;
    int16_t z;

    Vec3i() : x(0), y(0), z(0) {}
};

class Vec3f
{
public:
    float x;
    float y;
    float z;

    // --- constructors ---

    Vec3f()
        : x(0.0f), y(0.0f), z(0.0f) {}

    Vec3f(float xi, float yi, float zi)
        : x(xi), y(yi), z(zi) {}

    // Construct from Eigen vector
    Vec3f(const Eigen::Vector3f &v)
        : x(v[0]), y(v[1]), z(v[2]) {}

    // --- Eigen interop (zero-copy map over x,y,z storage) ---

    Eigen::Map<Eigen::Vector3f> eigen()
    {
        return Eigen::Map<Eigen::Vector3f>(&x);
    }

    Eigen::Map<const Eigen::Vector3f> eigen() const
    {
        return Eigen::Map<const Eigen::Vector3f>(&x);
    }

    // --- operators (Eigen-accelerated) ---

    bool operator==(Vec3f const &V) const
    {
        return (eigen() - V.eigen()).squaredNorm() < 1e-9f;
    }

    // Cross-type assignment from double-precision Vec3
    void operator=(Vec3 const &T)
    {
        x = static_cast<float>(T.x);
        y = static_cast<float>(T.y);
        z = static_cast<float>(T.z);
    }

    void operator+=(Vec3f const &T) { eigen() += T.eigen(); }
    void operator+=(Vec3 const &T)
    {
        x += static_cast<float>(T.x);
        y += static_cast<float>(T.y);
        z += static_cast<float>(T.z);
    }
    void operator-=(Vec3f const &T) { eigen() -= T.eigen(); }
    void operator-=(Vec3 const &T)
    {
        x -= static_cast<float>(T.x);
        y -= static_cast<float>(T.y);
        z -= static_cast<float>(T.z);
    }
    void operator*=(float d) { eigen() *= d; }

    Vec3f operator*(float d) const
    {
        Vec3f r;
        r.eigen() = eigen() * d;
        return r;
    }
    Vec3f operator/(float d) const
    {
        Vec3f r;
        r.eigen() = eigen() / d;
        return r;
    }

    Vec3f operator+(Vec3f const &V) const
    {
        Vec3f r;
        r.eigen() = eigen() + V.eigen();
        return r;
    }
    Vec3f operator-(Vec3f const &V) const
    {
        Vec3f r;
        r.eigen() = eigen() - V.eigen();
        return r;
    }

    // cross product (Eigen SIMD-friendly)
    Vec3f operator*(Vec3f const &T) const
    {
        Vec3f r;
        r.eigen() = eigen().cross(T.eigen());
        return r;
    }

    // --- utilities (Eigen-accelerated) ---

    void Set(float x0, float y0, float z0)
    {
        x = x0;
        y = y0;
        z = z0;
    }
    void Set(Vec3f const &V) { eigen() = V.eigen(); }
    void Copy(Vec3f const &V) { eigen() = V.eigen(); }

    void Normalize()
    {
        float abs = VAbs();
        if (abs < 1.e-10f)
            return;
        eigen() /= abs;
    }

    float VAbs() const { return eigen().norm(); }
    float dot(Vec3f const &V) const { return eigen().dot(V.eigen()); }

    bool IsSame(Vec3f const &V) const
    {
        return (eigen() - V.eigen()).squaredNorm() < 1e-9f;
    }

    void Translate(Vec3f const &T) { eigen() += T.eigen(); }

    // rotation methods (implemented in Vec3f.cpp)
    void Rotate(Vec3f &O, Vec3f const &R, double Angle);
    void RotateN(Vec3f n, float NTilt);
    void RotateX(Vec3f const &O, float XTilt);
    void RotateY(Vec3f const &O, float YTilt);
    void RotateZ(Vec3f const &O, float ZTilt);
    void RotateY(float YTilt);
    void RotZ(float ZTilt);
    void RotY(float YTilt);
};

// Safety: Eigen::Map requires contiguous x,y,z storage
static_assert(sizeof(Vec3f) == 3 * sizeof(float),
              "Vec3f must be tightly packed for Eigen::Map");
