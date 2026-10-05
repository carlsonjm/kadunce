/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <effect/effect.h>
#include <type_traits>

namespace Kadunce {
namespace detail {
template<typename> struct MemberReturn;
template<typename R, typename C, typename... Args>
struct MemberReturn<R (C::*)(Args...)> { using type = R; };
}

// From KWin 6.8 every paint hook reports whether painting succeeded, and a
// failure, such as a GPU reset, means painting stops at once. Earlier KWin
// returns nothing, and painting always goes on.
using PaintResult = detail::MemberReturn<decltype(&KWin::Effect::paintWindow)>::type;

// Calls the next paint hook in KWin's chain and says whether painting may go on.
template<typename Paint>
[[nodiscard]] bool painted(Paint &&paint)
{
    if constexpr (std::is_void_v<std::invoke_result_t<Paint>>) {
        paint();
        return true;
    } else {
        return paint();
    }
}

// What a Kadunce paint hook hands back to KWin's chain.
template<typename Result = PaintResult>
Result paintResult([[maybe_unused]] bool succeeded)
{
    if constexpr (std::is_void_v<Result>) return;
    else return succeeded;
}
} // namespace Kadunce
