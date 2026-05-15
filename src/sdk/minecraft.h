#pragma once

#include "pch.h"
#include "core/jvm_wrapper.h"
#include "sdk/entity.h"
#include "util/math.h"

namespace sdk {

class Minecraft {
public:
    static Minecraft& instance();

    bool initialize();
    void shutdown();
    bool isReady() const { return ready_; }

    bool isSingleplayerWorldSafe(std::string* reason = nullptr);
    bool getWindowSize(int& width, int& height);
    bool getCamera(util::CameraState& camera);
    bool collectEntities(const EntityFilters& filters, std::vector<EntitySnapshot>& entities);
    void applyFullbright(bool enabled);

private:
    Minecraft() = default;

    jobject getMinecraftObject(JNIEnv* env) const;
    jobject getObjectField(JNIEnv* env, jobject owner, jfieldID field, std::string_view label) const;
    jobject getGammaOption(JNIEnv* env, jobject minecraft) const;
    bool readGammaValue(JNIEnv* env, jobject gammaOption, double& value) const;
    bool setGammaValue(JNIEnv* env, jobject gammaOption, double value) const;

    mutable std::recursive_mutex mutex_;
    bool ready_{};

    core::GlobalRef minecraftClass_;
    core::GlobalRef windowClass_;
    core::GlobalRef optionsClass_;
    core::GlobalRef optionInstanceClass_;
    core::GlobalRef doubleClass_;
    core::GlobalRef gameRendererClass_;
    core::GlobalRef cameraClass_;
    core::GlobalRef vec3Class_;

    jmethodID getInstance_{};
    jmethodID hasSingleplayerServer_{};
    jmethodID getSingleplayerServer_{};
    jmethodID getCurrentServer_{};
    jmethodID getConnection_{};
    jmethodID getWindow_{};

    jfieldID singleplayerServerField_{};
    jfieldID levelField_{};
    jfieldID playerField_{};
    jfieldID gameRendererField_{};
    jfieldID optionsField_{};

    jmethodID windowGuiScaledWidth_{};
    jmethodID windowGuiScaledHeight_{};

    jmethodID optionsGamma_{};
    jmethodID optionInstanceGet_{};
    jmethodID optionInstanceSet_{};
    jmethodID doubleValueOf_{};
    jmethodID doubleDoubleValue_{};

    jmethodID gameRendererGetMainCamera_{};
    jmethodID cameraGetPosition_{};
    jmethodID cameraGetXRot_{};
    jmethodID cameraGetYRot_{};

    jfieldID vec3X_{};
    jfieldID vec3Y_{};
    jfieldID vec3Z_{};

    bool fullbrightApplied_{};
    bool originalGammaStored_{};
    double originalGamma_{};
    std::chrono::steady_clock::time_point lastFullbrightApply_{};
};

} // namespace sdk
