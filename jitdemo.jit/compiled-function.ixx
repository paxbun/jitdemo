// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// clang-format off
#ifdef _WIN32
#    include <Windows.h>
#else
#    include <sys/mman.h>
#    include <unistd.h>
#endif
// clang-format on

export module jitdemo.jit.compiled_function;

import jitdemo.expr.function;

using ::jitdemo::expr::Function;

export namespace jitdemo::jit
{

class CompiledFunction final : public Function
{
  public:
    using FunctionType  = double (*)(double*);

  private:
    static ::std::size_t const pageSize_;

  private:
    static ::std::size_t GetPageSize() noexcept;

    static bool ChangeProtection(::std::uint8_t* p,
                                 ::std::size_t   length,
                                 bool            writable = true) noexcept;

  private:
    ::std::vector<::std::shared_ptr<Function>> referencedFunctions_;
    ::std::size_t                              programSize_;
    ::std::size_t                              programOffset_;
    ::std::uint8_t*                            program_;

  public:
    CompiledFunction(::std::vector<::std::u8string> const&        params,
                     ::std::vector<::std::uint8_t> const&         func,
                     ::std::vector<::std::shared_ptr<Function>>&& referencedFunctions) :
        Function { ::std::vector<::std::u8string>(params) },
        referencedFunctions_ { ::std::move(referencedFunctions) },
        programSize_ { func.size() },
        programOffset_ {},
        program_ {}
    {
        Allocate();
        ::std::memcpy(program_, func.data(), func.size());
        LockContent();
    }

    ~CompiledFunction()
    {
        Deallocate();
    }

  private:
    void Allocate();

    void LockContent();

    void Deallocate() noexcept;

  public:
    virtual double Evaluate(::std::span<double> params) noexcept override
    {
        return reinterpret_cast<FunctionType>(program_)(params.data());
    }
};

}

module : private;

::std::size_t const jitdemo::jit::CompiledFunction::pageSize_ { GetPageSize() };

#ifdef _WIN32

::std::size_t jitdemo::jit::CompiledFunction::GetPageSize() noexcept
{
    ::SYSTEM_INFO systemInfo {};
    ::GetSystemInfo(&systemInfo);
    return static_cast<::std::size_t>(systemInfo.dwPageSize);
}

bool jitdemo::jit::CompiledFunction::ChangeProtection(::std::uint8_t* p,
                                                      ::std::size_t   length,
                                                      bool            writable) noexcept
{
    ::DWORD newProtection { static_cast<DWORD>(writable ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE) };
    ::DWORD before {};
    return VirtualProtect(p, length, newProtection, &before);
}

#else

::std::size_t jitdemo::jit::CompiledFunction::GetPageSize() noexcept
{
    return static_cast<::std::size_t>(::getpagesize());
}

bool jitdemo::jit::CompiledFunction::ChangeProtection(::std::uint8_t* p,
                                                      ::std::size_t   length,
                                                      bool            writable) noexcept
{
    int newProtection { PROT_EXEC };
    if (writable)
        newProtection |= (PROT_READ | PROT_WRITE);
    return ::mprotect(p, length, newProtection) == 0;
}

#endif

void jitdemo::jit::CompiledFunction::Allocate()
{
    ::std::size_t const numPages { (programSize_ + pageSize_ - 1) / pageSize_ };

    ::std::uint8_t* program {
        static_cast<::std::uint8_t*>(::std::malloc(pageSize_ + programSize_)),
    };

    if (!program)
        throw ::std::bad_alloc {};

    ::std::size_t const offset {
        pageSize_ - (reinterpret_cast<::std::size_t>(program) % pageSize_),
    };

    program += offset;

    if (!ChangeProtection(program, numPages * pageSize_))
    {
        ::std::free(program - offset);
        throw ::std::bad_alloc {};
    }

    program_       = program;
    programOffset_ = offset;
}

void jitdemo::jit::CompiledFunction::LockContent()
{
    ::std::size_t const numPages { (programSize_ + pageSize_ - 1) / pageSize_ };

    if (!ChangeProtection(program_, numPages * pageSize_, false))
    {
        throw ::std::runtime_error { "failed to change protetion attributes" };
    }
}

void jitdemo::jit::CompiledFunction::Deallocate() noexcept
{

    ::std::size_t const numPages { (programSize_ + pageSize_ - 1) / pageSize_ };

    if (ChangeProtection(program_, numPages * pageSize_))
    {
        ::std::free(program_ - programOffset_);

        programSize_   = 0;
        program_       = nullptr;
        programOffset_ = 0;
    }
}
