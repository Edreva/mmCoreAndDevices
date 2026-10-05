///////////////////////////////////////////////////////////////////////////////
// FILE:          LumascopeXYStage.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   XY stage of the Etaluma Lumascope, controlled through LumascopeHub.
//-----------------------------------------------------------------------------

#include "LumascopeXYStage.h"
#include "LumascopeHub.h"
#include "LumascopeProtocol.h"

namespace
{
   long ClampSteps(long requestedSteps, long minimumSteps, long maximumSteps)
   {
      if (requestedSteps < minimumSteps)
         return minimumSteps;
      if (requestedSteps > maximumSteps)
         return maximumSteps;
      return requestedSteps;
   }
}

LumascopeXYStage::LumascopeXYStage() :
   hub_(nullptr),
   initialized_(false)
{
   InitializeDefaultErrorMessages();
   for (const auto& errorEntry : LumascopeErrors::AllErrorTexts)
      SetErrorText(errorEntry.code, errorEntry.text);

   CreateHubIDProperty();
}

LumascopeXYStage::~LumascopeXYStage()
{
   Shutdown();
}

void LumascopeXYStage::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, LumascopeDeviceNames::XYStage);
}

int LumascopeXYStage::Initialize()
{
   if (initialized_)
      return DEVICE_OK;

   hub_ = dynamic_cast<LumascopeHub*>(GetParentHub());
   if (hub_ == nullptr || !hub_->IsConnected())
      return LumascopeErrors::HubNotAvailable;

   char hubLabel[MM::MaxStrLength];
   hub_->GetLabel(hubLabel);
   SetParentID(hubLabel);

   initialized_ = true;
   return DEVICE_OK;
}

int LumascopeXYStage::Shutdown()
{
   initialized_ = false;
   return DEVICE_OK;
}

bool LumascopeXYStage::Busy()
{
   if (!initialized_)
      return false;

   bool xAtTarget = true;
   if (hub_->IsAxisAtTarget(LumascopeProtocol::AxisX, xAtTarget) != DEVICE_OK)
      return false;
   if (!xAtTarget)
      return true;

   bool yAtTarget = true;
   if (hub_->IsAxisAtTarget(LumascopeProtocol::AxisY, yAtTarget) != DEVICE_OK)
      return false;
   return !yAtTarget;
}

int LumascopeXYStage::SetPositionSteps(long xSteps, long ySteps)
{
   if (!initialized_)
      return DEVICE_NOT_CONNECTED;

   long xMinimumSteps = 0, xMaximumSteps = 0, yMinimumSteps = 0, yMaximumSteps = 0;
   GetStepLimits(xMinimumSteps, xMaximumSteps, yMinimumSteps, yMaximumSteps);
   const long clampedXSteps = ClampSteps(xSteps, xMinimumSteps, xMaximumSteps);
   const long clampedYSteps = ClampSteps(ySteps, yMinimumSteps, yMaximumSteps);

   int result = hub_->MoveAxisToPositionSteps(LumascopeProtocol::AxisX, clampedXSteps);
   if (result != DEVICE_OK)
      return result;
   return hub_->MoveAxisToPositionSteps(LumascopeProtocol::AxisY, clampedYSteps);
}

int LumascopeXYStage::GetPositionSteps(long& xSteps, long& ySteps)
{
   if (!initialized_)
      return DEVICE_NOT_CONNECTED;

   int result = hub_->GetAxisPositionSteps(LumascopeProtocol::AxisX, xSteps);
   if (result != DEVICE_OK)
      return result;
   return hub_->GetAxisPositionSteps(LumascopeProtocol::AxisY, ySteps);
}

// The firmware HOME command homes Z first, then X and Y
int LumascopeXYStage::Home()
{
   if (!initialized_)
      return DEVICE_NOT_CONNECTED;
   return hub_->HomeAllAxes();
}

int LumascopeXYStage::Stop()
{
   if (!initialized_)
      return DEVICE_NOT_CONNECTED;

   int result = hub_->StopAxis(LumascopeProtocol::AxisX);
   if (result != DEVICE_OK)
      return result;
   return hub_->StopAxis(LumascopeProtocol::AxisY);
}

// Hardware zero is fixed at the home switches, so the origin is applied in software by the base class
int LumascopeXYStage::SetOrigin()
{
   return SetAdapterOriginUm(0.0, 0.0);
}

int LumascopeXYStage::GetLimitsUm(double& xMinimumUm, double& xMaximumUm, double& yMinimumUm, double& yMaximumUm)
{
   long xMinimumSteps = 0, xMaximumSteps = 0, yMinimumSteps = 0, yMaximumSteps = 0;
   GetStepLimits(xMinimumSteps, xMaximumSteps, yMinimumSteps, yMaximumSteps);
   xMinimumUm = xMinimumSteps * LumascopeProtocol::XyStepSizeUm;
   xMaximumUm = xMaximumSteps * LumascopeProtocol::XyStepSizeUm;
   yMinimumUm = yMinimumSteps * LumascopeProtocol::XyStepSizeUm;
   yMaximumUm = yMaximumSteps * LumascopeProtocol::XyStepSizeUm;
   return DEVICE_OK;
}

int LumascopeXYStage::GetStepLimits(long& xMinimumSteps, long& xMaximumSteps, long& yMinimumSteps, long& yMaximumSteps)
{
   if (!initialized_)
   {
      xMinimumSteps = LumascopeProtocol::XMinimumSteps;
      xMaximumSteps = LumascopeProtocol::XMaximumSteps;
      yMinimumSteps = LumascopeProtocol::YMinimumSteps;
      yMaximumSteps = LumascopeProtocol::YMaximumSteps;
      return DEVICE_OK;
   }
   hub_->GetAxisStepLimits(LumascopeProtocol::AxisX, xMinimumSteps, xMaximumSteps);
   hub_->GetAxisStepLimits(LumascopeProtocol::AxisY, yMinimumSteps, yMaximumSteps);
   return DEVICE_OK;
}

double LumascopeXYStage::GetStepSizeXUm()
{
   return LumascopeProtocol::XyStepSizeUm;
}

double LumascopeXYStage::GetStepSizeYUm()
{
   return LumascopeProtocol::XyStepSizeUm;
}

int LumascopeXYStage::IsXYStageSequenceable(bool& isSequenceable) const
{
   isSequenceable = false;
   return DEVICE_OK;
}
