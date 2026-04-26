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
// 该文件实现了 Quaternion 类，提供了基于四元数的旋转功能。Quaternion 类支持将三维向量
// 绕任意轴旋转指定角度，并且内部使用四元数共轭变换来实现旋转操作，避免了万向锁问题。该
//类还包含了设置旋转参数的方法，方便在仿真和图形应用中使用。
//
// ──────────────────────────────────────────────────────────────────────────────



#pragma once

#include <cmath>
#include "../Params.h"
#include "Vec3.h"
#include "Vec3f.h"

class Quaternion
{
public:
    Quaternion()
        : a(0.0), qx(0.0), qy(0.0), qz(0.0), ang_rad(0.0)
    {
        setmat();
    }

    void conj(Vec3 &vec)
    {
        v.x = vec.x;
        v.y = vec.y;
        v.z = vec.z;
        vec.x = 2.0 * ((m8 + m10) * v.x + (m6 - m4) * v.y + (m3 + m7) * v.z) + v.x;
        vec.y = 2.0 * ((m4 + m6) * v.x + (m5 + m10) * v.y + (m9 - m2) * v.z) + v.y;
        vec.z = 2.0 * ((m7 - m3) * v.x + (m2 + m9) * v.y + (m5 + m8) * v.z) + v.z;
    }

    void conj(Vec3f &vec)
    {
        v.x = vec.x;
        v.y = vec.y;
        v.z = vec.z;
        vec.x = static_cast<float>(2.0 * ((m8 + m10) * v.x + (m6 - m4) * v.y + (m3 + m7) * v.z) + v.x);
        vec.y = static_cast<float>(2.0 * ((m4 + m6) * v.x + (m5 + m10) * v.y + (m9 - m2) * v.z) + v.y);
        vec.z = static_cast<float>(2.0 * ((m7 - m3) * v.x + (m2 + m9) * v.y + (m5 + m8) * v.z) + v.z);
    }

    void conj(double &x, double &y, double &z)
    {
        v.x = x;
        v.y = y;
        v.z = z;
        x = 2.0 * ((m8 + m10) * v.x + (m6 - m4) * v.y + (m3 + m7) * v.z) + v.x;
        y = 2.0 * ((m4 + m6) * v.x + (m5 + m10) * v.y + (m9 - m2) * v.z) + v.y;
        z = 2.0 * ((m7 - m3) * v.x + (m2 + m9) * v.y + (m5 + m8) * v.z) + v.z;
    }

    void setmat()
    {
        m2 = a * qx;
        m3 = a * qy;
        m4 = a * qz;
        m5 = -qx * qx;
        m6 = qx * qy;
        m7 = qx * qz;
        m8 = -qy * qy;
        m9 = qy * qz;
        m10 = -qz * qz;
    }

    void set(double ang_deg, Vec3 const &v)
    {
        Vec3 n = v;
        n.Normalize();
        ang_rad = ang_deg * M_PI / 180.0;
        a = std::cos(ang_rad / 2.0);
        double sina = std::sin(ang_rad / 2.0);
        qx = n.x * sina;
        qy = n.y * sina;
        qz = n.z * sina;
        setmat();
    }

    void set(double ang_deg, Vec3f const &v)
    {
        Vec3f n = v;
        n.Normalize();
        ang_rad = ang_deg * M_PI / 180.0;
        a = std::cos(ang_rad / 2.0);
        double sina = std::sin(ang_rad / 2.0);
        qx = n.x * sina;
        qy = n.y * sina;
        qz = n.z * sina;
        setmat();
    }

private:
    double ang_rad;
    double m2, m3, m4, m5, m6, m7, m8, m9, m10;
    Vec3 v;
    double a, qx, qy, qz;
};