#include "Display.h"

#include "ModuleInterface.h"

#include <cstring>

MODULE_API void InitializeModuleData()
{
    RegisterDevice(g_DisplayName, MM::SLMDevice, "Description for My New Device");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
    if (deviceName == 0)
        return 0;

    if (std::strcmp(deviceName, g_DisplayName) == 0)
    {
        return new Display();
    }
    return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
    delete pDevice;
}


Display::Display() : CSLMBase<Display>()
{

}

Display::~Display()
{
    Shutdown();
}

void Display::GetName(char* name) const
{
    CDeviceUtils::CopyLimitedString(name, g_DisplayName);
}

int Display::Initialize()
{
    return DEVICE_OK;
}

int Display::Shutdown()
{
    return DEVICE_OK;
}

bool Display::Busy()
{
    return false;
}