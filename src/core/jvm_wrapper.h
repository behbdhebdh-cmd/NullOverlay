#pragma once

#include "pch.h"

namespace core {

class LocalFrame {
public:
    explicit LocalFrame(JNIEnv* env, jint capacity = 64);
    ~LocalFrame();

    LocalFrame(const LocalFrame&) = delete;
    LocalFrame& operator=(const LocalFrame&) = delete;

    bool ok() const { return ok_; }

private:
    JNIEnv* env_{};
    bool ok_{};
};

class GlobalRef {
public:
    GlobalRef() = default;
    explicit GlobalRef(jobject localRef);
    ~GlobalRef();

    GlobalRef(const GlobalRef&) = delete;
    GlobalRef& operator=(const GlobalRef&) = delete;

    GlobalRef(GlobalRef&& other) noexcept;
    GlobalRef& operator=(GlobalRef&& other) noexcept;

    void reset(jobject localRef = nullptr);
    jobject get() const { return object_; }
    explicit operator bool() const { return object_ != nullptr; }

private:
    jobject object_{};
};

class Jvm {
public:
    static bool initialize();
    static void shutdown();

    static bool isReady();
    static JNIEnv* env();
    static jvmtiEnv* jvmti();
    static void detachCurrentThreadIfOwned();

    static bool bootstrapMinecraftClassLoader(std::string_view minecraftBinaryName);
    static jclass findClass(std::string_view binaryOrInternalName);
    static jmethodID getMethodID(JNIEnv* env, jclass cls, const char* name, const char* signature);
    static jfieldID getFieldID(JNIEnv* env, jclass cls, const char* name, const char* signature);
    static bool clearException(JNIEnv* env, std::string_view context);
    static std::string toStdString(JNIEnv* env, jstring value);

private:
    Jvm() = default;
};

} // namespace core
