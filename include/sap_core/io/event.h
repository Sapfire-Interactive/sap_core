#pragma once

#include "sap_core/types.h"

namespace sap::io {

    enum class Event : u8 {
        None     = 0,
        Readable = 1,
        Writable = 2,
        Error    = 4,
        HangUp   = 8,
    };

    constexpr Event operator|(Event a, Event b) noexcept {
        return static_cast<Event>(static_cast<u8>(a) | static_cast<u8>(b));
    }

    constexpr Event operator&(Event a, Event b) noexcept {
        return static_cast<Event>(static_cast<u8>(a) & static_cast<u8>(b));
    }

    constexpr Event operator~(Event a) noexcept {
        return static_cast<Event>(~static_cast<u8>(a));
    }

    constexpr Event& operator|=(Event& a, Event b) noexcept {
        a = a | b;
        return a;
    }

    constexpr Event& operator&=(Event& a, Event b) noexcept {
        a = a & b;
        return a;
    }

    constexpr bool any(Event e) noexcept { return static_cast<u8>(e) != 0; }

    constexpr bool has(Event whole, Event bits) noexcept { return any(whole & bits); }

} // namespace sap::io
