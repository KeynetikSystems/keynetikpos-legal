// =============================================================================
// antidebug.h — AntiDebug: minimal anti-tamper check
// -----------------------------------------------------------------------------
// WHAT: Detects an attached debugger so main() can refuse to start, protecting
//       the licensing system from casual inspection.
// HOW:  Header-only static class. On Windows *release* builds only it calls
//       IsDebuggerPresent() and CheckRemoteDebuggerPresent(); in debug builds
//       and on other platforms it always returns false. main() shows a
//       deliberately generic error so an attacker isn't told which check fired.
// WHY:  A light deterrent only. Timing heuristics, VM detection, and process
//       scanning were removed because they locked out legitimate users (slow
//       machines, virtualized POS terminals, IT diagnostic tools) far more
//       often than they stopped crackers.
// =============================================================================
#pragma once

#include <QtGlobal>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Minimal anti-debug: only checks for an attached debugger, and only in
// release builds. Timing heuristics, VM detection, and process scanning were
// removed — they locked out legitimate users (slow machines, virtualized POS
// terminals, IT staff running diagnostic tools) far more often than crackers.
class AntiDebug {
public:
    static bool isThreatDetected() {
#if defined(Q_OS_WIN) && defined(NDEBUG)
        if (IsDebuggerPresent())
            return true;

        BOOL remoteDebugger = FALSE;
        CheckRemoteDebuggerPresent(GetCurrentProcess(), &remoteDebugger);
        return remoteDebugger == TRUE;
#else
        return false;
#endif
    }
};
