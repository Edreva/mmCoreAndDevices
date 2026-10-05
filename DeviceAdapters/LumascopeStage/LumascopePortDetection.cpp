///////////////////////////////////////////////////////////////////////////////
// FILE:          LumascopePortDetection.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Finds the COM port of the Lumascope motor controller by its
//                USB vendor and product IDs.
//-----------------------------------------------------------------------------

#include "LumascopePortDetection.h"
#include "LumascopeProtocol.h"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
// Must precede devguid.h so GUID_DEVCLASS_PORTS is defined rather than only declared
#include <initguid.h>
#include <devguid.h>

#include <algorithm>
#include <cctype>

#ifdef _MSC_VER
#pragma comment(lib, "setupapi.lib")
#endif

namespace
{
   std::string ToUpperCase(std::string text)
   {
      std::transform(text.begin(), text.end(), text.begin(),
         [](unsigned char character) { return static_cast<char>(std::toupper(character)); });
      return text;
   }

   std::string ReadPortName(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA& deviceInfoData)
   {
      HKEY deviceRegistryKey = SetupDiOpenDevRegKey(deviceInfoSet, &deviceInfoData,
         DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
      if (deviceRegistryKey == INVALID_HANDLE_VALUE)
         return std::string();

      char portName[256] = { 0 };
      DWORD portNameSize = sizeof(portName) - 1;
      DWORD valueType = 0;
      const LONG queryResult = RegQueryValueExA(deviceRegistryKey, "PortName", nullptr, &valueType,
         reinterpret_cast<LPBYTE>(portName), &portNameSize);
      RegCloseKey(deviceRegistryKey);

      if (queryResult != ERROR_SUCCESS || valueType != REG_SZ)
         return std::string();
      return std::string(portName);
   }
}

std::string FindLumascopeMotorControllerPort()
{
   HDEVINFO deviceInfoSet = SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
   if (deviceInfoSet == INVALID_HANDLE_VALUE)
      return std::string();

   const std::string wantedHardwareIdFragment = ToUpperCase(LumascopeProtocol::MotorControllerHardwareIdFragment);
   std::string motorControllerPort;

   SP_DEVINFO_DATA deviceInfoData;
   deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
   for (DWORD deviceIndex = 0; SetupDiEnumDeviceInfo(deviceInfoSet, deviceIndex, &deviceInfoData); ++deviceIndex)
   {
      // SPDRP_HARDWAREID is a double-null-terminated list; the first entry
      // (e.g. "USB\VID_2E8A&PID_0005&REV_0100&MI_00") is enough to match on.
      char hardwareIds[1024] = { 0 };
      if (!SetupDiGetDeviceRegistryPropertyA(deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID, nullptr,
            reinterpret_cast<PBYTE>(hardwareIds), sizeof(hardwareIds) - 2, nullptr))
         continue;

      if (ToUpperCase(hardwareIds).find(wantedHardwareIdFragment) == std::string::npos)
         continue;

      motorControllerPort = ReadPortName(deviceInfoSet, deviceInfoData);
      if (!motorControllerPort.empty())
         break;
   }

   SetupDiDestroyDeviceInfoList(deviceInfoSet);
   return motorControllerPort;
}

#else

std::string FindLumascopeMotorControllerPort()
{
   return std::string();
}

#endif
