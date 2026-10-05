///////////////////////////////////////////////////////////////////////////////
// FILE:          LumascopePositionStore.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Saves the stage position reference between sessions so the
//                stage does not need to be homed every time Micro-Manager starts.
//-----------------------------------------------------------------------------

#include "LumascopePositionStore.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace
{
   const char* const AxisNames[LumascopeAxisCount] = { "X", "Y", "Z" };
   const char* const CleanShutdownKey = "CleanShutdown";

   const char PathSeparator =
#ifdef _WIN32
      '\\';
#else
      '/';
#endif

   void CreateDirectoryIfMissing(const std::string& directoryPath)
   {
#ifdef _WIN32
      _mkdir(directoryPath.c_str());
#else
      mkdir(directoryPath.c_str(), 0755);
#endif
   }

   std::string GetStorageDirectory()
   {
#ifdef _WIN32
      const char* baseDirectory = std::getenv("LOCALAPPDATA");
      const std::string applicationFolder = "Micro-Manager";
#else
      const char* baseDirectory = std::getenv("HOME");
      const std::string applicationFolder = ".micro-manager";
#endif
      if (baseDirectory == nullptr || baseDirectory[0] == '\0')
         return std::string();

      const std::string applicationDirectory = std::string(baseDirectory) + PathSeparator + applicationFolder;
      const std::string lumascopeDirectory = applicationDirectory + PathSeparator + "Lumascope";
      CreateDirectoryIfMissing(applicationDirectory);
      CreateDirectoryIfMissing(lumascopeDirectory);
      return lumascopeDirectory;
   }

   std::string SanitizeForFileName(const std::string& text)
   {
      std::string sanitized;
      for (char character : text)
      {
         if (std::isalnum(static_cast<unsigned char>(character)) || character == '-' || character == '_')
            sanitized += character;
      }
      return sanitized.empty() ? std::string("Unknown") : sanitized;
   }
}

std::string GetLumascopePositionRecordPath(const std::string& controllerSerialNumber)
{
   const std::string storageDirectory = GetStorageDirectory();
   if (storageDirectory.empty())
      return std::string();
   return storageDirectory + PathSeparator + "Position-" + SanitizeForFileName(controllerSerialNumber) + ".txt";
}

bool LoadLumascopePositionRecord(const std::string& controllerSerialNumber, LumascopePositionRecord& record)
{
   const std::string recordPath = GetLumascopePositionRecordPath(controllerSerialNumber);
   if (recordPath.empty())
      return false;

   std::ifstream recordFile(recordPath);
   if (!recordFile)
      return false;

   LumascopePositionRecord loadedRecord;
   bool foundAxis[LumascopeAxisCount] = { false, false, false };
   bool foundCleanShutdown = false;

   std::string line;
   while (std::getline(recordFile, line))
   {
      std::istringstream lineStream(line);
      std::string key;
      lineStream >> key;

      if (key == CleanShutdownKey)
      {
         int cleanShutdownValue = 0;
         if (lineStream >> cleanShutdownValue)
         {
            loadedRecord.cleanShutdown = (cleanShutdownValue != 0);
            foundCleanShutdown = true;
         }
         continue;
      }

      for (int axisIndex = 0; axisIndex < LumascopeAxisCount; ++axisIndex)
      {
         if (key != AxisNames[axisIndex])
            continue;

         int referenceStateValue = 0;
         LumascopeAxisRecord& axisRecord = loadedRecord.axes[axisIndex];
         if (lineStream >> referenceStateValue >> axisRecord.offsetSteps >> axisRecord.lastCounterSteps)
         {
            axisRecord.referenceState = static_cast<LumascopeReferenceState>(referenceStateValue);
            foundAxis[axisIndex] = true;
         }
      }
   }

   if (!foundCleanShutdown)
      return false;
   for (bool axisWasFound : foundAxis)
   {
      if (!axisWasFound)
         return false;
   }

   record = loadedRecord;
   return true;
}

bool SaveLumascopePositionRecord(const std::string& controllerSerialNumber, const LumascopePositionRecord& record)
{
   const std::string recordPath = GetLumascopePositionRecordPath(controllerSerialNumber);
   if (recordPath.empty())
      return false;

   std::ofstream recordFile(recordPath, std::ios::trunc);
   if (!recordFile)
      return false;

   recordFile << CleanShutdownKey << ' ' << (record.cleanShutdown ? 1 : 0) << '\n';
   for (int axisIndex = 0; axisIndex < LumascopeAxisCount; ++axisIndex)
   {
      const LumascopeAxisRecord& axisRecord = record.axes[axisIndex];
      recordFile << AxisNames[axisIndex] << ' '
         << static_cast<int>(axisRecord.referenceState) << ' '
         << axisRecord.offsetSteps << ' '
         << axisRecord.lastCounterSteps << '\n';
   }
   return static_cast<bool>(recordFile);
}
