#pragma once

#include "opencv2/imgproc.hpp"

class SourcePatternRenderer
{
public:
	SourcePatternRenderer();
	SourcePatternRenderer(int imageWidth, int imageHeight); // Defaults to black background
	SourcePatternRenderer(int imageWidth, int imageHeight, std::string colorHex);
	~SourcePatternRenderer();

	void RenderEllipticalHalfAnnulus(int centerX, int centerY, int outerWidth, int outerHeight, 
		int innerWidth, int innerHeight, double ellipseRotation, double segmentRotation, std::string colorHex);
	
	void RenderEllipticalAnnulus(int centerX, int centerY, int outerWidth, int outerHeight,
		int innerWidth, int innerHeight, double ellipseRotation, std::string colorHex);

	void RenderEllipticalAnnulus(int centerX, int centerY, int width, int height, 
		double ellipseRotation, std::string colorHex, int thickness);

	void RenderOval(int centerX, int centerY, int width, int height,
		double ovalRotation, std::string colorHex);

	void RenderOval(int centerX, int centerY, int outerWidth, int outerHeight,
		int innerWidth, int innerHeight, double ellipseRotation, std::string innerColorHex, std::string outerColorHex);

	void RenderHalfOval(int centerX, int centerY, int width, int height,
		double ovalRotation, double segmementRotation, std::string colorHex);

	void RenderEllipse(int centerX, int centerY, int width, int height,
		double rotation, double startAngle, double stopAngle, cv::Scalar colorScalar, int thickness);
	
	void ClearFrame();

	unsigned int* getImageAsArray();
	std::vector<unsigned int> getImageAsVector();

	int convertXCoordOriginFromCenterToUpperLeft(int centerX);
	int convertYCoordOriginFromCenterToUpperLeft(int centerY);
private:
	cv::Mat image;
	cv::Mat backgroundImage;
	int imageWidth;
	int imageHeight;
};

cv::Scalar colorHexToScalar(std::string colorHex, int channelCount = 3);