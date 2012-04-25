#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup(){
	ofEnableAlphaBlending();
	ofSetVerticalSync(true);
	ofSetFrameRate(60);
    
	mode = MODE_TRACKING;
	
	eyeSmoothed.set(0,0,0);
	
	TM.setup();
	typeScene.setup();
	eyeApp.setup();
	ponger.setup();
	
	BT.setup("catch me!", 50,50,180,180);
	BT.setRetrigger(true);
	
	timeSince = 0;
	bMouseSimulation = false;
	
	// start fullscreen
	ofToggleFullscreen();
	
	osc.setup("localhost", 8338);
}

//--------------------------------------------------------------
void testApp::update() {	
	bool newFrame = TM.update();
	
	if (!bMouseSimulation){
		if(TM.bBeenCalibrated){
			eyeSmoothed = TM.getEyePoint();
			eyeRaw = TM.getRawEyePoint();
		}
	} else {
		eyeSmoothed.x = mouseX;
		eyeSmoothed.y = mouseY;
	}
	
	if (mode == MODE_TEST){
		if (BT.update(eyeSmoothed.x, eyeSmoothed.y)){
			BT.x = ofRandom(100,ofGetWidth()-100);
			BT.y = ofRandom(100,ofGetHeight()-100);
		}
	}
	
	if( mode == MODE_DRAW ){
		eyeApp.update(eyeSmoothed.x, eyeSmoothed.y);
	}
	
	if (mode == MODE_TYPING){
		typeScene.update(eyeSmoothed.x, eyeSmoothed.y);
	}
	
	if (mode == MODE_PONG){
		ponger.update(eyeSmoothed.x, eyeSmoothed.y);
	}
	
	if(newFrame) {
		ofxOscMessage msg;
		msg.setAddress("/gaze/smoothed");
		msg.addFloatArg(eyeSmoothed.x);
		msg.addFloatArg(eyeSmoothed.y);
		osc.sendMessage(msg);
		
		msg.clear();
		msg.setAddress("/gaze/raw");
		msg.addFloatArg(eyeRaw.x);
		msg.addFloatArg(eyeRaw.y);
		osc.sendMessage(msg);
	}
}

//--------------------------------------------------------------
void testApp::draw(){
	ofBackground(200);	
	ofSetColor(255);
	
	if (mode == MODE_TRACKING)	TM.draw();
	if (mode == MODE_TEST)		BT.draw();
	if (mode == MODE_DRAW )		eyeApp.draw();
	if (mode == MODE_TYPING)	typeScene.draw();
	if (mode == MODE_PONG)		ponger.draw();
	
	// draw a green dot to see how good the tracking is:
	if(TM.bBeenCalibrated || bMouseSimulation){
		if( mode != MODE_DRAW ){
			ofFill();
			ofSetColor(0,255,0,120);
			ofCircle(eyeSmoothed.x, eyeSmoothed.y, 20);
			ofSetColor(255,0,0,40);
			ofCircle(eyeRaw.x, eyeRaw.y, 10);
		}
	}
	
	ofSetColor(0);
	ofDrawBitmapString("FrameRate: " + ofToString((int) ofGetFrameRate()), 1, ofGetHeight() - 20);
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){
	
	switch (key){
		case	OF_KEY_RETURN:
			mode ++;
			mode %= num_modes;
			break;
			
		case	'm':
		case	'M':
			bMouseSimulation = !bMouseSimulation;
			break;
			
		case	'f':
		case	'F':
			ofToggleFullscreen();
			break;
	}
	
	if(mode == MODE_TRACKING) TM.keyPressed(key);
}

//--------------------------------------------------------------
void testApp::keyReleased(int key){

}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){

}

