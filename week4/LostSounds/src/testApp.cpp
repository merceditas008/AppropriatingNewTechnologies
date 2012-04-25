#include "testApp.h"

void testApp::setup() {
	ofSetVerticalSync(true);

	plotHeight = 128;
	bufferSize = 512;
    
    ///////////load the tracks 
    ///orig+high correspond to lost sounds coming from the convertion form the aiff file 44100 to mp3_192kbps
    track1.loadSound("orig+high.wav");
    ///orig+med correspond to lost sounds coming from the convertion form the aiff file 44100 to mp3_160kbps
    track2.loadSound("orig+med.wav");
    ///orig+high correspond to lost sounds coming from the convertion form the aiff file 44100 to mp3_128kbps
    track3.loadSound("orig+low.wav");

    

	//fft = ofxFft::create(bufferSize, OF_FFT_WINDOW_HAMMING);
	// To use FFTW, try:
	fft = ofxFft::create(bufferSize, OF_FFT_WINDOW_HAMMING, OF_FFT_FFTW);

	spectrogram.allocate(512, fft->getBinSize(), OF_IMAGE_GRAYSCALE);
	memset(spectrogram.getPixels(), 0, (int) (spectrogram.getWidth() * spectrogram.getHeight()) );
	spectrogramOffset = 0;

	drawBuffer.resize(bufferSize);
	middleBuffer.resize(bufferSize);
	audioBuffer.resize(bufferSize);
	
	drawBins.resize(fft->getBinSize());
	middleBins.resize(fft->getBinSize());
	audioBins.resize(fft->getBinSize());

	// 0 output channels,
	// 1 input channel
	// 44100 samples per second
	// [bins] samples per buffer
	// 4 num buffers (latency)

	ofSoundStreamSetup(0, 1, this, 44100, bufferSize, 4);

	mode = MIC;
	appWidth = ofGetWidth();
	appHeight = ofGetHeight();

	ofBackground(0, 0, 0);
    track1.play();
    track2.play();
    track3.play();
}

void testApp::draw() {
    if((10 < mouseX)&&(mouseX<160)&&(mouseY>600)&&(mouseY<640)){
        cout<<"track1"<<endl;
        track1.setVolume(100);
        track2.setVolume(0);
        track3.setVolume(0);
    }
    else if((200 < mouseX)&&(mouseX<350)&&(mouseY>600)&&(mouseY<640)){
        track1.setVolume(0);
        track2.setVolume(100);
        track3.setVolume(0);
        cout<<"track2"<<endl;

    }
    else if((380 <mouseX)&&(mouseX<530)&&(mouseY>600)&&(mouseY<640)){
        track1.setVolume(0);
        track2.setVolume(0);
        track3.setVolume(100);
        cout<<"track3"<<endl;

    }
    else{
        track1.setVolume(0);
        track2.setVolume(0);
        track3.setVolume(0);
    }
    ///////////DRAW BUTTONS TO SELECT THE DIFFERENT TRACKS
    ofSetColor(72,209,204);
    ofFill();
    
    ofRect(10, 600, 150, 40);
    ofRect(200, 600, 150, 40);
    ofRect(380, 600, 150, 40);
    ofSetColor(255);
    ofDrawBitmapString("mp3_192kbps",30, 625);
    ofDrawBitmapString("mp3_160kbps", 220, 625);
    ofDrawBitmapString("mp3_128kbps", 400, 625);



	ofPushMatrix();
	ofTranslate(16, 16);
	ofDrawBitmapString("Time Domain", 0, 0);
	
	soundMutex.lock();
	drawBuffer = middleBuffer;
	drawBins = middleBins;
	soundMutex.unlock();
	
	plot(drawBuffer, plotHeight / 2, 0);
	ofTranslate(0, plotHeight + 16);
	ofDrawBitmapString("Frequency Domain", 0, 0);
	plot(drawBins, -plotHeight, plotHeight / 2);
	ofTranslate(0, plotHeight + 16);
	spectrogram.update();
	spectrogram.draw(0, 0);
	ofPopMatrix();
	
}

void testApp::plot(vector<float>& buffer, float scale, float offset) {
	ofNoFill();
	int n = buffer.size();
	ofRect(0, 0, n, plotHeight);
	glPushMatrix();
	glTranslatef(0, plotHeight / 2 + offset, 0);
	ofBeginShape();
	for (int i = 0; i < n; i++) {
		ofVertex(i, buffer[i] * scale);
	}
	ofEndShape();
	glPopMatrix();
}

void testApp::audioReceived(float* input, int bufferSize, int nChannels) {
	if (mode == MIC) {
		// store input in audioInput buffer
		memcpy(&audioBuffer[0], input, sizeof(float) * bufferSize);
		
		float maxValue = 0;
		for(int i = 0; i < bufferSize; i++) {
			if(abs(audioBuffer[i]) > maxValue) {
				maxValue = abs(audioBuffer[i]);
			}
		}
		for(int i = 0; i < bufferSize; i++) {
			audioBuffer[i] /= maxValue;
		}
		
	} 
	
	fft->setSignal(&audioBuffer[0]);

	float* curFft = fft->getAmplitude();
	memcpy(&audioBins[0], curFft, sizeof(float) * fft->getBinSize());

	float maxValue = 0;
	for(int i = 0; i < fft->getBinSize(); i++) {
		if(abs(audioBins[i]) > maxValue) {
			maxValue = abs(audioBins[i]);
		}
	}
	for(int i = 0; i < fft->getBinSize(); i++) {
		audioBins[i] /= maxValue;
	}
	
	int spectrogramWidth = (int) spectrogram.getWidth();
	int spectrogramHeight = (int) spectrogram.getHeight();
	unsigned char* pixels = spectrogram.getPixels();
	for(int i = 0; i < spectrogramHeight; i++) {
		pixels[i * spectrogramWidth + spectrogramOffset] = (unsigned char) (255. * audioBins[i]);
	}
	spectrogramOffset = (spectrogramOffset + 1) % spectrogramWidth;
	
	soundMutex.lock();
	middleBuffer = audioBuffer;
	middleBins = audioBins;
	soundMutex.unlock();
}

void testApp::keyPressed(int key) {
	switch (key) {
	case 'm':
		mode = MIC;
		break;
	case 'n':
		mode = NOISE;
		break;
	case 's':
		mode = SINE;
		break;
	}
}
