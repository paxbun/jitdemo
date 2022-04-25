// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <vector>

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
    static bool      ChangeProtection(pointer p, size_type length) noexcept;

  public:
    pointer allocate(size_type programSize)
    {
        ::std::size_t const pageSize { GetPageSize() };
        ::std::uint8_t*     program { new uint8_t[pageSize + programSize] };

        size_type numPages { (programSize + pageSize - 1) / pageSize };
        size_type offset { pageSize - (reinterpret_cast<size_type>(program) % pageSize) };

        program += offset;

        if (!ChangeProtection(program, numPages * pageSize))
        {
            delete[](program - offset);
            throw ::std::bad_alloc {};
        }

        reinterpret_cast<size_type*>(program)[-1] = offset;

        return program;
    }

    void deallocate(pointer program, [[maybe_unused]] size_type program_size) noexcept
    {
        size_type offset { reinterpret_cast<size_type*>(program)[-1] };

        delete[](program - offset);
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

bool jitdemo::jit::CompiledFunctionAllocator::ChangeProtection(pointer p, size_type length) noexcept
{
    DWORD before {};
    return VirtualProtect(p, length, PAGE_EXECUTE_READWRITE, &before);
}

#else

::jitdemo::jit::CompiledFunctionAllocator::size_type
jitdemo::jit::CompiledFunctionAllocator::GetPageSize() noexcept
{
    return static_cast<::std::size_t>(::getpagesize());
}

bool jitdemo::jit::CompiledFunctionAllocator::ChangeProtection(pointer p, size_type length) noexcept
{
    return ::mprotect(p, length, PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
}

#endif
