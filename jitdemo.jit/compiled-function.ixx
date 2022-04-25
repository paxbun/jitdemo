// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

export module jitdemo.jit.compiled_function;

import jitdemo.expr.function;
import jitdemo.jit.compiled_function_allocator;

using ::jitdemo::expr::Function;

export namespace jitdemo::jit
{

class CompiledFunction final : public Function
{
  public:
    using AllocatorType = CompiledFunctionAllocator<::std::uint8_t>;
    using FunctionType  = double (*)(double*);

  private:
    ::std::vector<::std::uint8_t, AllocatorType> const func_;
    ::std::vector<::std::shared_ptr<Function>>         referencedFunctions_;

  public:
    CompiledFunction(::std::vector<::std::u8string> const&        params,
                     ::std::vector<::std::uint8_t> const&         func,
                     ::std::vector<::std::shared_ptr<Function>>&& referencedFunctions) :
        Function { ::std::vector<::std::u8string>(params) },
        func_(func.data(), func.data() + func.size()),
        referencedFunctions_ { ::std::move(referencedFunctions) }
    {
        if (!AllocatorType::LockContent(const_cast<::std::uint8_t*>(func_.data()), func_.size()))
            throw ::std::runtime_error { "failed to make the function content non-writable" };
    }

  public:
    virtual double Evaluate(::std::span<double> params) noexcept override
    {
        return reinterpret_cast<FunctionType>(func_.data())(params.data());
    }
};

}
