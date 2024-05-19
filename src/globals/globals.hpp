#pragma once

namespace fast_chess {
// Make sure that all processes are stopped, and no zombie processes are left after the
// program exits.
void stopProcesses();
void setCtrlCHandler();
}  // namespace fast_chess
