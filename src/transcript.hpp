#pragma once

#include "state.hpp"

// read whatever was appended to the log since last time and update the
// tail's activity; now is wall-clock seconds
void poll_tail(Tail& tail, double now);

const char* act_name(Act act);
