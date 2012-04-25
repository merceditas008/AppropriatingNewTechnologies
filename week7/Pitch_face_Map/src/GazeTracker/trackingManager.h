/*
 *  trackingManager
 *
 *  Created by ysugano on 11/01/25.
 *
 */

#ifndef trackingManager_h
#define trackingManager_h

#include <deque>

#include "GazeEstimator.h"

#include "ofxFaceTracker.h"
#include "ofxOpenCv.h"
#include "ofxCv.h"

#include "cv.h"
#include "util.h"

class trackingManager {
	
public:
	trackingManager();
	~trackingManager();
	
	void setup();
	bool update();
	void draw();
	
	bool bBeenCalibrated;
	ofPoint getEyePoint(){ return ofPoint(gazePt.x, gazePt.y); };
	ofPoint getRawEyePoint(){ return ofPoint(gazeRaw.x, gazeRaw.y); };
	
	void keyPressed(int key);
	
private:
	cv::Mat grayImage, colorImage;
	ofxCvColorImage dispImage;
	void updateDispImage();
	
	static const int eyeImageHeight = 5;
	static const int eyeImageWidth = 4*eyeImageHeight;
	static const int eyeOffset = eyeImageHeight/5;
	cv::Mat bEye, lEye, rEye, meanEye;
	bool grabEyeImages();
	
	ofTrueTypeFont dispFont;
	
	enum trackingModes{
		TM_MODE_TRACKING,
		TM_MODE_CALIBRATION,
	} currentMode;
	
	// face & eye tracking
	static const int img_width = 640;
	static const int img_height = 480;
	ofVideoGrabber cam;
	ofxFaceTracker tracker;
	static const int numEyePositions = 4;
	static const int eyeTemplateSize = 25;
	cv::Point2f eyePositions[numEyePositions];
	
	// eye tracking
	void detectEyeCorner();
	bool trackEyes();
	static const int searchWindowSize = 30;
	cv::Mat initTemplates[numEyePositions], prevTemplates[numEyePositions];
	
	// calibration
	static const int calibPointsNumX = 8;
	static const int calibPointsNumY = 5;
	int calibPointsNumAll;
	int calibPtIdx;
	static const int calibPointsRadMax = 200;
	static const int calibPointsRadSpeed = 250;
	float calibPtRad;
	int calibAreaX, calibAreaY;
	std::vector<cv::Point2f> calibrationPoints;
	std::vector<cv::Mat> calibrationEyes;
	
	// gaze estimator
	cv::Point2f gazePt, gazeRaw;
	static const int maxGazeHistory = 10;
	std::deque<cv::Point2f> gazeHistories;
	CGazeEstimator gazeEstimator;
};

#endif