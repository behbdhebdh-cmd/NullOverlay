#pragma once

#include "pch.h"
#include "core/jvm_wrapper.h"
#include "util/math.h"

namespace sdk {

enum class EntityCategory {
    Unknown,
    Player,
    Mob,
    Animal,
    Item,
};

struct EntityFilters {
    bool players{true};
    bool mobs{true};
    bool animals{true};
    bool items{true};
    float maxDistance{96.0f};
};

struct EntitySnapshot {
    EntityCategory category{EntityCategory::Unknown};
    std::string name;
    util::Vec3 position{};
    float health{-1.0f};
    float distance{0.0f};
    float width{0.6f};
    float height{1.8f};
};

class EntityReader {
public:
    bool initialize();
    void shutdown();
    bool isReady() const { return ready_; }

    bool read(JNIEnv* env,
              jobject entity,
              jobject localPlayer,
              const util::Vec3& cameraPosition,
              const EntityFilters& filters,
              EntitySnapshot& snapshot) const;

private:
    EntityCategory categorize(JNIEnv* env, jobject entity) const;
    bool categoryAllowed(EntityCategory category, const EntityFilters& filters) const;

    bool ready_{};
    core::GlobalRef entityClass_;
    core::GlobalRef livingEntityClass_;
    core::GlobalRef playerClass_;
    core::GlobalRef mobClass_;
    core::GlobalRef animalClass_;
    core::GlobalRef itemEntityClass_;
    core::GlobalRef componentClass_;

    jmethodID entityIsAlive_{};
    jmethodID entityGetName_{};
    jmethodID entityGetX_{};
    jmethodID entityGetY_{};
    jmethodID entityGetZ_{};
    jmethodID entityGetBbWidth_{};
    jmethodID entityGetBbHeight_{};
    jmethodID livingEntityGetHealth_{};
    jmethodID componentGetString_{};
};

EntityReader& entityReader();
const char* categoryName(EntityCategory category);

} // namespace sdk
