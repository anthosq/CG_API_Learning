#pragma once

//
// 参考 MintEngine 实现的资产管理系统基础设施。
// 所有需要引用计数的资产类应继承自 RefCounter。
// 使用 Ref<T> 进行自动生命周期管理。
//

#include <atomic>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <unordered_set>
#include <mutex>

namespace GLRenderer {

class RefCounter {
public:
    RefCounter() = default;
    virtual ~RefCounter() = default;

    // 禁止拷贝
    RefCounter(const RefCounter&) = delete;
    RefCounter& operator=(const RefCounter&) = delete;

    void IncRefCount() const {
        ++m_RefCount;
    }

    void DecRefCount() const {
        --m_RefCount;
    }

    uint32_t GetRefCount() const {
        return m_RefCount.load();
    }

private:
    mutable std::atomic<uint32_t> m_RefCount{0};
};

namespace RefUtils {
    void AddToLiveRefs(void* instance);
    void RemoveFromLiveRefs(void* instance);
    bool IsAlive(void* instance);

#ifdef _DEBUG
    inline std::unordered_set<void*> s_LiveRefs;
    inline std::mutex s_LiveRefsMutex;

    inline void AddToLiveRefs(void* instance) {
        std::lock_guard<std::mutex> lock(s_LiveRefsMutex);
        s_LiveRefs.insert(instance);
    }

    inline void RemoveFromLiveRefs(void* instance) {
        std::lock_guard<std::mutex> lock(s_LiveRefsMutex);
        s_LiveRefs.erase(instance);
    }

    inline bool IsAlive(void* instance) {
        std::lock_guard<std::mutex> lock(s_LiveRefsMutex);
        return s_LiveRefs.find(instance) != s_LiveRefs.end();
    }
#else
    inline void AddToLiveRefs(void* instance) {}
    inline void RemoveFromLiveRefs(void* instance) {}
    inline bool IsAlive(void* instance) { return true; }
#endif
}

template<typename T>
class Ref {
public:
    Ref() : m_Instance(nullptr) {}

    Ref(std::nullptr_t) : m_Instance(nullptr) {}

    Ref(T* instance) : m_Instance(instance) {
        static_assert(std::is_base_of<RefCounter, T>::value,
                      "Ref<T>: T must inherit from RefCounter");
        IncRef();
    }

    // 拷贝构造
    Ref(const Ref<T>& other) : m_Instance(other.m_Instance) {
        IncRef();
    }

    // 类型转换拷贝构造
    template<typename T2>
    Ref(const Ref<T2>& other) {
        m_Instance = static_cast<T*>(other.Raw());
        IncRef();
    }

    // 移动构造
    Ref(Ref<T>&& other) noexcept : m_Instance(other.m_Instance) {
        other.m_Instance = nullptr;
    }

    // 类型转换移动构造
    template<typename T2>
    Ref(Ref<T2>&& other) noexcept {
        m_Instance = static_cast<T*>(other.Raw());
        other.m_Instance = nullptr;
    }

    ~Ref() {
        DecRef();
    }

    // 赋值运算符
    Ref& operator=(std::nullptr_t) {
        DecRef();
        m_Instance = nullptr;
        return *this;
    }

    Ref& operator=(const Ref<T>& other) {
        if (this != &other) {
            other.IncRef();
            DecRef();
            m_Instance = other.m_Instance;
        }
        return *this;
    }

    template<typename T2>
    Ref& operator=(const Ref<T2>& other) {
        other.IncRef();
        DecRef();
        m_Instance = static_cast<T*>(other.Raw());
        return *this;
    }

    Ref& operator=(Ref<T>&& other) noexcept {
        if (this != &other) {
            DecRef();
            m_Instance = other.m_Instance;
            other.m_Instance = nullptr;
        }
        return *this;
    }

    template<typename T2>
    Ref& operator=(Ref<T2>&& other) noexcept {
        DecRef();
        m_Instance = static_cast<T*>(other.Raw());
        other.m_Instance = nullptr;
        return *this;
    }

    // 访问运算符
    T* operator->() { return m_Instance; }
    const T* operator->() const { return m_Instance; }

    T& operator*() { return *m_Instance; }
    const T& operator*() const { return *m_Instance; }

    // 获取原始指针
    T* Raw() { return m_Instance; }
    const T* Raw() const { return m_Instance; }

    // 布尔转换
    operator bool() const { return m_Instance != nullptr; }

    // 比较运算符
    bool operator==(const Ref<T>& other) const {
        return m_Instance == other.m_Instance;
    }

    bool operator!=(const Ref<T>& other) const {
        return m_Instance != other.m_Instance;
    }

    bool operator==(std::nullptr_t) const {
        return m_Instance == nullptr;
    }

    bool operator!=(std::nullptr_t) const {
        return m_Instance != nullptr;
    }

    // 重置
    void Reset(T* instance = nullptr) {
        DecRef();
        m_Instance = instance;
        IncRef();
    }

    // 类型转换
    template<typename T2>
    Ref<T2> As() const {
        return Ref<T2>(*this);
    }

    // 静态创建方法
    template<typename... Args>
    static Ref<T> Create(Args&&... args) {
        return Ref<T>(new T(std::forward<Args>(args)...));
    }

private:
    void IncRef() const {
        if (m_Instance) {
            m_Instance->IncRefCount();
            RefUtils::AddToLiveRefs(static_cast<void*>(m_Instance));
        }
    }

    void DecRef() const {
        if (m_Instance) {
            m_Instance->DecRefCount();
            if (m_Instance->GetRefCount() == 0) {
                RefUtils::RemoveFromLiveRefs(static_cast<void*>(m_Instance));
                delete m_Instance;
                m_Instance = nullptr;
            }
        }
    }

private:
    template<typename U>
    friend class Ref;

    mutable T* m_Instance;
};

template<typename T>
class WeakRef {
public:
    WeakRef() = default;
    WeakRef(const Ref<T>& ref) : m_Instance(ref.Raw()) {}
    WeakRef(T* instance) : m_Instance(instance) {}

    T* operator->() { return m_Instance; }
    const T* operator->() const { return m_Instance; }

    T& operator*() { return *m_Instance; }
    const T& operator*() const { return *m_Instance; }

    T* Raw() { return m_Instance; }
    const T* Raw() const { return m_Instance; }

    bool IsValid() const {
        return m_Instance && RefUtils::IsAlive(static_cast<void*>(m_Instance));
    }

    operator bool() const { return IsValid(); }

    template<typename T2>
    WeakRef<T2> As() {
        return WeakRef<T2>(dynamic_cast<T2*>(m_Instance));
    }

private:
    T* m_Instance = nullptr;
};

} // namespace GLRenderer
