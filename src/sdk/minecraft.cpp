#include "sdk/minecraft.h"

#include "config/mappings.h"
#include "sdk/world.h"
#include "util/logging.h"

namespace {

template <std::size_t N>
jmethodID resolveMethod(JNIEnv* env,
                        jclass cls,
                        const std::array<config::mappings::MemberMapping, N>& candidates,
                        std::string_view label,
                        bool isStatic = false) {
    for (const auto& candidate : candidates) {
        jmethodID method = isStatic
            ? env->GetStaticMethodID(cls, candidate.name, candidate.signature)
            : core::Jvm::getMethodID(env, cls, candidate.name, candidate.signature);
        if (method != nullptr && !core::Jvm::clearException(env, label)) {
            return method;
        }
        core::Jvm::clearException(env, label);
    }

    std::string message("Failed to resolve method ");
    message.append(label);
    util::logError(message);
    return nullptr;
}

template <std::size_t N>
jmethodID resolveOptionalMethod(JNIEnv* env,
                                jclass cls,
                                const std::array<config::mappings::MemberMapping, N>& candidates,
                                std::string_view label,
                                bool isStatic = false) {
    for (const auto& candidate : candidates) {
        jmethodID method = isStatic
            ? env->GetStaticMethodID(cls, candidate.name, candidate.signature)
            : core::Jvm::getMethodID(env, cls, candidate.name, candidate.signature);
        if (method != nullptr && !core::Jvm::clearException(env, label)) {
            return method;
        }
        core::Jvm::clearException(env, label);
    }

    std::string message("Optional method was not resolved: ");
    message.append(label);
    util::logWarning(message);
    return nullptr;
}

template <std::size_t N>
jfieldID resolveField(JNIEnv* env,
                      jclass cls,
                      const std::array<config::mappings::MemberMapping, N>& candidates,
                      std::string_view label) {
    for (const auto& candidate : candidates) {
        jfieldID field = core::Jvm::getFieldID(env, cls, candidate.name, candidate.signature);
        if (field != nullptr && !core::Jvm::clearException(env, label)) {
            return field;
        }
        core::Jvm::clearException(env, label);
    }

    std::string message("Failed to resolve field ");
    message.append(label);
    util::logError(message);
    return nullptr;
}

template <std::size_t N>
jfieldID resolveOptionalField(JNIEnv* env,
                              jclass cls,
                              const std::array<config::mappings::MemberMapping, N>& candidates,
                              std::string_view label) {
    for (const auto& candidate : candidates) {
        jfieldID field = core::Jvm::getFieldID(env, cls, candidate.name, candidate.signature);
        if (field != nullptr && !core::Jvm::clearException(env, label)) {
            return field;
        }
        core::Jvm::clearException(env, label);
    }

    std::string message("Optional field was not resolved: ");
    message.append(label);
    util::logWarning(message);
    return nullptr;
}

core::GlobalRef resolveGlobalClass(JNIEnv* env, std::string_view name) {
    jclass localClass = core::Jvm::findClass(name);
    if (localClass == nullptr) {
        std::string message("Failed to resolve class ");
        message.append(name);
        util::logError(message);
        return {};
    }

    core::GlobalRef global(localClass);
    env->DeleteLocalRef(localClass);
    return global;
}

bool requireObject(JNIEnv* env, jobject object, std::string* reason, const char* missingReason) {
    if (core::Jvm::clearException(env, missingReason)) {
        if (reason != nullptr) {
            *reason = missingReason;
        }
        return false;
    }

    if (object == nullptr) {
        if (reason != nullptr) {
            *reason = missingReason;
        }
        return false;
    }

    return true;
}

} // namespace

namespace sdk {

Minecraft& Minecraft::instance() {
    static Minecraft minecraft;
    return minecraft;
}

bool Minecraft::initialize() {
    std::lock_guard lock(mutex_);
    if (ready_) {
        return true;
    }

    if (!core::Jvm::isReady()) {
        util::logError("Minecraft SDK cannot initialize before JVM wrapper is ready");
        return false;
    }

    using namespace config::mappings;

    if (!core::Jvm::bootstrapMinecraftClassLoader(kMinecraftClass)) {
        util::logWarning("Minecraft SDK class-loader bootstrap failed; overlay remains inactive");
        return false;
    }

    JNIEnv* env = core::Jvm::env();
    if (env == nullptr) {
        return false;
    }

    core::LocalFrame frame(env, 96);
    if (!frame.ok()) {
        return false;
    }

    minecraftClass_ = resolveGlobalClass(env, kMinecraftClass);
    windowClass_ = resolveGlobalClass(env, kWindowClass);
    optionsClass_ = resolveGlobalClass(env, kOptionsClass);
    optionInstanceClass_ = resolveGlobalClass(env, kOptionInstanceClass);
    doubleClass_ = resolveGlobalClass(env, kDoubleClass);
    gameRendererClass_ = resolveGlobalClass(env, kGameRendererClass);
    cameraClass_ = resolveGlobalClass(env, kCameraClass);
    vec3Class_ = resolveGlobalClass(env, kVec3Class);

    if (!minecraftClass_ || !windowClass_ || !gameRendererClass_ || !cameraClass_ || !vec3Class_) {
        shutdown();
        return false;
    }

    const auto minecraftClass = static_cast<jclass>(minecraftClass_.get());
    const auto windowClass = static_cast<jclass>(windowClass_.get());
    const auto optionsClass = static_cast<jclass>(optionsClass_.get());
    const auto optionInstanceClass = static_cast<jclass>(optionInstanceClass_.get());
    const auto doubleClass = static_cast<jclass>(doubleClass_.get());
    const auto gameRendererClass = static_cast<jclass>(gameRendererClass_.get());
    const auto cameraClass = static_cast<jclass>(cameraClass_.get());
    const auto vec3Class = static_cast<jclass>(vec3Class_.get());

    getInstance_ = resolveMethod(env, minecraftClass, kMinecraftGetInstance, "Minecraft.getInstance", true);
    hasSingleplayerServer_ = resolveMethod(env, minecraftClass, kMinecraftHasSingleplayerServer, "Minecraft.hasSingleplayerServer");
    getSingleplayerServer_ = resolveOptionalMethod(env, minecraftClass, kMinecraftGetSingleplayerServer, "Minecraft.getSingleplayerServer");
    getCurrentServer_ = resolveMethod(env, minecraftClass, kMinecraftGetCurrentServer, "Minecraft.getCurrentServer");
    getConnection_ = resolveMethod(env, minecraftClass, kMinecraftGetConnection, "Minecraft.getConnection");
    getWindow_ = resolveMethod(env, minecraftClass, kMinecraftGetWindow, "Minecraft.getWindow");

    singleplayerServerField_ = resolveOptionalField(env, minecraftClass, kMinecraftSingleplayerServerField, "Minecraft.singleplayerServer");
    levelField_ = resolveField(env, minecraftClass, kMinecraftLevelField, "Minecraft.level");
    playerField_ = resolveField(env, minecraftClass, kMinecraftPlayerField, "Minecraft.player");
    gameRendererField_ = resolveField(env, minecraftClass, kMinecraftGameRendererField, "Minecraft.gameRenderer");
    optionsField_ = resolveOptionalField(env, minecraftClass, kMinecraftOptionsField, "Minecraft.options");

    windowGuiScaledWidth_ = resolveMethod(env, windowClass, kWindowGuiScaledWidth, "Window.getGuiScaledWidth");
    windowGuiScaledHeight_ = resolveMethod(env, windowClass, kWindowGuiScaledHeight, "Window.getGuiScaledHeight");

    if (optionsClass != nullptr && optionInstanceClass != nullptr && doubleClass != nullptr) {
        optionsGamma_ = resolveOptionalMethod(env, optionsClass, kOptionsGamma, "Options.gamma");
        optionInstanceGet_ = resolveOptionalMethod(env, optionInstanceClass, kOptionInstanceGet, "OptionInstance.get");
        optionInstanceSet_ = resolveOptionalMethod(env, optionInstanceClass, kOptionInstanceSet, "OptionInstance.set");
        doubleValueOf_ = resolveOptionalMethod(env, doubleClass, kDoubleValueOf, "Double.valueOf", true);
        doubleDoubleValue_ = resolveOptionalMethod(env, doubleClass, kDoubleDoubleValue, "Double.doubleValue");
    }

    gameRendererGetMainCamera_ = resolveMethod(env, gameRendererClass, kGameRendererGetMainCamera, "GameRenderer.getMainCamera");
    cameraGetPosition_ = resolveMethod(env, cameraClass, kCameraGetPosition, "Camera.getPosition");
    cameraGetXRot_ = resolveMethod(env, cameraClass, kCameraGetXRot, "Camera.getXRot");
    cameraGetYRot_ = resolveMethod(env, cameraClass, kCameraGetYRot, "Camera.getYRot");

    vec3X_ = resolveField(env, vec3Class, kVec3XField, "Vec3.x");
    vec3Y_ = resolveField(env, vec3Class, kVec3YField, "Vec3.y");
    vec3Z_ = resolveField(env, vec3Class, kVec3ZField, "Vec3.z");

    ready_ =
        getInstance_ != nullptr &&
        hasSingleplayerServer_ != nullptr &&
        getCurrentServer_ != nullptr &&
        getConnection_ != nullptr &&
        getWindow_ != nullptr &&
        levelField_ != nullptr &&
        playerField_ != nullptr &&
        gameRendererField_ != nullptr &&
        windowGuiScaledWidth_ != nullptr &&
        windowGuiScaledHeight_ != nullptr &&
        gameRendererGetMainCamera_ != nullptr &&
        cameraGetPosition_ != nullptr &&
        cameraGetXRot_ != nullptr &&
        cameraGetYRot_ != nullptr &&
        vec3X_ != nullptr &&
        vec3Y_ != nullptr &&
        vec3Z_ != nullptr;

    if (!ready_) {
        shutdown();
        return false;
    }

    util::logInfo("Minecraft SDK initialized");
    return true;
}

void Minecraft::shutdown() {
    std::lock_guard lock(mutex_);
    if (fullbrightApplied_) {
        applyFullbright(false);
    }

    ready_ = false;

    clientWorldReader().shutdown();
    entityReader().shutdown();

    minecraftClass_.reset();
    windowClass_.reset();
    optionsClass_.reset();
    optionInstanceClass_.reset();
    doubleClass_.reset();
    gameRendererClass_.reset();
    cameraClass_.reset();
    vec3Class_.reset();

    getInstance_ = nullptr;
    hasSingleplayerServer_ = nullptr;
    getSingleplayerServer_ = nullptr;
    getCurrentServer_ = nullptr;
    getConnection_ = nullptr;
    getWindow_ = nullptr;
    singleplayerServerField_ = nullptr;
    levelField_ = nullptr;
    playerField_ = nullptr;
    gameRendererField_ = nullptr;
    optionsField_ = nullptr;
    windowGuiScaledWidth_ = nullptr;
    windowGuiScaledHeight_ = nullptr;
    optionsGamma_ = nullptr;
    optionInstanceGet_ = nullptr;
    optionInstanceSet_ = nullptr;
    doubleValueOf_ = nullptr;
    doubleDoubleValue_ = nullptr;
    gameRendererGetMainCamera_ = nullptr;
    cameraGetPosition_ = nullptr;
    cameraGetXRot_ = nullptr;
    cameraGetYRot_ = nullptr;
    vec3X_ = nullptr;
    vec3Y_ = nullptr;
    vec3Z_ = nullptr;
    fullbrightApplied_ = false;
    originalGammaStored_ = false;
    originalGamma_ = 0.0;
    lastFullbrightApply_ = {};
}

bool Minecraft::isSingleplayerWorldSafe(std::string* reason) {
    if (!initialize()) {
        if (reason != nullptr) {
            *reason = "Minecraft SDK is not ready";
        }
        return false;
    }

    JNIEnv* env = core::Jvm::env();
    if (env == nullptr) {
        if (reason != nullptr) {
            *reason = "JNIEnv is unavailable";
        }
        return false;
    }

    core::LocalFrame frame(env, 64);
    if (!frame.ok()) {
        if (reason != nullptr) {
            *reason = "JNI local frame allocation failed";
        }
        return false;
    }

    jobject minecraft = getMinecraftObject(env);
    if (!requireObject(env, minecraft, reason, "Minecraft instance is unavailable")) {
        return false;
    }

    const jboolean hasSingleplayer = env->CallBooleanMethod(minecraft, hasSingleplayerServer_);
    if (core::Jvm::clearException(env, "Minecraft.hasSingleplayerServer")) {
        if (reason != nullptr) {
            *reason = "hasSingleplayerServer check failed";
        }
        return false;
    }

    if (hasSingleplayer != JNI_TRUE) {
        if (reason != nullptr) {
            *reason = "Integrated singleplayer server is not active";
        }
        return false;
    }

    if (getSingleplayerServer_ != nullptr || singleplayerServerField_ != nullptr) {
        jobject integratedServer = nullptr;
        if (getSingleplayerServer_ != nullptr) {
            integratedServer = env->CallObjectMethod(minecraft, getSingleplayerServer_);
        } else {
            integratedServer = env->GetObjectField(minecraft, singleplayerServerField_);
        }
        if (!requireObject(env, integratedServer, reason, "Integrated server object is unavailable")) {
            return false;
        }
    }

    jobject currentServer = env->CallObjectMethod(minecraft, getCurrentServer_);
    if (core::Jvm::clearException(env, "Minecraft.getCurrentServer")) {
        if (reason != nullptr) {
            *reason = "Current server state could not be validated";
        }
        return false;
    }

    if (currentServer != nullptr) {
        if (reason != nullptr) {
            *reason = "Remote server metadata is present";
        }
        return false;
    }

    jobject connection = env->CallObjectMethod(minecraft, getConnection_);
    if (!requireObject(env, connection, reason, "Client connection state is unavailable")) {
        return false;
    }

    jobject level = getObjectField(env, minecraft, levelField_, "Minecraft.level");
    if (!requireObject(env, level, reason, "Client level is unavailable")) {
        return false;
    }

    jobject player = getObjectField(env, minecraft, playerField_, "Minecraft.player");
    if (!requireObject(env, player, reason, "Local player is unavailable")) {
        return false;
    }

    if (reason != nullptr) {
        *reason = "Verified integrated singleplayer world";
    }
    return true;
}

bool Minecraft::getWindowSize(int& width, int& height) {
    width = 0;
    height = 0;

    if (!initialize()) {
        return false;
    }

    JNIEnv* env = core::Jvm::env();
    if (env == nullptr) {
        return false;
    }

    core::LocalFrame frame(env, 16);
    if (!frame.ok()) {
        return false;
    }

    jobject minecraft = getMinecraftObject(env);
    if (minecraft == nullptr) {
        return false;
    }

    jobject window = env->CallObjectMethod(minecraft, getWindow_);
    const bool windowException = core::Jvm::clearException(env, "Minecraft.getWindow");
    if (window == nullptr || windowException) {
        return false;
    }

    const jint windowWidth = env->CallIntMethod(window, windowGuiScaledWidth_);
    if (core::Jvm::clearException(env, "Window.getGuiScaledWidth")) {
        return false;
    }

    const jint windowHeight = env->CallIntMethod(window, windowGuiScaledHeight_);
    if (core::Jvm::clearException(env, "Window.getGuiScaledHeight")) {
        return false;
    }

    width = static_cast<int>(windowWidth);
    height = static_cast<int>(windowHeight);
    return width > 0 && height > 0;
}

bool Minecraft::getCamera(util::CameraState& camera) {
    camera = {};

    if (!initialize()) {
        return false;
    }

    JNIEnv* env = core::Jvm::env();
    if (env == nullptr) {
        return false;
    }

    core::LocalFrame frame(env, 32);
    if (!frame.ok()) {
        return false;
    }

    jobject minecraft = getMinecraftObject(env);
    if (minecraft == nullptr) {
        return false;
    }

    jobject gameRenderer = getObjectField(env, minecraft, gameRendererField_, "Minecraft.gameRenderer");
    if (gameRenderer == nullptr) {
        return false;
    }

    jobject cameraObject = env->CallObjectMethod(gameRenderer, gameRendererGetMainCamera_);
    const bool cameraException = core::Jvm::clearException(env, "GameRenderer.getMainCamera");
    if (cameraObject == nullptr || cameraException) {
        return false;
    }

    jobject position = env->CallObjectMethod(cameraObject, cameraGetPosition_);
    const bool positionException = core::Jvm::clearException(env, "Camera.getPosition");
    if (position == nullptr || positionException) {
        return false;
    }

    camera.position.x = static_cast<double>(env->GetDoubleField(position, vec3X_));
    if (core::Jvm::clearException(env, "Vec3.x")) {
        return false;
    }
    camera.position.y = static_cast<double>(env->GetDoubleField(position, vec3Y_));
    if (core::Jvm::clearException(env, "Vec3.y")) {
        return false;
    }
    camera.position.z = static_cast<double>(env->GetDoubleField(position, vec3Z_));
    if (core::Jvm::clearException(env, "Vec3.z")) {
        return false;
    }

    camera.pitchDegrees = static_cast<float>(env->CallFloatMethod(cameraObject, cameraGetXRot_));
    if (core::Jvm::clearException(env, "Camera.getXRot")) {
        return false;
    }
    camera.yawDegrees = static_cast<float>(env->CallFloatMethod(cameraObject, cameraGetYRot_));
    if (core::Jvm::clearException(env, "Camera.getYRot")) {
        return false;
    }

    camera.fovDegrees = 70.0f;
    camera.valid = true;
    return true;
}

bool Minecraft::collectEntities(const EntityFilters& filters, std::vector<EntitySnapshot>& entities) {
    entities.clear();

    if (!initialize()) {
        return false;
    }

    JNIEnv* env = core::Jvm::env();
    if (env == nullptr) {
        return false;
    }

    core::LocalFrame frame(env, 32);
    if (!frame.ok()) {
        return false;
    }

    jobject minecraft = getMinecraftObject(env);
    if (minecraft == nullptr) {
        return false;
    }

    jobject level = getObjectField(env, minecraft, levelField_, "Minecraft.level");
    jobject player = getObjectField(env, minecraft, playerField_, "Minecraft.player");
    if (level == nullptr || player == nullptr) {
        return false;
    }

    util::CameraState camera{};
    if (!getCamera(camera)) {
        return false;
    }

    if (!entityReader().initialize() || !clientWorldReader().initialize()) {
        entities.clear();
        return false;
    }

    return clientWorldReader().collectEntities(env, level, player, camera.position, filters, entities);
}

void Minecraft::applyFullbright(bool enabled) {
    if (!initialize()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (enabled && fullbrightApplied_ &&
        lastFullbrightApply_.time_since_epoch().count() != 0 &&
        now - lastFullbrightApply_ < std::chrono::seconds(1)) {
        return;
    }

    if (!enabled && !fullbrightApplied_) {
        return;
    }

    JNIEnv* env = core::Jvm::env();
    if (env == nullptr) {
        return;
    }

    core::LocalFrame frame(env, 32);
    if (!frame.ok()) {
        return;
    }

    jobject minecraft = getMinecraftObject(env);
    if (minecraft == nullptr) {
        return;
    }

    jobject gammaOption = getGammaOption(env, minecraft);
    if (gammaOption == nullptr) {
        static bool loggedMissingGamma = false;
        if (!loggedMissingGamma) {
            util::logWarning("Fullbright is unavailable because Minecraft Options.gamma could not be resolved");
            loggedMissingGamma = true;
        }
        return;
    }

    if (enabled) {
        if (!originalGammaStored_ && !readGammaValue(env, gammaOption, originalGamma_)) {
            originalGamma_ = 1.0;
        }
        originalGammaStored_ = true;
        if (setGammaValue(env, gammaOption, 15.0)) {
            if (!fullbrightApplied_) {
                util::logInfo("Fullbright enabled; stored original gamma=" + std::to_string(originalGamma_));
            }
            fullbrightApplied_ = true;
            lastFullbrightApply_ = now;
        }
        return;
    }

    if (originalGammaStored_ && setGammaValue(env, gammaOption, originalGamma_)) {
        util::logInfo("Fullbright disabled; restored original gamma=" + std::to_string(originalGamma_));
    }
    fullbrightApplied_ = false;
    originalGammaStored_ = false;
    originalGamma_ = 0.0;
    lastFullbrightApply_ = {};
}

jobject Minecraft::getMinecraftObject(JNIEnv* env) const {
    if (!ready_ || env == nullptr || !minecraftClass_ || getInstance_ == nullptr) {
        return nullptr;
    }

    jobject instance = env->CallStaticObjectMethod(static_cast<jclass>(minecraftClass_.get()), getInstance_);
    if (core::Jvm::clearException(env, "Minecraft.getInstance")) {
        return nullptr;
    }
    return instance;
}

jobject Minecraft::getObjectField(JNIEnv* env, jobject owner, jfieldID field, std::string_view label) const {
    if (env == nullptr || owner == nullptr || field == nullptr) {
        return nullptr;
    }

    jobject value = env->GetObjectField(owner, field);
    if (core::Jvm::clearException(env, label)) {
        return nullptr;
    }
    return value;
}

jobject Minecraft::getGammaOption(JNIEnv* env, jobject minecraft) const {
    if (env == nullptr || minecraft == nullptr || optionsField_ == nullptr || optionsGamma_ == nullptr) {
        return nullptr;
    }

    jobject options = env->GetObjectField(minecraft, optionsField_);
    if (core::Jvm::clearException(env, "Minecraft.options") || options == nullptr) {
        return nullptr;
    }

    jobject gammaOption = env->CallObjectMethod(options, optionsGamma_);
    env->DeleteLocalRef(options);
    if (core::Jvm::clearException(env, "Options.gamma")) {
        return nullptr;
    }
    return gammaOption;
}

bool Minecraft::readGammaValue(JNIEnv* env, jobject gammaOption, double& value) const {
    value = 0.0;
    if (env == nullptr || gammaOption == nullptr || optionInstanceGet_ == nullptr || doubleDoubleValue_ == nullptr) {
        return false;
    }

    jobject boxed = env->CallObjectMethod(gammaOption, optionInstanceGet_);
    if (core::Jvm::clearException(env, "OptionInstance.get") || boxed == nullptr) {
        return false;
    }

    value = static_cast<double>(env->CallDoubleMethod(boxed, doubleDoubleValue_));
    env->DeleteLocalRef(boxed);
    return !core::Jvm::clearException(env, "Double.doubleValue");
}

bool Minecraft::setGammaValue(JNIEnv* env, jobject gammaOption, double value) const {
    if (env == nullptr || gammaOption == nullptr || optionInstanceSet_ == nullptr || doubleValueOf_ == nullptr || !doubleClass_) {
        return false;
    }

    auto doubleClass = static_cast<jclass>(doubleClass_.get());
    jobject boxed = env->CallStaticObjectMethod(doubleClass, doubleValueOf_, static_cast<jdouble>(value));
    if (core::Jvm::clearException(env, "Double.valueOf") || boxed == nullptr) {
        return false;
    }

    env->CallVoidMethod(gammaOption, optionInstanceSet_, boxed);
    env->DeleteLocalRef(boxed);
    return !core::Jvm::clearException(env, "OptionInstance.set");
}

} // namespace sdk
