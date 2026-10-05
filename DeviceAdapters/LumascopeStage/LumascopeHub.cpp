///////////////////////////////////////////////////////////////////////////////
// FILE:          LumascopeHub.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Hub for the Etaluma Lumascope motor controller.
//                Protocol ported from LumaViewPro motorboard.py
//                (Copyright (c) 2024 Etaluma, Inc., MIT License).
//-----------------------------------------------------------------------------

#include "LumascopeHub.h"
#include "LumascopePortDetection.h"
#include "ModuleInterface.h"

#include <chrono>
#include <iterator>
#include <sstream>
#include <thread>
#include <vector>

namespace
{
   const char* const PropertyHomeOnInitialization = "HomeOnInitialization";
   const char* const HomeNever = "Never";
   const char* const HomeIfPositionUnknown = "IfPositionUnknown";
   const char* const HomeAlways = "Always";

   const char* const PropertyXYPositionReference = "XYPositionReference";
   const char* const PropertyZPositionReference = "ZPositionReference";
   const char* const PropertyControllerModel = "ControllerModel";
   const char* const PropertyControllerSerialNumber = "ControllerSerialNumber";
   const char* const PropertyFirmwareInformation = "FirmwareInformation";
   const char* const PropertyHasTurret = "HasTurret";
   const char* const ValueYes = "Yes";
   const char* const ValueNo = "No";
   const char* const UndefinedPort = "Undefined";
}

LumascopeHub::LumascopeHub() :
   port_(UndefinedPort),
   initialized_(false),
   connected_(false),
   hasTurret_(false)
{
   InitializeDefaultErrorMessages();
   for (const auto& errorEntry : LumascopeErrors::AllErrorTexts)
      SetErrorText(errorEntry.code, errorEntry.text);

   // Preselect the motor controller's port so the user normally never has to choose one
   const std::string detectedPort = FindLumascopeMotorControllerPort();
   if (!detectedPort.empty())
      port_ = detectedPort;

   CPropertyAction* portAction = new CPropertyAction(this, &LumascopeHub::OnPort);
   CreateProperty(MM::g_Keyword_Port, port_.c_str(), MM::String, false, portAction, true);

   CreateProperty(PropertyHomeOnInitialization, HomeIfPositionUnknown, MM::String, false, nullptr, true);
   AddAllowedValue(PropertyHomeOnInitialization, HomeNever);
   AddAllowedValue(PropertyHomeOnInitialization, HomeIfPositionUnknown);
   AddAllowedValue(PropertyHomeOnInitialization, HomeAlways);
}

LumascopeHub::~LumascopeHub()
{
   Shutdown();
}

void LumascopeHub::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, LumascopeDeviceNames::Hub);
}

bool LumascopeHub::Busy()
{
   return false;
}

int LumascopeHub::Initialize()
{
   if (initialized_)
      return DEVICE_OK;

   if (port_ == UndefinedPort)
      return LumascopeErrors::PortNotSet;

   int result = ConnectToController();
   if (result != DEVICE_OK)
      return result;

   result = RestorePositionReference();
   if (result != DEVICE_OK)
      return result;

   result = HomeOnInitializationIfRequested();
   if (result != DEVICE_OK)
      return result;

   CreateProperty(PropertyControllerModel, controllerModel_.c_str(), MM::String, true);
   CreateProperty(PropertyControllerSerialNumber, controllerSerialNumber_.c_str(), MM::String, true);
   CreateProperty(PropertyFirmwareInformation, firmwareInformation_.c_str(), MM::String, true);
   CreateProperty(PropertyHasTurret, hasTurret_ ? ValueYes : ValueNo, MM::String, true);

   CPropertyAction* xyReferenceAction = new CPropertyAction(this, &LumascopeHub::OnXYPositionReference);
   CreateProperty(PropertyXYPositionReference, "", MM::String, true, xyReferenceAction);
   CPropertyAction* zReferenceAction = new CPropertyAction(this, &LumascopeHub::OnZPositionReference);
   CreateProperty(PropertyZPositionReference, "", MM::String, true, zReferenceAction);

   initialized_ = true;
   return DEVICE_OK;
}

int LumascopeHub::Shutdown()
{
   if (initialized_ && connected_)
      SavePositionRecord(true);

   initialized_ = false;
   connected_ = false;
   return DEVICE_OK;
}

int LumascopeHub::DetectInstalledDevices()
{
   ClearInstalledDevices();

   const char* peripheralNames[] = { LumascopeDeviceNames::XYStage, LumascopeDeviceNames::ZStage };
   for (const char* peripheralName : peripheralNames)
   {
      MM::Device* peripheralDevice = ::CreateDevice(peripheralName);
      if (peripheralDevice != nullptr)
         AddInstalledDevice(peripheralDevice);
   }
   return DEVICE_OK;
}

bool LumascopeHub::IsConnected() const
{
   return connected_;
}

bool LumascopeHub::HasTurret() const
{
   return hasTurret_;
}

// Ctrl-D soft-resets MicroPython, so it is only sent if the controller does not answer normally
int LumascopeHub::ConnectToController()
{
   std::lock_guard<std::recursive_mutex> lock(communicationMutex_);

   DiscardPendingInput();

   int result = QueryControllerIdentity(LumascopeProtocol::IdentityQueryAttemptsBeforeWake);
   if (result != DEVICE_OK)
   {
      LogMessage("Lumascope controller did not answer FULLINFO; sending wake sequence", false);
      result = SendRawText(LumascopeProtocol::WakeSequence);
      if (result != DEVICE_OK)
         return result;

      std::string wakeResponse;
      if (ReadResponseLine(wakeResponse, LumascopeProtocol::WakeResponseTimeoutMilliseconds) == DEVICE_OK)
         LogMessage("Lumascope wake response: " + CleanResponse(wakeResponse), true);
      DiscardPendingInput();

      result = QueryControllerIdentity(LumascopeProtocol::IdentityQueryAttemptsAfterWake);
      if (result != DEVICE_OK)
         return result;
   }

   std::string infoResponse;
   if (ExchangeCommand(LumascopeProtocol::CommandInfo, infoResponse) == DEVICE_OK)
      firmwareInformation_ = infoResponse;

   LogMessage("Connected to Lumascope motor controller. Model: " + controllerModel_ +
      ", serial: " + controllerSerialNumber_ + ", firmware: " + firmwareInformation_, false);

   connected_ = true;
   return DEVICE_OK;
}

int LumascopeHub::QueryControllerIdentity(int attempts)
{
   for (int attempt = 0; attempt < attempts; ++attempt)
   {
      std::string response;
      if (ExchangeCommand(LumascopeProtocol::CommandFullInfo, response) != DEVICE_OK)
         continue;

      std::istringstream tokenStream(response);
      std::vector<std::string> tokens{ std::istream_iterator<std::string>(tokenStream),
                                       std::istream_iterator<std::string>() };

      std::string model;
      std::string serialNumber;
      for (size_t tokenIndex = 0; tokenIndex + 1 < tokens.size(); ++tokenIndex)
      {
         if (tokens[tokenIndex] == LumascopeProtocol::FullInfoModelToken)
            model = tokens[tokenIndex + 1];
         else if (tokens[tokenIndex] == LumascopeProtocol::FullInfoSerialToken)
            serialNumber = tokens[tokenIndex + 1];
      }

      if (!model.empty())
      {
         controllerModel_ = model;
         controllerSerialNumber_ = serialNumber;
         hasTurret_ = (model.back() == LumascopeProtocol::TurretModelSuffix);
         return DEVICE_OK;
      }

      LogMessage("Unexpected FULLINFO response: " + response, false);
      DiscardPendingInput();
   }
   return LumascopeErrors::ControllerNotIdentified;
}

// The motors are open loop, so absolute position is only known after homing.
// The controller's step counters survive a Micro-Manager restart, and are zeroed
// by a power cycle while the stage stays where it was. Comparing the counters with
// those saved at the last clean shutdown tells which case applies.
int LumascopeHub::RestorePositionReference()
{
   long currentCounters[LumascopeAxisCount] = { 0, 0, 0 };
   int result = ReadAllAxisCounters(currentCounters);
   if (result != DEVICE_OK)
      return result;

   positionRecord_ = LumascopePositionRecord();

   LumascopePositionRecord savedRecord;
   if (!LoadLumascopePositionRecord(controllerSerialNumber_, savedRecord))
   {
      LogMessage("No saved Lumascope position reference; stage position is unknown", false);
      SavePositionRecord(false);
      return DEVICE_OK;
   }

   bool countersMatchLastShutdown = true;
   bool countersAreAllZero = true;
   for (int axisIndex = 0; axisIndex < LumascopeAxisCount; ++axisIndex)
   {
      if (currentCounters[axisIndex] != savedRecord.axes[axisIndex].lastCounterSteps)
         countersMatchLastShutdown = false;
      if (currentCounters[axisIndex] != 0)
         countersAreAllZero = false;
   }

   bool restoreOffsetsUnchanged = false;
   bool restoreOffsetsAfterPowerCycle = false;
   if (savedRecord.cleanShutdown && countersMatchLastShutdown)
   {
      restoreOffsetsUnchanged = true;
      LogMessage("Lumascope controller counters unchanged since last session; restoring position reference", false);
   }
   else if (savedRecord.cleanShutdown && countersAreAllZero)
   {
      restoreOffsetsAfterPowerCycle = true;
      LogMessage("Lumascope controller was power cycled; restoring position from last session", false);
   }
   else if (!savedRecord.cleanShutdown && !countersAreAllZero)
   {
      restoreOffsetsUnchanged = true;
      LogMessage("Previous session did not shut down cleanly; assuming controller counters are still valid", false);
   }
   else
   {
      LogMessage("Saved Lumascope position reference does not match the controller; stage position is unknown", false);
   }

   if (restoreOffsetsUnchanged || restoreOffsetsAfterPowerCycle)
   {
      for (int axisIndex = 0; axisIndex < LumascopeAxisCount; ++axisIndex)
      {
         const LumascopeAxisRecord& savedAxis = savedRecord.axes[axisIndex];
         LumascopeAxisRecord& currentAxis = positionRecord_.axes[axisIndex];

         currentAxis.offsetSteps = savedAxis.offsetSteps;
         if (restoreOffsetsAfterPowerCycle)
            currentAxis.offsetSteps += savedAxis.lastCounterSteps;

         currentAxis.referenceState = (savedAxis.referenceState == LumascopeReferenceState::Unknown)
            ? LumascopeReferenceState::Unknown
            : LumascopeReferenceState::Restored;
      }
   }

   SavePositionRecord(false);
   return DEVICE_OK;
}

int LumascopeHub::HomeOnInitializationIfRequested()
{
   char homeOnInitializationValue[MM::MaxStrLength];
   GetProperty(PropertyHomeOnInitialization, homeOnInitializationValue);
   const std::string homeMode(homeOnInitializationValue);

   if (homeMode == HomeAlways)
      return HomeAllAxes();

   if (homeMode == HomeIfPositionUnknown)
   {
      if (!IsAxisReferenced(LumascopeProtocol::AxisX) || !IsAxisReferenced(LumascopeProtocol::AxisY))
         return HomeAllAxes();
      if (!IsAxisReferenced(LumascopeProtocol::AxisZ))
         return HomeZAxis();
   }
   return DEVICE_OK;
}

// Written with cleanShutdown = false while running, so a crash is detectable next time
void LumascopeHub::SavePositionRecord(bool isCleanShutdown)
{
   positionRecord_.cleanShutdown = false;
   if (isCleanShutdown)
   {
      long currentCounters[LumascopeAxisCount] = { 0, 0, 0 };
      if (ReadAllAxisCounters(currentCounters) == DEVICE_OK)
      {
         for (int axisIndex = 0; axisIndex < LumascopeAxisCount; ++axisIndex)
            positionRecord_.axes[axisIndex].lastCounterSteps = currentCounters[axisIndex];
         positionRecord_.cleanShutdown = true;
      }
   }

   if (!SaveLumascopePositionRecord(controllerSerialNumber_, positionRecord_))
      LogMessage("Could not save Lumascope position reference to " +
         GetLumascopePositionRecordPath(controllerSerialNumber_), false);
}

int LumascopeHub::ExchangeCommand(const std::string& command, std::string& response, long timeoutMilliseconds)
{
   std::lock_guard<std::recursive_mutex> lock(communicationMutex_);

   if (port_ == UndefinedPort)
      return LumascopeErrors::PortNotSet;

   const auto exchangeStart = std::chrono::steady_clock::now();

   int result = SendRawText(command + LumascopeProtocol::CommandTerminator);
   if (result != DEVICE_OK)
      return result;

   std::string rawLine;
   result = ReadResponseLine(rawLine, timeoutMilliseconds);
   if (result != DEVICE_OK)
   {
      LogMessage("No response from Lumascope motor controller to command: " + command, false);
      // Flush so a late reply cannot be mistaken for the answer to the next command
      DiscardPendingInput();
      return result;
   }

   response = CleanResponse(rawLine);

   const long long elapsedMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - exchangeStart).count();
   LogMessage(command + " -> " + response + " (" + std::to_string(elapsedMilliseconds) + " ms)", true);
   return DEVICE_OK;
}

int LumascopeHub::SendRawText(const std::string& text)
{
   return WriteToComPort(port_.c_str(),
      reinterpret_cast<const unsigned char*>(text.data()),
      static_cast<unsigned>(text.size()));
}

// Reads with our own deadline instead of GetSerialAnswer so that long operations
// such as homing can wait without changing the serial port's AnswerTimeout.
int LumascopeHub::ReadResponseLine(std::string& line, long timeoutMilliseconds)
{
   const auto readStart = std::chrono::steady_clock::now();
   const auto spinWaitEnd = readStart + std::chrono::milliseconds(LumascopeProtocol::ResponseSpinWaitMilliseconds);
   const auto deadline = readStart + std::chrono::milliseconds(timeoutMilliseconds);

   while (true)
   {
      const size_t newlinePosition = receiveBuffer_.find('\n');
      if (newlinePosition != std::string::npos)
      {
         line = receiveBuffer_.substr(0, newlinePosition);
         receiveBuffer_.erase(0, newlinePosition + 1);
         return DEVICE_OK;
      }

      unsigned char readChunk[256];
      unsigned long bytesRead = 0;
      int result = ReadFromComPort(port_.c_str(), readChunk, sizeof(readChunk), bytesRead);
      if (result != DEVICE_OK)
         return result;

      if (bytesRead > 0)
      {
         receiveBuffer_.append(reinterpret_cast<const char*>(readChunk), bytesRead);
         continue;
      }

      const auto now = std::chrono::steady_clock::now();
      if (now >= deadline)
         return LumascopeErrors::ResponseTimeout;

      if (now < spinWaitEnd)
         std::this_thread::yield();
      else
         CDeviceUtils::SleepMs(1);
   }
}

void LumascopeHub::DiscardPendingInput()
{
   receiveBuffer_.clear();
   PurgeComPort(port_.c_str());
}

std::string LumascopeHub::TrimWhitespace(const std::string& text)
{
   const char* whitespaceCharacters = " \t\r\n";
   const size_t firstCharacter = text.find_first_not_of(whitespaceCharacters);
   if (firstCharacter == std::string::npos)
      return std::string();
   const size_t lastCharacter = text.find_last_not_of(whitespaceCharacters);
   return text.substr(firstCharacter, lastCharacter - firstCharacter + 1);
}

// The firmware sometimes leaves earlier output on the same line separated by a
// carriage return; motorboard.py keeps only the text after the last one.
std::string LumascopeHub::CleanResponse(const std::string& rawLine)
{
   std::string cleanedLine = TrimWhitespace(rawLine);
   const size_t lastCarriageReturn = cleanedLine.find_last_of('\r');
   if (lastCarriageReturn != std::string::npos)
      cleanedLine = cleanedLine.substr(lastCarriageReturn + 1);
   return TrimWhitespace(cleanedLine);
}

int LumascopeHub::ParseSignedSteps(const std::string& response, long& steps)
{
   long long parsedValue = 0;
   try
   {
      size_t charactersConsumed = 0;
      parsedValue = std::stoll(response, &charactersConsumed);
      if (charactersConsumed != response.size())
         return LumascopeErrors::InvalidResponse;
   }
   catch (const std::exception&)
   {
      return LumascopeErrors::InvalidResponse;
   }

   // Positions may come back as unsigned 32-bit two's complement
   if (parsedValue > LumascopeProtocol::SignedThirtyTwoBitMaximum)
      parsedValue -= LumascopeProtocol::TwosComplementModulus;

   steps = static_cast<long>(parsedValue);
   return DEVICE_OK;
}

int LumascopeHub::AxisIndex(char axis)
{
   for (int axisIndex = 0; axisIndex < LumascopeAxisCount; ++axisIndex)
   {
      if (LumascopeProtocol::AxesInRecordOrder[axisIndex] == axis)
         return axisIndex;
   }
   return -1;
}

long LumascopeHub::MaximumStepsForAxis(char axis)
{
   switch (axis)
   {
   case LumascopeProtocol::AxisX: return LumascopeProtocol::XMaximumSteps;
   case LumascopeProtocol::AxisY: return LumascopeProtocol::YMaximumSteps;
   case LumascopeProtocol::AxisZ: return LumascopeProtocol::ZMaximumSteps;
   default: return 0;
   }
}

const char* LumascopeHub::ReferenceStateName(LumascopeReferenceState referenceState)
{
   switch (referenceState)
   {
   case LumascopeReferenceState::Homed: return "Homed";
   case LumascopeReferenceState::Restored: return "Restored from last session";
   default: return "Unknown (not homed)";
   }
}

int LumascopeHub::GetAxisCounterSteps(char axis, long& counterSteps)
{
   std::string response;
   int result = ExchangeCommand(std::string(LumascopeProtocol::CommandActualReadPrefix) + axis, response);
   if (result != DEVICE_OK)
      return result;

   result = ParseSignedSteps(response, counterSteps);
   if (result != DEVICE_OK)
      LogMessage("Could not parse position response for axis " + std::string(1, axis) + ": " + response, false);
   return result;
}

int LumascopeHub::ReadAllAxisCounters(long (&counterSteps)[LumascopeAxisCount])
{
   for (int axisIndex = 0; axisIndex < LumascopeAxisCount; ++axisIndex)
   {
      int result = GetAxisCounterSteps(LumascopeProtocol::AxesInRecordOrder[axisIndex], counterSteps[axisIndex]);
      if (result != DEVICE_OK)
         return result;
   }
   return DEVICE_OK;
}

int LumascopeHub::MoveAxisToCounterSteps(char axis, long counterSteps)
{
   long long transmittedSteps = counterSteps;
   if (transmittedSteps < 0)
      transmittedSteps += LumascopeProtocol::TwosComplementModulus;

   const std::string command = std::string(LumascopeProtocol::CommandTargetWritePrefix) + axis +
      std::to_string(transmittedSteps);

   // The reply carries no information but must be read to keep commands and replies paired
   std::string response;
   return ExchangeCommand(command, response);
}

int LumascopeHub::MoveAxisToPositionSteps(char axis, long positionSteps)
{
   const int axisIndex = AxisIndex(axis);
   if (axisIndex < 0)
      return LumascopeErrors::InvalidAxis;
   return MoveAxisToCounterSteps(axis, positionSteps - positionRecord_.axes[axisIndex].offsetSteps);
}

int LumascopeHub::GetAxisPositionSteps(char axis, long& positionSteps)
{
   const int axisIndex = AxisIndex(axis);
   if (axisIndex < 0)
      return LumascopeErrors::InvalidAxis;

   long counterSteps = 0;
   int result = GetAxisCounterSteps(axis, counterSteps);
   if (result != DEVICE_OK)
      return result;

   positionSteps = counterSteps + positionRecord_.axes[axisIndex].offsetSteps;
   return DEVICE_OK;
}

bool LumascopeHub::IsAxisReferenced(char axis)
{
   const int axisIndex = AxisIndex(axis);
   return axisIndex >= 0 &&
      positionRecord_.axes[axisIndex].referenceState != LumascopeReferenceState::Unknown;
}

// Without a reference the starting point could be anywhere in the travel range,
// so allow a full range of travel in both directions from it.
void LumascopeHub::GetAxisStepLimits(char axis, long& minimumSteps, long& maximumSteps)
{
   maximumSteps = MaximumStepsForAxis(axis);
   minimumSteps = IsAxisReferenced(axis) ? 0 : -maximumSteps;
}

int LumascopeHub::IsAxisAtTarget(char axis, bool& isAtTarget)
{
   std::string response;
   int result = ExchangeCommand(std::string(LumascopeProtocol::CommandStatusReadPrefix) + axis, response);
   if (result != DEVICE_OK)
      return result;

   unsigned long long statusRegister = 0;
   try
   {
      statusRegister = static_cast<unsigned long long>(std::stoll(response));
   }
   catch (const std::exception&)
   {
      LogMessage("Could not parse status response for axis " + std::string(1, axis) + ": " + response, false);
      return LumascopeErrors::InvalidResponse;
   }

   isAtTarget = ((statusRegister >> LumascopeProtocol::StatusPositionReachedBit) & 1ULL) != 0;
   return DEVICE_OK;
}

int LumascopeHub::WaitForAxisAtTarget(char axis, long timeoutMilliseconds)
{
   const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMilliseconds);

   while (true)
   {
      bool isAtTarget = false;
      int result = IsAxisAtTarget(axis, isAtTarget);
      if (result != DEVICE_OK)
         return result;
      if (isAtTarget)
         return DEVICE_OK;
      if (std::chrono::steady_clock::now() >= deadline)
         return LumascopeErrors::MoveTimeout;
      CDeviceUtils::SleepMs(LumascopeProtocol::MovePollIntervalMilliseconds);
   }
}

// The firmware has no stop command, so retarget the axis to wherever it currently is
int LumascopeHub::StopAxis(char axis)
{
   std::lock_guard<std::recursive_mutex> lock(communicationMutex_);

   long counterSteps = 0;
   int result = GetAxisCounterSteps(axis, counterSteps);
   if (result != DEVICE_OK)
      return result;
   return MoveAxisToCounterSteps(axis, counterSteps);
}

// HOME references Z first, then X and Y, and zeroes the controller counters at the switches
int LumascopeHub::HomeAllAxes()
{
   std::string response;
   int result = ExchangeCommand(LumascopeProtocol::CommandHomeAll, response,
      LumascopeProtocol::HomingTimeoutMilliseconds);
   if (result != DEVICE_OK)
      return result;

   if (response.find(LumascopeProtocol::HomeAllCompleteMarker) == std::string::npos)
      LogMessage("HOME finished with an unexpected response: " + response, false);

   for (LumascopeAxisRecord& axisRecord : positionRecord_.axes)
   {
      axisRecord.offsetSteps = 0;
      axisRecord.referenceState = LumascopeReferenceState::Homed;
   }
   SavePositionRecord(false);
   return DEVICE_OK;
}

int LumascopeHub::HomeZAxis()
{
   std::string response;
   int result = ExchangeCommand(LumascopeProtocol::CommandHomeZ, response,
      LumascopeProtocol::HomingTimeoutMilliseconds);
   if (result != DEVICE_OK)
      return result;

   LumascopeAxisRecord& zAxisRecord = positionRecord_.axes[AxisIndex(LumascopeProtocol::AxisZ)];
   zAxisRecord.offsetSteps = 0;
   zAxisRecord.referenceState = LumascopeReferenceState::Homed;
   SavePositionRecord(false);
   return DEVICE_OK;
}

int LumascopeHub::OnPort(MM::PropertyBase* property, MM::ActionType actionType)
{
   if (actionType == MM::BeforeGet)
   {
      property->Set(port_.c_str());
   }
   else if (actionType == MM::AfterSet)
   {
      if (initialized_)
      {
         property->Set(port_.c_str());
         return LumascopeErrors::PortChangeForbidden;
      }
      property->Get(port_);
   }
   return DEVICE_OK;
}

int LumascopeHub::OnXYPositionReference(MM::PropertyBase* property, MM::ActionType actionType)
{
   if (actionType == MM::BeforeGet)
   {
      const LumascopeReferenceState xState = positionRecord_.axes[AxisIndex(LumascopeProtocol::AxisX)].referenceState;
      const LumascopeReferenceState yState = positionRecord_.axes[AxisIndex(LumascopeProtocol::AxisY)].referenceState;
      const LumascopeReferenceState combinedState =
         (xState == LumascopeReferenceState::Unknown || yState == LumascopeReferenceState::Unknown)
         ? LumascopeReferenceState::Unknown : xState;
      property->Set(ReferenceStateName(combinedState));
   }
   return DEVICE_OK;
}

int LumascopeHub::OnZPositionReference(MM::PropertyBase* property, MM::ActionType actionType)
{
   if (actionType == MM::BeforeGet)
      property->Set(ReferenceStateName(positionRecord_.axes[AxisIndex(LumascopeProtocol::AxisZ)].referenceState));
   return DEVICE_OK;
}
