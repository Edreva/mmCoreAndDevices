//
//
//

#include "DisplayIlluminator.h"

#include "Monitors.h"
#include "SLMWindowThread.h"
#include "SleepBlocker.h"
#include "OffscreenBuffer.h"

#include "ModuleInterface.h"
#include "DeviceUtils.h"

#include <Windows.h>

#include <boost/lexical_cast.hpp>

#include <algorithm>

const char* g_DisplayIllumName = "DisplayIlluminator";
const char* g_PropName_GraphicsPort = "GraphicsPort";
const char* g_PropName_TestModeWidth = "TestModeWidth";
const char* g_PropName_TestModeHeight = "TestModeHeight";
const char* g_PropName_DisplayHeightPx = "DisplayHeight_pixels";
const char* g_PropName_DisplayWidthPx = "DisplayWidth_pixels";
const char* g_PropName_PixelSize = "PixelSize_um";
const char* g_PropName_ActiveImage = "ActiveImage";
const char* g_PropName_DpcPatternCount = "DpcPatternCount";
const char* g_PropName_DpcWidth = "DpcWidth";
const char* g_PropName_DpcHeight = "DpcHeight";
const char* g_PropName_DpcInnerWidth = "DpcInnerWidth";
const char* g_PropName_DpcInnerHeight = "DpcInnerHeight";
const char* g_PropName_PcWidth = "PcWidth";
const char* g_PropName_PcHeight = "PcHeight";
const char* g_PropName_PcInnerWidth = "PcInnerWidth";
const char* g_PropName_PcInnerHeight = "PcInnerHeight";
const char* g_PropName_DfWidth = "DfWidth";
const char* g_PropName_DfHeight = "DfHeight";
const char* g_PropName_DfInnerWidth = "DfInnerWidth";
const char* g_PropName_DfInnerHeight = "DfInnerHeight";
const char* g_PropName_BfWidth = "BfWidth";
const char* g_PropName_BfHeight = "BfHeight";
const char* g_PropName_Rotation = "Rotation";
const char* g_PropName_CenterX = "CenterX";
const char* g_PropName_CenterY = "CenterY";
const char* g_PropName_MonoColor = "MonoColor";
const char* g_PropName_RbOuterColor = "RheinbergOuterColor";


enum {
	ERR_INVALID_TESTMODE_SIZE = 20000,
	ERR_CANNOT_DETACH,
	ERR_CANNOT_ATTACH,
	ERR_OFFSCREEN_BUFFER_UNAVAILABLE,
	ERR_NO_DISPLAY_CONTEXT
};


MODULE_API void InitializeModuleData()
{
	RegisterDevice(g_DisplayIllumName, MM::SLMDevice,
		"Display screen controlled through computer graphics output");
}


MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
	if (deviceName == 0)
		return 0;

	if (strcmp(deviceName, g_DisplayIllumName) == 0)
	{
		DisplayIlluminator* pDisplayIlluminator = new DisplayIlluminator(g_DisplayIllumName);
		return pDisplayIlluminator;
	}

	return 0;
}


MODULE_API void DeleteDevice(MM::Device* pDevice)
{
	delete pDevice;
}

DisplayIlluminator::DisplayIlluminator(const char* name) : 
	name_(name),
	width_(0),
	height_(0),
    invert_(false),
    shouldBlitInverted_(false),
    sleepBlocker_(0),
    windowThread_(0),
    monoColor_("00FF00"),
    pixelSize_(0.0),
    exposureMs_(0.0),
    centerX_(0),
    centerY_(0),
    rotation_(0),
    dpcPatternCount_(0),
    dpcHeight_(0),
    dpcWidth_(0),
    dpcInnerWidth_(0),
    dpcInnerHeight_(0),
    pcHeight_(0),
    pcWidth_(0),
    pcInnerHeight_(0),
    pcInnerWidth_(0),
    dfHeight_(0),
    dfWidth_(0),
    dfInnerHeight_(0),
    dfInnerWidth_(0),
    bfHeight_(0),
    bfWidth_(0),
    rbOuterColor_("FF0000"),
    imageName_("Off")
{
   InitializeDefaultErrorMessages();
   SetErrorText(ERR_INVALID_TESTMODE_SIZE,
		 "Invalid test mode window size");
   SetErrorText(ERR_CANNOT_DETACH,
		 "Failed to detach monitor from desktop");
   SetErrorText(ERR_CANNOT_ATTACH,
		 "Failed to attach monitor to desktop");
   SetErrorText(ERR_OFFSCREEN_BUFFER_UNAVAILABLE,
		 "Cannot set image (device uninitialized?)");

   availableMonitors_ = GetMonitorNames(true, false);

   // Pre-init Properties

   CreateStringProperty(g_PropName_GraphicsPort, "TestMode", false, 0, true);
   AddAllowedValue(g_PropName_GraphicsPort, "TestMode", 0);

   // Map available displays 0 thru N to property data 1 thru N + 1
   for (unsigned i = 0; i < availableMonitors_.size(); ++i)
   {
	   AddAllowedValue(g_PropName_GraphicsPort,
		   availableMonitors_[i].c_str(), i + 1);
   }

   CreateIntegerProperty(g_PropName_TestModeWidth, 128, false, 0, true);
   CreateIntegerProperty(g_PropName_TestModeHeight, 128, false, 0, true);
}


DisplayIlluminator::~DisplayIlluminator()
{
	Shutdown();
}


void DisplayIlluminator::GetName(char* name) const
{
	CDeviceUtils::CopyLimitedString(name, name_.c_str());
}

int DisplayIlluminator::Initialize()
{
    Shutdown();

    //
    // Create post-init properties
    //

    int err = CreateStringProperty(MM::g_Keyword_Name, name_.c_str(), true);
    if (err != DEVICE_OK)
        return err;
    err = CreateStringProperty(MM::g_Keyword_Description,
        "Display screen controlled through computer graphics output", true);
    if (err != DEVICE_OK)
        return err;

    // TODO: Implement or delete if unnecessary 
    //err = CreateStringProperty(g_PropName_Inversion, inversionStr_.c_str(), false,
    //    new CPropertyAction(this, &DisplayIlluminator::OnInversion));
    //if (err != DEVICE_OK)
    //    return err;
    //AddAllowedValue(g_PropName_Inversion, "Off", 0);
    //AddAllowedValue(g_PropName_Inversion, "On", 1);

    //err = CreateStringProperty(g_PropName_MonoColor, monoColorStr_.c_str(), false,
    //    new CPropertyAction(this, &DisplayIlluminator::OnMonochromeColor));
    //if (err != DEVICE_OK)
    //    return err;
    //AddAllowedValue(g_PropName_MonoColor, "White", SLM_COLOR_WHITE);
    //AddAllowedValue(g_PropName_MonoColor, "Red", SLM_COLOR_RED);
    //AddAllowedValue(g_PropName_MonoColor, "Green", SLM_COLOR_GREEN);
    //AddAllowedValue(g_PropName_MonoColor, "Blue", SLM_COLOR_BLUE);
    //AddAllowedValue(g_PropName_MonoColor, "Cyan", SLM_COLOR_CYAN);
    //AddAllowedValue(g_PropName_MonoColor, "Magenta", SLM_COLOR_MAGENTA);
    //AddAllowedValue(g_PropName_MonoColor, "Yellow", SLM_COLOR_YELLOW);

    CreateFloatProperty(g_PropName_PixelSize, pixelSize_, false); // User entered pixel size. Future use when real image sizes are needed.

    err = InitializeMonitor();
    if (err != DEVICE_OK)
        return err;

    err = InitializeImages();
    if (err != DEVICE_OK)
        return err;

    return DEVICE_OK;
}


int DisplayIlluminator::InitializeMonitor()
{
    // Set up the monitor and window
    long graphicsPortIndex;
    int err = GetCurrentPropertyData(g_PropName_GraphicsPort, graphicsPortIndex);
    if (err != DEVICE_OK)
        return err;

    LONG x, y, w, h;
    std::vector<std::string> desktopMonitors;
    if (graphicsPortIndex == 0) // Test mode
    {
        err = GetProperty(g_PropName_TestModeWidth, w);
        if (err != DEVICE_OK)
            return err;
        err = GetProperty(g_PropName_TestModeHeight, h);
        if (err != DEVICE_OK)
            return err;

        if (w < 1 || h < 1)
            return ERR_INVALID_TESTMODE_SIZE;

        // The top-left of the primary desktop monior is (0, 0), so this is a
        // safe position for the window
        x = y = 100;

        desktopMonitors = GetMonitorNames(false, true);
    }
    else // Real monitor
    {
        // Map property data 1 thru N + 1 to available monitors 0 thru N
        monitorName_ = availableMonitors_[graphicsPortIndex - 1];

        if (!DetachMonitorFromDesktop(monitorName_))
        {
            monitorName_ = "";
            return ERR_CANNOT_DETACH;
        }

        desktopMonitors = GetMonitorNames(false, true);

        LONG posX, posY;
        GetRightmostMonitorTopRight(desktopMonitors, posX, posY);

        if (!AttachMonitorToDesktop(monitorName_, posX, posY))
        {
            monitorName_ = "";
            return ERR_CANNOT_ATTACH;
        }

        GetMonitorRect(monitorName_, x, y, w, h);
    }

    std::string windowTitle = "MM_DisplayIlluminator " +
        boost::lexical_cast<std::string>(w) + "x" +
        boost::lexical_cast<std::string>(h) + " [" +
        (monitorName_.empty() ? "Test Mode" : monitorName_) +
        "]";

    windowThread_ = new SLMWindowThread(monitorName_.empty(),
        windowTitle, x, y, w, h);
    windowThread_->Show();

    RECT mouseClipRect;
    if (GetBoundingRect(desktopMonitors, mouseClipRect))
        sleepBlocker_ = new SleepBlocker(mouseClipRect);
    else
        sleepBlocker_ = new SleepBlocker();
    sleepBlocker_->Start();

    width_ = w;
    height_ = h;

    CreateIntegerProperty(g_PropName_DisplayHeightPx, height_, true);
    CreateIntegerProperty(g_PropName_DisplayWidthPx, width_, true);

    return DEVICE_OK;
}


int DisplayIlluminator::InitializeImages()
{
    // Set initial values
    unsigned int initialAnnulusThickness = 20;

    centerX_ = static_cast<unsigned int>(round(width_ / 2));
    centerY_ = static_cast<unsigned int>(round(height_ / 2));
    dpcWidth_ = static_cast<unsigned int>(round(std::min(height_, width_)));
    dpcHeight_ = dpcWidth_;
    bfWidth_ = dpcWidth_ - initialAnnulusThickness;
    bfHeight_ = bfWidth_;
    dfWidth_ = dpcWidth_;
    dfHeight_ = dfWidth_;
    pcWidth_ = 0.75 * dpcWidth_; // Arbitrary TODO: update
    pcHeight_ = 0.75 * dpcHeight_;
    pcInnerWidth_ = pcWidth_ - initialAnnulusThickness;
    pcInnerHeight_ = pcHeight_ - initialAnnulusThickness;
    dfInnerWidth_ = dfWidth_ - initialAnnulusThickness;
    dfInnerHeight_ = dfHeight_ - initialAnnulusThickness;
    dpcPatternCount_ = 4;



    CreateIntegerProperty(g_PropName_DpcPatternCount, dpcPatternCount_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnDpcPatternCount));
    CreateIntegerProperty(g_PropName_DpcWidth, dpcWidth_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnDpcWidth));
    CreateIntegerProperty(g_PropName_DpcHeight, dpcHeight_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnDpcHeight));
    CreateIntegerProperty(g_PropName_DpcInnerWidth, dpcInnerWidth_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnDpcInnerWidth));
    CreateIntegerProperty(g_PropName_DpcInnerHeight, dpcInnerHeight_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnDpcInnerHeight));
    CreateIntegerProperty(g_PropName_PcWidth, pcWidth_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnPcWidth));
    CreateIntegerProperty(g_PropName_PcHeight, pcHeight_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnPcHeight));
    CreateIntegerProperty(g_PropName_PcInnerWidth, pcInnerWidth_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnPcInnerWidth));
    CreateIntegerProperty(g_PropName_PcInnerHeight, pcInnerHeight_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnPcInnerHeight));
    CreateIntegerProperty(g_PropName_DfWidth, dfWidth_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnDfWidth));
    CreateIntegerProperty(g_PropName_DfHeight, dfHeight_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnDfHeight));
    CreateIntegerProperty(g_PropName_DfInnerWidth, dfInnerWidth_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnDfInnerWidth));
    CreateIntegerProperty(g_PropName_DfInnerHeight, dfInnerHeight_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnDfInnerHeight));
    CreateIntegerProperty(g_PropName_BfWidth, bfWidth_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnBfWidth));
    CreateIntegerProperty(g_PropName_BfHeight, bfHeight_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnBfHeight));
    CreateIntegerProperty(g_PropName_Rotation, rotation_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnRotation));
    CreateIntegerProperty(g_PropName_CenterX, centerX_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnCenterX));
    CreateIntegerProperty(g_PropName_CenterY, centerY_, false,
        new CPropertyAction(this, &DisplayIlluminator::OnCenterY));
    CreateStringProperty(g_PropName_MonoColor, monoColor_.c_str(), false,
        new CPropertyAction(this, &DisplayIlluminator::OnMonoColor));
    CreateStringProperty(g_PropName_RbOuterColor, rbOuterColor_.c_str(), false,
        new CPropertyAction(this, &DisplayIlluminator::OnRbOuterColor));

    int err = CreateStringProperty(g_PropName_ActiveImage, imageName_.c_str(), false,
        new CPropertyAction(this, &DisplayIlluminator::OnActiveImage));
    if (err != DEVICE_OK)
        return err;

    // Create preset images to select via device property manager
    CreateImages();
    UpdateAllowedImages();

    // Initialise display
    SetImage(images_[imageName_]);
    DisplayImage();

    return DEVICE_OK;
}

// Copied from GenericSLM
int DisplayIlluminator::Shutdown()
{
    width_ = height_ = 0;

    if (sleepBlocker_)
    {
        sleepBlocker_->Stop();
        delete sleepBlocker_;
        sleepBlocker_ = 0;
    }

    if (windowThread_)
    {
        delete windowThread_;
        windowThread_ = 0;
    }

    if (!monitorName_.empty())
    {
        DetachMonitorFromDesktop(monitorName_);
        monitorName_ = "";
    }

    return DEVICE_OK;
}


// Copied from GenericSLM
bool DisplayIlluminator::Busy()
{
    // TODO We _could_ make the wait for vertical sync asynchronous
    // (Make sure first that Projector knows to wait for non-busy)
    return false;
}

unsigned int DisplayIlluminator::GetWidth()
{
    return width_;
}


unsigned int DisplayIlluminator::GetHeight()
{
    return height_;
}


unsigned int DisplayIlluminator::GetNumberOfComponents()
{
    return 3;
}


unsigned int DisplayIlluminator::GetBytesPerPixel()
{
    return 4;
}


[[deprecated("Currently unused")]]
int DisplayIlluminator::SetExposure(double exposureMs)
{
    exposureMs_ = exposureMs;
    return DEVICE_OK;
}


[[deprecated("Currently unused")]]
double DisplayIlluminator::GetExposure()
{
    return exposureMs_;
}


// Copied from GenericSLM
int DisplayIlluminator::SetImage(unsigned char* pixels)
{
    OffscreenBuffer* offscreen = windowThread_->GetOffscreenBuffer();
    if (!offscreen)
        return ERR_OFFSCREEN_BUFFER_UNAVAILABLE;

    offscreen->DrawImage(pixels, monoColor_, invert_);
    return DEVICE_OK;
}


// Copied from GenericSLM
int DisplayIlluminator::SetImage(unsigned int* pixels)
{
    OffscreenBuffer* offscreen = windowThread_->GetOffscreenBuffer();
    if (!offscreen)
        return ERR_OFFSCREEN_BUFFER_UNAVAILABLE;

    offscreen->DrawImage(pixels);
    shouldBlitInverted_ = invert_;
    return DEVICE_OK;
}


// Copied from GenericSLM
int DisplayIlluminator::SetPixelsTo(unsigned char red, unsigned char green, unsigned char blue)
{
    OffscreenBuffer* offscreen = windowThread_->GetOffscreenBuffer();
    if (!offscreen)
        return ERR_OFFSCREEN_BUFFER_UNAVAILABLE;

    unsigned char xorMask = invert_ ? 0xff : 0x00;

    COLORREF color(RGB(red ^ xorMask, green ^ xorMask, blue ^ xorMask));

    offscreen->FillWithColor(color);
    shouldBlitInverted_ = false;
    return DisplayImage();
}


// Copied from GenericSLM
int DisplayIlluminator::SetPixelsTo(unsigned char intensity)
{
    OffscreenBuffer* offscreen = windowThread_->GetOffscreenBuffer();
    if (!offscreen)
        return ERR_OFFSCREEN_BUFFER_UNAVAILABLE;

    intensity ^= (invert_ ? 0xff : 0x00);

    unsigned char redChannel = (unsigned char)strtoul(monoColor_.substr(0, 2).c_str(), nullptr, 16);
    unsigned char greenChannel = (unsigned char)strtoul(monoColor_.substr(2, 2).c_str(), nullptr, 16);
    unsigned char blueChannel = (unsigned char)strtoul(monoColor_.substr(4, 2).c_str(), nullptr, 16);

    COLORREF color(RGB(
        (intensity * redChannel) >> 8, // Accept tiny imprecision for speed
        (intensity * greenChannel) >> 8,
        (intensity * blueChannel) >> 8));

    offscreen->FillWithColor(color);
    shouldBlitInverted_ = false;
    return DisplayImage();
}


// Copied from GenericSLM
int DisplayIlluminator::DisplayImage()
{
    OffscreenBuffer* offscreen = windowThread_->GetOffscreenBuffer();
    if (!offscreen)
        return ERR_OFFSCREEN_BUFFER_UNAVAILABLE;

    DWORD op = shouldBlitInverted_ ? NOTSRCCOPY : SRCCOPY;

    int ret = refreshWaiter_.WaitForVerticalBlank();
    if (ret != DEVICE_OK)
    {
        return ret;
    }
    HDC onscreenDC = windowThread_->GetDC();
    if (onscreenDC == NULL)
    {
        return ERR_NO_DISPLAY_CONTEXT;
    }
    DWORD result = offscreen->BlitTo(onscreenDC, op);
    // Wait until the image is actually displayed
    ret = refreshWaiter_.WaitForVerticalBlank();
    if (ret != DEVICE_OK)
    {
        return ret;
    }
    // TODO use FormatMessage function to generate error string

    // If we do not release the HDC< we'll run out of available contexts
    // Alternatively, we could possibly store one and re-use...
    windowThread_->ReleaseDC(onscreenDC);
    return (int)result;
}


int DisplayIlluminator::OnActiveImage(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        pProp->Set(imageName_.c_str());
    }
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(imageName_);
        SetImage(images_[imageName_]);
        DisplayImage();
    }

    return DEVICE_OK;
}

int DisplayIlluminator::OnImagePropUpdate(MM::PropertyBase* pProp, MM::ActionType eAct, unsigned& prop)
{
    if (eAct == MM::BeforeGet)
    {
        pProp->Set((long)prop);
    }
    else if (eAct == MM::AfterSet)
    {
        long tempLong;
        pProp->Get(tempLong);
        prop = (unsigned) tempLong;

        UpdateImages();
        SetImage(images_[imageName_]);
        DisplayImage();
    }

    return DEVICE_OK;
}

int DisplayIlluminator::OnImagePropUpdate(MM::PropertyBase* pProp, MM::ActionType eAct, unsigned& prop, std::set<ImageTypes> imagesToUpdate)
{
    if (eAct == MM::BeforeGet)
    {
        pProp->Set((long)prop);
    }
    else if (eAct == MM::AfterSet)
    {
        long tempLong;
        pProp->Get(tempLong);
        prop = (unsigned)tempLong;

        UpdateImages(imagesToUpdate);
        SetImage(images_[imageName_]);
        DisplayImage();
    }

    return DEVICE_OK;
}

int DisplayIlluminator::OnImagePropUpdate(MM::PropertyBase* pProp, MM::ActionType eAct, std::string& prop)
{
    if (eAct == MM::BeforeGet)
    {
        pProp->Set(prop.c_str());
    }
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(prop);

        UpdateImages();
        SetImage(images_[imageName_]);
        DisplayImage();
    }

    return DEVICE_OK;
}

int DisplayIlluminator::OnImagePropUpdate(MM::PropertyBase* pProp, MM::ActionType eAct, std::string& prop, std::set<ImageTypes> imagesToUpdate)
{
    if (eAct == MM::BeforeGet)
    {
        pProp->Set(prop.c_str());
    }
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(prop);

        UpdateImages(imagesToUpdate);
        SetImage(images_[imageName_]);
        DisplayImage();
    }

    return DEVICE_OK;
}

int DisplayIlluminator::OnDpcWidth(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, dpcWidth_, { DPC });
}
int DisplayIlluminator::OnDpcHeight(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, dpcHeight_, { DPC });
}
int DisplayIlluminator::OnDpcInnerWidth(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, dpcInnerWidth_, { DPC });
}
int DisplayIlluminator::OnDpcInnerHeight(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, dpcInnerHeight_, { DPC });
}
int DisplayIlluminator::OnPcWidth(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, pcWidth_, { PC });
}
int DisplayIlluminator::OnPcHeight(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, pcHeight_, { PC });
}
int DisplayIlluminator::OnPcInnerWidth(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, pcInnerWidth_, { PC });
}
int DisplayIlluminator::OnPcInnerHeight(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, pcInnerHeight_, { PC });
}
int DisplayIlluminator::OnDfWidth(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, dfWidth_, { DF, RB });
}
int DisplayIlluminator::OnDfHeight(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, dfHeight_, { DF, RB });
}
int DisplayIlluminator::OnDfInnerWidth(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, dfInnerWidth_, { DF, RB });
}
int DisplayIlluminator::OnDfInnerHeight(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, dfInnerHeight_, { DF, RB });
}
int DisplayIlluminator::OnBfWidth(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, bfWidth_, { BF, RB });
}
int DisplayIlluminator::OnBfHeight(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, bfHeight_, { BF, RB });
}
int DisplayIlluminator::OnRotation(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, rotation_);
}
int DisplayIlluminator::OnCenterX(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, centerX_);
}
int DisplayIlluminator::OnCenterY(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, centerY_);
}
int DisplayIlluminator::OnMonoColor(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, monoColor_);
}
int DisplayIlluminator::OnRbOuterColor(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return OnImagePropUpdate(pProp, eAct, rbOuterColor_, { RB });
}
int DisplayIlluminator::OnDpcPatternCount(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::AfterSet)
    {
        UpdateAllowedImages();
        imageName_ = "Off";
    }
    return OnImagePropUpdate(pProp, eAct, dpcPatternCount_, { DPC });
}

void DisplayIlluminator::UpdateAllowedImages()
{
    ClearAllowedValues(g_PropName_ActiveImage);
    AddAllowedValue(g_PropName_ActiveImage, "Off");
    for (int i = 0; i < dpcPatternCount_; i++)
    {
        std::string imageName = "DPC" + std::to_string(i + 1);
        AddAllowedValue(g_PropName_ActiveImage, imageName.c_str());
    }
    // TODO: Rework this to allow better granularity of available images
    AddAllowedValue(g_PropName_ActiveImage, "BF");
    AddAllowedValue(g_PropName_ActiveImage, "DF");
    AddAllowedValue(g_PropName_ActiveImage, "PC");
    AddAllowedValue(g_PropName_ActiveImage, "RB");
}

void DisplayIlluminator::CreateDpcImages()
{
    for (int i = 0; i < dpcPatternCount_; i++)
    {
        SourcePatternRenderer dpcPatternRenderer(width_, height_);
        std::string imageName = "DPC" + std::to_string(i + 1);
        dpcPatternRenderer.RenderHalfOval(centerX_, centerY_, dpcWidth_, dpcHeight_, rotation_, i * 360.0 / dpcPatternCount_, monoColor_);
        images_[imageName] = dpcPatternRenderer;
    }
}

void DisplayIlluminator::UpdateDpcImages()
{
    for (int i = 0; i < dpcPatternCount_; i++)
    {
        std::string imageName = "DPC" + std::to_string(i + 1);
        images_[imageName].ClearFrame();
        if (dpcInnerHeight_ <= 0 && dpcInnerWidth_ <= 0)
        {
            images_[imageName].RenderHalfOval(centerX_, centerY_, dpcWidth_, dpcHeight_, rotation_, i * 360.0 / dpcPatternCount_, monoColor_);
        }
        else
        {
            images_[imageName].RenderEllipticalHalfAnnulus(centerX_, centerY_, dpcWidth_, dpcHeight_, dpcInnerWidth_, dpcInnerHeight_, rotation_, i * 360.0 / dpcPatternCount_, monoColor_);
        }

    }
}

void DisplayIlluminator::CreatePcImage()
{
    SourcePatternRenderer pcRenderer(width_, height_);
    pcRenderer.RenderEllipticalAnnulus(centerX_, centerY_, pcWidth_, pcHeight_, pcInnerWidth_, pcInnerHeight_, rotation_, monoColor_);
    images_["PC"] = pcRenderer;
}

void DisplayIlluminator::UpdatePcImage()
{
    images_["PC"].ClearFrame();
    images_["PC"].RenderEllipticalAnnulus(centerX_, centerY_, pcWidth_, pcHeight_, pcInnerWidth_, pcInnerHeight_, rotation_, monoColor_);
}

void DisplayIlluminator::CreateDfImage()
{
    SourcePatternRenderer dfRenderer(width_, height_);
    dfRenderer.RenderEllipticalAnnulus(centerX_, centerY_, dfWidth_, dfHeight_, dfInnerWidth_, dfInnerHeight_, rotation_, monoColor_);
    images_["DF"] = dfRenderer;
}

void DisplayIlluminator::UpdateDfImage()
{
    images_["DF"].ClearFrame();
    images_["DF"].RenderEllipticalAnnulus(centerX_, centerY_, dfWidth_, dfHeight_, dfInnerWidth_, dfInnerHeight_, rotation_, monoColor_);
}

void DisplayIlluminator::CreateBfImage()
{
    SourcePatternRenderer bfRenderer(width_, height_);
    bfRenderer.RenderOval(centerX_, centerY_, bfWidth_, bfHeight_, rotation_, monoColor_);
    images_["BF"] = bfRenderer;
}

void DisplayIlluminator::UpdateBfImage()
{
    images_["BF"].ClearFrame();
    images_["BF"].RenderOval(centerX_, centerY_, bfWidth_, bfHeight_, rotation_, monoColor_);
}

void DisplayIlluminator::CreateRbImage()
{
    SourcePatternRenderer rbRenderer(width_, height_);
    rbRenderer.RenderOval(centerX_, centerY_, dfWidth_, dfHeight_, dfInnerWidth_, dfInnerHeight_, rotation_, monoColor_, rbOuterColor_);
    images_["RB"] = rbRenderer;
}

void DisplayIlluminator::UpdateRbImage()
{
    images_["RB"].ClearFrame();
    images_["RB"].RenderOval(centerX_, centerY_, dfWidth_, dfHeight_, dfInnerWidth_, dfInnerHeight_, rotation_, monoColor_, rbOuterColor_);
}

void DisplayIlluminator::CreateImages()
{
    images_["Off"] = SourcePatternRenderer(width_, height_);
    CreateBfImage();
    CreateDpcImages();
    CreatePcImage();
    CreateDfImage();
    CreateRbImage();
}

void DisplayIlluminator::UpdateImages(std::set<ImageTypes> imagesToUpdate) 
{
    if (imagesToUpdate.count(BF) > 0) { UpdateBfImage(); }
    if (imagesToUpdate.count(DPC) > 0) { UpdateDpcImages(); }
    if (imagesToUpdate.count(RB) > 0) { UpdateRbImage(); }
    if (imagesToUpdate.count(PC) > 0) { UpdatePcImage(); }
    if (imagesToUpdate.count(DF) > 0) { UpdateDfImage(); }
}

void DisplayIlluminator::UpdateImages()
{
    UpdateBfImage();
    UpdateDpcImages();
    UpdatePcImage();
    UpdateDfImage();
    UpdateRbImage();
}

int DisplayIlluminator::SetImage(SourcePatternRenderer imageRenderer)
{
    return this->SetImage(imageRenderer.getImageAsArray());
}


