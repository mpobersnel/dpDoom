#pragma once

#include <stdint.h>

struct FramePresentTime
{
	uint64_t Nanoseconds;
	uint32_t Milliseconds;
	double TicFrac;
	int Tic;
};

// Time point the frame is expected to be displayed on the monitor
extern FramePresentTime PresentTime;

void I_SetupFramePresentTime();

// Freezes tic counting temporarily. While frozen the present time stays the same.
// You not call I_WaitForTic() while freezing time, since the tic will never arrive (unless it's the current one).
void I_FreezeTime(bool frozen);

// Wait until the specified tic has been presented. Time must not be frozen.
void I_WaitForTic(int tic);

// Returns the hardware clock time, in milliseconds
uint32_t I_ClockTimeMS();

// Returns the hardware clock time, in nanoseconds
uint64_t I_ClockTimeNS();
