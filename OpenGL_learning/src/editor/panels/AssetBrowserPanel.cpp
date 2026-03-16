#include "AssetBrowserPanel.h"
#include "editor/EditorContext.h"
#include "asset/AssetManager.h"
#include <imgui.h>

namespace GLRenderer {

AssetBrowserPanel::AssetBrowserPanel()
    : Panel("Asset Browser")
{
    m_RootDirectory = "assets";
    m_CurrentDirectory = m_RootDirectory;
}

void AssetBrowserPanel::OnDraw(EditorContext& context) {
    // 工具栏
    {
        // 返回上级目录按钮
        bool canGoBack = m_CurrentDirectory != m_RootDirectory;
        if (!canGoBack) {
            ImGui::BeginDisabled();
        }

        if (ImGui::Button("<-")) {
            m_CurrentDirectory = m_CurrentDirectory.parent_path();
            if (m_CurrentDirectory.string().length() < m_RootDirectory.string().length()) {
                m_CurrentDirectory = m_RootDirectory;
            }
        }

        if (!canGoBack) {
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        // 当前路径显示
        std::string relativePath = std::filesystem::relative(m_CurrentDirectory, m_RootDirectory).string();
        if (relativePath.empty() || relativePath == ".") {
            relativePath = "assets";
        } else {
            relativePath = "assets/" + relativePath;
        }
        ImGui::Text("%s", relativePath.c_str());

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200);

        // 搜索框
        ImGui::SetNextItemWidth(200);
        ImGui::InputTextWithHint("##Search", "Search...", m_SearchBuffer, sizeof(m_SearchBuffer));
    }

    ImGui::Separator();

    // 主内容区域（两栏布局）
    float panelWidth = ImGui::GetContentRegionAvail().x;

    // 左侧：目录树
    ImGui::BeginChild("DirectoryTree", ImVec2(200, 0), true);
    DrawDirectoryTree(m_RootDirectory);
    ImGui::EndChild();

    ImGui::SameLine();

    // 右侧：资产网格
    ImGui::BeginChild("AssetGrid", ImVec2(0, 0), true);
    DrawAssetGrid();
    ImGui::EndChild();
}

void AssetBrowserPanel::DrawDirectoryTree(const std::filesystem::path& directory, int depth) {
    if (!std::filesystem::exists(directory)) {
        return;
    }

    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (!entry.is_directory()) {
            continue;
        }

        std::string name = entry.path().filename().string();

        // 跳过隐藏目录
        if (name[0] == '.') {
            continue;
        }

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_SpanAvailWidth;

        // 检查是否有子目录
        bool hasSubdirs = false;
        for (const auto& sub : std::filesystem::directory_iterator(entry.path())) {
            if (sub.is_directory()) {
                hasSubdirs = true;
                break;
            }
        }

        if (!hasSubdirs) {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }

        // 如果是当前目录，高亮
        if (entry.path() == m_CurrentDirectory) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        bool opened = ImGui::TreeNodeEx(name.c_str(), flags);

        // 点击选择目录
        if (ImGui::IsItemClicked()) {
            m_CurrentDirectory = entry.path();
        }

        if (opened) {
            DrawDirectoryTree(entry.path(), depth + 1);
            ImGui::TreePop();
        }
    }
}

void AssetBrowserPanel::DrawAssetGrid() {
    if (!std::filesystem::exists(m_CurrentDirectory)) {
        ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "Directory not found");
        return;
    }

    // 计算每行能放多少个图标
    float padding = 16.0f;
    float cellSize = m_ThumbnailSize + padding;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columnCount = static_cast<int>(panelWidth / cellSize);
    if (columnCount < 1) columnCount = 1;

    ImGui::Columns(columnCount, nullptr, false);

    for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
        std::string filename = entry.path().filename().string();

        // 搜索过滤
        if (m_SearchBuffer[0] != '\0') {
            if (filename.find(m_SearchBuffer) == std::string::npos) {
                continue;
            }
        }

        // 跳过隐藏文件
        if (filename[0] == '.') {
            continue;
        }

        ImGui::PushID(filename.c_str());

        // 获取类型
        AssetType type = AssetType::Unknown;
        const char* icon = "?";

        if (entry.is_directory()) {
            icon = "[D]";
        } else {
            type = GetAssetTypeFromExtension(entry.path());
            icon = GetFileIcon(type);
        }

        // 绘制按钮
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

        bool isSelected = (m_SelectedPath == entry.path());
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 0.5f));
        }

        // 图标区域
        ImGui::BeginGroup();

        // 缩略图占位（实际项目中应该显示真正的缩略图）
        ImVec2 buttonSize(m_ThumbnailSize, m_ThumbnailSize);
        if (ImGui::Button(icon, buttonSize)) {
            m_SelectedPath = entry.path();
        }

        // 双击处理
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            if (entry.is_directory()) {
                m_CurrentDirectory = entry.path();
            } else {
                // 双击文件：导入资产
                if (type == AssetType::Texture) {
                    AssetManager::Get().ImportTexture(entry.path());
                } else if (type == AssetType::Model) {
                    AssetManager::Get().ImportModel(entry.path());
                }
            }
        }

        // 右键菜单
        if (ImGui::BeginPopupContextItem()) {
            m_SelectedPath = entry.path();

            if (!entry.is_directory()) {
                if (ImGui::MenuItem("Import")) {
                    if (type == AssetType::Texture) {
                        AssetManager::Get().ImportTexture(entry.path());
                    } else if (type == AssetType::Model) {
                        AssetManager::Get().ImportModel(entry.path());
                    }
                }
            }

            if (ImGui::MenuItem("Show in Explorer")) {
                // TODO: 打开文件管理器
            }

            ImGui::EndPopup();
        }

        if (isSelected) {
            ImGui::PopStyleColor();
        }

        // 文件名（截断显示）
        std::string displayName = filename;
        if (displayName.length() > 12) {
            displayName = displayName.substr(0, 10) + "...";
        }

        float textWidth = ImGui::CalcTextSize(displayName.c_str()).x;
        float offset = (m_ThumbnailSize - textWidth) * 0.5f;
        if (offset > 0) {
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
        }
        ImGui::TextWrapped("%s", displayName.c_str());

        ImGui::EndGroup();

        ImGui::PopStyleColor();
        ImGui::NextColumn();

        ImGui::PopID();
    }

    ImGui::Columns(1);
}

const char* AssetBrowserPanel::GetFileIcon(AssetType type) const {
    switch (type) {
        case AssetType::Texture:  return "[T]";
        case AssetType::Model:    return "[M]";
        case AssetType::Shader:   return "[S]";
        case AssetType::Material: return "[m]";
        case AssetType::Cubemap:  return "[C]";
        default:                  return "[?]";
    }
}

} // namespace GLRenderer
