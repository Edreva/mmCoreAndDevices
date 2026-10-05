///////////////////////////////////////////////////////////////////////////////
// FILE:          LumascopeZStage.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Z (focus) stage of the Etaluma Lumascope, controlled through LumascopeHub.
//-----------------------------------------------------------------------------

#pragma once

#include "DeviceBase.h"

class LumascopeHub;

class LumascopeZStage : public CStageBase<LumascopeZStage>
{
public:
   LumascopeZStage();
   ~LumascopeZStage();

   int Initialize();
   int Shutdown();
   void GetName(char* name) const;
   bool Busy();

   int SetPositionUm(double positionUm);
   int GetPositionUm(double& positionUm);
   int SetPositionSteps(long steps);
   int GetPositionSteps(long& steps);
   int SetOrigin();
   int GetLimits(double& lowerLimitUm, double& upperLimitUm);
   int Home();
   int Stop();
   int IsStageSequenceable(bool& isSequenceable) const;
   bool IsContinuousFocusDrive() const;

   int OnBacklashCompensation(MM::PropertyBase* property, MM::ActionType actionType);
   int OnBacklashDistance(MM::PropertyBase* property, MM::ActionType actionType);

private:
   int MoveToTargetSteps(long targetSteps);

   LumascopeHub* hub_;
   bool initialized_;
   bool backlashCompensationEnabled_;
   double backlashDistanceUm_;
};
