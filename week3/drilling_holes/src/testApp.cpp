#include "testApp.h"

ofVec3f getCentroid(ofMesh& mesh){
    ofVec3f sum;
    for(int i=0;i<mesh.getNumVertices();i++){
        sum+=mesh.getVertex(i);
    }
    sum/=mesh.getNumVertices();
    
    return sum;
}


//--------------------------------------------------------------
void testApp::setup() {
	ofSetLogLevel(OF_LOG_VERBOSE);
    //shader.load("DOFCloud");
    //focusDistance = 50;
	//aperture = .2;
	
	// enable depth->video image calibration
	kinect.setRegistration(true);
    
	kinect.init();
	//kinect.init(true); // shows infrared instead of RGB video image
	//kinect.init(false, false); // disable video image (faster fps)
	kinect.open();
	
#ifdef USE_TWO_KINECTS
	kinect2.init();
	kinect2.open();
#endif
	
	colorImg.allocate(kinect.width, kinect.height);
	grayImage.allocate(kinect.width, kinect.height);
	grayThreshNear.allocate(kinect.width, kinect.height);
	grayThreshFar.allocate(kinect.width, kinect.height);
	
	nearThreshold = 230;
	farThreshold = 70;
	bThreshWithOpenCV = true;
	
	ofSetFrameRate(60);
	
	// zero the tilt on startup
	angle = 0;
	kinect.setCameraTiltAngle(angle);
	
	// start from the front
	bDrawPointCloud = false;
    
    float xini=320;
    float yini=240;
}

//--------------------------------------------------------------
void testApp::update() {
	
	ofBackground(0);
	
	kinect.update();

#ifdef USE_TWO_KINECTS
	kinect2.update();
#endif
}

//--------------------------------------------------------------
void testApp::draw() {
	
	ofSetColor(255, 255, 255);
	
	
		easyCam.begin();
		drawPointCloud();
		easyCam.end();
		
	// draw instructions
	ofSetColor(255, 255, 255);
	stringstream reportStream;
	    
}

void testApp::drawPointCloud() {
	int w = 640;
	int h = 480;
    int threshold_close=500;
    int threshold_far=1000;
    
    int ratio=200;
	ofMesh mesh;
	mesh.setMode(OF_PRIMITIVE_POINTS);
	int step = 2;
	for(int y = 0; y < h; y += step) {
		for(int x = 0; x < w; x += step) {
            //
			if((kinect.getDistanceAt(x, y) > threshold_close)&&(kinect.getDistanceAt(x, y) < threshold_far)) {
               
                 float distc=ofDist(x,y,xini,yini);
                
                //look for hole 
                if (distc < ratio){
                    
                    float z=kinect.getDistanceAt(x, y);
                    // drill the hole around the center
                     mesh.addColor(distc/100);
                    float z2=z+20*(ratio/(distc+00000000.1));
                    ofVec3f pos(x - w/ 2, y - h / 2, z2);
                    mesh.addVertex(pos);
                    }
                
                else{
                     mesh.addColor(255);
                    float z=kinect.getDistanceAt(x, y);
                    ofVec3f pos2(x - w/ 2, y - h / 2, z);
                    mesh.addVertex(pos2);


                }
				
                
			}
		}
	}
    
    ofVec3f centroid = getCentroid(mesh);
    xini= centroid.x+w/2;
    yini=centroid.y+h/2;
    	glPointSize(1);
	ofPushMatrix();
	// the projected points are 'upside down' and 'backwards' 
	ofScale(1, -1, -1);
	ofTranslate(0, 0, -1000); // center the points a bit
	glEnable(GL_DEPTH_TEST);
	mesh.drawVertices();
	glDisable(GL_DEPTH_TEST);
	ofPopMatrix();
}

//--------------------------------------------------------------
void testApp::exit() {
	kinect.setCameraTiltAngle(0); // zero the tilt on exit
	kinect.close();
	
#ifdef USE_TWO_KINECTS
	kinect2.close();
#endif
}

//--------------------------------------------------------------
void testApp::keyPressed (int key) {
	switch (key) {
		case ' ':
			bThreshWithOpenCV = !bThreshWithOpenCV;
			break;
			
		case'p':
			bDrawPointCloud = !bDrawPointCloud;
			break;
			
		case '>':
		case '.':
			farThreshold ++;
			if (farThreshold > 255) farThreshold = 255;
			break;
			
		case '<':
		case ',':
			farThreshold --;
			if (farThreshold < 0) farThreshold = 0;
			break;
			
		case '+':
		case '=':
			nearThreshold ++;
			if (nearThreshold > 255) nearThreshold = 255;
			break;
			
		case '-':
			nearThreshold --;
			if (nearThreshold < 0) nearThreshold = 0;
			break;
			
		case 'w':
			kinect.enableDepthNearValueWhite(!kinect.isDepthNearValueWhite());
			break;
			
		case 'o':
			kinect.setCameraTiltAngle(angle); // go back to prev tilt
			kinect.open();
			break;
			
		case 'c':
			kinect.setCameraTiltAngle(0); // zero the tilt
			kinect.close();
			break;
			
		case OF_KEY_UP:
			angle++;
			if(angle>30) angle=30;
			kinect.setCameraTiltAngle(angle);
			break;
			
		case OF_KEY_DOWN:
			angle--;
			if(angle<-30) angle=-30;
			kinect.setCameraTiltAngle(angle);
			break;
	}
}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button)
{}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button)
{}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button)
{}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h)
{}
