#pragma once

#include "clock.h"
#include "platform.h"
#include "stl/result.h"
#include "types.h"

namespace sap::core {
    class SAP_CORE_API Timer {
    public:
        enum class Mode {
            OneShot,
            Periodic,
        };
        Timer(Mode mode, i64 delay_ns, stl::function<void()> callback);
        ~Timer();

        Timer(const Timer&) = delete;
        Timer& operator=(const Timer&) = delete;
        Timer(Timer&&) = default;
        Timer& operator=(Timer&&) = default;

        stl::result<> start();
        stl::result<> stop();
        stl::result<> reset(i64 new_delay_ns);

    private:
        i64 m_start_ns;
        i64 m_delay_ns;
        stl::function<void()> m_callback;
        stl::jthread m_thread;
        Mode m_mode;
    };
} // namespace sap::core