#pragma once
#include <cstdint>
#include <Windows.h>

namespace UrhVulkanCommon
{
    inline std::uint64_t HandleKey(void* handle)
    {
        return static_cast<std::uint64_t>(reinterpret_cast<UINT_PTR>(handle));
    }
}
