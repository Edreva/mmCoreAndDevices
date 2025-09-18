// Ewan Drever-Smith 
// 2025
// Based on GenericSLM device adapter

#pragma once

#include "DeviceBase.h"
#include "DeviceUtils.h"

#include "RefreshWaiter.h"

#include "SourcePatternRenderer.h"
#include <set>

class SLMWindowThread;
class SleepBlocker;



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

	enum ImageTypes { BF, DF, DPC, PC, RB };

	virtual int SetImage(unsigned char* pixels);
	virtual int SetImage(unsigned int* pixels);
	virtual int SetPixelsTo(unsigned char intensity);
	virtual int SetPixelsTo(unsigned char red, unsigned char green, unsigned char blue);
	virtual int DisplayImage();

	virtual int IsSLMSequenceable(bool& isSequenceable) const
	{
		isSequenceable = false; return DEVICE_OK;
	}
private:
	void CreateImages();
	void CreateDpcImages();
	void CreatePcImage();
	void CreateDfImage();
	void CreateBfImage();
	void CreateRbImage();
	void UpdateRbImage();
	void UpdateImages();
	void UpdateDpcImages();
	void UpdatePcImage();
	void UpdateDfImage();
	void UpdateBfImage();
	void UpdateImages(std::set<ImageTypes> imagesToUpdate);
	int InitializeImages();
	int InitializeMonitor();
	void UpdateAllowedImages();
	
	// Action Handlers
	// Generics
	int OnImagePropUpdate(MM::PropertyBase* pProp, MM::ActionType eAct, unsigned& prop); 
	int OnImagePropUpdate(MM::PropertyBase* pProp, MM::ActionType eAct, unsigned& prop, std::set<ImageTypes>);
	int OnImagePropUpdate(MM::PropertyBase* pProp, MM::ActionType eAct, std::string& prop);
	int OnImagePropUpdate(MM::PropertyBase* pProp, MM::ActionType eAct, std::string& prop, std::set<ImageTypes>);

	int OnActiveImage(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnDpcPatternCount(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnDpcWidth(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnDpcHeight(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnDpcInnerWidth(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnDpcInnerHeight(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPcWidth(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPcHeight(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPcInnerWidth(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPcInnerHeight(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnDfWidth(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnDfHeight(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnDfInnerWidth(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnDfInnerHeight(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnBfWidth(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnBfHeight(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnRotation(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnCenterX(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnCenterY(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnMonoColor(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnRbOuterColor(MM::PropertyBase* pProp, MM::ActionType eAct);

	int SetImage(cv::Mat image);
	int SetImage(SourcePatternRenderer imageRenderer);

private:
	const std::string name_;

	std::vector<std::string> availableMonitors_;

	std::string monitorName_; // Empty string if using test mode
	unsigned width_, height_;

	SLMWindowThread* windowThread_;
	SleepBlocker* sleepBlocker_;
	RefreshWaiter refreshWaiter_;

	unsigned centerX_, centerY_; // TODO: Either rename to reflect the fact they represent the top left corner, or adjust use.
	unsigned dpcWidth_, dpcHeight_;
	unsigned dpcInnerWidth_, dpcInnerHeight_;
	unsigned pcWidth_, pcHeight_;
	unsigned pcInnerWidth_, pcInnerHeight_;
	unsigned dfWidth_, dfHeight_;
	unsigned dfInnerWidth_, dfInnerHeight_;
	unsigned bfWidth_, bfHeight_;
	unsigned dpcPatternCount_;
	unsigned rotation_;

	std::string monoColor_;
	std::string rbOuterColor_;
	std::string imageName_; // Name of currently selected image
	//std::map< std::string, std::vector<unsigned int>> images_; // Maps image names to pixel vectors
	std::map< std::string, SourcePatternRenderer> images_;
	// Currently unused - TODO
	double exposureMs_;
	bool invert_;
	bool shouldBlitInverted_;
	float pixelSize_;

};
