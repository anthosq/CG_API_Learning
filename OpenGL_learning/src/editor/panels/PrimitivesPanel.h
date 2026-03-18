#pragma once

// PrimitivesPanel - 基础几何体创建面板
//
// 提供创建各种基础几何体的 UI：
// - Cube (立方体)
// - Sphere (球体)
// - Plane (平面)
// - Cylinder (圆柱体)
//
// 创建的实体会自动添加 TransformComponent, MeshComponent, MaterialComponent

#include "Panel.h"
#include "graphics/MeshFactory.h"

namespace GLRenderer {

class PrimitivesPanel : public Panel {
public:
    PrimitivesPanel();
    ~PrimitivesPanel() override = default;

protected:
    void OnDraw(EditorContext& context) override;

private:
    // 创建几何体实体
    void CreateCube(EditorContext& context);
    void CreateSphere(EditorContext& context);
    void CreatePlane(EditorContext& context);
    void CreateCylinder(EditorContext& context);

    // 辅助：生成唯一名称
    std::string GenerateUniqueName(EditorContext& context, const std::string& baseName);

private:
    // 几何体计数（用于命名）
    int m_CubeCount = 0;
    int m_SphereCount = 0;
    int m_PlaneCount = 0;
    int m_CylinderCount = 0;
};

} // namespace GLRenderer
