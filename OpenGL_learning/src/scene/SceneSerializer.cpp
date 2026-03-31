#include "SceneSerializer.h"
#include "scene/ecs/Components.h"
#include "scene/ecs/Entity.h"
#include "asset/AssetManager.h"
#include "asset/MaterialAsset.h"
#include "graphics/MeshSource.h"
#include "graphics/MeshFactory.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <sstream>

using json = nlohmann::json;

namespace GLRenderer {

// GLM helpers

static json ToJson(const glm::vec2& v) { return {v.x, v.y}; }
static json ToJson(const glm::vec3& v) { return {v.x, v.y, v.z}; }
static json ToJson(const glm::vec4& v) { return {v.x, v.y, v.z, v.w}; }
static json ToJson(const glm::quat& q) { return {q.w, q.x, q.y, q.z}; }

static glm::vec2 Vec2(const json& j) { return {j[0].get<float>(), j[1].get<float>()}; }
static glm::vec3 Vec3(const json& j) { return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()}; }
static glm::vec4 Vec4(const json& j) { return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()}; }
static glm::quat Quat(const json& j) { return glm::quat(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>()); }

// AssetHandle → string（十六进制 uint64，避免 JSON number 精度丢失）
static std::string HandleToStr(AssetHandle h)
{
    std::ostringstream oss;
    oss << std::hex << static_cast<uint64_t>(h);
    return oss.str();
}

static AssetHandle StrToHandle(const std::string& s)
{
    if (s.empty() || s == "0") return AssetHandle(0);
    return AssetHandle(static_cast<uint64_t>(std::stoull(s, nullptr, 16)));
}

// 序列化：单个实体

static json SerializeEntity(ECS::Entity entity,
                            const std::unordered_map<uint32_t, uint64_t>& handleToUUID)
{
    json jEntity;

    // IDComponent — UUID（所有实体必有）
    {
        auto& c = entity.GetComponent<ECS::IDComponent>();
        jEntity["UUID"] = static_cast<uint64_t>(c.ID);
    }

    json& jc = jEntity["Components"];

    // TagComponent
    if (entity.HasComponent<ECS::TagComponent>()) {
        auto& c = entity.GetComponent<ECS::TagComponent>();
        jc["Tag"] = { {"Name", c.Name}, {"Tag", c.Tag}, {"Active", c.Active} };
    }

    // TransformComponent
    if (entity.HasComponent<ECS::TransformComponent>()) {
        auto& c = entity.GetComponent<ECS::TransformComponent>();
        json jt;
        jt["Position"] = ToJson(c.Position);
        jt["Rotation"] = ToJson(c.Rotation);
        jt["Scale"]    = ToJson(c.Scale);

        // 父实体 UUID（若有）
        if (c.Parent != entt::null) {
            auto it = handleToUUID.find(static_cast<uint32_t>(c.Parent));
            if (it != handleToUUID.end())
                jt["ParentUUID"] = it->second;
        }
        jc["Transform"] = jt;
    }

    // HierarchyComponent（只序列化子 UUID，反序列化时从 Transform.Parent 重建即可）
    // 此处跳过 HierarchyComponent，父子关系由 TransformComponent.Parent 唯一表达

    // MeshComponent
    if (entity.HasComponent<ECS::MeshComponent>()) {
        auto& c  = entity.GetComponent<ECS::MeshComponent>();
        auto& am = AssetManager::Get();

        json jmesh;
        jmesh["Visible"] = c.Visible;

        // 基元：存类型字符串（handle 跨会话无效）
        static const std::pair<const char*, const char*> kPrims[] = {
            {"__prim:cube",     "cube"},
            {"__prim:sphere",   "sphere"},
            {"__prim:plane",    "plane"},
            {"__prim:cylinder", "cylinder"},
            {"__prim:capsule",  "capsule"},
        };
        bool isPrimitive = false;
        for (auto& [key, type] : kPrims) {
            if (am.FindAssetByKey(key) == c.MeshHandle) {
                jmesh["PrimitiveType"] = type;
                isPrimitive = true;
                break;
            }
        }

        // 文件模型：存源路径（比 handle 更可靠）
        if (!isPrimitive && c.MeshHandle.IsValid()) {
            const auto* meta = am.GetMetadata(c.MeshHandle);
            if (meta && !meta->SourcePath.empty()) {
                jmesh["MeshPath"] = meta->SourcePath.string();
            }
        }


        // 材质：参考 Hazel，把 MaterialAsset 属性内联写入场景（替代 .hmaterial 文件）
        // 纹理用文件路径存储，嵌入纹理（无路径）保存空字符串
        if (c.Materials) {
            auto getTexPath = [&](AssetHandle h) -> std::string {
                if (!h.IsValid()) return "";
                const auto* m = am.GetMetadata(h);
                return (m && !m->SourcePath.empty()) ? m->SourcePath.string() : "";
            };

            json jmats = json::array();
            for (auto& [slot, matHandle] : c.Materials->GetMaterials()) {
                auto mat = am.GetAsset<MaterialAsset>(matHandle);
                if (!mat) continue;

                json jmat;
                jmat["Slot"]        = slot;
                jmat["AlbedoColor"] = ToJson(mat->GetAlbedoColor());
                jmat["Metallic"]    = mat->GetMetallic();
                jmat["Roughness"]   = mat->GetRoughness();
                jmat["Emission"]    = mat->GetEmission();
                jmat["UseNormalMap"]= mat->IsUsingNormalMap();
                jmat["Transparent"] = mat->IsTransparent();
                jmat["AlbedoMap"]   = getTexPath(mat->GetAlbedoMap());
                jmat["NormalMap"]   = getTexPath(mat->GetNormalMap());
                jmat["MetallicMap"] = getTexPath(mat->GetMetallicMap());
                jmat["RoughnessMap"]= getTexPath(mat->GetRoughnessMap());
                jmat["AOMap"]       = getTexPath(mat->GetAOMap());
                jmat["EmissiveMap"] = getTexPath(mat->GetEmissiveMap());
                jmats.push_back(jmat);
            }
            if (!jmats.empty())
                jmesh["Materials"] = jmats;
        }

        jc["Mesh"] = jmesh;
    }

    // VisibilityComponent
    if (entity.HasComponent<ECS::VisibilityComponent>()) {
        auto& c = entity.GetComponent<ECS::VisibilityComponent>();
        jc["Visibility"] = {
            {"Visible",       c.Visible},
            {"CastShadow",    c.CastShadow},
            {"ReceiveShadow", c.ReceiveShadow},
            {"RenderLayer",   c.RenderLayer}
        };
    }

    // DirectionalLightComponent
    if (entity.HasComponent<ECS::DirectionalLightComponent>()) {
        auto& c = entity.GetComponent<ECS::DirectionalLightComponent>();
        jc["DirectionalLight"] = {
            {"Direction",   ToJson(c.Direction)},
            {"Color",       ToJson(c.Color)},
            {"Intensity",   c.Intensity},
            {"CastShadows", c.CastShadows},
            {"ShadowBias",  c.ShadowBias}
        };
    }

    // PointLightComponent
    if (entity.HasComponent<ECS::PointLightComponent>()) {
        auto& c = entity.GetComponent<ECS::PointLightComponent>();
        jc["PointLight"] = {
            {"Color",           ToJson(c.Color)},
            {"Intensity",       c.Intensity},
            {"Constant",        c.Constant},
            {"Linear",          c.Linear},
            {"Quadratic",       c.Quadratic},
            {"CastShadows",     c.CastShadows},
            {"ShadowNearPlane", c.ShadowNearPlane},
            {"ShadowFarPlane",  c.ShadowFarPlane}
        };
    }

    // SpotLightComponent
    if (entity.HasComponent<ECS::SpotLightComponent>()) {
        auto& c = entity.GetComponent<ECS::SpotLightComponent>();
        jc["SpotLight"] = {
            {"Direction",    ToJson(c.Direction)},
            {"Color",        ToJson(c.Color)},
            {"Intensity",    c.Intensity},
            {"InnerCutoff",  c.InnerCutoff},
            {"OuterCutoff",  c.OuterCutoff},
            {"Constant",     c.Constant},
            {"Linear",       c.Linear},
            {"Quadratic",    c.Quadratic},
            {"CastShadows",  c.CastShadows}
        };
    }

    // CameraComponent
    if (entity.HasComponent<ECS::CameraComponent>()) {
        auto& c = entity.GetComponent<ECS::CameraComponent>();
        jc["Camera"] = {
            {"FOV",          c.FOV},
            {"NearPlane",    c.NearPlane},
            {"FarPlane",     c.FarPlane},
            {"Primary",      c.Primary},
            {"Orthographic", c.Orthographic},
            {"OrthoSize",    c.OrthoSize}
        };
    }

    // SpriteComponent
    if (entity.HasComponent<ECS::SpriteComponent>()) {
        auto& c = entity.GetComponent<ECS::SpriteComponent>();
        jc["Sprite"] = {
            {"TextureHandle", HandleToStr(c.TextureHandle)},
            {"Color",         ToJson(c.Color)},
            {"Size",          ToJson(c.Size)},
            {"Billboard",     c.Billboard},
            {"FixedSize",     c.FixedSize},
            {"SortOrder",     c.SortOrder}
        };
    }

    // RotatorComponent
    if (entity.HasComponent<ECS::RotatorComponent>()) {
        auto& c = entity.GetComponent<ECS::RotatorComponent>();
        jc["Rotator"] = { {"Axis", ToJson(c.Axis)}, {"Speed", c.Speed} };
    }

    // FloatingComponent
    if (entity.HasComponent<ECS::FloatingComponent>()) {
        auto& c = entity.GetComponent<ECS::FloatingComponent>();
        jc["Floating"] = {
            {"Amplitude",    c.Amplitude},
            {"Frequency",    c.Frequency},
            {"Phase",        c.Phase},
            {"BasePosition", ToJson(c.BasePosition)}
        };
    }

    // OrbiterComponent
    if (entity.HasComponent<ECS::OrbiterComponent>()) {
        auto& c = entity.GetComponent<ECS::OrbiterComponent>();
        jc["Orbiter"] = {
            {"CenterPoint",   ToJson(c.CenterPoint)},
            {"Axis",          ToJson(c.Axis)},
            {"Radius",        c.Radius},
            {"Speed",         c.Speed},
            {"CurrentAngle",  c.CurrentAngle}
        };
    }

    // OscillatorComponent
    if (entity.HasComponent<ECS::OscillatorComponent>()) {
        auto& c = entity.GetComponent<ECS::OscillatorComponent>();
        jc["Oscillator"] = {
            {"Amplitude",  c.Amplitude},
            {"Frequency",  c.Frequency},
            {"Phase",      c.Phase}
        };
    }

    // ParticleComponent（跳过 RuntimeSystem / EmitAccumulator）
    if (entity.HasComponent<ECS::ParticleComponent>()) {
        auto& c = entity.GetComponent<ECS::ParticleComponent>();
        jc["Particle"] = {
            {"EmitDirection",  ToJson(c.EmitDirection)},
            {"EmitSpread",     c.EmitSpread},
            {"EmitRate",       c.EmitRate},
            {"LifetimeMin",    c.LifetimeMin},
            {"LifetimeMax",    c.LifetimeMax},
            {"SpeedMin",       c.SpeedMin},
            {"SpeedMax",       c.SpeedMax},
            {"ColorBegin",     ToJson(c.ColorBegin)},
            {"ColorEnd",       ToJson(c.ColorEnd)},
            {"Gravity",        ToJson(c.Gravity)},
            {"SizeBegin",      c.SizeBegin},
            {"SizeEnd",        c.SizeEnd},
            {"MaxParticles",   c.MaxParticles},
            {"Looping",        c.Looping},
            {"Playing",        c.Playing},
            {"TextureHandle",  HandleToStr(c.TextureHandle)}
        };
    }

    // FluidComponent（跳过 Runtime，Play 时由 PhysicsSystem 懒创建）
    if (entity.HasComponent<ECS::FluidComponent>()) {
        auto& c = entity.GetComponent<ECS::FluidComponent>();
        jc["Fluid"] = {
            {"ParticleRadius",   c.ParticleRadius},
            {"RestDensity",      c.RestDensity},
            {"Viscosity",        c.Viscosity},
            {"VorticityEps",     c.VorticityEps},
            {"MaxParticles",     c.MaxParticles},
            {"SolverIters",      c.SolverIters},
            {"Substeps",         c.Substeps},
            {"BoundaryMin",      ToJson(c.BoundaryMin)},
            {"BoundaryMax",      ToJson(c.BoundaryMax)},
            {"SceneRestitution", c.SceneRestitution}
        };
    }

    // FluidEmitterComponent
    if (entity.HasComponent<ECS::FluidEmitterComponent>()) {
        auto& c = entity.GetComponent<ECS::FluidEmitterComponent>();
        jc["FluidEmitter"] = {
            {"Direction",        ToJson(c.Direction)},
            {"Color",            ToJson(c.Color)},
            {"ConeAngleDeg",     c.ConeAngleDeg},
            {"EmitRate",         c.EmitRate},
            {"InitialSpeed",     c.InitialSpeed},
            {"ParticleLifetime", c.ParticleLifetime},
            {"Active",           c.Active}
        };
    }

    // DebugDrawComponent
    if (entity.HasComponent<ECS::DebugDrawComponent>()) {
        auto& c = entity.GetComponent<ECS::DebugDrawComponent>();
        jc["DebugDraw"] = {
            {"DrawBounds",    c.DrawBounds},
            {"DrawAxes",      c.DrawAxes},
            {"DrawWireframe", c.DrawWireframe},
            {"Color",         ToJson(c.Color)}
        };
    }

    return jEntity;
}

// 前向声明（定义在下方）
static void DeserializeComponents(ECS::Entity entity, const json& jc,
                                  std::unordered_map<uint64_t, ECS::Entity>& uuidToEntity,
                                  std::vector<std::pair<ECS::Entity, uint64_t>>& pendingParents);

static json BuildSceneJson(const ECS::World& world)
{
    std::unordered_map<uint32_t, uint64_t> handleToUUID;
    {
        auto view = world.View<ECS::IDComponent>();
        for (auto e : view) {
            handleToUUID[static_cast<uint32_t>(e)] =
                static_cast<uint64_t>(view.get<ECS::IDComponent>(e).ID);
        }
    }

    json jScene;
    jScene["Scene"]    = world.GetName();
    jScene["Version"]  = 1;
    jScene["Entities"] = json::array();

    auto view = world.View<ECS::IDComponent>();
    for (auto e : view) {
        ECS::Entity entity(e, const_cast<ECS::World*>(&world));
        jScene["Entities"].push_back(SerializeEntity(entity, handleToUUID));
    }
    return jScene;
}

static bool LoadSceneJson(ECS::World& world, const json& jScene)
{
    world.Clear();

    if (jScene.contains("Scene"))
        std::cout << "[SceneSerializer] 加载场景: " << jScene["Scene"].get<std::string>() << std::endl;

    std::unordered_map<uint64_t, ECS::Entity> uuidToEntity;
    std::vector<std::pair<ECS::Entity, uint64_t>> pendingParents;

    for (auto& jEntity : jScene["Entities"]) {
        uint64_t uuid = jEntity.value("UUID", uint64_t(0));
        std::string name = "Entity";
        if (jEntity.contains("Components") && jEntity["Components"].contains("Tag"))
            name = jEntity["Components"]["Tag"].value("Name", "Entity");

        ECS::Entity entity = world.CreateEntityWithUUID(UUID(uuid), name);
        uuidToEntity[uuid] = entity;
    }

    for (auto& jEntity : jScene["Entities"]) {
        uint64_t uuid = jEntity.value("UUID", uint64_t(0));
        auto it = uuidToEntity.find(uuid);
        if (it == uuidToEntity.end()) continue;

        if (jEntity.contains("Components"))
            DeserializeComponents(it->second, jEntity["Components"],
                                  uuidToEntity, pendingParents);
    }

    for (auto& [child, parentUUID] : pendingParents) {
        auto it = uuidToEntity.find(parentUUID);
        if (it == uuidToEntity.end()) continue;

        ECS::Entity parent = it->second;

        if (child.HasComponent<ECS::TransformComponent>())
            child.GetComponent<ECS::TransformComponent>().Parent = parent.GetHandle();

        if (!parent.HasComponent<ECS::HierarchyComponent>())
            parent.AddComponent<ECS::HierarchyComponent>();
        parent.GetComponent<ECS::HierarchyComponent>().AddChild(child.GetHandle());
    }

    world.Each<ECS::MeshComponent>([](ECS::Entity /*entity*/, ECS::MeshComponent& meshComp) {
        if (!meshComp.MeshHandle.IsValid()) return;

        AssetManager::Get().EnsureLoaded(meshComp.MeshHandle);

        if (meshComp.Materials) return;

        auto meshSource = AssetManager::Get().GetAsset<MeshSource>(meshComp.MeshHandle);
        if (!meshSource) return;

        const auto& defaultMats = meshSource->GetMaterials();
        if (defaultMats.empty()) return;

        meshComp.Materials = MaterialTable::Create(static_cast<uint32_t>(defaultMats.size()));
        for (uint32_t i = 0; i < static_cast<uint32_t>(defaultMats.size()); ++i)
            meshComp.Materials->SetMaterial(i, defaultMats[i]);
    });

    std::cout << "[SceneSerializer] 加载完成，共 " << world.GetEntityCount() << " 个实体" << std::endl;
    return true;
}

bool SceneSerializer::Serialize(const ECS::World& world, const std::filesystem::path& path)
{
    json jScene = BuildSceneJson(world);

    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "[SceneSerializer] 无法写入: " << path << std::endl;
        return false;
    }

    file << jScene.dump(2);
    std::cout << "[SceneSerializer] 已保存: " << path
              << "  (" << jScene["Entities"].size() << " 个实体)" << std::endl;
    return true;
}

std::string SceneSerializer::SerializeToString(const ECS::World& world)
{
    return BuildSceneJson(world).dump();
}

bool SceneSerializer::DeserializeFromString(ECS::World& world, const std::string& jsonStr)
{
    json jScene;
    try {
        jScene = json::parse(jsonStr);
    } catch (const json::parse_error& e) {
        std::cerr << "[SceneSerializer] JSON 解析失败: " << e.what() << std::endl;
        return false;
    }
    return LoadSceneJson(world, jScene);
}

static void DeserializeComponents(ECS::Entity entity, const json& jc,
                                  std::unordered_map<uint64_t, ECS::Entity>& uuidToEntity,
                                  std::vector<std::pair<ECS::Entity, uint64_t>>& pendingParents)
{
    auto has = [&](const char* key) { return jc.contains(key); };

    // TagComponent
    if (has("Tag")) {
        auto& jt = jc["Tag"];
        auto& c  = entity.HasComponent<ECS::TagComponent>()
                       ? entity.GetComponent<ECS::TagComponent>()
                       : entity.AddComponent<ECS::TagComponent>();
        c.Name   = jt.value("Name",   "Entity");
        c.Tag    = jt.value("Tag",    "Untagged");
        c.Active = jt.value("Active", true);
    }

    // TransformComponent
    if (has("Transform")) {
        auto& jt = jc["Transform"];
        auto& c  = entity.HasComponent<ECS::TransformComponent>()
                       ? entity.GetComponent<ECS::TransformComponent>()
                       : entity.AddComponent<ECS::TransformComponent>();
        if (jt.contains("Position")) c.Position = Vec3(jt["Position"]);
        if (jt.contains("Rotation")) c.Rotation = Quat(jt["Rotation"]);
        if (jt.contains("Scale"))    c.Scale    = Vec3(jt["Scale"]);
        c.UpdateMatrices();

        // 父实体延迟绑定
        if (jt.contains("ParentUUID")) {
            uint64_t parentUUID = jt["ParentUUID"].get<uint64_t>();
            pendingParents.emplace_back(entity, parentUUID);
        }
    }

    // MeshComponent
    if (has("Mesh")) {
        auto& jm = jc["Mesh"];
        auto& c  = entity.AddComponent<ECS::MeshComponent>();
        auto& am = AssetManager::Get();
        c.Visible = jm.value("Visible", true);

        // 优先级 1：基元类型字符串（当前会话直接调工厂）
        if (jm.contains("PrimitiveType")) {
            std::string pt = jm.value("PrimitiveType", "");
            if      (pt == "cube")     c.MeshHandle = MeshFactory::CreateCube();
            else if (pt == "sphere")   c.MeshHandle = MeshFactory::CreateSphere();
            else if (pt == "plane")    c.MeshHandle = MeshFactory::CreatePlane();
            else if (pt == "cylinder") c.MeshHandle = MeshFactory::CreateCylinder();
            else if (pt == "capsule")  c.MeshHandle = MeshFactory::CreateCapsule();
        }
        // 优先级 2：文件路径（不依赖 registry handle）
        else if (jm.contains("MeshPath")) {
            std::string mp = jm.value("MeshPath", "");
            if (!mp.empty())
                c.MeshHandle = am.ImportMeshSource(mp);
        }

        // 优先级 3：旧格式 Handle（向后兼容）
        else {
            c.MeshHandle = StrToHandle(jm.value("Handle", "0"));
        }

        // 材质：从内联数据重建 MaterialAsset（Hazel 式 .hmaterial 的内联替代）
        if (jm.contains("Materials") && !jm["Materials"].empty()) {
            auto importTex = [&](const std::string& path, bool srgb) -> AssetHandle {
                if (path.empty()) return AssetHandle(0);
                TextureSpec spec;
                spec.SRGB = srgb;
                return am.ImportTexture(path, spec);
            };

            uint32_t matCount = static_cast<uint32_t>(jm["Materials"].size());
            c.Materials = MaterialTable::Create(matCount);

            for (auto& jmat : jm["Materials"]) {
                uint32_t slot = jmat.value("Slot", 0u);
                auto mat = MaterialAsset::Create(jmat.value("Transparent", false));

                if (jmat.contains("AlbedoColor")) mat->SetAlbedoColor(Vec3(jmat["AlbedoColor"]));
                mat->SetMetallic (jmat.value("Metallic",  0.0f));
                mat->SetRoughness(jmat.value("Roughness", 0.5f));
                mat->SetEmission (jmat.value("Emission",  0.0f));
                mat->SetUseNormalMap(jmat.value("UseNormalMap", false));

                // 纹理：用路径重新导入，sRGB 约定：Albedo/Emissive 用 sRGB，其余线性
                AssetHandle albedo = importTex(jmat.value("AlbedoMap",    ""), true);
                AssetHandle normal = importTex(jmat.value("NormalMap",    ""), false);
                AssetHandle metal  = importTex(jmat.value("MetallicMap",  ""), false);
                AssetHandle rough  = importTex(jmat.value("RoughnessMap", ""), false);
                AssetHandle ao     = importTex(jmat.value("AOMap",        ""), false);
                AssetHandle emis   = importTex(jmat.value("EmissiveMap",  ""), true);

                if (albedo.IsValid()) mat->SetAlbedoMap(albedo);
                if (normal.IsValid()) mat->SetNormalMap(normal);
                if (metal.IsValid())  mat->SetMetallicMap(metal);
                if (rough.IsValid())  mat->SetRoughnessMap(rough);
                if (ao.IsValid())     mat->SetAOMap(ao);
                if (emis.IsValid())   mat->SetEmissiveMap(emis);

                AssetHandle matHandle = am.AddMemoryOnlyAsset(mat);
                c.Materials->SetMaterial(slot, matHandle);
            }
        }
    }

    // VisibilityComponent
    if (has("Visibility")) {
        auto& jv = jc["Visibility"];
        auto& c  = entity.AddComponent<ECS::VisibilityComponent>();
        c.Visible       = jv.value("Visible",       true);
        c.CastShadow    = jv.value("CastShadow",    true);
        c.ReceiveShadow = jv.value("ReceiveShadow", true);
        c.RenderLayer   = jv.value("RenderLayer",   0u);
    }

    // DirectionalLightComponent
    if (has("DirectionalLight")) {
        auto& jl = jc["DirectionalLight"];
        auto& c  = entity.AddComponent<ECS::DirectionalLightComponent>();
        if (jl.contains("Direction")) c.Direction = Vec3(jl["Direction"]);
        if (jl.contains("Color"))     c.Color     = Vec3(jl["Color"]);
        c.Intensity   = jl.value("Intensity",   1.0f);
        c.CastShadows = jl.value("CastShadows", true);
        c.ShadowBias  = jl.value("ShadowBias",  0.005f);
    }

    // PointLightComponent
    if (has("PointLight")) {
        auto& jl = jc["PointLight"];
        auto& c  = entity.AddComponent<ECS::PointLightComponent>();
        if (jl.contains("Color")) c.Color = Vec3(jl["Color"]);
        c.Intensity       = jl.value("Intensity",       1.0f);
        c.Constant        = jl.value("Constant",        1.0f);
        c.Linear          = jl.value("Linear",          0.09f);
        c.Quadratic       = jl.value("Quadratic",       0.032f);
        c.CastShadows     = jl.value("CastShadows",     true);
        c.ShadowNearPlane = jl.value("ShadowNearPlane", 0.1f);
        c.ShadowFarPlane  = jl.value("ShadowFarPlane",  25.0f);
    }

    // SpotLightComponent
    if (has("SpotLight")) {
        auto& jl = jc["SpotLight"];
        auto& c  = entity.AddComponent<ECS::SpotLightComponent>();
        if (jl.contains("Direction")) c.Direction = Vec3(jl["Direction"]);
        if (jl.contains("Color"))     c.Color     = Vec3(jl["Color"]);
        c.Intensity   = jl.value("Intensity",   1.0f);
        c.InnerCutoff = jl.value("InnerCutoff", 12.5f);
        c.OuterCutoff = jl.value("OuterCutoff", 17.5f);
        c.Constant    = jl.value("Constant",    1.0f);
        c.Linear      = jl.value("Linear",      0.09f);
        c.Quadratic   = jl.value("Quadratic",   0.032f);
        c.CastShadows = jl.value("CastShadows", false);
    }

    // CameraComponent
    if (has("Camera")) {
        auto& jcam = jc["Camera"];
        auto& c    = entity.AddComponent<ECS::CameraComponent>();
        c.FOV          = jcam.value("FOV",          45.0f);
        c.NearPlane    = jcam.value("NearPlane",    0.1f);
        c.FarPlane     = jcam.value("FarPlane",     1000.0f);
        c.Primary      = jcam.value("Primary",      false);
        c.Orthographic = jcam.value("Orthographic", false);
        c.OrthoSize    = jcam.value("OrthoSize",    10.0f);
    }

    // SpriteComponent
    if (has("Sprite")) {
        auto& js = jc["Sprite"];
        auto& c  = entity.AddComponent<ECS::SpriteComponent>();
        c.TextureHandle = StrToHandle(js.value("TextureHandle", "0"));
        if (js.contains("Color")) c.Color = Vec4(js["Color"]);
        if (js.contains("Size"))  c.Size  = Vec2(js["Size"]);
        c.Billboard = js.value("Billboard", true);
        c.FixedSize = js.value("FixedSize", false);
        c.SortOrder = js.value("SortOrder", 0);
    }

    // RotatorComponent
    if (has("Rotator")) {
        auto& jr = jc["Rotator"];
        auto& c  = entity.AddComponent<ECS::RotatorComponent>();
        if (jr.contains("Axis")) c.Axis = Vec3(jr["Axis"]);
        c.Speed = jr.value("Speed", 45.0f);
    }

    // FloatingComponent
    if (has("Floating")) {
        auto& jf = jc["Floating"];
        auto& c  = entity.AddComponent<ECS::FloatingComponent>();
        c.Amplitude = jf.value("Amplitude", 0.5f);
        c.Frequency = jf.value("Frequency", 1.0f);
        c.Phase     = jf.value("Phase",     0.0f);
        if (jf.contains("BasePosition")) c.BasePosition = Vec3(jf["BasePosition"]);
    }

    // OrbiterComponent
    if (has("Orbiter")) {
        auto& jo = jc["Orbiter"];
        auto& c  = entity.AddComponent<ECS::OrbiterComponent>();
        if (jo.contains("CenterPoint")) c.CenterPoint = Vec3(jo["CenterPoint"]);
        if (jo.contains("Axis"))        c.Axis        = Vec3(jo["Axis"]);
        c.Radius       = jo.value("Radius",       5.0f);
        c.Speed        = jo.value("Speed",        30.0f);
        c.CurrentAngle = jo.value("CurrentAngle", 0.0f);
    }

    // OscillatorComponent
    if (has("Oscillator")) {
        auto& jo = jc["Oscillator"];
        auto& c  = entity.AddComponent<ECS::OscillatorComponent>();
        c.Amplitude = jo.value("Amplitude", 1.0f);
        c.Frequency = jo.value("Frequency", 1.0f);
        c.Phase     = jo.value("Phase",     0.0f);
    }

    // ParticleComponent
    if (has("Particle")) {
        auto& jp = jc["Particle"];
        auto& c  = entity.AddComponent<ECS::ParticleComponent>();
        if (jp.contains("EmitDirection")) c.EmitDirection = Vec3(jp["EmitDirection"]);
        c.EmitSpread  = jp.value("EmitSpread",  glm::radians(30.0f));
        c.EmitRate    = jp.value("EmitRate",    50.0f);
        c.LifetimeMin = jp.value("LifetimeMin", 1.0f);
        c.LifetimeMax = jp.value("LifetimeMax", 3.0f);
        c.SpeedMin    = jp.value("SpeedMin",    2.0f);
        c.SpeedMax    = jp.value("SpeedMax",    5.0f);
        if (jp.contains("ColorBegin")) c.ColorBegin = Vec4(jp["ColorBegin"]);
        if (jp.contains("ColorEnd"))   c.ColorEnd   = Vec4(jp["ColorEnd"]);
        if (jp.contains("Gravity"))    c.Gravity    = Vec3(jp["Gravity"]);
        c.SizeBegin    = jp.value("SizeBegin",    0.15f);
        c.SizeEnd      = jp.value("SizeEnd",      0.05f);
        c.MaxParticles = jp.value("MaxParticles", 4096);
        c.Looping      = jp.value("Looping",      true);
        c.Playing      = jp.value("Playing",      true);
        c.TextureHandle = StrToHandle(jp.value("TextureHandle", "0"));
    }

    // FluidComponent
    if (has("Fluid")) {
        auto& jf = jc["Fluid"];
        auto& c  = entity.AddComponent<ECS::FluidComponent>();
        c.ParticleRadius = jf.value("ParticleRadius", 0.05f);
        c.RestDensity    = jf.value("RestDensity",    1000.0f);
        c.Viscosity      = jf.value("Viscosity",      0.01f);
        c.VorticityEps   = jf.value("VorticityEps",   0.0f);
        c.MaxParticles   = jf.value("MaxParticles",   32768);
        c.SolverIters    = jf.value("SolverIters",    4);
        c.Substeps       = jf.value("Substeps",       1);
        if (jf.contains("BoundaryMin")) c.BoundaryMin = Vec3(jf["BoundaryMin"]);
        if (jf.contains("BoundaryMax")) c.BoundaryMax = Vec3(jf["BoundaryMax"]);
        c.SceneRestitution = jf.value("SceneRestitution", 0.1f);
        // Runtime 不反序列化，PhysicsSystem 在首帧 Update 时懒创建
    }

    // FluidEmitterComponent
    if (has("FluidEmitter")) {
        auto& je = jc["FluidEmitter"];
        auto& c  = entity.AddComponent<ECS::FluidEmitterComponent>();
        if (je.contains("Direction"))    c.Direction        = Vec3(je["Direction"]);
        if (je.contains("Color"))        c.Color            = Vec4(je["Color"]);
        c.ConeAngleDeg     = je.value("ConeAngleDeg",     15.0f);
        c.EmitRate         = je.value("EmitRate",         300.0f);
        c.InitialSpeed     = je.value("InitialSpeed",     4.0f);
        c.ParticleLifetime = je.value("ParticleLifetime", 3.0f);
        c.Active           = je.value("Active",           true);
    }

    // DebugDrawComponent
    if (has("DebugDraw")) {
        auto& jd = jc["DebugDraw"];
        auto& c  = entity.AddComponent<ECS::DebugDrawComponent>();
        c.DrawBounds    = jd.value("DrawBounds",    false);
        c.DrawAxes      = jd.value("DrawAxes",      false);
        c.DrawWireframe = jd.value("DrawWireframe", false);
        if (jd.contains("Color")) c.Color = Vec3(jd["Color"]);
    }
}

bool SceneSerializer::Deserialize(ECS::World& world, const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[SceneSerializer] 无法读取: " << path << std::endl;
        return false;
    }

    json jScene;
    try {
        jScene = json::parse(file);
    } catch (const json::parse_error& e) {
        std::cerr << "[SceneSerializer] JSON 解析失败: " << e.what() << std::endl;
        return false;
    }

    return LoadSceneJson(world, jScene);
}

} // namespace GLRenderer
