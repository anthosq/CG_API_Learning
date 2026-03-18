#include "UUID.h"
#include <random>

namespace GLRenderer {
    static std::random_device rd;
    static std::mt19937_64 eng(rd());
    static std::uniform_int_distribution<uint64_t> distr;

    static std::mt19937 eng32(rd());
    static std::uniform_int_distribution<uint32_t> distr32;

    UUID::UUID() : m_UUID(distr(eng)) {}

    UUID::UUID(uint64_t uuid) : m_UUID(uuid) {}

    // Copy constructor uses = default in header

    UUID32::UUID32() : m_UUID(distr32(eng32)) {}

    UUID32::UUID32(uint32_t uuid) : m_UUID(uuid) {}

    // Copy constructor uses = default in header
}