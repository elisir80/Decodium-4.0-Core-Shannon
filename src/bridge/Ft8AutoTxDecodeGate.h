#pragma once

#include <cstdint>

namespace decodium {

// One bounded rendezvous between the completed RX slot and automatic FT8 TX.
// Deadlines are absolute offsets in the TX slot, not delays from a timer tick.
// Keep manual TX, other modes, MAM and legacy-owned scheduling independent.
class Ft8AutoTxDecodeGate
{
public:
    static constexpr int periodMs = 15000;
    static constexpr int decodeDeadlineMs = 1200;
    static constexpr int commitDeadlineMs = 1400;
    static constexpr int audioDeadlineMs = 1800;
    static constexpr int tailMarginMs = 250;

    struct Context {
        std::int64_t slot = -1;
        std::uint64_t session = 0;
        std::uint64_t generation = 0;

        bool operator==(const Context& other) const
        {
            return slot == other.slot && session == other.session
                && generation == other.generation;
        }
    };
    enum class Decision { Wait, Ready, SkipSlot };

    void reset() { *this = Ft8AutoTxDecodeGate {}; }

    bool matches(const Context& context) const { return m_context == context; }

    bool armTimer(const Context& context)
    {
        select(context);
        if (m_timerArmed) return false;
        m_timerArmed = true;
        return true;
    }

    void decoded(const Context& context, int elapsedMs)
    {
        select(context);
        // A late callback may update the next QSO message, but cannot reopen
        // this slot or replace a payload that has already been committed.
        if (!m_committed && elapsedMs >= 0 && elapsedMs < decodeDeadlineMs)
            m_ready = true;
    }

    Decision decision(const Context& context, int elapsedMs) const
    {
        if (elapsedMs < 0 || elapsedMs >= commitDeadlineMs
            || (matches(context) && m_committed))
            return Decision::SkipSlot;
        if (matches(context) && m_ready) return Decision::Ready;
        return elapsedMs < decodeDeadlineMs ? Decision::Wait : Decision::SkipSlot;
    }

    void committed(const Context& context)
    {
        select(context);
        m_committed = true;
    }

    static bool fullFrameFits(int elapsedMs, std::int64_t payloadMs)
    {
        return elapsedMs >= 0 && elapsedMs <= audioDeadlineMs && payloadMs > 0
            && elapsedMs + payloadMs + tailMarginMs <= periodMs;
    }

private:
    void select(const Context& context)
    {
        if (matches(context)) return;
        reset();
        m_context = context;
    }

    Context m_context;
    bool m_ready = false;
    bool m_timerArmed = false;
    bool m_committed = false;
};

} // namespace decodium
