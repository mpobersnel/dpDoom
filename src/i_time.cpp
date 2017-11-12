/*
** i_time.cpp
** Implements the timer
**
**---------------------------------------------------------------------------
** Copyright 1998-2916 Randy Heit
** Copyright 2917 Magnus Norddahl
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <chrono>
#include <thread>
#include <stdint.h>
#include "i_time.h"
#include "doomdef.h"

//==========================================================================
//
// Tick time functions
//
//==========================================================================

static uint64_t MSToNS(unsigned int ms)
{
	return static_cast<uint64_t>(ms) * 1'000'000;
}

static uint32_t NSToMS(uint64_t ns)
{
	return static_cast<uint32_t>(ns / 1'000'000);
}

static int NSToTic(uint64_t ns)
{
	return static_cast<int>(ns * TICRATE / 1'000'000'000);
}

static uint64_t TicToNS(int tic)
{
	return static_cast<uint64_t>(tic) * 1'000'000'000 / TICRATE;
}

static uint64_t FirstFrameStartTime;
static uint64_t FirstTicTime;
static uint64_t FreezeTime;

FramePresentTime PresentTime;

void I_SetupFramePresentTime()
{
	uint64_t now = I_ClockTimeNS();

	if (FirstFrameStartTime == 0)
	{
		FirstFrameStartTime = now;
		FirstTicTime = now;
	}

	PresentTime.Nanoseconds = now - FirstFrameStartTime;
	PresentTime.Milliseconds = NSToMS(PresentTime.Nanoseconds);

	if (FreezeTime != 0)
		now = FreezeTime;
	
	int currentTic = NSToTic(now - FirstTicTime);
	uint64_t ticStartTime = FirstTicTime + TicToNS(currentTic);
	uint64_t ticNextTime = FirstTicTime + TicToNS(currentTic + 1);

	PresentTime.Tic = currentTic + 1;
	PresentTime.TicFrac = (now - ticStartTime) / (double)(ticNextTime - ticStartTime);
}

void I_FreezeTime(bool frozen)
{
	if (frozen && FreezeTime == 0)
	{
		FreezeTime = I_ClockTimeNS();
	}
	else if (!frozen && FreezeTime != 0)
	{
		FirstTicTime += I_ClockTimeNS() - FreezeTime;
		FreezeTime = 0;
		I_SetupFramePresentTime();
	}
}

void I_WaitForTic(int tic)
{
	while (PresentTime.Tic <= tic)
	{
		std::this_thread::yield();
		I_SetupFramePresentTime();
	}
}

void I_WaitVBL(int count)
{
	// I_WaitVBL is never used to actually synchronize to the vertical blank.
	// Instead, it's used for delay purposes. Doom used a 70 Hz display mode,
	// so that's what we use to determine how long to wait for.

	std::this_thread::sleep_for(std::chrono::milliseconds(1000 * count / 70));
	I_SetupFramePresentTime();
}

uint32_t I_ClockTimeMS()
{
	return NSToMS(I_ClockTimeNS());
}

uint64_t I_ClockTimeNS()
{
	using namespace std::chrono;
	return (uint64_t)duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count();
}
