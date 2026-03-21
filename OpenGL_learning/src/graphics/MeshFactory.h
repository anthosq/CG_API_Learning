#pragma once

// - 创建基础几何体 (立方体、球体、平面, 胶囊等)
// - 创建碰撞体线框
// 设计原则:
// - 单例方法
// - 基元几何体在初始化时注册到 AssetManager

#include "MeshSource.h"
#include "core/Ref.h"
#include "asset/AssetTypes.h"
#include <memory>
#include <glm/glm.hpp>

namespace GLRenderer {

class MeshFactory {
public:
    static AssetHandle CreateCube();
    static AssetHandle CreateSphere();
    static AssetHandle CreatePlane();
    static AssetHandle CreateQuad();
    static AssetHandle CreateCylinder();
    static AssetHandle CreateCapsule();
};

} // namespace GLRenderer