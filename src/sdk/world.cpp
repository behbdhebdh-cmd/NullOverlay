#include "sdk/world.h"

#include "config/mappings.h"
#include "util/logging.h"

namespace {

template <std::size_t N>
jmethodID resolveMethod(JNIEnv* env,
                        jclass cls,
                        const std::array<config::mappings::MemberMapping, N>& candidates,
                        std::string_view label) {
    for (const auto& candidate : candidates) {
        jmethodID method = core::Jvm::getMethodID(env, cls, candidate.name, candidate.signature);
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

} // namespace

namespace sdk {

ClientWorldReader& clientWorldReader() {
    static ClientWorldReader reader;
    return reader;
}

bool ClientWorldReader::initialize() {
    if (ready_) {
        return true;
    }

    JNIEnv* env = core::Jvm::env();
    if (env == nullptr) {
        util::logError("ClientWorldReader cannot initialize without JNIEnv");
        return false;
    }

    core::LocalFrame frame(env, 32);
    if (!frame.ok()) {
        return false;
    }

    jclass localLevelClass = core::Jvm::findClass(config::mappings::kClientLevelClass);
    if (localLevelClass == nullptr) {
        util::logError("ClientLevel class was not found");
        return false;
    }

    clientLevelClass_.reset(localLevelClass);
    env->DeleteLocalRef(localLevelClass);

    if (!clientLevelClass_) {
        return false;
    }

    entitiesForRendering_ = resolveMethod(
        env,
        static_cast<jclass>(clientLevelClass_.get()),
        config::mappings::kClientLevelEntitiesForRendering,
        "ClientLevel.entitiesForRendering");

    jclass iterableClass = env->FindClass("java/lang/Iterable");
    const bool iterableClassException = core::Jvm::clearException(env, "FindClass(java/lang/Iterable)");
    if (iterableClass == nullptr || iterableClassException) {
        shutdown();
        return false;
    }

    iterableIterator_ = env->GetMethodID(iterableClass, "iterator", "()Ljava/util/Iterator;");
    const bool iterableIteratorException = core::Jvm::clearException(env, "Iterable.iterator");
    if (iterableIterator_ == nullptr || iterableIteratorException) {
        shutdown();
        return false;
    }

    jclass iteratorClass = env->FindClass("java/util/Iterator");
    const bool iteratorClassException = core::Jvm::clearException(env, "FindClass(java/util/Iterator)");
    if (iteratorClass == nullptr || iteratorClassException) {
        shutdown();
        return false;
    }

    iteratorHasNext_ = env->GetMethodID(iteratorClass, "hasNext", "()Z");
    iteratorNext_ = env->GetMethodID(iteratorClass, "next", "()Ljava/lang/Object;");
    const bool iteratorMethodException = core::Jvm::clearException(env, "Iterator methods");
    if (iteratorHasNext_ == nullptr ||
        iteratorNext_ == nullptr ||
        iteratorMethodException) {
        shutdown();
        return false;
    }

    ready_ = entitiesForRendering_ != nullptr;
    if (ready_) {
        util::logInfo("ClientWorldReader initialized");
    }
    return ready_;
}

void ClientWorldReader::shutdown() {
    ready_ = false;
    clientLevelClass_.reset();
    entitiesForRendering_ = nullptr;
    iterableIterator_ = nullptr;
    iteratorHasNext_ = nullptr;
    iteratorNext_ = nullptr;
}

bool ClientWorldReader::collectEntities(JNIEnv* env,
                                        jobject level,
                                        jobject localPlayer,
                                        const util::Vec3& cameraPosition,
                                        const EntityFilters& filters,
                                        std::vector<EntitySnapshot>& entities) const {
    entities.clear();

    if (!ready_ || !entityReader().isReady() || env == nullptr || level == nullptr) {
        return false;
    }

    core::LocalFrame frame(env, 128);
    if (!frame.ok()) {
        return false;
    }

    jobject iterable = env->CallObjectMethod(level, entitiesForRendering_);
    const bool iterableException = core::Jvm::clearException(env, "ClientLevel.entitiesForRendering");
    if (iterable == nullptr || iterableException) {
        return false;
    }

    jobject iterator = env->CallObjectMethod(iterable, iterableIterator_);
    const bool iteratorException = core::Jvm::clearException(env, "Iterable.iterator");
    if (iterator == nullptr || iteratorException) {
        return false;
    }

    constexpr int kMaxEntitiesPerFrame = 4096;
    for (int visited = 0; visited < kMaxEntitiesPerFrame; ++visited) {
        const jboolean hasNext = env->CallBooleanMethod(iterator, iteratorHasNext_);
        if (core::Jvm::clearException(env, "Iterator.hasNext") || hasNext != JNI_TRUE) {
            break;
        }

        jobject entity = env->CallObjectMethod(iterator, iteratorNext_);
        if (core::Jvm::clearException(env, "Iterator.next") || entity == nullptr) {
            continue;
        }

        EntitySnapshot snapshot{};
        if (entityReader().read(env, entity, localPlayer, cameraPosition, filters, snapshot)) {
            entities.emplace_back(std::move(snapshot));
        }

        env->DeleteLocalRef(entity);
    }

    return true;
}

} // namespace sdk
