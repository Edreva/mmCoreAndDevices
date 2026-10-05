///////////////////////////////////////////////////////////////////////////////
// FILE:          LumascopeHub.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Hub for the Etaluma Lumascope motor controller. Owns the
//                serial connection shared by the XY and Z stages and the
//                mapping from controller step counters to stage positions.
//-----------------------------------------------------------------------------

#pragma once

#include "DeviceBase.h"
#include "LumascopePositionStore.h"
#include "LumascopeProtocol.h"

#include <mutex>
#include <string>

namespace LumascopeDeviceNames
{
   const char* const Hub = "LumascopeHub";
   const char* const XYStage = "LumascopeXYStage";
   const char* const ZStage = "LumascopeZStage";
}

namespace LumascopeErrors
{
   const int PortNotSet = 10101;
   const int PortChangeForbidden = 10102;
   const int ResponseTimeout = 10103;
   const int InvalidResponse = 10104;
   const int ControllerNotIdentified = 10105;
   const int HubNotAvailable = 10106;
   const int MoveTimeout = 10107;
   const int InvalidAxis = 10108;

   struct ErrorText
   {
      int code;
      const char* text;
   };

   const ErrorText AllErrorTexts[] =
   {
      { PortNotSet, "No serial port selected for the Lumascope motor controller." },
      { PortChangeForbidden, "The serial port cannot be changed after initialization." },
      { ResponseTimeout, "The Lumascope motor controller did not respond in time." },
      { InvalidResponse, "The Lumascope motor controller returned an unexpected response." },
      { ControllerNotIdentified, "Could not identify a Lumascope motor controller on this port (FULLINFO failed)." },
      { HubNotAvailable, "The LumascopeHub is missing or not connected. Load and initialize the hub first." },
      { MoveTimeout, "A Lumascope axis did not reach its target position in time." },
      { InvalidAxis, "Internal error: unknown Lumascope axis." },
   };
}

class LumascopeHub : public HubBase<LumascopeHub>
{
public:
   LumascopeHub();
   ~LumascopeHub();

   int Initialize();
   int Shutdown();
   void GetName(char* name) const;
   bool Busy();
   int DetectInstalledDevices();

   bool IsConnected() const;
   bool HasTurret() const;

   // Positions here are stage positions in microsteps, with the controller's
   // counter offset already applied (0 = home switch once referenced).
   int MoveAxisToPositionSteps(char axis, long positionSteps);
   int GetAxisPositionSteps(char axis, long& positionSteps);
   void GetAxisStepLimits(char axis, long& minimumSteps, long& maximumSteps);
   bool IsAxisReferenced(char axis);

   int IsAxisAtTarget(char axis, bool& isAtTarget);
   int WaitForAxisAtTarget(char axis, long timeoutMilliseconds);
   int StopAxis(char axis);
   int HomeAllAxes();
   int HomeZAxis();

   int ExchangeCommand(const std::string& command, std::string& response,
      long timeoutMilliseconds = LumascopeProtocol::DefaultResponseTimeoutMilliseconds);

   int OnPort(MM::PropertyBase* property, MM::ActionType actionType);
   int OnXYPositionReference(MM::PropertyBase* property, MM::ActionType actionType);
   int OnZPositionReference(MM::PropertyBase* property, MM::ActionType actionType);

private:
   int ConnectToController();
   int QueryControllerIdentity(int attempts);
   int RestorePositionReference();
   int HomeOnInitializationIfRequested();
   void SavePositionRecord(bool isCleanShutdown);

   int MoveAxisToCounterSteps(char axis, long counterSteps);
   int GetAxisCounterSteps(char axis, long& counterSteps);
   int ReadAllAxisCounters(long (&counterSteps)[LumascopeAxisCount]);

   int SendRawText(const std::string& text);
   int ReadResponseLine(std::string& line, long timeoutMilliseconds);
   void DiscardPendingInput();

   static int AxisIndex(char axis);
   static long MaximumStepsForAxis(char axis);
   static const char* ReferenceStateName(LumascopeReferenceState referenceState);
   static std::string TrimWhitespace(const std::string& text);
   static std::string CleanResponse(const std::string& rawLine);
   static int ParseSignedSteps(const std::string& response, long& steps);

   std::string port_;
   std::string receiveBuffer_;
   std::string controllerModel_;
   std::string controllerSerialNumber_;
   std::string firmwareInformation_;
   bool initialized_;
   bool connected_;
   bool hasTurret_;
   LumascopePositionRecord positionRecord_;
   std::recursive_mutex communicationMutex_;
};
