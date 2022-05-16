// Copyright (c) 2022 Chanjung Kim. All rights reserved.

module;

#include <bitset>
#include <cstring>
#include <ranges>
#include <vector>

export module jitdemo.jit.memory_occupation_manager;

export namespace jitdemo::jit
{

enum class FloatRegister
{
    Xmm0 = 0,
    Xmm1 = 1,
    Xmm2 = 2,
    Xmm3 = 3,
    Xmm4 = 4,
    Xmm5 = 5,
    Xmm6 = 6,
    Xmm7 = 7,
};

constexpr ::std::size_t NumFloatRegisters { 8 };

class MemoryOccupationManager
{
  private:
    ::std::bitset<NumFloatRegisters> floatRegisterOccupations_;
    ::std::vector<bool>              stackOccupations_;

  public:
    auto GetOccupiedRegisters() noexcept
    {
        return ::std::views::iota(::std::size_t {}, NumFloatRegisters)
               | ::std::views::filter(
                   [this](::std::size_t idx) { return floatRegisterOccupations_.test(idx); })
               | ::std::views::transform(
                   [](::std::size_t idx) { return static_cast<FloatRegister>(idx); });
    }

    bool TryBorrowNewRegister(FloatRegister& result) noexcept;

    void ReturnRegister(FloatRegister reg) noexcept;

    ::std::size_t BorrowNewStackEntry() noexcept;

    void ReturnStackEntry(::std::size_t entry) noexcept;

    ::std::size_t GetMaxNumStackEntries() const noexcept;
};

}

module : private;

using ::jitdemo::jit::MemoryOccupationManager;

bool MemoryOccupationManager::TryBorrowNewRegister(FloatRegister& result) noexcept
{
    if (floatRegisterOccupations_.all())
        return false;

    for (::std::size_t i {}; i < NumFloatRegisters; ++i)
    {
        if (!floatRegisterOccupations_.test(i))
        {
            floatRegisterOccupations_.set(i);
            result = static_cast<FloatRegister>(i);
            return true;
        }
    }

    [[unlikely]] return false;
}

void jitdemo::jit::MemoryOccupationManager::ReturnRegister(FloatRegister reg) noexcept
{
    floatRegisterOccupations_.set(static_cast<::std::size_t>(reg), false);
}

::std::size_t jitdemo::jit::MemoryOccupationManager::BorrowNewStackEntry() noexcept
{
    for (::std::size_t i {}; i < stackOccupations_.size(); ++i)
    {
        if (!stackOccupations_[i])
            return i;
    }

    ::std::size_t result { stackOccupations_.size() };
    stackOccupations_.push_back(false);
    return result;
}

void jitdemo::jit::MemoryOccupationManager::ReturnStackEntry(::std::size_t entry) noexcept
{
    stackOccupations_[entry] = false;
}

::std::size_t jitdemo::jit::MemoryOccupationManager::GetMaxNumStackEntries() const noexcept
{
    return stackOccupations_.size();
}
