#pragma once

#include "Panel.h"
#include "asset/AssetTypes.h"
#include "graphics/Texture.h"
#include <filesystem>
#include <unordered_map>
#include <memory>

namespace GLRenderer {

class AssetBrowserPanel : public Panel {
public:
    AssetBrowserPanel();
    ~AssetBrowserPanel() override;

protected:
    void OnDraw(EditorContext& context) override;

private:
    void DrawDirectoryTree(const std::filesystem::path& directory, int depth = 0);
    void DrawAssetGrid();
    void DrawAssetContextMenu();

    // 获取文件图标（基于类型）
    const char* GetFileIcon(AssetType type) const;

    // 获取预览纹理（自动加载）
    Texture* GetPreviewTexture(const std::filesystem::path& path);

    // 清理未使用的预览纹理
    void CleanupPreviewCache();

    // 当前目录
    std::filesystem::path m_CurrentDirectory;
    std::filesystem::path m_RootDirectory;

    // 视图设置
    float m_ThumbnailSize = 80.0f;

    // 搜索
    char m_SearchBuffer[256] = "";

    // 选择
    std::filesystem::path m_SelectedPath;

    // 预览纹理缓存
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_PreviewCache;
    std::filesystem::path m_LastDirectory;  // 用于检测目录切换

    // 延迟加载队列
    std::vector<std::filesystem::path> m_PendingLoads;
    int m_MaxLoadsPerFrame = 2;  // 每帧最多加载 2 个纹理
    int m_LoadsThisFrame = 0;
};

} // namespace GLRenderer
