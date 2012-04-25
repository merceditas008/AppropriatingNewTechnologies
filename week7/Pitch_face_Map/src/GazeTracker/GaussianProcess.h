/*
 *  GaussianProcess
 *
 *  Gaussian process regression for appearance-based gaze estimation
 *  ref. Rasmussen and Williams, "Gaussian Processes for Machine Learning"
 *  http://www.gaussianprocess.org/gpml/chapters/
 *
 *  Created by yugano on 11/01/25.
 *
 */

#ifndef GaussianProcess_h
#define GaussianProcess_h

#include <vector>
#include "cv.h"

class CGaussianProcess
{
public:
	CGaussianProcess(void);
	~CGaussianProcess(void);
	
	void AddExemplar(IplImage *image, double label);
	double GetMean(IplImage *testimage, double scale, double sigma);
	
	// leave-one-out cross validation
	double GetLOOLPP(double noise, double scale, double sigma);
	void GetDerLOOLPP(double noise, double scale, double sigma, double *d_noise, double *d_scale, double *d_sigma);
	
	void SetUp();
	void SetUp(double mean, double scale);
	void Clear();
	void UpdateGPMatrices(double noise, double scale, double sigma);
	
private:
	std::vector<IplImage*> m_ExemplarImages;
	std::vector<double> m_ExemplarLabels;
	
	double m_Mean, m_Scale;
	CvMat *m_GP_L, *m_GP_y, *m_GP_Alpha;
	
	// cholesky solver
	static const bool m_CholeskyUpper = false;
	void CholeskyDecomp(CvMat *K, CvMat *L);
	void CholeskySolve(CvMat *L, CvMat *y, CvMat *alpha);
	
	void GetCovarianceMatrix(CvMat *K, double noise, double scale, double sigma);
	
	double CalcCovariance(IplImage *img1, IplImage *img2, double scale, double sigma);
};

#endif