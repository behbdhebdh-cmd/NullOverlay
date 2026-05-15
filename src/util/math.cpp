#include "util/math.h"

namespace {

constexpr double kPi = 3.14159265358979323846;

double radians(double degrees) {
    return degrees * kPi / 180.0;
}

} // namespace

namespace util {

double distance(const Vec3& a, const Vec3& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

Mat4 makeViewMatrix(const CameraState& camera) {
    const double yaw = radians(static_cast<double>(camera.yawDegrees) + 180.0);
    const double pitch = radians(static_cast<double>(camera.pitchDegrees));

    const double sinYaw = std::sin(yaw);
    const double cosYaw = std::cos(yaw);
    const double sinPitch = std::sin(pitch);
    const double cosPitch = std::cos(pitch);

    Mat4 view{};
    view(0, 0) = cosYaw;
    view(0, 1) = 0.0;
    view(0, 2) = -sinYaw;
    view(0, 3) = -(view(0, 0) * camera.position.x + view(0, 2) * camera.position.z);

    view(1, 0) = -sinYaw * sinPitch;
    view(1, 1) = cosPitch;
    view(1, 2) = -cosYaw * sinPitch;
    view(1, 3) = -(view(1, 0) * camera.position.x + view(1, 1) * camera.position.y + view(1, 2) * camera.position.z);

    view(2, 0) = sinYaw * cosPitch;
    view(2, 1) = sinPitch;
    view(2, 2) = cosYaw * cosPitch;
    view(2, 3) = -(view(2, 0) * camera.position.x + view(2, 1) * camera.position.y + view(2, 2) * camera.position.z);

    view(3, 3) = 1.0;
    return view;
}

Mat4 makeProjectionMatrix(float fovDegrees, float aspectRatio, float nearPlane, float farPlane) {
    const double clampedFov = std::clamp(static_cast<double>(fovDegrees), 30.0, 120.0);
    const double aspect = std::max(0.01, static_cast<double>(aspectRatio));
    const double nearZ = std::max(0.001, static_cast<double>(nearPlane));
    const double farZ = std::max(nearZ + 1.0, static_cast<double>(farPlane));
    const double f = 1.0 / std::tan(radians(clampedFov) * 0.5);

    Mat4 projection{};
    projection(0, 0) = f / aspect;
    projection(1, 1) = f;
    projection(2, 2) = farZ / (farZ - nearZ);
    projection(2, 3) = -(farZ * nearZ) / (farZ - nearZ);
    projection(3, 2) = 1.0;
    return projection;
}

Vec3 transformPoint(const Mat4& matrix, const Vec3& point) {
    return {
        matrix(0, 0) * point.x + matrix(0, 1) * point.y + matrix(0, 2) * point.z + matrix(0, 3),
        matrix(1, 0) * point.x + matrix(1, 1) * point.y + matrix(1, 2) * point.z + matrix(1, 3),
        matrix(2, 0) * point.x + matrix(2, 1) * point.y + matrix(2, 2) * point.z + matrix(2, 3),
    };
}

bool projectPoint(const Vec3& world, const Mat4& view, const Mat4& projection, int width, int height, Vec2& screen) {
    if (width <= 0 || height <= 0) {
        return false;
    }

    const Vec3 cameraSpace = transformPoint(view, world);
    if (cameraSpace.z <= 0.05) {
        return false;
    }

    const double ndcX = (cameraSpace.x * projection(0, 0)) / cameraSpace.z;
    const double ndcY = (cameraSpace.y * projection(1, 1)) / cameraSpace.z;

    const double projectedX = (ndcX * 0.5 + 0.5) * static_cast<double>(width);
    const double projectedY = (0.5 - ndcY * 0.5) * static_cast<double>(height);

    if (!std::isfinite(projectedX) || !std::isfinite(projectedY)) {
        return false;
    }

    constexpr double kMargin = 256.0;
    if (projectedX < -kMargin || projectedX > static_cast<double>(width) + kMargin ||
        projectedY < -kMargin || projectedY > static_cast<double>(height) + kMargin) {
        return false;
    }

    screen.x = static_cast<float>(projectedX);
    screen.y = static_cast<float>(projectedY);
    return true;
}

bool worldToScreen(const Vec3& world, const CameraState& camera, int width, int height, Vec2& screen) {
    if (!camera.valid || width <= 0 || height <= 0) {
        return false;
    }

    const double yaw = radians(static_cast<double>(camera.yawDegrees));
    const double pitch = radians(static_cast<double>(camera.pitchDegrees));
    const double sinYaw = std::sin(yaw);
    const double cosYaw = std::cos(yaw);
    const double sinPitch = std::sin(pitch);
    const double cosPitch = std::cos(pitch);

    const Vec3 delta{
        world.x - camera.position.x,
        world.y - camera.position.y,
        world.z - camera.position.z,
    };

    const Vec3 forward{
        -sinYaw * cosPitch,
        -sinPitch,
        cosYaw * cosPitch,
    };
    const Vec3 right{
        -cosYaw,
        0.0,
        -sinYaw,
    };
    const Vec3 up{
        right.y * forward.z - right.z * forward.y,
        right.z * forward.x - right.x * forward.z,
        right.x * forward.y - right.y * forward.x,
    };

    const double cameraX = delta.x * right.x + delta.y * right.y + delta.z * right.z;
    const double cameraY = delta.x * up.x + delta.y * up.y + delta.z * up.z;
    const double cameraZ = delta.x * forward.x + delta.y * forward.y + delta.z * forward.z;
    if (cameraZ <= 0.05) {
        return false;
    }

    const double aspectRatio = static_cast<double>(width) / static_cast<double>(height);
    const double verticalFov = std::clamp(static_cast<double>(camera.fovDegrees), 30.0, 120.0);
    const double yScale = 1.0 / std::tan(radians(verticalFov) * 0.5);
    const double xScale = yScale / std::max(0.01, aspectRatio);
    const double ndcX = (cameraX * xScale) / cameraZ;
    const double ndcY = (cameraY * yScale) / cameraZ;
    const double projectedX = (ndcX * 0.5 + 0.5) * static_cast<double>(width);
    const double projectedY = (0.5 - ndcY * 0.5) * static_cast<double>(height);

    if (!std::isfinite(projectedX) || !std::isfinite(projectedY)) {
        return false;
    }

    constexpr double kMargin = 256.0;
    if (projectedX < -kMargin || projectedX > static_cast<double>(width) + kMargin ||
        projectedY < -kMargin || projectedY > static_cast<double>(height) + kMargin) {
        return false;
    }

    screen.x = static_cast<float>(projectedX);
    screen.y = static_cast<float>(projectedY);
    return true;
}

} // namespace util
