#include "AssimpMeshImporter.h"
#include "AssetManager.h"
#include "MaterialAsset.h"
#include "graphics/Texture.h"

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include <assimp/config.h>

#include <iostream>
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
    aiProcess_GlobalScale;              // 应用 FBX 内嵌的单位缩放
    // 注意: 不使用 aiProcess_FlipUVs。
    // 纹理加载已通过 stbi_set_flip_vertically_on_load(true) 翻转图像行序。
    // 若同时翻转 UV 和图像会造成双重翻转，导致纹理上下颠倒。

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
        aiMaterial* aiMat = scene->mMaterials[i];

        // 创建 MaterialAsset
        auto materialAsset = MaterialAsset::Create(false);

        // 获取材质名称
        aiString matName;
        if (aiMat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
            materialAsset->GetMaterial()->SetName(matName.C_Str());
        }

        // 获取基础颜色
        aiColor3D diffuseColor(1.0f, 1.0f, 1.0f);
        if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) == AI_SUCCESS) {
            materialAsset->SetAlbedoColor(glm::vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b));
        }

        // 加载 Albedo/Diffuse 贴图（颜色贴图，sRGB 编码，需 GPU 自动线性化）
        AssetHandle albedoHandle = LoadMaterialTexture(aiMat, aiTextureType_DIFFUSE, scene, true);
        if (!albedoHandle.IsValid()) {
            albedoHandle = LoadMaterialTexture(aiMat, aiTextureType_BASE_COLOR, scene, true);
        }
        if (albedoHandle.IsValid()) {
            materialAsset->SetAlbedoMap(albedoHandle);
            // 有贴图时重置颜色为白色，避免 Assimp 读出的 diffuse color 与贴图相乘产生色偏
            materialAsset->SetAlbedoColor(glm::vec3(1.0f));
        }

        // 加载 Normal 贴图
        AssetHandle normalHandle = LoadMaterialTexture(aiMat, aiTextureType_NORMALS, scene);
        if (!normalHandle.IsValid()) {
            normalHandle = LoadMaterialTexture(aiMat, aiTextureType_HEIGHT, scene);
        }
        if (normalHandle.IsValid()) {
            materialAsset->SetNormalMap(normalHandle);
        }

        // 加载 Metallic 贴图
        AssetHandle metallicHandle = LoadMaterialTexture(aiMat, aiTextureType_METALNESS, scene);
        if (metallicHandle.IsValid()) {
            materialAsset->SetMetallicMap(metallicHandle);
            // 有贴图时将标量设为 1.0，让贴图原值生效（标量为乘数）
            materialAsset->SetMetallic(1.0f);
        }

        // 加载 Roughness 贴图
        AssetHandle roughnessHandle = LoadMaterialTexture(aiMat, aiTextureType_DIFFUSE_ROUGHNESS, scene);
        if (roughnessHandle.IsValid()) {
            materialAsset->SetRoughnessMap(roughnessHandle);
            materialAsset->SetRoughness(1.0f);
        }

        // 加载 AO 贴图
        AssetHandle aoHandle = LoadMaterialTexture(aiMat, aiTextureType_AMBIENT_OCCLUSION, scene);
        if (!aoHandle.IsValid()) {
            aoHandle = LoadMaterialTexture(aiMat, aiTextureType_LIGHTMAP, scene);
        }
        if (aoHandle.IsValid()) {
            materialAsset->SetAOMap(aoHandle);
        }

        // 注册到 AssetManager
        AssetHandle matHandle = AssetManager::Get().AddMemoryOnlyAsset(materialAsset);
        materials.push_back(matHandle);

        std::cout << "[AssimpMeshImporter] Created material: " << materialAsset->GetMaterial()->GetName() << std::endl;
    }

    return materials;
}

AssetHandle AssimpMeshImporter::LoadMaterialTexture(aiMaterial* mat, aiTextureType type,
                                                    const aiScene* scene, bool srgb) {
    if (mat->GetTextureCount(type) == 0) {
        return AssetHandle(0);
    }

    aiString path;
    mat->GetTexture(type, 0, &path);

    std::string pathStr = path.C_Str();

    // 检查是否已加载（同一文件 sRGB 与非 sRGB 分开缓存）
    std::string cacheKey = pathStr + (srgb ? ":srgb" : "");
    auto it = m_LoadedTextures.find(cacheKey);
    if (it != m_LoadedTextures.end()) {
        return it->second;
    }

    // 嵌入纹理（FBX/glTF 内嵌，pcData 为压缩字节流）
    auto embedTex = scene->GetEmbeddedTexture(path.C_Str());
    if (embedTex) {
        if (embedTex->mHeight == 0) {
            // mHeight==0 表示压缩格式（PNG/JPG），mWidth 是字节数
            TextureSpec spec;
            spec.SRGB = srgb;
            auto texture = Texture2D::CreateFromMemory(
                reinterpret_cast<const unsigned char*>(embedTex->pcData),
                static_cast<int>(embedTex->mWidth), spec);
            if (texture) {
                texture->Handle = AssetHandle();
                AssetHandle handle = AssetManager::Get().AddMemoryOnlyAsset(texture);
                m_LoadedTextures[cacheKey] = handle;
                return handle;
            }
        }
        // 未压缩格式（ARGB8888 原始像素）暂不支持，回退到文件加载
        return AssetHandle(0);
    }

    // 从文件加载
    std::filesystem::path texPath = std::filesystem::path(m_Directory) / pathStr;
    if (std::filesystem::exists(texPath)) {
        TextureSpec spec;
        spec.SRGB = srgb;
        AssetHandle handle = AssetManager::Get().ImportTexture(texPath, spec);
        m_LoadedTextures[cacheKey] = handle;
        return handle;
    }

    return AssetHandle(0);
}


} // namespace GLRenderer
