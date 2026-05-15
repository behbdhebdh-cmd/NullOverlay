#pragma once

#include "pch.h"
#include "sdk/entity.h"
#include "util/math.h"

namespace sdk {

class ClientWorldReader {
public:
    bool initialize();
    void shutdown();
    bool isReady() const { return ready_; }

    bool collectEntities(JNIEnv* env,
                         jobject level,
                         jobject localPlayer,
                         const util::Vec3& cameraPosition,
                         const EntityFilters& filters,
                         std::vector<EntitySnapshot>& entities) const;

private:
    bool ready_{};
    core::GlobalRef clientLevelClass_;
    jmethodID entitiesForRendering_{};
    jmethodID iterableIterator_{};
    jmethodID iteratorHasNext_{};
    jmethodID iteratorNext_{};
};

ClientWorldReader& clientWorldReader();

} // namespace sdk
