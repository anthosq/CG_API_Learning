#include "AssimpMeshImporter.h"
#include "AssetManager.h"
#include "graphics/Texture.h"
#include "utils/DDSLoader.h"

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/config.h>

#include <stb_image.h>
#include <iostream>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace GLRenderer {

namespace Utils {
    static glm::mat4 Mat4FromAIMatrix4x4(const aiMatrix4x4& matrix) {
        glm::mat4 result;
        result[0][0] = matrix.a1; result[1][0] = matrix.a2; result[2][0] = matrix.a3; result[3][0] = matrix.a4;
        result[0][1] = matrix.b1; result[1][1] = matrix.b2; result[2][1] = matrix.b3; result[3][1] = matrix.b4;
        result[0][2] = matrix.c1; result[1][2] = matrix.c2; result[2][2] = matrix.c3; result[3][2] = matrix.c4;
        result[0][3] = matrix.d1; result[1][3] = matrix.d2; result[2][3] = matrix.d3; result[3][3] = matrix.d4;
        return result;
    }
}

static const uint32_t s_MeshImportFlags =
    aiProcess_CalcTangentSpace |
    aiProcess_Triangulate |
    aiProcess_SortByPType |
    aiProcess_RemoveComponent |         // 配合 AI_CONFIG_PP_RVC_FLAGS 移除原始法线
    aiProcess_GenSmoothNormals |        // 重新生成平滑法线
    aiProcess_GenUVCoords |
    aiProcess_OptimizeMeshes |
    aiProcess_JoinIdenticalVertices |
    aiProcess_LimitBoneWeights |
    aiProcess_ValidateDataStructure |
    aiProcess_GlobalScale |             // 应用 FBX 内嵌的单位缩放
    aiProcess_FlipUVs;

AssimpMeshImporter::AssimpMeshImporter(const std::filesystem::path& path)
    : m_Path(path) {
    m_Directory = path.parent_path().string();
}

Ref<MeshSource> AssimpMeshImporter::ImportToMeshSource() {
    auto meshSource = Ref<MeshSource>(new MeshSource());

    std::cout << "[AssimpMeshImporter] Loading mesh: " << m_Path.string() << std::endl;

    Assimp::Importer importer;

    // 移除原始法线，强制重新生成平滑法线
    importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, aiComponent_NORMALS);

    const aiScene* scene = importer.ReadFile(m_Path.string(), s_MeshImportFlags);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "[AssimpMeshImporter] Failed to load mesh: " << importer.GetErrorString() << std::endl;
        meshSource->SetFlag(AssetFlag::Invalid);
        return nullptr;
    }

    // 加载材质
    auto materials = LoadMaterials(scene);
    meshSource->SetMaterials(materials);

    // 遍历节点
    uint32_t baseVertex = 0;
    uint32_t baseIndex = 0;

    TraverseNodes(meshSource, scene->mRootNode, scene, glm::mat4(1.0f), 0);

    // 创建 GPU 缓冲
    meshSource->CreateGPUBuffers();

    std::cout << "[AssimpMeshImporter] Loaded mesh successfully. "
              << "Vertices: " << meshSource->GetVertexCount()
              << ", Indices: " << meshSource->GetIndexCount()
              << ", Submeshes: " << meshSource->GetSubmeshCount() << std::endl;

    return meshSource;
}

void AssimpMeshImporter::TraverseNodes(Ref<MeshSource> meshSource,
                                       const aiNode* node,
                                       const aiScene* scene,
                                       const glm::mat4& parentTransform,
                                       uint32_t level) {
    glm::mat4 localTransform = Utils::Mat4FromAIMatrix4x4(node->mTransformation);
    glm::mat4 transform = parentTransform * localTransform;

    // 获取当前的 baseVertex 和 baseIndex
    uint32_t baseVertex = meshSource->GetVertexCount();
    uint32_t baseIndex = meshSource->GetIndexCount();

    // 处理当前节点的所有 mesh
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(meshSource, mesh, scene, transform, baseVertex, baseIndex);
    }

    // 递归处理子节点
    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        TraverseNodes(meshSource, node->mChildren[i], scene, transform, level + 1);
    }
}

void AssimpMeshImporter::ProcessMesh(Ref<MeshSource> meshSource,
                                     const aiMesh* mesh,
                                     const aiScene* scene,
                                     const glm::mat4& transform,
                                     uint32_t& baseVertex,
                                     uint32_t& baseIndex) {
    Submesh submesh;
    submesh.BaseVertex = meshSource->GetVertexCount();
    submesh.BaseIndex = meshSource->GetIndexCount();
    submesh.MaterialIndex = mesh->mMaterialIndex;
    submesh.VertexCount = mesh->mNumVertices;
    submesh.IndexCount = mesh->mNumFaces * 3;
    submesh.MeshName = mesh->mName.C_Str();
    submesh.Transform = transform;
    submesh.LocalTransform = transform;

    // 处理顶点 (使用友元访问)
    std::vector<Vertex>& vertices = meshSource->m_Vertices;
    std::vector<Index>& indices = meshSource->m_Indices;

    vertices.reserve(vertices.size() + mesh->mNumVertices);

    AABB submeshBounds;

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;

        // Position
        vertex.Position.x = mesh->mVertices[i].x;
        vertex.Position.y = mesh->mVertices[i].y;
        vertex.Position.z = mesh->mVertices[i].z;

        submeshBounds.Encapsulate(vertex.Position);

        // Normal
        if (mesh->HasNormals()) {
            vertex.Normal.x = mesh->mNormals[i].x;
            vertex.Normal.y = mesh->mNormals[i].y;
            vertex.Normal.z = mesh->mNormals[i].z;
        } else {
            vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        // TexCoords
        if (mesh->mTextureCoords[0]) {
            vertex.TexCoord.x = mesh->mTextureCoords[0][i].x;
            vertex.TexCoord.y = mesh->mTextureCoords[0][i].y;

            // Tangent & Binormal
            if (mesh->HasTangentsAndBitangents()) {
                vertex.Tangent.x = mesh->mTangents[i].x;
                vertex.Tangent.y = mesh->mTangents[i].y;
                vertex.Tangent.z = mesh->mTangents[i].z;

                vertex.Binormal.x = mesh->mBitangents[i].x;
                vertex.Binormal.y = mesh->mBitangents[i].y;
                vertex.Binormal.z = mesh->mBitangents[i].z;
            }
        } else {
            vertex.TexCoord = glm::vec2(0.0f);
        }

        vertices.push_back(vertex);
    }

    submesh.BoundingBox = submeshBounds;

    // 处理索引
    indices.reserve(indices.size() + mesh->mNumFaces * 3);

    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }

    meshSource->AddSubmesh(submesh);
}

std::vector<AssetHandle> AssimpMeshImporter::LoadMaterials(const aiScene* scene) {
    std::vector<AssetHandle> materials;
    materials.reserve(scene->mNumMaterials);

    for (unsigned int i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* material = scene->mMaterials[i];

        // 尝试加载漫反射贴图
        AssetHandle diffuseHandle = LoadMaterialTexture(material, aiTextureType_DIFFUSE, scene);
        materials.push_back(diffuseHandle);

        // TODO: 创建 MaterialAsset 并设置其他贴图
        // AssetHandle normalHandle = LoadMaterialTexture(material, aiTextureType_NORMALS, scene);
        // AssetHandle specularHandle = LoadMaterialTexture(material, aiTextureType_SPECULAR, scene);
    }

    return materials;
}

AssetHandle AssimpMeshImporter::LoadMaterialTexture(aiMaterial* mat, aiTextureType type, const aiScene* scene) {
    if (mat->GetTextureCount(type) == 0) {
        return AssetHandle(0);
    }

    aiString path;
    mat->GetTexture(type, 0, &path);

    std::string pathStr = path.C_Str();

    // 检查是否已加载
    auto it = m_LoadedTextures.find(pathStr);
    if (it != m_LoadedTextures.end()) {
        return it->second;
    }

    // 检查是否是嵌入纹理
    auto embedTex = scene->GetEmbeddedTexture(path.C_Str());
    if (embedTex) {
        unsigned int texId = TextureFromAssimp(embedTex);
        if (texId != 0) {
            // 直接使用 OpenGL 纹理 ID 创建 Texture2D（这里需要特殊处理）
            // 目前简化处理，返回空句柄
            return AssetHandle(0);
        }
    } else {
        // 从文件加载
        std::filesystem::path texPath = std::filesystem::path(m_Directory) / pathStr;
        if (std::filesystem::exists(texPath)) {
            AssetHandle handle = AssetManager::Get().ImportTexture(texPath);
            m_LoadedTextures[pathStr] = handle;
            return handle;
        }
    }

    return AssetHandle(0);
}

unsigned int AssimpMeshImporter::TextureFromFile(const char* path) {
    std::string filename = m_Directory + '/' + std::string(path);

    std::string extension = filename.substr(filename.find_last_of('.') + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

    if (extension == "dds") {
        return DDSLoader::LoadDDS(filename);
    }

    stbi_set_flip_vertically_on_load(true);

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);

    if (data) {
        GLenum format = GL_RGBA;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cerr << "[AssimpMeshImporter] Failed to load texture: " << filename << std::endl;
        stbi_image_free(data);
        return 0;
    }

    return textureID;
}

unsigned int AssimpMeshImporter::TextureFromAssimp(const void* texturePtr) {
    const aiTexture* texture = static_cast<const aiTexture*>(texturePtr);

    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width = texture->mWidth;
    int height = texture->mHeight;

    glBindTexture(GL_TEXTURE_2D, textureID);

    if (height == 0) {
        // 压缩格式 (PNG/JPG 等嵌入数据)
        stbi_set_flip_vertically_on_load(true);
        int nrComponents;
        unsigned char* data = stbi_load_from_memory(
            reinterpret_cast<unsigned char*>(texture->pcData),
            width,
            &width, &height, &nrComponents, 0);

        if (data) {
            GLenum format = GL_RGBA;
            if (nrComponents == 1)
                format = GL_RED;
            else if (nrComponents == 3)
                format = GL_RGB;
            else if (nrComponents == 4)
                format = GL_RGBA;

            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            stbi_image_free(data);
        } else {
            std::cerr << "[AssimpMeshImporter] Failed to load embedded compressed texture" << std::endl;
            return 0;
        }
    } else {
        // 未压缩格式 (ARGB8888)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, texture->pcData);
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return textureID;
}

} // namespace GLRenderer
