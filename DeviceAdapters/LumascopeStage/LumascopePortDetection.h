///////////////////////////////////////////////////////////////////////////////
// FILE:          LumascopePortDetection.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Finds the COM port of the Lumascope motor controller by its
//                USB vendor and product IDs.
//-----------------------------------------------------------------------------

#pragma once

#include <string>

// Returns the port name (e.g. "COM5"), or an empty string if no motor controller
// is plugged in or the platform is not supported.
std::string FindLumascopeMotorControllerPort();
