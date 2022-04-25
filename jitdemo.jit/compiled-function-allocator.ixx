// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <cstdint>
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

export namespace jitdemo::jit
{

class CompiledFunctionAllocator final
{
  public:
    using pointer            = ::std::uint8_t*;
    using const_pointer      = ::std::uint8_t const*;
    using void_pointer       = void*;
    using const_void_pointer = void const*;
    using value_type         = ::std::uint8_t;
    using size_type          = ::std::size_t;
    using difference_type    = ::std::ptrdiff_t;

  private:
    static size_type GetPageSize() noexcept;
    static bool      ChangeProtection(pointer p, size_type length, bool writable = true) noexcept;

  public:
  public:
    static bool LockContent(pointer program, size_type programSize) noexcept
    {
        size_type const pageSize { GetPageSize() };
        size_type const numPages { (programSize + pageSize - 1) / pageSize };

        return ChangeProtection(program, numPages * pageSize);
    }

  public:
    pointer allocate(size_type programSize)
    {
        size_type const pageSize { GetPageSize() };
        size_type const numPages { (programSize + pageSize - 1) / pageSize };

        ::std::uint8_t* program { new uint8_t[pageSize + programSize + pageSize] };
        size_type const offset { pageSize - (reinterpret_cast<size_type>(program) % pageSize) };

        program += offset;

        if (!ChangeProtection(program, numPages * pageSize))
        {
            delete[](program - offset);
            throw ::std::bad_alloc {};
        }

        reinterpret_cast<size_type*>(program)[-1] = offset;

        return program;
    }

    void deallocate(pointer program, size_type programSize) noexcept
    {
        size_type const pageSize { GetPageSize() };
        size_type const numPages { (programSize + pageSize - 1) / pageSize };

        if (ChangeProtection(program, numPages * pageSize))
        {
            size_type offset { reinterpret_cast<size_type*>(program)[-1] };
            delete[](program - offset);
        }
    }
};

}

module : private;

#ifdef _WIN32

::jitdemo::jit::CompiledFunctionAllocator::size_type
jitdemo::jit::CompiledFunctionAllocator::GetPageSize() noexcept
{
    ::SYSTEM_INFO systemInfo {};
    ::GetSystemInfo(&systemInfo);
    return static_cast<::std::size_t>(systemInfo.dwPageSize);
}

bool jitdemo::jit::CompiledFunctionAllocator::ChangeProtection(pointer   p,
                                                               size_type length,
                                                               bool      writable) noexcept
{
    ::DWORD newProtection { writable ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE };
    ::DWORD before {};
    return VirtualProtect(p, length, newProtection, &before);
}

#else

::jitdemo::jit::CompiledFunctionAllocator::size_type
jitdemo::jit::CompiledFunctionAllocator::GetPageSize() noexcept
{
    return static_cast<::std::size_t>(::getpagesize());
}

bool jitdemo::jit::CompiledFunctionAllocator::ChangeProtection(pointer   p,
                                                               size_type length,
                                                               bool      writable) noexcept
{
    int newProtection { PROT_EXEC };
    if (writable)
        newProtection |= (PROT_READ | PROT_WRITE);
    return ::mprotect(p, length, newProtection) == 0;
}

#endif
