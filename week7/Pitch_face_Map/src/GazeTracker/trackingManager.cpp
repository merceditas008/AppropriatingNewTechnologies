#include "trackingManager.h"
#include "ofxXmlSettings.h"

using namespace std;
using namespace ofxCv;
using namespace cv;

trackingManager::trackingManager(){
}

trackingManager::~trackingManager(){
}

void trackingManager::setup(){
	//--- set up tracking
	cam.initGrabber(img_width, img_height);
	tracker.setup();
	
	dispImage.allocate(img_width, img_height);
	
	bEye.create(eyeImageHeight, eyeImageWidth, CV_8U);
	lEye.create(eyeImageHeight, eyeImageWidth/2, CV_8U);
	rEye.create(eyeImageHeight, eyeImageWidth/2, CV_8U);
	meanEye.create(eyeImageHeight, eyeImageWidth, CV_32F);
	meanEye.setTo(Scalar::all(0));
	
	ofxXmlSettings XML;
	XML.loadFile("settings/trackingSettings.xml");
	
	for (int i=0; i<numEyePositions; i++) {
		initTemplates[i].create(eyeTemplateSize, eyeTemplateSize, CV_8U);
		prevTemplates[i].create(eyeTemplateSize, eyeTemplateSize, CV_8U);
		eyePositions[i] = Point2d(0,0);
	}
	
	dispFont.loadFont("fonts/HelveticaNeueMed.ttf", 32);
	
	currentMode = TM_MODE_TRACKING;
	bBeenCalibrated = false;
}

bool trackingManager::update(){
	//--- update video/camera input
	cam.update();
	
	//--- eye tracking (on new frames)	
	bool newFrame = cam.isFrameNew();
	if(newFrame){
		// copy camera image
		colorImage = toCv(cam);
		cvtColor(colorImage, grayImage, CV_BGR2GRAY);
		IplImage dispImage_buffer = colorImage;
		dispImage = &dispImage_buffer;
		
		// update face tracker
		tracker.update(toCv(cam));
		eyePositions[0] = toCv(tracker.getImagePoint(36)); // left eye out
		eyePositions[1] = toCv(tracker.getImagePoint(39)); // left eye in
		eyePositions[2] = toCv(tracker.getImagePoint(42)); // right eye in
		eyePositions[3] = toCv(tracker.getImagePoint(45)); // right eye out
		
		// update eye images
		if(tracker.getFound()){
			grabEyeImages();
		}
		
		// gaze detection
		if(currentMode == TM_MODE_TRACKING) {
			gazeRaw = gazeEstimator.GetEstimation(bEye);
			
			gazeHistories.push_back(gazeRaw);
			
			int num_history = gazeHistories.size();
			if(num_history > maxGazeHistory)
			{
				gazeHistories.pop_front();
				num_history--;
			}
			
			double w = 1.0/num_history;
			gazePt = w*gazeHistories[0];
			
			for(int i=1; i<num_history; i++) {
				gazePt += w*gazeHistories[i];
			}
		}
		
		updateDispImage();
	}
	
	static float prevTime = ofGetElapsedTimef();
	float currTime = ofGetElapsedTimef();
	
	// animate calibration points
	if(currentMode == TM_MODE_CALIBRATION){
		calibPtRad -= calibPointsRadSpeed*(currTime - prevTime);
		if(calibPtRad < 0) {
			// add current eye images
			calibrationEyes.push_back(bEye.clone());
			gazeEstimator.AddTrainingData(bEye, calibrationPoints[calibPtIdx]);
			
			calibPtIdx++;
			calibPtRad = calibPointsRadMax;
		}
		if(calibPtIdx == calibPointsNumAll) {
			// start gaze tracker
			gazeEstimator.Initialize(calibAreaX, calibAreaY);
			bBeenCalibrated = true;
			
			currentMode = TM_MODE_TRACKING;
		}
	}
	
	prevTime = ofGetElapsedTimef();
	return newFrame;
}

void trackingManager::updateDispImage(){
	Mat dispMat(dispImage.getCvImage());
	Mat greyMat(dispMat.size(), CV_8U);
	cvtColor(dispMat, greyMat, CV_BGR2GRAY);
	Mat roi;
	
	// draw eye images		
	roi = greyMat(cv::Rect(0,0,eyeImageWidth,eyeImageHeight));
	bEye.copyTo(roi);
	
	// draw template images if true
	if(false)
	{
		for (int i=0; i<numEyePositions; i++) {
			roi = greyMat(cv::Rect(i*eyeTemplateSize,eyeImageHeight,eyeTemplateSize,eyeTemplateSize));
			initTemplates[i].copyTo(roi);
			roi = greyMat(cv::Rect(i*eyeTemplateSize,eyeImageHeight+eyeTemplateSize,eyeTemplateSize,eyeTemplateSize));
			prevTemplates[i].copyTo(roi);
		}
	}
	
	cvtColor(greyMat, dispMat, CV_GRAY2BGR);
	
	// draw detected eye positions
	for (int i=0; i<numEyePositions; i++) {
		circle(dispMat, eyePositions[i], 5, CV_RGB(255,0,0));
	}
}

void trackingManager::draw(){
	if(currentMode == TM_MODE_CALIBRATION) {
		ofHideCursor();
		ofSetColor(0,20);
		ofCircle(calibrationPoints[calibPtIdx].x, calibrationPoints[calibPtIdx].y, calibPtRad);
		
		ofSetColor(0);
		static const int lineW = 10;
		ofLine(calibrationPoints[calibPtIdx].x-lineW, calibrationPoints[calibPtIdx].y, 
					 calibrationPoints[calibPtIdx].x+lineW, calibrationPoints[calibPtIdx].y);
		ofLine(calibrationPoints[calibPtIdx].x, calibrationPoints[calibPtIdx].y-lineW, 
					 calibrationPoints[calibPtIdx].x, calibrationPoints[calibPtIdx].y+lineW);
	}
	else {
		ofShowCursor();
		ofSetColor(255);
		dispImage.draw((ofGetWidth()-img_width)/2, (ofGetHeight()-img_height)/2, img_width, img_height);
		
		ofSetColor(0);
		if(currentMode == TM_MODE_TRACKING) {
			dispFont.drawString("Press the SPACE key again to enter calibration mode", 100, 100);
		}
	}
}

bool trackingManager::grabEyeImages(){
	for (int i=0; i<numEyePositions; i++) {
		if(eyePositions[i].x == 0) return false;
		if(eyePositions[i].y == 0) return false;
	}
	
	Mat EyeRaw(eyeImageHeight, eyeImageWidth/2, CV_8U);
	static const double sigmaColor = 10;
	static const double sigmaSpace = 3;
	
	static const Point2f dst[] = {
		Point2f(eyeOffset, 0),
		Point2f(eyeOffset, eyeImageHeight/2),
		Point2f(eyeImageWidth/2-eyeOffset, eyeImageHeight/2)
	};
	static const double ratio = (double)(eyeImageHeight/2)/(eyeImageWidth/2 - 2*eyeOffset);
	
	// left eye
	float left_w = norm(eyePositions[1]-eyePositions[0]);
	Point2f left_hv = (1.0/left_w)*(eyePositions[1]-eyePositions[0]);
	Point2f left_up(left_hv.y, -left_hv.x);
	Point2f src_left[] = {
		eyePositions[0] + ratio*left_w*left_up,
		Point2f(eyePositions[0].x, eyePositions[0].y),
		Point2f(eyePositions[1].x, eyePositions[1].y),
	};
	Mat leftM = getAffineTransform(src_left, dst);
	warpAffine(grayImage, EyeRaw, leftM, lEye.size());
	bilateralFilter(EyeRaw, lEye, -1, sigmaColor, sigmaSpace, BORDER_CONSTANT);
	equalizeHist(lEye, lEye);
	threshold(lEye, lEye, 0, 255, THRESH_TRUNC | THRESH_OTSU);
	
	// right eye
	float right_w = norm(eyePositions[3]-eyePositions[2]);
	Point2f right_hv = (1.0/right_w)*(eyePositions[3]-eyePositions[2]);
	Point2f right_up(right_hv.y, -right_hv.x);
	Point2f src_right[] = {
		eyePositions[2] + ratio*right_w*right_up,
		Point2f(eyePositions[2].x, eyePositions[2].y),
		Point2f(eyePositions[3].x, eyePositions[3].y),
	};
	Mat rightM = getAffineTransform(src_right, dst);
	warpAffine(grayImage, EyeRaw, rightM, rEye.size());
	bilateralFilter(EyeRaw, rEye, -1, sigmaColor, sigmaSpace, BORDER_CONSTANT);
	equalizeHist(rEye, rEye);
	threshold(rEye, rEye, 0, 255, THRESH_TRUNC | THRESH_OTSU);
	
	// merge both eyes
	Mat roi;
	roi = bEye(cv::Rect(0,0,eyeImageWidth/2,eyeImageHeight));
	lEye.copyTo(roi);
	roi = bEye(cv::Rect(eyeImageWidth/2,0,eyeImageWidth/2,eyeImageHeight));
	rEye.copyTo(roi);
	
	// compute running average
	static const float factor = 0.5;
	accumulateWeighted(bEye, meanEye, factor);
	meanEye.convertTo(bEye, CV_8U);
	
	return true;
}

bool trackingManager::trackEyes()
{
	for (int i=0; i<numEyePositions; i++) {
		if(eyePositions[i].x < searchWindowSize/2) return false;
		if(eyePositions[i].x > grayImage.size().width-searchWindowSize) return false;
		if(eyePositions[i].y < searchWindowSize/2) return false;
		if(eyePositions[i].y > grayImage.size().height-searchWindowSize) return false;
	}
	
	int rw = searchWindowSize - eyeTemplateSize + 1;
	Mat initV(cv::Size(rw,rw), CV_32F);
	Mat prevV(cv::Size(rw,rw), CV_32F);
	Mat resultV(cv::Size(rw,rw), CV_32F);
	double alpha = 2.0; // resultV = initV + alpha*prevV
	
	for (int i=0; i<numEyePositions; i++) {
		cv::Rect roi(eyePositions[i].x-searchWindowSize/2, eyePositions[i].y-searchWindowSize/2, 
								 searchWindowSize, searchWindowSize);
		
		matchTemplate(grayImage(roi), initTemplates[i], initV, CV_TM_CCOEFF_NORMED);
		matchTemplate(grayImage(roi), prevTemplates[i], prevV, CV_TM_CCOEFF_NORMED);
		addWeighted(prevV, alpha, initV, 1.0, 0.0, resultV);
		
		double minVal, maxVal;
		cv::Point minLoc, maxLoc;
		minMaxLoc(resultV, &minVal, &maxVal, &minLoc, &maxLoc);
		
		eyePositions[i].x = roi.x + maxLoc.x + eyeTemplateSize/2;
		eyePositions[i].y = roi.y + maxLoc.y + eyeTemplateSize/2;
		
		cv::Rect tmpl(eyePositions[i].x-eyeTemplateSize/2, eyePositions[i].y-eyeTemplateSize/2, 
									eyeTemplateSize, eyeTemplateSize);
		
		if(tmpl.x < 0) tmpl.x = 0;
		else if(tmpl.x > grayImage.size().width-eyeTemplateSize) tmpl.x = grayImage.size().width-eyeTemplateSize;
		if(tmpl.y < 0) tmpl.y = 0;
		else if(tmpl.y > grayImage.size().height-eyeTemplateSize) tmpl.y = grayImage.size().height-eyeTemplateSize;
		
		// update prevTemplates
		grayImage(tmpl).copyTo(prevTemplates[i]);
	}
}

void trackingManager::keyPressed(int key) {
	if (key == ' '){
		if(currentMode == TM_MODE_TRACKING) {
			currentMode = TM_MODE_CALIBRATION;
			
			calibrationPoints.clear();
			calibAreaX = ofGetWidth();
			calibAreaY = ofGetHeight();
			static const float gridOffset = 10;
			float gridW = (float)(calibAreaX-2*gridOffset)/(calibPointsNumX-1);
			float gridH = (float)(calibAreaY-2*gridOffset)/(calibPointsNumY-1);
			for (int y=0; y<calibPointsNumY; y++) {
				int start, end;
				if(y % 2 == 0) {
					for (int x=0; x<calibPointsNumX; x++) {
						calibrationPoints.push_back(Point2f(gridOffset+x*gridW, gridOffset+y*gridH));
					}
				} else {
					for (int x=calibPointsNumX - 1; x>=0; x--) {
						calibrationPoints.push_back(Point2f(gridOffset+x*gridW, gridOffset+y*gridH));
					}
				}
			}
			
			// additional points
			if(false) {
				calibrationPoints.push_back(Point2f(gridOffset, gridOffset));
				calibrationPoints.push_back(Point2f(calibAreaX-gridOffset, gridOffset));
				calibrationPoints.push_back(Point2f(gridOffset, calibAreaY-gridOffset));
				calibrationPoints.push_back(Point2f(calibAreaX-gridOffset, calibAreaY-gridOffset));
			}
			
			calibPointsNumAll = calibrationPoints.size();
			
			calibPtIdx = 0;
			calibPtRad = calibPointsRadMax;
			calibrationEyes.clear();
			gazeEstimator.Reset();
			bBeenCalibrated = false;
		}
		else {
			currentMode = TM_MODE_TRACKING;
		}
	}
}