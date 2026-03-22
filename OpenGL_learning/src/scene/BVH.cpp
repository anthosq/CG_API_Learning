#include "BVH.h"
#include <algorithm>
#include <limits>
#include <cassert>

namespace GLRenderer {

static constexpr int   LEAF_THRESHOLD = 4;    // 叶子节点最多包含的物体数
static constexpr int   NUM_BINS       = 32;   // Binned SAH 的 bin 数量

void BVH::Clear() {
    m_Nodes.clear();
}

void BVH::Build(std::span<const AABB> worldAABBs) {
    m_Nodes.clear();
    if (worldAABBs.empty()) return;

    // 初始化下标列表（之后排序/分割这个列表，不动原始数据）
    std::vector<int> indices(worldAABBs.size());
    for (int i = 0; i < static_cast<int>(indices.size()); i++)
        indices[i] = i;

    BuildRecursive(indices, 0, static_cast<int>(indices.size()), worldAABBs);
}

int32_t BVH::BuildRecursive(std::vector<int>& indices, int begin, int end,
                             std::span<const AABB> worldAABBs) {
    assert(begin < end);

    // 分配当前节点，记录索引
    int32_t nodeIndex = static_cast<int32_t>(m_Nodes.size());
    m_Nodes.emplace_back();
    BVHNode& node = m_Nodes.back();

    // 计算当前节点的包围盒（合并所有子 AABB）
    AABB bounds;
    for (int i = begin; i < end; i++)
        bounds.Encapsulate(worldAABBs[indices[i]]);
    node.Bounds = bounds;

    int count = end - begin;

    // 叶子节点：物体数量 <= 阈值，直接存第一个物体的下标
    // （简化：叶子只存单个物体；数量 > 1 时继续分割直到 <= LEAF_THRESHOLD）
    if (count <= LEAF_THRESHOLD) {
        if (count == 1) {
            node.ItemIndex = indices[begin];
        } else {
            // 多个物体共享叶子：创建子节点链，每个叶子存一个物体
            int mid = begin + count / 2;
            // 必须先获取索引再调用 BuildRecursive，
            // 因为 emplace_back 可能使引用 node 失效
            node.Left  = BuildRecursive(indices, begin, mid, worldAABBs);
            node.Right = BuildRecursive(indices, mid,   end, worldAABBs);
        }
        return nodeIndex;
    }

    // 内部节点：用 Binned SAH 找最优分割点
    int mid = SplitSAH(indices, begin, end, worldAABBs);

    // SAH 无法分割（所有 AABB 重合）时退化为中点分割
    if (mid == begin || mid == end)
        mid = begin + count / 2;

    node.Left  = BuildRecursive(indices, begin, mid, worldAABBs);
    node.Right = BuildRecursive(indices, mid,   end, worldAABBs);

    return nodeIndex;
}

int BVH::SplitSAH(std::vector<int>& indices, int begin, int end,
                   std::span<const AABB> worldAABBs) {
    // 计算所有物体中心点的包围盒，确定最长轴
    AABB centroidBounds;
    for (int i = begin; i < end; i++)
        centroidBounds.Encapsulate(worldAABBs[indices[i]].GetCenter());

    glm::vec3 extent = centroidBounds.GetSize();
    int axis = 0;
    if (extent.y > extent.x) axis = 1;
    if (extent.z > extent[axis]) axis = 2;

    float axisMin = centroidBounds.Min[axis];
    float axisMax = centroidBounds.Max[axis];

    // 所有中心重合，无法分割
    if (axisMax - axisMin < 1e-6f)
        return begin + (end - begin) / 2;

    // 建立 bins
    struct Bin {
        AABB  Bounds;
        int   Count = 0;
    };
    Bin bins[NUM_BINS];

    float scale = float(NUM_BINS) / (axisMax - axisMin);
    for (int i = begin; i < end; i++) {
        glm::vec3 center = worldAABBs[indices[i]].GetCenter();
        int b = static_cast<int>((center[axis] - axisMin) * scale);
        b = std::clamp(b, 0, NUM_BINS - 1);
        bins[b].Bounds.Encapsulate(worldAABBs[indices[i]]);
        bins[b].Count++;
    }

    // 对每个可能的分割平面（NUM_BINS-1 个）计算 SAH cost
    // cost = SA(left)/SA(parent) * N(left) + SA(right)/SA(parent) * N(right)
    float parentSA = m_Nodes.back().Bounds.GetSurfaceArea();  // 当前节点 SA
    parentSA = (parentSA < 1e-6f) ? 1.0f : parentSA;

    float bestCost = std::numeric_limits<float>::max();
    int   bestSplit = begin + (end - begin) / 2;

    // 前缀扫描（left side）
    AABB  leftAABB[NUM_BINS - 1];
    int   leftCount[NUM_BINS - 1];
    {
        AABB acc; int cnt = 0;
        for (int i = 0; i < NUM_BINS - 1; i++) {
            acc.Encapsulate(bins[i].Bounds);
            cnt += bins[i].Count;
            leftAABB[i]  = acc;
            leftCount[i] = cnt;
        }
    }

    // 后缀扫描（right side）+ 求最优分割
    {
        AABB acc; int cnt = 0;
        for (int i = NUM_BINS - 2; i >= 0; i--) {
            acc.Encapsulate(bins[i + 1].Bounds);
            cnt += bins[i + 1].Count;

            float saL = leftAABB[i].GetSurfaceArea();
            float saR = acc.GetSurfaceArea();
            float cost = (saL * float(leftCount[i]) + saR * float(cnt)) / parentSA;

            if (cost < bestCost) {
                bestCost = cost;
                // 将 indices 按该分割点重排，返回分割下标
                float splitPos = axisMin + float(i + 1) / float(NUM_BINS) * (axisMax - axisMin);
                auto mid_it = std::partition(indices.begin() + begin,
                                             indices.begin() + end,
                    [&](int idx) {
                        return worldAABBs[idx].GetCenter()[axis] < splitPos;
                    });
                bestSplit = static_cast<int>(mid_it - indices.begin());
            }
        }
    }

    return bestSplit;
}

void BVH::Query(const Frustum& frustum, std::vector<int>& outIndices) const {
    if (m_Nodes.empty()) return;

    // 迭代式遍历（避免递归栈开销）
    std::stack<int32_t> stack;
    stack.push(0);

    while (!stack.empty()) {
        int32_t idx = stack.top();
        stack.pop();

        const BVHNode& node = m_Nodes[idx];

        if (!frustum.TestAABB(node.Bounds))
            continue;  // 包围盒在视锥外，剪枝整个子树

        if (node.IsLeaf()) {
            if (node.ItemIndex >= 0)
                outIndices.push_back(node.ItemIndex);
        } else {
            if (node.Left  >= 0) stack.push(node.Left);
            if (node.Right >= 0) stack.push(node.Right);
        }
    }
}

} // namespace GLRenderer
