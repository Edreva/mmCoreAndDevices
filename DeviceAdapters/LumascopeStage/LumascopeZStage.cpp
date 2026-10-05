///////////////////////////////////////////////////////////////////////////////
// FILE:          LumascopeZStage.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Z (focus) stage of the Etaluma Lumascope, controlled through LumascopeHub.
//                Backlash handling ported from LumaViewPro motorboard.py
//                (Copyright (c) 2024 Etaluma, Inc., MIT License).
//-----------------------------------------------------------------------------

#include "LumascopeZStage.h"
#include "LumascopeHub.h"
#include "LumascopeProtocol.h"

#include <algorithm>
#include <cmath>

namespace
{
   const char* const PropertyBacklashCompensation = "BacklashCompensation";
   const char* const PropertyBacklashDistanceUm = "BacklashDistance(um)";
   const char* const ValueYes = "Yes";
   const char* const ValueNo = "No";
}

LumascopeZStage::LumascopeZStage() :
   hub_(nullptr),
   initialized_(false),
   backlashCompensationEnabled_(true),
   backlashDistanceUm_(LumascopeProtocol::DefaultZBacklashUm)
{
   InitializeDefaultErrorMessages();
   for (const auto& errorEntry : LumascopeErrors::AllErrorTexts)
      SetErrorText(errorEntry.code, errorEntry.text);

   CreateHubIDProperty();
}

LumascopeZStage::~LumascopeZStage()
{
   Shutdown();
}

void LumascopeZStage::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, LumascopeDeviceNames::ZStage);
}

int LumascopeZStage::Initialize()
{
   if (initialized_)
      return DEVICE_OK;

   hub_ = dynamic_cast<LumascopeHub*>(GetParentHub());
   if (hub_ == nullptr || !hub_->IsConnected())
      return LumascopeErrors::HubNotAvailable;

   char hubLabel[MM::MaxStrLength];
   hub_->GetLabel(hubLabel);
   SetParentID(hubLabel);

   CPropertyAction* backlashCompensationAction = new CPropertyAction(this, &LumascopeZStage::OnBacklashCompensation);
   CreateProperty(PropertyBacklashCompensation, ValueYes, MM::String, false, backlashCompensationAction);
   AddAllowedValue(PropertyBacklashCompensation, ValueYes);
   AddAllowedValue(PropertyBacklashCompensation, ValueNo);

   CPropertyAction* backlashDistanceAction = new CPropertyAction(this, &LumascopeZStage::OnBacklashDistance);
   CreateFloatProperty(PropertyBacklashDistanceUm, backlashDistanceUm_, false, backlashDistanceAction);
   SetPropertyLimits(PropertyBacklashDistanceUm, 0.0, LumascopeProtocol::MaximumZBacklashUm);

   initialized_ = true;
   return DEVICE_OK;
}

int LumascopeZStage::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

bool LumascopeZStage::Busy()
{
   if (!initialized_)
      return false;

   bool zAtTarget = true;
   if (hub_->IsAxisAtTarget(LumascopeProtocol::AxisZ, zAtTarget) != DEVICE_OK)
      return false;
   return !zAtTarget;
}

int LumascopeZStage::SetPositionUm(double positionUm)
{
   return SetPositionSteps(static_cast<long>(std::lround(positionUm / LumascopeProtocol::ZStepSizeUm)));
}

int LumascopeZStage::GetPositionUm(double& positionUm)
{
   long steps = 0;
   int result = GetPositionSteps(steps);
   if (result != DEVICE_OK)
      return result;
   positionUm = steps * LumascopeProtocol::ZStepSizeUm;
   return DEVICE_OK;
}

int LumascopeZStage::SetPositionSteps(long steps)
{
   if (!initialized_)
      return DEVICE_NOT_CONNECTED;

   long minimumSteps = 0;
   long maximumSteps = 0;
   hub_->GetAxisStepLimits(LumascopeProtocol::AxisZ, minimumSteps, maximumSteps);
   const long clampedSteps = std::max(minimumSteps, std::min(steps, maximumSteps));
   return MoveToTargetSteps(clampedSteps);
}

int LumascopeZStage::GetPositionSteps(long& steps)
{
   if (!initialized_)
      return DEVICE_NOT_CONNECTED;
   return hub_->GetAxisPositionSteps(LumascopeProtocol::AxisZ, steps);
}

// Downward moves first go past the target by the backlash distance and then come
// back up, so the focus drive always settles from the same direction.
// This blocks until the overshoot leg completes; the final leg is reported via Busy().
// The clearance check near the bottom of travel only makes sense once Z is referenced.
int LumascopeZStage::MoveToTargetSteps(long targetSteps)
{
   if (backlashCompensationEnabled_ && backlashDistanceUm_ > 0.0)
   {
      long currentSteps = 0;
      int result = hub_->GetAxisPositionSteps(LumascopeProtocol::AxisZ, currentSteps);
      if (result != DEVICE_OK)
         return result;

      const double currentUm = currentSteps * LumascopeProtocol::ZStepSizeUm;
      const double targetUm = targetSteps * LumascopeProtocol::ZStepSizeUm;
      const bool isMovingDown = currentUm > targetUm;
      const bool hasRoomForOvershoot = !hub_->IsAxisReferenced(LumascopeProtocol::AxisZ) ||
         targetUm > backlashDistanceUm_ + LumascopeProtocol::ZBacklashMinimumClearanceUm;

      if (isMovingDown && hasRoomForOvershoot)
      {
         long minimumSteps = 0;
         long maximumSteps = 0;
         hub_->GetAxisStepLimits(LumascopeProtocol::AxisZ, minimumSteps, maximumSteps);
         const long overshootSteps = std::max(minimumSteps,
            static_cast<long>(std::lround((targetUm - backlashDistanceUm_) / LumascopeProtocol::ZStepSizeUm)));

         result = hub_->MoveAxisToPositionSteps(LumascopeProtocol::AxisZ, overshootSteps);
         if (result != DEVICE_OK)
            return result;

         result = hub_->WaitForAxisAtTarget(LumascopeProtocol::AxisZ,
            LumascopeProtocol::MoveCompletionTimeoutMilliseconds);
         if (result != DEVICE_OK)
            return result;
      }
   }

   return hub_->MoveAxisToPositionSteps(LumascopeProtocol::AxisZ, targetSteps);
}

// Hardware zero is fixed at the home switch and this base class has no software origin
int LumascopeZStage::SetOrigin()
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

int LumascopeZStage::GetLimits(double& lowerLimitUm, double& upperLimitUm)
{
   if (!initialized_)
   {
      lowerLimitUm = LumascopeProtocol::ZMinimumUm;
      upperLimitUm = LumascopeProtocol::ZMaximumUm;
      return DEVICE_OK;
   }
   long minimumSteps = 0;
   long maximumSteps = 0;
   hub_->GetAxisStepLimits(LumascopeProtocol::AxisZ, minimumSteps, maximumSteps);
   lowerLimitUm = minimumSteps * LumascopeProtocol::ZStepSizeUm;
   upperLimitUm = maximumSteps * LumascopeProtocol::ZStepSizeUm;
   return DEVICE_OK;
}

int LumascopeZStage::Home()
{
   if (!initialized_)
      return DEVICE_NOT_CONNECTED;
   return hub_->HomeZAxis();
}

int LumascopeZStage::Stop()
{
   if (!initialized_)
      return DEVICE_NOT_CONNECTED;
   return hub_->StopAxis(LumascopeProtocol::AxisZ);
}

int LumascopeZStage::IsStageSequenceable(bool& isSequenceable) const
{
   isSequenceable = false;
   return DEVICE_OK;
}

bool LumascopeZStage::IsContinuousFocusDrive() const
{
   return false;
}

int LumascopeZStage::OnBacklashCompensation(MM::PropertyBase* property, MM::ActionType actionType)
{
   if (actionType == MM::BeforeGet)
   {
      property->Set(backlashCompensationEnabled_ ? ValueYes : ValueNo);
   }
   else if (actionType == MM::AfterSet)
   {
      std::string value;
      property->Get(value);
      backlashCompensationEnabled_ = (value == ValueYes);
   }
   return DEVICE_OK;
}

int LumascopeZStage::OnBacklashDistance(MM::PropertyBase* property, MM::ActionType actionType)
{
   if (actionType == MM::BeforeGet)
   {
      property->Set(backlashDistanceUm_);
   }
   else if (actionType == MM::AfterSet)
   {
      property->Get(backlashDistanceUm_);
   }
   return DEVICE_OK;
}
