#pragma once

#include "DeviceBase.h"
#include "GenericSLM.h"

const char* g_DisplayName = "TestDisplay";

class Display : public GenericSLM
{
public:
    Display();
    ~Display();

    // MMDevice API
    int Initialize();
    int Shutdown();
    void GetName(char* name) const;
    bool Busy();

    // GenericSLM API


};