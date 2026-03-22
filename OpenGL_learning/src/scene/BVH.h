#pragma once

// BVH - Bounding Volume Hierarchy
//
// 线性存储的 SAH 二叉 BVH，用于场景视锥裁剪加速。
// 每帧在 FlushDrawList 前从 m_OpaqueDrawList 对应的 world AABB 重建。
//
// 使用示例:
//   m_SceneBVH.Build(m_WorldAABBs);
//   m_SceneBVH.Query(cameraFrustum, visibleIndices);  // visibleIndices[i] = DrawList 下标

#include "core/AABB.h"
#include "core/Frustum.h"
#include <vector>
#include <span>
#include <cstdint>
#include <stack>

namespace GLRenderer {

struct BVHNode {
    AABB    Bounds;
    int32_t Left      = -1;   // 左子节点索引；-1 = 叶子
    int32_t Right     = -1;   // 右子节点索引；-1 = 叶子
    int32_t ItemIndex = -1;   // 叶子节点对应的 DrawCommand 下标

    bool IsLeaf() const { return Left == -1; }
};

class BVH {
public:
    // 从 world AABB 列表重建 BVH。
    // worldAABBs[i] 对应外部 DrawList 的第 i 个命令。
    void Build(std::span<const AABB> worldAABBs);

    // 视锥查询：将通过 Frustum::TestAABB 的叶子节点的原始下标追加到 outIndices。
    void Query(const Frustum& frustum, std::vector<int>& outIndices) const;

    void Clear();
    bool IsBuilt() const { return !m_Nodes.empty(); }
    int  NodeCount() const { return static_cast<int>(m_Nodes.size()); }

private:
    // 递归构建，返回根节点在 m_Nodes 中的索引
    int32_t BuildRecursive(std::vector<int>& indices, int begin, int end,
                           std::span<const AABB> worldAABBs);

    // Binned SAH：沿最长轴划分，返回分割点 mid（[begin, mid) 和 [mid, end)）
    int SplitSAH(std::vector<int>& indices, int begin, int end,
                 std::span<const AABB> worldAABBs);

    std::vector<BVHNode> m_Nodes;
};

} // namespace GLRenderer
