// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

// clang-format off
#ifdef _WIN32
#    include <Windows.h>
#else
#    include <sys/mman.h>
#    include <unistd.h>
#endif
// clang-format on

export module jitdemo.jit.compiled_function_allocator;

namespace jitdemo::jit
{

::std::size_t GetPageSize() noexcept;

bool ChangeProtection(::std::uint8_t* p, ::std::size_t length, bool writable = true) noexcept;

}

export namespace jitdemo::jit
{

template <typename T>
class CompiledFunctionAllocator final
{
  public:
    using pointer            = T*;
    using const_pointer      = T const*;
    using void_pointer       = void*;
    using const_void_pointer = void const*;
    using value_type         = T;
    using size_type          = ::std::size_t;
    using difference_type    = ::std::ptrdiff_t;

    template <typename U>
    struct rebind
    {
        using other = CompiledFunctionAllocator<U>;
    };

  public:
    static bool LockContent(pointer program, size_type programSize) noexcept
    {
        size_type const pageSize { GetPageSize() };
        size_type const numPages { (programSize + pageSize - 1) / pageSize };

        return ChangeProtection(program, numPages * pageSize);
    }

  public:
    constexpr CompiledFunctionAllocator() noexcept {}

    template <typename U>
    constexpr CompiledFunctionAllocator(CompiledFunctionAllocator<U> const&) noexcept
    {}

  public:
    pointer allocate(size_type programSize)
    {
        size_type const pageSize { GetPageSize() };
        size_type const numPages { (programSize + pageSize - 1) / pageSize };

        ::std::uint8_t* program {
            reinterpret_cast<::std::uint8_t*>(::std::malloc(pageSize + programSize + pageSize)),
        };
        size_type const offset { pageSize - (reinterpret_cast<size_type>(program) % pageSize) };

        program += offset;

        if (!ChangeProtection(program, numPages * pageSize))
        {
            delete[](program - offset);
            throw ::std::bad_alloc {};
        }

        reinterpret_cast<size_type*>(program)[-1] = offset;

        return reinterpret_cast<pointer>(program);
    }

    void deallocate(pointer program, size_type programSize) noexcept
    {
        size_type const pageSize { GetPageSize() };
        size_type const numPages { (programSize + pageSize - 1) / pageSize };

        if (ChangeProtection(reinterpret_cast<::std::uint8_t*>(program), numPages * pageSize))
        {
            size_type offset { reinterpret_cast<size_type*>(program)[-1] };
            delete[](program - offset);
        }
    }

  public:
    template <typename U>
    constexpr bool operator==(CompiledFunctionAllocator<U> const&) noexcept
    {
        return true;
    }

    template <typename U>
    constexpr bool operator!=(CompiledFunctionAllocator<U> const& other) noexcept
    {
        return !operator==(other);
    }
};

}

module : private;

#ifdef _WIN32

::std::size_t jitdemo::jit::GetPageSize() noexcept
{
    ::SYSTEM_INFO systemInfo {};
    ::GetSystemInfo(&systemInfo);
    return static_cast<::std::size_t>(systemInfo.dwPageSize);
}

bool jitdemo::jit::ChangeProtection(::std::uint8_t* p, ::std::size_t length, bool writable) noexcept
{
    ::DWORD newProtection { static_cast<DWORD>(writable ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE) };
    ::DWORD before {};
    return VirtualProtect(p, length, newProtection, &before);
}

#else

::std::size_t jitdemo::jit::GetPageSize() noexcept
{
    return static_cast<::std::size_t>(::getpagesize());
}

bool jitdemo::jit::ChangeProtection(::std::uint8_t* p, ::std::size_t length, bool writable) noexcept
{
    int newProtection { PROT_EXEC };
    if (writable)
        newProtection |= (PROT_READ | PROT_WRITE);
    return ::mprotect(p, length, newProtection) == 0;
}

#endif
