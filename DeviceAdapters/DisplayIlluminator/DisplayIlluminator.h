// Ewan Drever-Smith 
// 2025
// Based on GenericSLM device adapter

#pragma once

#include "DeviceBase.h"
#include "DeviceUtils.h"

#include "SLMColor.h"
#include "RefreshWaiter.h"

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
	
	// Action Handlers
	int OnDisplayImage(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
	const std::string name_;

	std::vector<std::string> availableMonitors_;

	std::string monitorName_; // Empty string if using test mode
	unsigned width_, height_;

	SLMWindowThread* windowThread_;
	SleepBlocker* sleepBlocker_;
	RefreshWaiter refreshWaiter_;

	std::string imageName_; // Name of currently selected image
	std::map< std::string, std::vector<unsigned char>> images_; // Maps image names to pixel vectors
																// TODO: Add or change to accommodate unsigned int (colour) images
	// Currently unused - TODO
	double exposureMs_;
	bool invert_;
	bool shouldBlitInverted_;
	float pixelSize_;
	SLMColor monoColor_;
};

std::vector<unsigned char> HalfCircleFrame(unsigned int frameHeight, unsigned int frameWidth, unsigned int diameter, int rotation, int centerX, int centerY);
float DistanceFromCenter(unsigned int centerX, unsigned int centerY, unsigned int pointX, unsigned int pointY);
bool IsPointInHalfCircle(unsigned int centerX, unsigned int centerY, unsigned int pointX, unsigned int pointY, unsigned int diameter, int rotationDeg);
