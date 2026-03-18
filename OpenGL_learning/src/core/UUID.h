#pragma once

//
// 基于 64 位随机数的 UUID 实现。
// 用于资产、实体等的唯一标识。
//

#include <cstdint>
#include <functional>

namespace GLRenderer {

class UUID {
public:
    UUID();
    UUID(uint64_t uuid);
    UUID(const UUID& other) = default;
    UUID& operator=(const UUID& other) = default;

    operator uint64_t() const { return m_UUID; }

    bool operator==(const UUID& other) const { return m_UUID == other.m_UUID; }
    bool operator!=(const UUID& other) const { return m_UUID != other.m_UUID; }
    bool operator<(const UUID& other) const { return m_UUID < other.m_UUID; }

    bool IsValid() const { return m_UUID != 0; }

private:
    uint64_t m_UUID;
};

// UUID32 - 32 位唯一标识符
class UUID32 {
public:
    UUID32();
    UUID32(uint32_t uuid);
    UUID32(const UUID32& other) = default;
    UUID32& operator=(const UUID32& other) = default;

    operator uint32_t() const { return m_UUID; }

    bool operator==(const UUID32& other) const { return m_UUID == other.m_UUID; }
    bool operator!=(const UUID32& other) const { return m_UUID != other.m_UUID; }
    bool operator<(const UUID32& other) const { return m_UUID < other.m_UUID; }

    bool IsValid() const { return m_UUID != 0; }

private:
    uint32_t m_UUID;
};

}

namespace std {
    template<>
    struct hash<GLRenderer::UUID> {
        size_t operator()(const GLRenderer::UUID& uuid) const {
            return std::hash<uint64_t>()(static_cast<uint64_t>(uuid));
        }
    };

    template<>
    struct hash<GLRenderer::UUID32> {
        size_t operator()(const GLRenderer::UUID32& uuid) const {
            return std::hash<uint32_t>()(static_cast<uint32_t>(uuid));
        }
    };
}