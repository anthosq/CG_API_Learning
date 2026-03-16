#pragma once

// ============================================================================
// AssetBrowserPanel.h - 资产浏览器面板
// ============================================================================

#include "Panel.h"
#include "asset/AssetTypes.h"
#include <filesystem>

namespace GLRenderer {

class AssetBrowserPanel : public Panel {
public:
    AssetBrowserPanel();

protected:
    void OnDraw(EditorContext& context) override;

private:
    void DrawDirectoryTree(const std::filesystem::path& directory, int depth = 0);
    void DrawAssetGrid();
    void DrawAssetContextMenu();

    // 获取文件图标（基于类型）
    const char* GetFileIcon(AssetType type) const;

    // 当前目录
    std::filesystem::path m_CurrentDirectory;
    std::filesystem::path m_RootDirectory;

    // 视图设置
    float m_ThumbnailSize = 80.0f;

    // 搜索
    char m_SearchBuffer[256] = "";

    // 选择
    std::filesystem::path m_SelectedPath;
};

} // namespace GLRenderer
