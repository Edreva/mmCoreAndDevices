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

// Map prop name to member var ptr and array of imagetypes enum to update when changed
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
    pixelSize_(0.0),
    exposureMs_(0.0)

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

    CreateFloatProperty(g_PropName_PixelSize, pixelSize_, false); // User entered pixel size. Future use when real image sizes are needed.

    err = InitializeDisplay();
    if (err != DEVICE_OK)
        return err;

    err = InitializeImages();
    if (err != DEVICE_OK)
        return err;

    return DEVICE_OK;
}


int DisplayIlluminator::InitializeDisplay()
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
    // Set startup values
    unsigned int initialAnnulusThickness = 20;
    centerX = static_cast<unsigned int>(round(width_ / 2));
    centerY = static_cast<unsigned int>(round(height_ / 2));
    dpcWidth = static_cast<unsigned int>(round(std::min(height_, width_)));
    dpcHeight = dpcWidth;
    bfWidth = dpcWidth - initialAnnulusThickness;
    bfHeight = bfWidth;
    dfWidth = dpcWidth;
    dfHeight = dfWidth;
    pcWidth = 0.75 * dpcWidth; // Arbitrary TODO: update
    pcHeight = 0.75 * dpcHeight;
    pcInnerWidth = pcWidth - initialAnnulusThickness;
    pcInnerHeight = pcHeight - initialAnnulusThickness;
    dfInnerWidth = dfWidth - initialAnnulusThickness;
    dfInnerHeight = dfHeight - initialAnnulusThickness;
    dpcPatternCount = 4;
    activeImage = "Off";
    monoColor = "00FF00";
    rbOuterColor = "FF0000";
    rbInnerColor = "0000FF";

    CreateProperty(g_PropName_ActiveImage, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_BfHeight, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_BfWidth, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_DfHeight, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_DfWidth, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_DfInnerHeight, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_DfInnerWidth, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_PcHeight, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_PcWidth, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_PcInnerHeight, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_PcInnerWidth, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_DpcHeight, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_DpcWidth, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_DpcInnerHeight, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_DpcInnerWidth, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_DpcPatternCount, false, new CPropertyAction(this, &DisplayIlluminator::OnDpcPatternCount));
    CreateProperty(g_PropName_CenterX, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_CenterY, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_Rotation, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_MonoColor, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_RbOuterColor, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));
    CreateProperty(g_PropName_RbInnerColor, false, new CPropertyAction(this, &DisplayIlluminator::OnImagePropUpdate));

    // Create preset images to select via device property manager
    CreateImages();
    UpdateAllowedImages();

    // Initialise display
    BlitActiveImageToBuffer();
    DisplayImage();

    return DEVICE_OK;
}

// Generic version that uses the variant associated with propName in imagePropertyModeMap to dispatch to the correct CreateProperty overload
void DisplayIlluminator::CreateProperty(const char* propName, bool readOnly, MM::ActionFunctor* pAct, bool isPreInitProperty)
{
    boost::apply_visitor([this, propName, readOnly, pAct, isPreInitProperty](auto&& value)
        {
            this->CreateProperty(propName, *value, readOnly, pAct, isPreInitProperty);
        }, imagePropertyModeMap.at(propName).propertyValuePtr);
};

void DisplayIlluminator::CreateProperty(const char* propName, std::string initialValue, bool readOnly, MM::ActionFunctor* pAct, bool isPreInitProperty)
{
    CreateStringProperty(propName, initialValue.c_str(), readOnly, pAct, isPreInitProperty);
};

void DisplayIlluminator::CreateProperty(const char* propName, unsigned initialValue, bool readOnly, MM::ActionFunctor* pAct, bool isPreInitProperty)
{
    CreateIntegerProperty(propName, initialValue, readOnly, pAct, isPreInitProperty);
};


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

    offscreen->DrawImage(pixels, monoColor, invert_);
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

	// Convert hex string to RGB
    unsigned char redChannel = (unsigned char)strtoul(monoColor.substr(0, 2).c_str(), nullptr, 16);
    unsigned char greenChannel = (unsigned char)strtoul(monoColor.substr(2, 2).c_str(), nullptr, 16);
    unsigned char blueChannel = (unsigned char)strtoul(monoColor.substr(4, 2).c_str(), nullptr, 16);

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

void DisplayIlluminator::SetImageProperty(MM::PropertyBase* pProp)
{
    boost::apply_visitor([this, pProp](auto&& value)
        {
           SetImageProperty(pProp, value);
        }, imagePropertyModeMap.at(pProp->GetName()).propertyValuePtr);

}

void DisplayIlluminator::SetImageProperty(MM::PropertyBase* pProp, unsigned* propertyValuePtr)
{
   pProp->Set(static_cast<long>(*propertyValuePtr));
}

void DisplayIlluminator::SetImageProperty(MM::PropertyBase* pProp, std::string* propertyValuePtr)
{
    pProp->Set(propertyValuePtr->c_str());
}

void DisplayIlluminator::GetImageProperty(MM::PropertyBase* pProp)
{
    boost::apply_visitor([this, pProp](auto&& value)
        {
            GetImageProperty(pProp, value);
        }, imagePropertyModeMap.at(pProp->GetName()).propertyValuePtr);
}

void DisplayIlluminator::GetImageProperty(MM::PropertyBase* pProp, unsigned* propertyValuePtr)
{
    long temp;
    pProp->Get(temp);
	*propertyValuePtr = static_cast<unsigned>(temp); // Could use a numeric cast here to be safer
	GetCoreCallback()->OnPropertyChanged(this, pProp->GetName().c_str(), std::to_string(*propertyValuePtr).c_str());
}

void DisplayIlluminator::GetImageProperty(MM::PropertyBase* pProp, std::string* propertyValuePtr)
{
	pProp->Get(*propertyValuePtr);
    GetCoreCallback()->OnPropertyChanged(this, pProp->GetName().c_str(), propertyValuePtr->c_str());
}

int DisplayIlluminator::OnImagePropUpdate(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (imagePropertyModeMap.count(pProp->GetName()) <= 0)
    {
        return DEVICE_INVALID_PROPERTY;
    }

    if (eAct == MM::BeforeGet)
    {
		SetImageProperty(pProp);
    }
    else if (eAct == MM::AfterSet)
    {
        GetImageProperty(pProp);
        UpdateImages(imagePropertyModeMap.at(pProp->GetName()).relatedImageModes);
        BlitActiveImageToBuffer();
		DisplayImage();
    }
    return DEVICE_OK;
}

int DisplayIlluminator::OnActiveImage(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
        pProp->Set(activeImage.c_str());
    }
    else if (eAct == MM::AfterSet)
    {
        pProp->Get(activeImage);
        BlitActiveImageToBuffer();
        DisplayImage();

        // Fire the property changed event through the Core
        GetCoreCallback()->OnPropertyChanged(this, pProp->GetName().c_str(), activeImage.c_str());
    }

    return DEVICE_OK;
}

int DisplayIlluminator::OnDpcPatternCount(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    if (eAct == MM::BeforeGet)
    {
		pProp->Set(static_cast<long>(dpcPatternCount));
    }
    if (eAct == MM::AfterSet)
    {
        long temp;
		pProp->Get(temp);
		dpcPatternCount = static_cast<unsigned>(temp);
        CreateDpcImages();
        UpdateAllowedImages();
        activeImage = "Off";
        BlitActiveImageToBuffer();
        DisplayImage();
    }
    return DEVICE_OK;
}

void DisplayIlluminator::UpdateAllowedImages()
{
    ClearAllowedValues(g_PropName_ActiveImage);
    AddAllowedValue(g_PropName_ActiveImage, "Off");
    for (int i = 0; i < dpcPatternCount; i++)
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
    for (int i = 0; i < dpcPatternCount; i++)
    {
        SourcePatternRenderer dpcPatternRenderer(width_, height_);
        std::string imageName = "DPC" + std::to_string(i + 1);
        dpcPatternRenderer.RenderHalfOval(centerX, centerY, dpcWidth, dpcHeight, rotation, i * 360.0 / dpcPatternCount, monoColor);
        images_[imageName] = dpcPatternRenderer;
    }
}

void DisplayIlluminator::UpdateDpcImages()
{
    for (int i = 0; i < dpcPatternCount; i++)
    {
        std::string imageName = "DPC" + std::to_string(i + 1);
        images_[imageName].ClearFrame();
        if (dpcInnerHeight <= 0 && dpcInnerWidth <= 0)
        {
            images_[imageName].RenderHalfOval(centerX, centerY, dpcWidth, dpcHeight, rotation, i * 360.0 / dpcPatternCount, monoColor);
        }
        else
        {
            images_[imageName].RenderEllipticalHalfAnnulus(centerX, centerY, dpcWidth, dpcHeight, dpcInnerWidth, dpcInnerHeight, rotation, i * 360.0 / dpcPatternCount, monoColor);
        }

    }
}

void DisplayIlluminator::CreatePcImage()
{
    SourcePatternRenderer pcRenderer(width_, height_);
    pcRenderer.RenderEllipticalAnnulus(centerX, centerY, pcWidth, pcHeight, pcInnerWidth, pcInnerHeight, rotation, monoColor);
    images_["PC"] = pcRenderer;
}

void DisplayIlluminator::UpdatePcImage()
{
    images_["PC"].ClearFrame();
    images_["PC"].RenderEllipticalAnnulus(centerX, centerY, pcWidth, pcHeight, pcInnerWidth, pcInnerHeight, rotation, monoColor);
}

void DisplayIlluminator::CreateDfImage()
{
    SourcePatternRenderer dfRenderer(width_, height_);
    dfRenderer.RenderEllipticalAnnulus(centerX, centerY, dfWidth, dfHeight, dfInnerWidth, dfInnerHeight, rotation, monoColor);
    images_["DF"] = dfRenderer;
}

void DisplayIlluminator::UpdateDfImage()
{
    images_["DF"].ClearFrame();
    images_["DF"].RenderEllipticalAnnulus(centerX, centerY, dfWidth, dfHeight, dfInnerWidth, dfInnerHeight, rotation, monoColor);
}

void DisplayIlluminator::CreateBfImage()
{
    SourcePatternRenderer bfRenderer(width_, height_);
    bfRenderer.RenderOval(centerX, centerY, bfWidth, bfHeight, rotation, monoColor);
    images_["BF"] = bfRenderer;
}

void DisplayIlluminator::UpdateBfImage()
{
    images_["BF"].ClearFrame();
    images_["BF"].RenderOval(centerX, centerY, bfWidth, bfHeight, rotation, monoColor);
}

void DisplayIlluminator::CreateRbImage()
{
    SourcePatternRenderer rbRenderer(width_, height_);
    rbRenderer.RenderOval(centerX, centerY, dfWidth, dfHeight, dfInnerWidth, dfInnerHeight, rotation, monoColor, rbOuterColor);
    images_["RB"] = rbRenderer;
}

void DisplayIlluminator::UpdateRbImage()
{
    images_["RB"].ClearFrame();
    images_["RB"].RenderOval(centerX, centerY, dfWidth, dfHeight, dfInnerWidth, dfInnerHeight, rotation, monoColor, rbOuterColor);
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

int DisplayIlluminator::BlitActiveImageToBuffer()
{
   return SetImage(images_[activeImage]);   
}
