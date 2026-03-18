#pragma once

//
// EditorContext 包含在所有编辑器面板之间共享的状态，如：
// - 当前选中的实体
// - 当前选中的资产
// - ECS World 引用
//

#include "scene/ecs/ECS.h"
#include "asset/AssetTypes.h"
#include <vector>

namespace GLRenderer {

struct EditorContext {
    // ECS 引用

    ECS::World* World = nullptr;

    // 选择状态

    // 当前选中的实体
    ECS::Entity SelectedEntity;

    // 多选（未来扩展）
    std::vector<ECS::Entity> MultiSelection;

    // 当前选中的资产
    AssetHandle SelectedAsset;
    // 编辑器状态

    // 播放/暂停状态（未来用于运行时预览）
    bool IsPlaying = false;
    bool IsPaused = false;

    // Gizmo 设置
    enum class GizmoMode { Translate, Rotate, Scale };
    enum class GizmoSpace { Local, World };

    GizmoMode CurrentGizmoMode = GizmoMode::Translate;
    GizmoSpace CurrentGizmoSpace = GizmoSpace::Local;

    // 辅助方法
    bool HasSelection() const {
        return SelectedEntity.IsValid();
    }

    void ClearSelection() {
        SelectedEntity = ECS::Entity();
        MultiSelection.clear();
    }

    void Select(ECS::Entity entity) {
        SelectedEntity = entity;
        MultiSelection.clear();
        if (entity.IsValid()) {
            MultiSelection.push_back(entity);
        }
    }

    void AddToSelection(ECS::Entity entity) {
        if (entity.IsValid()) {
            // 检查是否已在选择中
            for (const auto& e : MultiSelection) {
                if (e.GetHandle() == entity.GetHandle()) {
                    return;
                }
            }
            MultiSelection.push_back(entity);
            if (!SelectedEntity.IsValid()) {
                SelectedEntity = entity;
            }
        }
    }
};

} // namespace GLRenderer
