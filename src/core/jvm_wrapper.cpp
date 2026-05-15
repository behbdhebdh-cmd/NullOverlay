#include "core/jvm_wrapper.h"

#include "util/logging.h"

namespace {

using GetCreatedJavaVMsFn = jint(JNICALL*)(JavaVM**, jsize, jsize*);

std::mutex g_jvmMutex;
JavaVM* g_vm = nullptr;
jvmtiEnv* g_jvmti = nullptr;
core::GlobalRef g_minecraftClassLoader;
jmethodID g_classLoaderLoadClass = nullptr;
thread_local bool g_attachedByOverlay = false;

std::string toInternalName(std::string_view name) {
    std::string internal(name);
    if (!internal.empty() && internal.front() == 'L' && internal.back() == ';') {
        internal = internal.substr(1, internal.size() - 2);
    }
    std::replace(internal.begin(), internal.end(), '.', '/');
    return internal;
}

std::string toBinaryName(std::string_view name) {
    std::string binary(name);
    if (!binary.empty() && binary.front() == 'L' && binary.back() == ';') {
        binary = binary.substr(1, binary.size() - 2);
    }
    std::replace(binary.begin(), binary.end(), '/', '.');
    return binary;
}

std::string toJvmSignature(std::string_view name) {
    return "L" + toInternalName(name) + ";";
}

void deallocateJvmti(char* pointer) {
    if (g_jvmti != nullptr && pointer != nullptr) {
        g_jvmti->Deallocate(reinterpret_cast<unsigned char*>(pointer));
    }
}

void deallocateJvmti(jclass* pointer) {
    if (g_jvmti != nullptr && pointer != nullptr) {
        g_jvmti->Deallocate(reinterpret_cast<unsigned char*>(pointer));
    }
}

} // namespace

namespace core {

LocalFrame::LocalFrame(JNIEnv* env, jint capacity)
    : env_(env) {
    ok_ = env_ != nullptr && env_->PushLocalFrame(capacity) == JNI_OK;
}

LocalFrame::~LocalFrame() {
    if (ok_ && env_ != nullptr) {
        env_->PopLocalFrame(nullptr);
    }
}

GlobalRef::GlobalRef(jobject localRef) {
    reset(localRef);
}

GlobalRef::~GlobalRef() {
    reset();
}

GlobalRef::GlobalRef(GlobalRef&& other) noexcept
    : object_(std::exchange(other.object_, nullptr)) {
}

GlobalRef& GlobalRef::operator=(GlobalRef&& other) noexcept {
    if (this != &other) {
        reset();
        object_ = std::exchange(other.object_, nullptr);
    }
    return *this;
}

void GlobalRef::reset(jobject localRef) {
    JNIEnv* currentEnv = Jvm::env();
    if (object_ != nullptr && currentEnv != nullptr) {
        currentEnv->DeleteGlobalRef(object_);
    }

    object_ = nullptr;
    if (localRef != nullptr && currentEnv != nullptr) {
        object_ = currentEnv->NewGlobalRef(localRef);
        Jvm::clearException(currentEnv, "NewGlobalRef");
    }
}

bool Jvm::initialize() {
    std::lock_guard lock(g_jvmMutex);
    if (g_vm != nullptr) {
        return true;
    }

    HMODULE jvmModule = GetModuleHandleW(L"jvm.dll");
    if (jvmModule == nullptr) {
        util::logError("jvm.dll is not loaded in the current process");
        return false;
    }

    const auto getCreatedJavaVMs = reinterpret_cast<GetCreatedJavaVMsFn>(
        GetProcAddress(jvmModule, "JNI_GetCreatedJavaVMs"));
    if (getCreatedJavaVMs == nullptr) {
        util::logError("JNI_GetCreatedJavaVMs export was not found in jvm.dll");
        return false;
    }

    JavaVM* vm = nullptr;
    jsize vmCount = 0;
    if (getCreatedJavaVMs(&vm, 1, &vmCount) != JNI_OK || vmCount == 0 || vm == nullptr) {
        util::logError("No active JVM was returned by JNI_GetCreatedJavaVMs");
        return false;
    }

    g_vm = vm;

    void* jvmti = nullptr;
    if (g_vm->GetEnv(&jvmti, JVMTI_VERSION_1_2) == JNI_OK && jvmti != nullptr) {
        g_jvmti = static_cast<jvmtiEnv*>(jvmti);
    } else if (g_vm->GetEnv(&jvmti, JVMTI_VERSION_1_1) == JNI_OK && jvmti != nullptr) {
        g_jvmti = static_cast<jvmtiEnv*>(jvmti);
    } else {
        util::logError("JVMTI is unavailable; loaded-class discovery cannot run");
        g_vm = nullptr;
        return false;
    }

    util::logInfo("Attached to existing JVM and acquired JVMTI");
    return true;
}

void Jvm::shutdown() {
    std::lock_guard lock(g_jvmMutex);
    g_minecraftClassLoader.reset();
    g_classLoaderLoadClass = nullptr;
    JavaVM* vm = g_vm;
    g_jvmti = nullptr;
    g_vm = nullptr;

    if (vm != nullptr && g_attachedByOverlay) {
        vm->DetachCurrentThread();
        g_attachedByOverlay = false;
    }
}

bool Jvm::isReady() {
    return g_vm != nullptr && g_jvmti != nullptr;
}

JNIEnv* Jvm::env() {
    JavaVM* vm = g_vm;
    if (vm == nullptr) {
        return nullptr;
    }

    JNIEnv* currentEnv = nullptr;
    const jint status = vm->GetEnv(reinterpret_cast<void**>(&currentEnv), JNI_VERSION_1_8);
    if (status == JNI_OK) {
        return currentEnv;
    }

    if (status != JNI_EDETACHED) {
        return nullptr;
    }

    if (vm->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&currentEnv), nullptr) != JNI_OK) {
        return nullptr;
    }

    g_attachedByOverlay = true;
    return currentEnv;
}

jvmtiEnv* Jvm::jvmti() {
    return g_jvmti;
}

void Jvm::detachCurrentThreadIfOwned() {
    JavaVM* vm = g_vm;
    if (vm != nullptr && g_attachedByOverlay) {
        vm->DetachCurrentThread();
        g_attachedByOverlay = false;
    }
}

bool Jvm::bootstrapMinecraftClassLoader(std::string_view minecraftBinaryName) {
    if (!isReady()) {
        return false;
    }

    JNIEnv* currentEnv = env();
    if (currentEnv == nullptr) {
        util::logError("Cannot bootstrap class loader without a JNIEnv");
        return false;
    }

    if (g_minecraftClassLoader) {
        return true;
    }

    LocalFrame frame(currentEnv, 128);
    if (!frame.ok()) {
        util::logError("Failed to allocate JNI local frame while bootstrapping class loader");
        return false;
    }

    jclass classClass = currentEnv->FindClass("java/lang/Class");
    const bool classException = clearException(currentEnv, "FindClass(java/lang/Class)");
    if (classClass == nullptr || classException) {
        return false;
    }

    jmethodID getClassLoader = currentEnv->GetMethodID(classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
    const bool getClassLoaderException = clearException(currentEnv, "Class.getClassLoader");
    if (getClassLoader == nullptr || getClassLoaderException) {
        return false;
    }

    jclass classLoaderClass = currentEnv->FindClass("java/lang/ClassLoader");
    const bool loaderClassException = clearException(currentEnv, "FindClass(java/lang/ClassLoader)");
    if (classLoaderClass == nullptr || loaderClassException) {
        return false;
    }

    g_classLoaderLoadClass = currentEnv->GetMethodID(
        classLoaderClass,
        "loadClass",
        "(Ljava/lang/String;)Ljava/lang/Class;");
    const bool loadClassException = clearException(currentEnv, "ClassLoader.loadClass");
    if (g_classLoaderLoadClass == nullptr || loadClassException) {
        return false;
    }

    const std::string wantedSignature = toJvmSignature(minecraftBinaryName);

    jint classCount = 0;
    jclass* loadedClasses = nullptr;
    const jvmtiError loadedResult = g_jvmti->GetLoadedClasses(&classCount, &loadedClasses);
    if (loadedResult != JVMTI_ERROR_NONE || loadedClasses == nullptr || classCount <= 0) {
        util::logError("JVMTI GetLoadedClasses failed while locating Minecraft class loader");
        deallocateJvmti(loadedClasses);
        return false;
    }

    jobject foundLoader = nullptr;
    for (jint i = 0; i < classCount; ++i) {
        char* signature = nullptr;
        char* generic = nullptr;
        const jvmtiError sigResult = g_jvmti->GetClassSignature(loadedClasses[i], &signature, &generic);
        deallocateJvmti(generic);

        if (sigResult == JVMTI_ERROR_NONE && signature != nullptr && wantedSignature == signature) {
            foundLoader = currentEnv->CallObjectMethod(loadedClasses[i], getClassLoader);
            if (clearException(currentEnv, "Class.getClassLoader(Minecraft)")) {
                foundLoader = nullptr;
            }
            deallocateJvmti(signature);
            break;
        }

        deallocateJvmti(signature);
    }

    for (jint i = 0; i < classCount; ++i) {
        if (loadedClasses[i] != nullptr) {
            currentEnv->DeleteLocalRef(loadedClasses[i]);
        }
    }
    deallocateJvmti(loadedClasses);

    if (foundLoader == nullptr) {
        util::logWarning("Minecraft class was not loaded yet or no Forge class loader was available");
        return false;
    }

    g_minecraftClassLoader.reset(foundLoader);
    currentEnv->DeleteLocalRef(foundLoader);

    if (!g_minecraftClassLoader) {
        util::logError("Failed to cache Minecraft class loader");
        return false;
    }

    util::logInfo("Cached Minecraft/Forge class loader from loaded Minecraft class");
    return true;
}

jclass Jvm::findClass(std::string_view binaryOrInternalName) {
    JNIEnv* currentEnv = env();
    if (currentEnv == nullptr) {
        return nullptr;
    }

    if (g_minecraftClassLoader && g_classLoaderLoadClass != nullptr) {
        const std::string binary = toBinaryName(binaryOrInternalName);
        jstring name = currentEnv->NewStringUTF(binary.c_str());
        const bool nameException = clearException(currentEnv, "NewStringUTF(class name)");
        if (name == nullptr || nameException) {
            return nullptr;
        }

        jobject loaded = currentEnv->CallObjectMethod(
            g_minecraftClassLoader.get(),
            g_classLoaderLoadClass,
            name);
        currentEnv->DeleteLocalRef(name);

        const bool loadException = clearException(currentEnv, "ClassLoader.loadClass");
        if (loaded == nullptr || loadException) {
            return nullptr;
        }

        return static_cast<jclass>(loaded);
    }

    const std::string internal = toInternalName(binaryOrInternalName);
    jclass cls = currentEnv->FindClass(internal.c_str());
    const bool findException = clearException(currentEnv, "FindClass");
    if (cls == nullptr || findException) {
        return nullptr;
    }
    return cls;
}

jmethodID Jvm::getMethodID(JNIEnv* currentEnv, jclass cls, const char* name, const char* signature) {
    if (currentEnv == nullptr || cls == nullptr || name == nullptr || signature == nullptr) {
        return nullptr;
    }

    jmethodID method = currentEnv->GetMethodID(cls, name, signature);
    if (method != nullptr && !clearException(currentEnv, name)) {
        return method;
    }
    clearException(currentEnv, name);

    jclass current = static_cast<jclass>(currentEnv->NewLocalRef(cls));
    while (current != nullptr) {
        jclass superClass = currentEnv->GetSuperclass(current);
        clearException(currentEnv, "GetSuperclass");
        currentEnv->DeleteLocalRef(current);
        current = superClass;
        if (current == nullptr) {
            break;
        }

        method = currentEnv->GetMethodID(current, name, signature);
        if (method != nullptr && !clearException(currentEnv, name)) {
            currentEnv->DeleteLocalRef(current);
            return method;
        }
        clearException(currentEnv, name);
    }

    return nullptr;
}

jfieldID Jvm::getFieldID(JNIEnv* currentEnv, jclass cls, const char* name, const char* signature) {
    if (currentEnv == nullptr || cls == nullptr || name == nullptr || signature == nullptr) {
        return nullptr;
    }

    jfieldID field = currentEnv->GetFieldID(cls, name, signature);
    if (field != nullptr && !clearException(currentEnv, name)) {
        return field;
    }
    clearException(currentEnv, name);

    jclass current = static_cast<jclass>(currentEnv->NewLocalRef(cls));
    while (current != nullptr) {
        jclass superClass = currentEnv->GetSuperclass(current);
        clearException(currentEnv, "GetSuperclass");
        currentEnv->DeleteLocalRef(current);
        current = superClass;
        if (current == nullptr) {
            break;
        }

        field = currentEnv->GetFieldID(current, name, signature);
        if (field != nullptr && !clearException(currentEnv, name)) {
            currentEnv->DeleteLocalRef(current);
            return field;
        }
        clearException(currentEnv, name);
    }

    return nullptr;
}

bool Jvm::clearException(JNIEnv* currentEnv, std::string_view context) {
    if (currentEnv == nullptr || !currentEnv->ExceptionCheck()) {
        return false;
    }

    currentEnv->ExceptionClear();

    std::string message("Cleared JNI exception at ");
    message.append(context);
    util::logWarning(message);
    return true;
}

std::string Jvm::toStdString(JNIEnv* currentEnv, jstring value) {
    if (currentEnv == nullptr || value == nullptr) {
        return {};
    }

    const char* chars = currentEnv->GetStringUTFChars(value, nullptr);
    const bool stringException = clearException(currentEnv, "GetStringUTFChars");
    if (chars == nullptr || stringException) {
        return {};
    }

    std::string result(chars);
    currentEnv->ReleaseStringUTFChars(value, chars);
    clearException(currentEnv, "ReleaseStringUTFChars");
    return result;
}

} // namespace core
