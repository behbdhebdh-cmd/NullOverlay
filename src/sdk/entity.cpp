#include "sdk/entity.h"

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

core::GlobalRef resolveGlobalClass(JNIEnv* env, std::string_view name, bool required) {
    jclass localClass = core::Jvm::findClass(name);
    if (localClass == nullptr) {
        if (required) {
            std::string message("Failed to resolve class ");
            message.append(name);
            util::logError(message);
        }
        return {};
    }

    core::GlobalRef global(localClass);
    env->DeleteLocalRef(localClass);
    return global;
}

bool boolCall(JNIEnv* env, jobject object, jmethodID method) {
    if (env == nullptr || object == nullptr || method == nullptr) {
        return false;
    }
    const jboolean result = env->CallBooleanMethod(object, method);
    if (core::Jvm::clearException(env, "boolean entity call")) {
        return false;
    }
    return result == JNI_TRUE;
}

double doubleCall(JNIEnv* env, jobject object, jmethodID method) {
    if (env == nullptr || object == nullptr || method == nullptr) {
        return 0.0;
    }
    const jdouble result = env->CallDoubleMethod(object, method);
    if (core::Jvm::clearException(env, "double entity call")) {
        return 0.0;
    }
    return static_cast<double>(result);
}

float floatCall(JNIEnv* env, jobject object, jmethodID method, float fallback = 0.0f) {
    if (env == nullptr || object == nullptr || method == nullptr) {
        return fallback;
    }
    const jfloat result = env->CallFloatMethod(object, method);
    if (core::Jvm::clearException(env, "float entity call")) {
        return fallback;
    }
    return static_cast<float>(result);
}

} // namespace

namespace sdk {

EntityReader& entityReader() {
    static EntityReader reader;
    return reader;
}

const char* categoryName(EntityCategory category) {
    switch (category) {
    case EntityCategory::Player:
        return "Player";
    case EntityCategory::Mob:
        return "Mob";
    case EntityCategory::Animal:
        return "Animal";
    case EntityCategory::Item:
        return "Item";
    case EntityCategory::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

bool EntityReader::initialize() {
    if (ready_) {
        return true;
    }

    JNIEnv* env = core::Jvm::env();
    if (env == nullptr) {
        util::logError("EntityReader cannot initialize without JNIEnv");
        return false;
    }

    core::LocalFrame frame(env, 64);
    if (!frame.ok()) {
        return false;
    }

    using namespace config::mappings;
    entityClass_ = resolveGlobalClass(env, kEntityClass, true);
    livingEntityClass_ = resolveGlobalClass(env, kLivingEntityClass, true);
    playerClass_ = resolveGlobalClass(env, kPlayerClass, false);
    mobClass_ = resolveGlobalClass(env, kMobClass, false);
    animalClass_ = resolveGlobalClass(env, kAnimalClass, false);
    itemEntityClass_ = resolveGlobalClass(env, kItemEntityClass, false);
    componentClass_ = resolveGlobalClass(env, kComponentClass, true);

    if (!entityClass_ || !livingEntityClass_ || !componentClass_) {
        shutdown();
        return false;
    }

    const auto entityClass = static_cast<jclass>(entityClass_.get());
    const auto livingClass = static_cast<jclass>(livingEntityClass_.get());
    const auto componentClass = static_cast<jclass>(componentClass_.get());

    entityIsAlive_ = resolveMethod(env, entityClass, kEntityIsAlive, "Entity.isAlive");
    entityGetName_ = resolveMethod(env, entityClass, kEntityGetName, "Entity.getName");
    entityGetX_ = resolveMethod(env, entityClass, kEntityGetX, "Entity.getX");
    entityGetY_ = resolveMethod(env, entityClass, kEntityGetY, "Entity.getY");
    entityGetZ_ = resolveMethod(env, entityClass, kEntityGetZ, "Entity.getZ");
    entityGetBbWidth_ = resolveMethod(env, entityClass, kEntityGetBbWidth, "Entity.getBbWidth");
    entityGetBbHeight_ = resolveMethod(env, entityClass, kEntityGetBbHeight, "Entity.getBbHeight");
    livingEntityGetHealth_ = resolveMethod(env, livingClass, kLivingEntityGetHealth, "LivingEntity.getHealth");
    componentGetString_ = resolveMethod(env, componentClass, kComponentGetString, "Component.getString");

    ready_ = entityIsAlive_ != nullptr &&
        entityGetName_ != nullptr &&
        entityGetX_ != nullptr &&
        entityGetY_ != nullptr &&
        entityGetZ_ != nullptr &&
        entityGetBbWidth_ != nullptr &&
        entityGetBbHeight_ != nullptr &&
        componentGetString_ != nullptr;

    if (!ready_) {
        shutdown();
        return false;
    }

    util::logInfo("EntityReader initialized");
    return true;
}

void EntityReader::shutdown() {
    ready_ = false;
    entityClass_.reset();
    livingEntityClass_.reset();
    playerClass_.reset();
    mobClass_.reset();
    animalClass_.reset();
    itemEntityClass_.reset();
    componentClass_.reset();

    entityIsAlive_ = nullptr;
    entityGetName_ = nullptr;
    entityGetX_ = nullptr;
    entityGetY_ = nullptr;
    entityGetZ_ = nullptr;
    entityGetBbWidth_ = nullptr;
    entityGetBbHeight_ = nullptr;
    livingEntityGetHealth_ = nullptr;
    componentGetString_ = nullptr;
}

bool EntityReader::read(JNIEnv* env,
                        jobject entity,
                        jobject localPlayer,
                        const util::Vec3& cameraPosition,
                        const EntityFilters& filters,
                        EntitySnapshot& snapshot) const {
    if (!ready_ || env == nullptr || entity == nullptr) {
        return false;
    }

    if (localPlayer != nullptr && env->IsSameObject(entity, localPlayer) == JNI_TRUE) {
        return false;
    }

    if (!boolCall(env, entity, entityIsAlive_)) {
        return false;
    }

    EntitySnapshot next{};
    next.category = categorize(env, entity);
    if (!categoryAllowed(next.category, filters)) {
        return false;
    }

    next.position = {
        doubleCall(env, entity, entityGetX_),
        doubleCall(env, entity, entityGetY_),
        doubleCall(env, entity, entityGetZ_),
    };

    next.distance = static_cast<float>(util::distance(next.position, cameraPosition));
    if (next.distance > filters.maxDistance) {
        return false;
    }

    next.width = std::max(0.1f, floatCall(env, entity, entityGetBbWidth_, 0.6f));
    next.height = std::max(0.1f, floatCall(env, entity, entityGetBbHeight_, 1.8f));

    if (livingEntityClass_ && env->IsInstanceOf(entity, static_cast<jclass>(livingEntityClass_.get())) == JNI_TRUE) {
        next.health = floatCall(env, entity, livingEntityGetHealth_, -1.0f);
    }

    jobject component = env->CallObjectMethod(entity, entityGetName_);
    if (!core::Jvm::clearException(env, "Entity.getName") && component != nullptr) {
        auto nameString = static_cast<jstring>(env->CallObjectMethod(component, componentGetString_));
        if (!core::Jvm::clearException(env, "Component.getString") && nameString != nullptr) {
            next.name = core::Jvm::toStdString(env, nameString);
            env->DeleteLocalRef(nameString);
        }
        env->DeleteLocalRef(component);
    }

    if (next.name.empty()) {
        next.name = categoryName(next.category);
    }

    snapshot = std::move(next);
    return true;
}

EntityCategory EntityReader::categorize(JNIEnv* env, jobject entity) const {
    if (env == nullptr || entity == nullptr) {
        return EntityCategory::Unknown;
    }

    if (playerClass_ && env->IsInstanceOf(entity, static_cast<jclass>(playerClass_.get())) == JNI_TRUE) {
        return EntityCategory::Player;
    }
    if (itemEntityClass_ && env->IsInstanceOf(entity, static_cast<jclass>(itemEntityClass_.get())) == JNI_TRUE) {
        return EntityCategory::Item;
    }
    if (animalClass_ && env->IsInstanceOf(entity, static_cast<jclass>(animalClass_.get())) == JNI_TRUE) {
        return EntityCategory::Animal;
    }
    if (mobClass_ && env->IsInstanceOf(entity, static_cast<jclass>(mobClass_.get())) == JNI_TRUE) {
        return EntityCategory::Mob;
    }

    return EntityCategory::Unknown;
}

bool EntityReader::categoryAllowed(EntityCategory category, const EntityFilters& filters) const {
    switch (category) {
    case EntityCategory::Player:
        return filters.players;
    case EntityCategory::Mob:
        return filters.mobs;
    case EntityCategory::Animal:
        return filters.animals;
    case EntityCategory::Item:
        return filters.items;
    case EntityCategory::Unknown:
        return filters.mobs;
    }
    return false;
}

} // namespace sdk
