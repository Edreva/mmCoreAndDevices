///////////////////////////////////////////////////////////////////////////////
// FILE:          LumascopeProtocol.h
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Serial protocol constants for the Etaluma Lumascope motor
//                controller. Ported from LumaViewPro motorboard.py
//                (Copyright (c) 2024 Etaluma, Inc., MIT License).
//                Keep every protocol detail in this file so that upstream
//                firmware changes only need to be mirrored here.
//-----------------------------------------------------------------------------

#pragma once

namespace LumascopeProtocol
{
   // Substring of the Windows hardware ID of the motor controller's Raspberry Pi Pico.
   // The LED controller is a different device (VID 0x0424, PID 0x704C).
   const char* const MotorControllerHardwareIdFragment = "VID_2E8A&PID_0005";

   const char* const CommandTerminator = "\n";

   // Ctrl-D followed by newline. Forces MicroPython to soft reset if the
   // firmware failed to start; running firmware replies with a single complaint line.
   const char* const WakeSequence = "\x04\n";

   const char* const CommandFullInfo = "FULLINFO";
   const char* const CommandInfo = "INFO";
   const char* const CommandHomeAll = "HOME";
   const char* const CommandHomeZ = "ZHOME";
   const char* const CommandTargetWritePrefix = "TARGET_W";
   const char* const CommandTargetReadPrefix = "TARGET_R";
   const char* const CommandActualReadPrefix = "ACTUAL_R";
   const char* const CommandStatusReadPrefix = "STATUS_R";

   const char* const FullInfoModelToken = "Model:";
   const char* const FullInfoSerialToken = "Serial:";
   const char TurretModelSuffix = 'T';
   const char* const HomeAllCompleteMarker = "XYZ home complete";

   const char AxisX = 'X';
   const char AxisY = 'Y';
   const char AxisZ = 'Z';
   // Same order as the axes in LumascopePositionRecord
   const char AxesInRecordOrder[] = { AxisX, AxisY, AxisZ };

   // Bit 9 of the Trinamic RAMP_STAT register (motorboard.py tests index 22 of the 32-character bit string)
   const int StatusPositionReachedBit = 9;

   // Negative step targets are transmitted as unsigned 32-bit two's complement
   const long long TwosComplementModulus = 0x100000000LL;
   const long long SignedThirtyTwoBitMaximum = 0x7FFFFFFFLL;

   // 2.54 mm lead screw, 200 full steps per revolution, 256 microsteps per full step
   const double XyMicrostepsPerMillimeter = 20157.0;
   // 0.30 mm lead screw, 200 full steps per revolution, 256 microsteps per full step
   const double ZMicrostepsPerMillimeter = 170667.0;

   const double XyStepSizeUm = 1000.0 / XyMicrostepsPerMillimeter;
   const double ZStepSizeUm = 1000.0 / ZMicrostepsPerMillimeter;

   const double XMinimumUm = 0.0;
   const double XMaximumUm = 120000.0;
   const double YMinimumUm = 0.0;
   const double YMaximumUm = 80000.0;
   const double ZMinimumUm = 0.0;
   const double ZMaximumUm = 14000.0;

   const long XMinimumSteps = 0;
   const long XMaximumSteps = static_cast<long>(XMaximumUm * XyMicrostepsPerMillimeter / 1000.0);
   const long YMinimumSteps = 0;
   const long YMaximumSteps = static_cast<long>(YMaximumUm * XyMicrostepsPerMillimeter / 1000.0);
   const long ZMinimumSteps = 0;
   const long ZMaximumSteps = static_cast<long>(ZMaximumUm * ZMicrostepsPerMillimeter / 1000.0);

   const double DefaultZBacklashUm = 25.0;
   // Overshoot is skipped for targets this close to the bottom of travel, as in motorboard.py
   const double ZBacklashMinimumClearanceUm = 50.0;
   const double MaximumZBacklashUm = 200.0;

   // Replies normally arrive within a few milliseconds. Busy-yield for this long
   // before sleeping, because a 1 ms sleep lasts about 15 ms on Windows.
   const long ResponseSpinWaitMilliseconds = 50;
   const long DefaultResponseTimeoutMilliseconds = 2000;
   const long WakeResponseTimeoutMilliseconds = 1000;
   const long HomingTimeoutMilliseconds = 120000;
   const long MoveCompletionTimeoutMilliseconds = 30000;
   const long MovePollIntervalMilliseconds = 5;
   const int IdentityQueryAttemptsBeforeWake = 1;
   const int IdentityQueryAttemptsAfterWake = 3;
}
