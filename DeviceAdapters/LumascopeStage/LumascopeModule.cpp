///////////////////////////////////////////////////////////////////////////////
// FILE:          LumascopeModule.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Module entry points for the Etaluma Lumascope device adapter.
//-----------------------------------------------------------------------------

#include "LumascopeHub.h"
#include "LumascopeXYStage.h"
#include "LumascopeZStage.h"
#include "ModuleInterface.h"

#include <cstring>

MODULE_API void InitializeModuleData()
{
   RegisterDevice(LumascopeDeviceNames::Hub, MM::HubDevice, "Etaluma Lumascope motor controller");
   RegisterDevice(LumascopeDeviceNames::XYStage, MM::XYStageDevice, "Etaluma Lumascope XY stage");
   RegisterDevice(LumascopeDeviceNames::ZStage, MM::StageDevice, "Etaluma Lumascope Z (focus) stage");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == nullptr)
      return nullptr;

   if (std::strcmp(deviceName, LumascopeDeviceNames::Hub) == 0)
      return new LumascopeHub();
   if (std::strcmp(deviceName, LumascopeDeviceNames::XYStage) == 0)
      return new LumascopeXYStage();
   if (std::strcmp(deviceName, LumascopeDeviceNames::ZStage) == 0)
      return new LumascopeZStage();

   return nullptr;
}

MODULE_API void DeleteDevice(MM::Device* device)
{
   delete device;
}
