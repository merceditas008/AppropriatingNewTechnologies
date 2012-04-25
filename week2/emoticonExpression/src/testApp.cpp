#include "testApp.h"

using namespace ofxCv;
using namespace cv;

void testApp::setup() {
	ofSetVerticalSync(true);
	cam.initGrabber(640, 480);
	
	tracker.setup();
	tracker.setRescale(.5);
    ////LOAD EMOTICONS THAT WILL TRANSLATE YOUR FACE EXPRESSIONS
    emotHappy.loadImage("happy.jpg");
    emotSurprised.loadImage("surprise.jpg");
    show_emot=false;
    
}

void testApp::update() {
	cam.update();
	if(cam.isFrameNew()) {
		if(tracker.update(toCv(cam))) {
			classifier.classify(tracker);
		}		
	}
}

void testApp::draw() {
	ofSetColor(255);
	cam.draw(0, 0);
	//tracker.draw();
    float escale=tracker.getScale();
   
    
    ofVec2f position = tracker.getPosition();
	int w = 100, h = 12;
	ofPushStyle();
	ofPushMatrix();
	ofTranslate(5, 10);
	int n = classifier.size();
    primary = classifier.getPrimaryExpression();
    
      for(int i = 0; i < n; i++){
		ofSetColor(i == primary ? ofColor::red : ofColor::black);
		ofRect(0, 0, w * classifier.getProbability(i) + .5, h);
		ofSetColor(255);
		ofDrawBitmapString(classifier.getDescription(i), 5, 9);
		ofTranslate(0, h + 5);
  }
	ofPopMatrix();
	ofPopStyle();
	
	ofDrawBitmapString(ofToString((int) ofGetFrameRate()), ofGetWidth() - 20, ofGetHeight() - 10);
	drawHighlightString(
		string() +
		"r - reset\n" +
		"e - add expression\n" +
		"a - add sample\n" +
		"s - save expressions\n"
		"l - load expressions",
		14, ofGetHeight() - 7 * 12);
/////////ACTIVATE THE EMOTICON_MODE WHEN LOADING EXPRESSIONS_that helps to track them
    if (show_emot){
    if(primary==0){
        ofTranslate(position.x-100, position.y-100);
        ofScale(escale/6,escale/6,escale/6);
        emotSurprised.draw(0,0);
           }
    if(primary==2){
        ofTranslate(position.x-100, position.y-100);
        ofScale(escale/6,escale/6,escale/6);
        emotHappy.draw(0,0);
        
    }
    }

    
    
}

void testApp::keyPressed(int key) {
	if(key == 'r') {
		tracker.reset();
		classifier.reset();
	}
	if(key == 'e') {
		classifier.addExpression();
	}
	if(key == 'a') {
		classifier.addSample(tracker);
	}
	if(key == 's') {
		classifier.save("expressions");
	}
	if(key == 'l') {
		classifier.load("expressions");
        show_emot=true;
	}
}