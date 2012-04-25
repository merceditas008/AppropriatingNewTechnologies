#include "GazeEstimator.h"

#include <iostream>

// alglib
#include "solvers.h"
#include "optimization.h"

#define GazeEstimator_debug

using namespace std;
using namespace cv;
using namespace alglib;

static const double PI = 6*asin(0.5);

// global function for GC optimization
void GradientWrapperForGC(const real_1d_array &x, double &func, real_1d_array &grad, void *ptr)
{
	CGazeEstimator *ge = (CGazeEstimator*)ptr;
	
	double noisex = x[0];
	double noisey = x[1];
	double scale = x[2];
	double sigma = x[3];
	
	double d_noisex, d_noisey, d_scale, d_sigma;
	func = ge->GetGradient(noisex, noisey, scale, sigma, 
												 &d_noisex, &d_noisey, &d_scale, &d_sigma);
	
#ifdef GazeEstimator_debug
	cout << d_noisex << ", " << d_noisey << ", " << d_scale << ", " << d_sigma << ": " << -func << endl;
#endif
	
	grad[0] = d_noisex;
	grad[1] = d_noisey;
	grad[2] = d_scale;
	grad[3] = d_sigma;
}


/* CGazeEstimator Class */

CGazeEstimator::CGazeEstimator(void)
{
	m_isUpdated = false;
}

CGazeEstimator::~CGazeEstimator(void)
{
}

void CGazeEstimator::AddTrainingData(const Mat &image, const Point2f gaze)
{
	m_TrainingEyes.push_back(image.clone());
	IplImage ipl_image = image;
	m_GPx.AddExemplar(&ipl_image, gaze.x);
	m_GPy.AddExemplar(&ipl_image, gaze.y);
}

double CGazeEstimator::GetGradient(double noisex, double noisey, double scale, double sigma, 
																	 double *d_noisex, double *d_noisey, double *d_scale, double *d_sigma)
{
	// minimize -> maximize
	*d_noisex = 0.0, *d_noisey = 0.0, *d_sigma = 0.0, *d_scale = 0.0;
	
	m_GPx.GetDerLOOLPP(noisex, scale, sigma, d_noisex, d_scale, d_sigma);
	m_GPy.GetDerLOOLPP(noisey, scale, sigma, d_noisey, d_scale, d_sigma);
	
	*d_noisex *= -1;
	*d_noisey *= -1;
	*d_sigma *= -1;
	*d_scale *= -1;
	
	double val = m_GPx.GetLOOLPP(noisex, scale, sigma) + m_GPy.GetLOOLPP(noisey, scale, sigma);
	
	return -val;
}

void CGazeEstimator::UpdateParameters()
{
	real_1d_array x;
	double epsg = 0;
	double epsf = 1E-3;
	double epsx = 0;
	double stpmax = 1.0;
	ae_int_t maxits = 100;
	mincgstate state;
	mincgreport rep;
	
	// initial states
	double init = m_GPx.GetLOOLPP(m_NoiseX, m_Scale, m_Sigma) + m_GPy.GetLOOLPP(m_NoiseY, m_Scale, m_Sigma);
	x.setlength(4);
	x[0] = m_NoiseX;
	x[1] = m_NoiseY;
	x[2] = m_Scale;
	x[3] = m_Sigma;
	
	mincgcreate(x, state);
	mincgsetcond(state, epsg, epsf, epsx, maxits);
	mincgsetstpmax(state, stpmax);
	mincgoptimize(state, GradientWrapperForGC, NULL, this);
	mincgresults(state, x, rep);
	
	// parse results
	m_NoiseX = abs(x[0]);
	m_NoiseY = abs(x[1]);
	m_Scale = abs(x[2]);
	m_Sigma = abs(x[3]);
	
#ifdef GazeEstimator_debug
	double result = m_GPx.GetLOOLPP(m_NoiseX, m_Scale, m_Sigma) + m_GPy.GetLOOLPP(m_NoiseY, m_Scale, m_Sigma);
	cout << "Updated parameters:" << endl;
	cout << "m_NoiseX = " << m_NoiseX << endl;
	cout << "m_NoiseY = " << m_NoiseY << endl;
	cout << "m_Scale = " << m_Scale << endl;
	cout << "m_Sigma = " << m_Sigma << endl;
#endif
}

void CGazeEstimator::Reset()
{
	m_GPx.Clear();
	m_GPy.Clear();
	m_TrainingEyes.clear();
	
	m_isUpdated = false;
}

void CGazeEstimator::Initialize(int areaWidth, int areaHeight)
{
	// check image variance
	int num_eyes = m_TrainingEyes.size();
	Mat meanEye = Mat::zeros(m_TrainingEyes[0].size(), CV_32F);
	Mat sqmeanEye = Mat::zeros(m_TrainingEyes[0].size(), CV_32F);
	for(int i=0; i<num_eyes; i++)
	{
		accumulate(m_TrainingEyes[i], meanEye);
		accumulateSquare(m_TrainingEyes[i], sqmeanEye);
	}
	meanEye = (1.0/num_eyes)*meanEye;
	sqmeanEye = (1.0/num_eyes)*sqmeanEye;
	
	pow(meanEye, 2, meanEye);
	Mat varEye = sqmeanEye - meanEye;
	float var = sqrt(sum(varEye).val[0]);
	
	// initial parameters
	m_NoiseX = 0.001;
	m_NoiseY = 0.001;
	m_Scale = 0.2;
	m_Sigma = 2*var;
	
	m_GPx.SetUp(areaWidth/2, areaWidth/2);
	m_GPy.SetUp(areaHeight/2, areaHeight/2);
	
	UpdateParameters();
	
	m_GPx.UpdateGPMatrices(m_NoiseX, m_Scale, m_Sigma);
	m_GPy.UpdateGPMatrices(m_NoiseY, m_Scale, m_Sigma);
	
	m_isUpdated = true;
}

Point2f CGazeEstimator::GetEstimation(const Mat &image)
{
	if(!m_isUpdated) return Point2f(0,0);
	
	Point2f result;
	IplImage ipl_image = image;
	result.x = m_GPx.GetMean(&ipl_image, m_Scale, m_Sigma);
	result.y = m_GPy.GetMean(&ipl_image, m_Scale, m_Sigma);
	
	return result;
}
