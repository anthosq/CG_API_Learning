#pragma once

#include "Panel.h"
#include "scene/ecs/ECS.h"

namespace GLRenderer {

class InspectorPanel : public Panel {
public:
    InspectorPanel() : Panel("Inspector") {}

protected:
    void OnDraw(EditorContext& context) override;

private:
    // 组件绘制方法
    void DrawTagComponent(ECS::TagComponent& tag);
    void DrawTransformComponent(ECS::TransformComponent& transform);
    void DrawMeshComponent(ECS::MeshComponent& mesh, ECS::Entity entity);
    void DrawPointLightComponent(ECS::PointLightComponent& light);
    void DrawDirectionalLightComponent(ECS::DirectionalLightComponent& light);
    void DrawRotatorComponent(ECS::RotatorComponent& rotator);
    void DrawFloatingComponent(ECS::FloatingComponent& floating);
    void DrawOscillatorComponent(ECS::OscillatorComponent& osc);
    void DrawParticleComponent(ECS::ParticleComponent& particle);
    void DrawFluidComponent(ECS::FluidComponent& fluid);
    void DrawFluidEmitterComponent(ECS::FluidEmitterComponent& emitter);

    // 材质编辑器
    void DrawMaterialEditor(Ref<MaterialAsset> matAsset);

    // 通用组件头部（带删除按钮）
    template<typename T>
    bool DrawComponentHeader(ECS::Entity entity, const char* name);

    // 添加组件按钮
    void DrawAddComponentButton(ECS::Entity entity);
};

} // namespace GLRenderer
