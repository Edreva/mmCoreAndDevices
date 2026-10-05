///////////////////////////////////////////////////////////////////////////////
// FILE:          LumascopePositionStore.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Saves the stage position reference between sessions so the
//                stage does not need to be homed every time Micro-Manager starts.
//-----------------------------------------------------------------------------

#pragma once

#include <string>

enum class LumascopeReferenceState
{
   Unknown = 0,
   Homed = 1,
   Restored = 2
};

const int LumascopeAxisCount = 3; // X, Y, Z in that order

struct LumascopeAxisRecord
{
   LumascopeReferenceState referenceState = LumascopeReferenceState::Unknown;
   // Stage position = controller step counter + offset
   long offsetSteps = 0;
   // Controller step counter at the last clean shutdown
   long lastCounterSteps = 0;
};

struct LumascopePositionRecord
{
   bool cleanShutdown = false;
   LumascopeAxisRecord axes[LumascopeAxisCount];
};

bool LoadLumascopePositionRecord(const std::string& controllerSerialNumber, LumascopePositionRecord& record);
bool SaveLumascopePositionRecord(const std::string& controllerSerialNumber, const LumascopePositionRecord& record);
std::string GetLumascopePositionRecordPath(const std::string& controllerSerialNumber);
