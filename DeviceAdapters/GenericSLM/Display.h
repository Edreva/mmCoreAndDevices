#pragma once
#include "GenericSLM.h"
class Display : public GenericSLM
{
public:
	Display(const char* name);
	virtual ~Display();
	// Display API
	virtual int StoreImage(unsigned char* pixels);

private:
	const std::string name_;
	float pixelSize_; // um
	std::string imageName_;
	std::map< std::string, unsigned char*> images_;
	std::vector< unsigned char > off_image_;
	std::vector< unsigned char > on_image_;
};

