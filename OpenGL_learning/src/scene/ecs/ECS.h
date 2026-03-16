#pragma once

// ECS.h - EnTT Entity Component System Integration
// 使用 EnTT 库作为底层实现，提供高性能的实体组件系统。
//
// EnTT 特点：
// - 基于 Sparse Set 的高效存储
// - Cache-friendly 的数据布局
// - 编译时类型安全
// - 零开销抽象
//
// 使用示例：
//   World world;
//   Entity entity = world.CreateEntity("Player");
//   entity.AddComponent<TransformComponent>();
//   entity.AddComponent<MeshComponent>(mesh);
//
//   // 系统遍历
//   world.View<TransformComponent, MeshComponent>().each(
//       [](auto entity, auto& transform, auto& mesh) {
//           // 渲染逻辑
//       });
#include <entt/entt.hpp>

// 核心 ECS 文件
#include "Components.h"
#include "Entity.h"
#include "World.h"
#include "Systems.h"

namespace GLRenderer {
namespace ECS {

// 类型别名，方便使用
using Registry = entt::registry;
using EntityID = entt::entity;

// 空实体常量
constexpr EntityID NullEntity = entt::null;

} // namespace ECS
} // namespace GLRenderer
