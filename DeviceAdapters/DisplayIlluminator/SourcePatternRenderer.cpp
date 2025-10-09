#include "SourcePatternRenderer.h"

SourcePatternRenderer::SourcePatternRenderer() :
	backgroundImage(cv::Mat()),
	image(cv::Mat()),
	imageWidth(0),
	imageHeight(0)
{

}

SourcePatternRenderer::SourcePatternRenderer(int imageWidth, int imageHeight) :
	backgroundImage(cv::Mat::zeros(imageHeight, imageWidth, CV_8UC4)),
	image(cv::Mat::zeros(imageHeight, imageWidth, CV_8UC4)),
	imageWidth(imageWidth),
	imageHeight(imageHeight)
{
}

SourcePatternRenderer::SourcePatternRenderer(int imageWidth, int imageHeight, std::string colorHex) :
	backgroundImage(cv::Mat::zeros(imageHeight, imageWidth, CV_8UC4)),
	image(cv::Mat::zeros(imageHeight, imageWidth, CV_8UC4)),
	imageWidth(imageWidth),
	imageHeight(imageHeight)
{
	backgroundImage.setTo(colorHexToScalar(colorHex));
	image.setTo(colorHexToScalar(colorHex));
}

SourcePatternRenderer::~SourcePatternRenderer()
{

}


void SourcePatternRenderer::RenderEllipticalHalfAnnulus(int centerX, int centerY, int outerWidth, int outerHeight, int innerWidth, int innerHeight,
	double ellipseRotation, double segmentRotation, std::string colorHex)
{
	RenderEllipse(centerX, centerY, outerWidth, outerHeight, ellipseRotation, segmentRotation, segmentRotation + 180, colorHexToScalar(colorHex), -1);
	RenderEllipse(centerX, centerY, innerWidth, innerHeight, ellipseRotation, segmentRotation, segmentRotation + 180, cv::Scalar(0, 0, 0), -1);
}

void SourcePatternRenderer::RenderEllipticalAnnulus(int centerX, int centerY, int outerWidth, int outerHeight, int innerWidth, int innerHeight,
	double ellipseRotation, std::string colorHex)
{
	RenderEllipse(centerX, centerY, outerWidth, outerHeight, ellipseRotation, 0, 360, colorHexToScalar(colorHex), -1);
	RenderEllipse(centerX, centerY, innerWidth, innerHeight, ellipseRotation, 0, 360, cv::Scalar(0,0,0), -1);
}

void SourcePatternRenderer::RenderOval(int centerX, int centerY, int outerWidth, int outerHeight, int innerWidth, int innerHeight,
	double ellipseRotation, std::string innerColorHex, std::string outerColorHex)
{
	RenderEllipse(centerX, centerY, outerWidth, outerHeight, ellipseRotation, 0, 360, colorHexToScalar(outerColorHex), -1);
	RenderEllipse(centerX, centerY, innerWidth, innerHeight, ellipseRotation, 0, 360, colorHexToScalar(innerColorHex), -1);
}

void SourcePatternRenderer::RenderEllipticalAnnulus(int centerX, int centerY, int width, int height, 
	double ellipseRotation, std::string colorHex, int thickness)
{
	RenderEllipse(centerX, centerY, width, height, ellipseRotation, 0, 360, colorHexToScalar(colorHex), thickness);
}

void SourcePatternRenderer::RenderOval(int centerX, int centerY, int width, int height,
	double ovalRotation, std::string colorHex)
{
	RenderEllipse(centerX, centerY, width, height, ovalRotation, 0, 360, colorHexToScalar(colorHex), -1);
}

void SourcePatternRenderer::RenderHalfOval(int centerX, int centerY, int width, int height,
	double ovalRotation, double segmementRotation, std::string colorHex)
{
	RenderEllipse(centerX, centerY, width, height, ovalRotation, segmementRotation, segmementRotation + 180, colorHexToScalar(colorHex), -1);
}

void SourcePatternRenderer::RenderEllipse(int centerX, int centerY, int width, int height, 
	double rotation, double startAngle, double stopAngle, cv::Scalar colorScalar, int thickness)
{
	int xPos = convertXCoordOriginFromCenterToUpperLeft(centerX);
	int yPos = convertYCoordOriginFromCenterToUpperLeft(centerY);
	cv::ellipse(image, cv::Point(xPos, yPos), cv::Size(width/2, height/2), rotation, startAngle, stopAngle, colorScalar, thickness);
}

void SourcePatternRenderer::ClearFrame()
{
	image = backgroundImage.clone();
}

unsigned int* SourcePatternRenderer::getImageAsArray()
{
	return reinterpret_cast<unsigned int*>(image.data); // TODO: Determine whether best to use is_contiguous? and .ptr<>() method
}

std::vector<unsigned int> SourcePatternRenderer::getImageAsVector()
{
	int imageSize = image.cols * image.rows;
	unsigned int* imagePtr = getImageAsArray();
	return std::vector<unsigned int>(imagePtr, imagePtr + imageSize);
}

// Max channel count of 4
// Must be bare string without '0x' prefix
cv::Scalar colorHexToScalar(std::string colorHex, int channelCount) {
	int length = colorHex.length();
	int channelCharLength = length / channelCount;
	double scalarVals[4] = { 0, 0, 0, 0 };
	for (int i = 0; i < channelCount; i++)
	{
		scalarVals[i] = strtod((std::string("0x") + colorHex.substr(i * channelCharLength, channelCharLength)).c_str(), nullptr);
	}
	return cv::Scalar(scalarVals[2], scalarVals[1], scalarVals[0], scalarVals[3]);
}

// Helper function to convert coordinates from centered origin to upper left corner
// TODO: Rename this to something more succinct.
int SourcePatternRenderer::convertXCoordOriginFromCenterToUpperLeft(int centerX) {
	return centerX + imageWidth / 2;
}

int SourcePatternRenderer::convertYCoordOriginFromCenterToUpperLeft(int centerY) {
	return centerY + imageHeight / 2; 
}