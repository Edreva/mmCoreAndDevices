///////////////////////////////////////////////////////////////////////////////
// FILE:          LumascopeXYStage.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   XY stage of the Etaluma Lumascope, controlled through LumascopeHub.
//-----------------------------------------------------------------------------

#pragma once

#include "DeviceBase.h"

class LumascopeHub;

class LumascopeXYStage : public CXYStageBase<LumascopeXYStage>
{
public:
   LumascopeXYStage();
   ~LumascopeXYStage();

   int Initialize();
   int Shutdown();
   void GetName(char* name) const;
   bool Busy();

   int SetPositionSteps(long xSteps, long ySteps);
   int GetPositionSteps(long& xSteps, long& ySteps);
   int Home();
   int Stop();
   int SetOrigin();
   int GetLimitsUm(double& xMinimumUm, double& xMaximumUm, double& yMinimumUm, double& yMaximumUm);
   int GetStepLimits(long& xMinimumSteps, long& xMaximumSteps, long& yMinimumSteps, long& yMaximumSteps);
   double GetStepSizeXUm();
   double GetStepSizeYUm();
   int IsXYStageSequenceable(bool& isSequenceable) const;

private:
   LumascopeHub* hub_;
   bool initialized_;
};
