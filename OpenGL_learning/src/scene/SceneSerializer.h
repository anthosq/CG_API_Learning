#pragma once

#include "scene/ecs/World.h"
#include <filesystem>

namespace GLRenderer {

// SceneSerializer — 场景序列化/反序列化
//
// 文件格式：.glscene（JSON）
// 每个实体包含 UUID + Components 字典
class SceneSerializer {
public:
    // 将 world 序列化为 .glscene 文件，返回是否成功
    static bool Serialize(const ECS::World& world, const std::filesystem::path& path);

    // 从 .glscene 文件反序列化，覆盖 world（先 Clear），返回是否成功
    static bool Deserialize(ECS::World& world, const std::filesystem::path& path);

    // 内存序列化：用于 Play Mode 快照（Enter Play 存档，Exit Play 恢复）
    static std::string SerializeToString(const ECS::World& world);
    static bool        DeserializeFromString(ECS::World& world, const std::string& jsonStr);
};

} // namespace GLRenderer
