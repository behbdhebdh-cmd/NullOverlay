#pragma once

#include <array>

namespace config::mappings {

struct MemberMapping {
    const char* name;
    const char* signature;
};

inline constexpr const char* kMinecraftClass = "net.minecraft.client.Minecraft";
inline constexpr const char* kClientLevelClass = "net.minecraft.client.multiplayer.ClientLevel";
inline constexpr const char* kLocalPlayerClass = "net.minecraft.client.player.LocalPlayer";
inline constexpr const char* kGameRendererClass = "net.minecraft.client.renderer.GameRenderer";
inline constexpr const char* kCameraClass = "net.minecraft.client.Camera";
inline constexpr const char* kWindowClass = "com.mojang.blaze3d.platform.Window";
inline constexpr const char* kOptionsClass = "net.minecraft.client.Options";
inline constexpr const char* kOptionInstanceClass = "net.minecraft.client.OptionInstance";
inline constexpr const char* kDoubleClass = "java.lang.Double";
inline constexpr const char* kVec3Class = "net.minecraft.world.phys.Vec3";
inline constexpr const char* kEntityClass = "net.minecraft.world.entity.Entity";
inline constexpr const char* kLivingEntityClass = "net.minecraft.world.entity.LivingEntity";
inline constexpr const char* kPlayerClass = "net.minecraft.world.entity.player.Player";
inline constexpr const char* kMobClass = "net.minecraft.world.entity.Mob";
inline constexpr const char* kAnimalClass = "net.minecraft.world.entity.animal.Animal";
inline constexpr const char* kItemEntityClass = "net.minecraft.world.entity.item.ItemEntity";
inline constexpr const char* kComponentClass = "net.minecraft.network.chat.Component";

inline constexpr std::array<MemberMapping, 1> kMinecraftGetInstance{{
    {"getInstance", "()Lnet/minecraft/client/Minecraft;"},
}};

inline constexpr std::array<MemberMapping, 1> kMinecraftHasSingleplayerServer{{
    {"hasSingleplayerServer", "()Z"},
}};

inline constexpr std::array<MemberMapping, 1> kMinecraftGetSingleplayerServer{{
    {"getSingleplayerServer", "()Lnet/minecraft/server/integrated/IntegratedServer;"},
}};

inline constexpr std::array<MemberMapping, 1> kMinecraftSingleplayerServerField{{
    {"singleplayerServer", "Lnet/minecraft/server/integrated/IntegratedServer;"},
}};

inline constexpr std::array<MemberMapping, 1> kMinecraftGetCurrentServer{{
    {"getCurrentServer", "()Lnet/minecraft/client/multiplayer/ServerData;"},
}};

inline constexpr std::array<MemberMapping, 1> kMinecraftGetConnection{{
    {"getConnection", "()Lnet/minecraft/client/multiplayer/ClientPacketListener;"},
}};

inline constexpr std::array<MemberMapping, 1> kMinecraftGetWindow{{
    {"getWindow", "()Lcom/mojang/blaze3d/platform/Window;"},
}};

inline constexpr std::array<MemberMapping, 1> kMinecraftLevelField{{
    {"level", "Lnet/minecraft/client/multiplayer/ClientLevel;"},
}};

inline constexpr std::array<MemberMapping, 1> kMinecraftPlayerField{{
    {"player", "Lnet/minecraft/client/player/LocalPlayer;"},
}};

inline constexpr std::array<MemberMapping, 1> kMinecraftGameRendererField{{
    {"gameRenderer", "Lnet/minecraft/client/renderer/GameRenderer;"},
}};

inline constexpr std::array<MemberMapping, 1> kMinecraftOptionsField{{
    {"options", "Lnet/minecraft/client/Options;"},
}};

inline constexpr std::array<MemberMapping, 2> kWindowGuiScaledWidth{{
    {"getScreenWidth", "()I"},
    {"getGuiScaledWidth", "()I"},
}};

inline constexpr std::array<MemberMapping, 2> kWindowGuiScaledHeight{{
    {"getScreenHeight", "()I"},
    {"getGuiScaledHeight", "()I"},
}};

inline constexpr std::array<MemberMapping, 1> kGameRendererGetMainCamera{{
    {"getMainCamera", "()Lnet/minecraft/client/Camera;"},
}};

inline constexpr std::array<MemberMapping, 1> kOptionsGamma{{
    {"gamma", "()Lnet/minecraft/client/OptionInstance;"},
}};

inline constexpr std::array<MemberMapping, 1> kOptionInstanceGet{{
    {"get", "()Ljava/lang/Object;"},
}};

inline constexpr std::array<MemberMapping, 1> kOptionInstanceSet{{
    {"set", "(Ljava/lang/Object;)V"},
}};

inline constexpr std::array<MemberMapping, 1> kDoubleValueOf{{
    {"valueOf", "(D)Ljava/lang/Double;"},
}};

inline constexpr std::array<MemberMapping, 1> kDoubleDoubleValue{{
    {"doubleValue", "()D"},
}};

inline constexpr std::array<MemberMapping, 1> kCameraGetPosition{{
    {"getPosition", "()Lnet/minecraft/world/phys/Vec3;"},
}};

inline constexpr std::array<MemberMapping, 1> kCameraGetXRot{{
    {"getXRot", "()F"},
}};

inline constexpr std::array<MemberMapping, 1> kCameraGetYRot{{
    {"getYRot", "()F"},
}};

inline constexpr std::array<MemberMapping, 1> kVec3XField{{
    {"x", "D"},
}};

inline constexpr std::array<MemberMapping, 1> kVec3YField{{
    {"y", "D"},
}};

inline constexpr std::array<MemberMapping, 1> kVec3ZField{{
    {"z", "D"},
}};

inline constexpr std::array<MemberMapping, 1> kClientLevelEntitiesForRendering{{
    {"entitiesForRendering", "()Ljava/lang/Iterable;"},
}};

inline constexpr std::array<MemberMapping, 1> kEntityIsAlive{{
    {"isAlive", "()Z"},
}};

inline constexpr std::array<MemberMapping, 1> kEntityGetName{{
    {"getName", "()Lnet/minecraft/network/chat/Component;"},
}};

inline constexpr std::array<MemberMapping, 1> kEntityGetX{{
    {"getX", "()D"},
}};

inline constexpr std::array<MemberMapping, 1> kEntityGetY{{
    {"getY", "()D"},
}};

inline constexpr std::array<MemberMapping, 1> kEntityGetZ{{
    {"getZ", "()D"},
}};

inline constexpr std::array<MemberMapping, 1> kEntityGetBbWidth{{
    {"getBbWidth", "()F"},
}};

inline constexpr std::array<MemberMapping, 1> kEntityGetBbHeight{{
    {"getBbHeight", "()F"},
}};

inline constexpr std::array<MemberMapping, 1> kLivingEntityGetHealth{{
    {"getHealth", "()F"},
}};

inline constexpr std::array<MemberMapping, 1> kComponentGetString{{
    {"getString", "()Ljava/lang/String;"},
}};

} // namespace config::mappings
