//
//  CGazeEstimator
//  
//  Wrapper class for gaze tracking algorithm
//
//  Created by ysugano on 12/02/26.
//

#ifndef GazeTracker_h
#define GazeTracker_h

#include "GaussianProcess.h"

class CGazeEstimator
{
public:
	CGazeEstimator(void);
	~CGazeEstimator(void);
	
	void Reset();
	void Initialize(int areaWidth, int areaHeight);
	void AddTrainingData(const cv::Mat &image, const cv::Point2f gaze);
	cv::Point2f GetEstimation(const cv::Mat &image);
	
	//void AddExemplar(IplImage *image, double labelx, double labely);
	//bool GetMean(IplImage *testimage, double mean[2]);
	
	// public function for parameter optimization
	double GetGradient(double noisex, double noisey, double scale, double sigma, 
										 double *d_noisex, double *d_noisey, double *d_scale, double *d_sigma);
	
private:
	CGaussianProcess m_GPx, m_GPy;
	std::vector<cv::Mat> m_TrainingEyes;
	
	bool m_isUpdated;
	
	// parameters
	double m_Scale, m_Sigma, m_NoiseX, m_NoiseY;
	
	void UpdateParameters();
	
};

#endif
