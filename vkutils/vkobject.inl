#include <utility>
#include <cassert>

namespace vkutils {
    // UniqueHandle<> implementation
    template<typename tHandle, typename tParent, DestroyFn<tParent, tHandle>& tDestroyFn>
    inline UniqueHandle<tHandle, tParent, tDestroyFn>::UniqueHandle(tParent parent, tHandle handle) noexcept
        : handle(handle), parent(parent) {
    }

    template<typename tHandle, typename tParent, DestroyFn<tParent, tHandle>& tDestroyFn>
    inline UniqueHandle<tHandle, tParent, tDestroyFn>::~UniqueHandle() {
        if (VK_NULL_HANDLE != handle) {
            assert(VK_NULL_HANDLE != parent);
            tDestroyFn(parent, handle, nullptr);
        }
    }

    template<typename tHandle, typename tParent, DestroyFn<tParent, tHandle>& tDestroyFn>
    inline UniqueHandle<tHandle, tParent, tDestroyFn>::UniqueHandle(UniqueHandle&& other) noexcept
        : handle(std::exchange(other.handle, VK_NULL_HANDLE)),
          parent(std::exchange(other.parent, VK_NULL_HANDLE)) {
    }

    template<typename tHandle, typename tParent, DestroyFn<tParent, tHandle>& tDestroyFn>
    inline UniqueHandle<tHandle, tParent, tDestroyFn>& UniqueHandle<tHandle, tParent, tDestroyFn>::operator=(
        UniqueHandle&& other) noexcept {
        std::swap(handle, other.handle);
        std::swap(parent, other.parent);
        return *this;
    }
}
