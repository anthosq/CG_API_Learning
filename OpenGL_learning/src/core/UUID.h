#pragma once
#include <cstdint>
#include <functional>

namespace GLRenderer {
    class UUID {
    public:
        UUID();
        UUID(uint64_t uuid);
        UUID(const UUID& other);
        operator uint64_t() { return m_UUID; }
        operator const uint64_t() const { return m_UUID; }
    private:
        uint64_t m_UUID;
    };

    class UUID32 {
    public:
        UUID32();
        UUID32(uint32_t uuid);
        UUID32(const UUID32& other);
        operator uint32_t() { return m_UUID; }
        operator const uint32_t() const { return m_UUID; }
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