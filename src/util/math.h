#pragma once

#include "pch.h"

namespace util {

struct Vec2 {
    float x{};
    float y{};
};

struct Vec3 {
    double x{};
    double y{};
    double z{};
};

struct Mat4 {
    std::array<double, 16> m{};

    double operator()(int row, int column) const {
        return m[static_cast<std::size_t>(row * 4 + column)];
    }

    double& operator()(int row, int column) {
        return m[static_cast<std::size_t>(row * 4 + column)];
    }
};

struct CameraState {
    Vec3 position{};
    float pitchDegrees{};
    float yawDegrees{};
    float fovDegrees{70.0f};
    bool valid{};
};

double distance(const Vec3& a, const Vec3& b);
Mat4 makeViewMatrix(const CameraState& camera);
Mat4 makeProjectionMatrix(float fovDegrees, float aspectRatio, float nearPlane, float farPlane);
Vec3 transformPoint(const Mat4& matrix, const Vec3& point);
bool projectPoint(const Vec3& world, const Mat4& view, const Mat4& projection, int width, int height, Vec2& screen);
bool worldToScreen(const Vec3& world, const CameraState& camera, int width, int height, Vec2& screen);

} // namespace util
