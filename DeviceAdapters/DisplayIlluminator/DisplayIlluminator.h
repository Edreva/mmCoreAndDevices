// Ewan Drever-Smith 
// 2025
// Based on GenericSLM device adapter

#pragma once

#include "DeviceBase.h"
#include "DeviceUtils.h"

#include "RefreshWaiter.h"

#include "SourcePatternRenderer.h"
#include <set>
#include <boost/variant.hpp> // Add this include to replace variant_fwd

class SLMWindowThread;
class SleepBlocker;

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
const char* g_PropName_RbInnerColor = "RheinbergInnerColor";

class DisplayIlluminator : public CSLMBase<DisplayIlluminator>
{
public:

	DisplayIlluminator(const char* name);
	~DisplayIlluminator();

	// Device API
	virtual int Initialize();
	virtual int Shutdown();

	virtual void GetName(char* pszName) const;
	virtual bool Busy();

	// SLM API
	virtual unsigned int GetWidth();
	virtual unsigned int GetHeight();
	virtual unsigned int GetNumberOfComponents();
	virtual unsigned int GetBytesPerPixel();

	virtual int SetExposure(double exposureMs);
	virtual double GetExposure();

	virtual int SetImage(unsigned char* pixels);
	virtual int SetImage(unsigned int* pixels);
	virtual int SetPixelsTo(unsigned char intensity);
	virtual int SetPixelsTo(unsigned char red, unsigned char green, unsigned char blue);
	virtual int DisplayImage();

	virtual int IsSLMSequenceable(bool& isSequenceable) const
	{
		isSequenceable = false; return DEVICE_OK;
	}

	enum ImageTypes { BF, DF, DPC, PC, RB };

private:
	int InitializeImages();
	int InitializeDisplay();

	void CreateImages();
	void CreateDpcImages();
	void CreatePcImage();
	void CreateDfImage();
	void CreateBfImage();
	void CreateRbImage();

	void UpdateImages();
	void UpdateImages(std::set<ImageTypes> imagesToUpdate);
	void UpdateDpcImages();
	void UpdatePcImage();
	void UpdateDfImage();
	void UpdateBfImage();
	void UpdateRbImage();

	// Updates the list of allowed images in the ActiveImage property based on the current DPC pattern count
	// In future this could be extended to allow more granular control of available images
	void UpdateAllowedImages();
	
	// Action Handlers
	// Generic handler that updates the relevant images based on the property changed
	int OnImagePropUpdate(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnActiveImage(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnDpcPatternCount(MM::PropertyBase* pProp, MM::ActionType eAct);


	int BlitActiveImageToBuffer();
	//int SetImage(cv::Mat image);
	int SetImage(SourcePatternRenderer imageRenderer);


	struct PropertyImageModeMapping
	{
		boost::variant<unsigned*, std::string*> propertyValuePtr;
		std::set<ImageTypes> relatedImageModes;
	};

	// Generic version that uses the variant associated with propName in imagePropertyModeMap to dispatch to the correct CreateProperty overload
	void CreateProperty(const char* propName, bool readOnly, MM::ActionFunctor* pAct, bool isPreInitProperty = false);
	void CreateProperty(const char* propName, std::string initialValue, bool readOnly, MM::ActionFunctor* pAct, bool isPreInitProperty = false);
	void CreateProperty(const char* propName, unsigned initialValue, bool readOnly, MM::ActionFunctor* pAct, bool isPreInitProperty = false);

	// Generic version that uses the variant associated with pProp.GetName() in imagePropertyModeMap to dispatch to the correct GetImageProperty overload
	void GetImageProperty(MM::PropertyBase* pProp);
	void GetImageProperty(MM::PropertyBase* pProp, unsigned* propertyValuePtr);
	void GetImageProperty(MM::PropertyBase* pProp, std::string* propertyValuePtr);

	// Generic version that uses the variant associated with pProp.GetName() in imagePropertyModeMap to dispatch to the correct SetImageProperty overload
	void SetImageProperty(MM::PropertyBase* pProp);
	void SetImageProperty(MM::PropertyBase* pProp, unsigned* propertyValuePtr);
	void SetImageProperty(MM::PropertyBase* pProp, std::string* propertyValuePtr);

private:
	const std::string name_;
	std::vector<std::string> availableMonitors_;
	std::string monitorName_; // Empty string if using test mode
	unsigned width_, height_;

	SLMWindowThread* windowThread_;
	SleepBlocker* sleepBlocker_;
	RefreshWaiter refreshWaiter_;

	// Image Properties (references are added to imagePropertyModeMap)
	// Bright-field
	unsigned bfWidth, bfHeight;
	// Dark-field
	unsigned dfWidth, dfHeight;
	unsigned dfInnerWidth, dfInnerHeight;
	// Phase contrast
	unsigned pcWidth, pcHeight;
	unsigned pcInnerWidth, pcInnerHeight;
	// Differential phase contrast
	unsigned dpcPatternCount;
	unsigned dpcWidth, dpcHeight;
	unsigned dpcInnerWidth, dpcInnerHeight;
	// Rheinburg illumination contrast
	std::string rbOuterColor;
	std::string rbInnerColor;
	// Common properties
	unsigned rotation;
	unsigned centerX;
	unsigned centerY;
	std::string activeImage;
	std::string monoColor;

	const std::map<std::string, PropertyImageModeMapping> imagePropertyModeMap =
	{
		{ g_PropName_BfWidth, PropertyImageModeMapping{&bfWidth, {BF}}},
		{ g_PropName_BfHeight, PropertyImageModeMapping{&bfHeight, {BF}}},
		{ g_PropName_DfWidth, PropertyImageModeMapping{&dfWidth, {DF, RB}}},
		{ g_PropName_DfHeight, PropertyImageModeMapping{&dfHeight, {DF, RB}}},
		{ g_PropName_DfInnerWidth, PropertyImageModeMapping{&dfInnerWidth, {DF, RB}}},
		{ g_PropName_DfInnerHeight, PropertyImageModeMapping{&dfInnerHeight, {DF, RB}}},
		{ g_PropName_PcWidth, PropertyImageModeMapping{&pcWidth, {PC}}},
		{ g_PropName_PcHeight, PropertyImageModeMapping{&pcHeight, {PC}}},
		{ g_PropName_PcInnerWidth, PropertyImageModeMapping{&pcInnerWidth, {PC}}},
		{ g_PropName_PcInnerHeight, PropertyImageModeMapping{&pcInnerHeight, {PC}}},
		{ g_PropName_DpcPatternCount, PropertyImageModeMapping{&dpcPatternCount, {DPC}}},
		{ g_PropName_DpcWidth, PropertyImageModeMapping{&dpcWidth, {DPC}}},
		{ g_PropName_DpcHeight, PropertyImageModeMapping{&dpcHeight, {DPC}}},
		{ g_PropName_DpcInnerWidth, PropertyImageModeMapping{&dpcInnerWidth, {DPC}}},
		{ g_PropName_DpcInnerHeight, PropertyImageModeMapping{&dpcInnerHeight, {DPC}}},
		{ g_PropName_Rotation, PropertyImageModeMapping{&rotation, {BF, DF, PC, DPC, RB}}},
		{ g_PropName_CenterX, PropertyImageModeMapping{&centerX, {BF, DF, PC, DPC, RB}}},
		{ g_PropName_CenterY, PropertyImageModeMapping{&centerY, {BF, DF, PC, DPC, RB}}},
		{ g_PropName_RbOuterColor, PropertyImageModeMapping{&rbOuterColor, {BF, DF, PC, DPC, RB}}},
		{ g_PropName_RbInnerColor, PropertyImageModeMapping{&rbInnerColor, {BF, DF, PC, DPC, RB}}},
		{ g_PropName_MonoColor, PropertyImageModeMapping{&monoColor, {BF, DF, PC, DPC}}},
		{ g_PropName_ActiveImage, PropertyImageModeMapping{&activeImage, {BF, DF, PC, DPC, RB}}},
	};

	std::map< std::string, SourcePatternRenderer> images_;

	// Currently unused
	double exposureMs_;
	bool invert_;
	bool shouldBlitInverted_;
	float pixelSize_;

};