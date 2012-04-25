#include "GaussianProcess.h"

#include <iostream>

// alglib
#include "solvers.h"
#include "optimization.h"

using namespace std;
using namespace alglib;

static const double PI = 6*asin(0.5);

CGaussianProcess::CGaussianProcess(void)
{
	m_GP_L = NULL;
	m_GP_y = NULL;
	m_GP_Alpha = NULL;
}

CGaussianProcess::~CGaussianProcess(void)
{
	if(m_GP_L != NULL) cvReleaseMat(&m_GP_L);
	if(m_GP_y != NULL) cvReleaseMat(&m_GP_y);
	if(m_GP_Alpha != NULL) cvReleaseMat(&m_GP_Alpha);
	
	for(int i=0; i<m_ExemplarImages.size(); i++)
		cvReleaseImage(&m_ExemplarImages[i]);
}

void CGaussianProcess::Clear()
{
	if(m_GP_L != NULL) cvReleaseMat(&m_GP_L);
	if(m_GP_y != NULL) cvReleaseMat(&m_GP_y);
	if(m_GP_Alpha != NULL) cvReleaseMat(&m_GP_Alpha);
	
	for(int i=0; i<m_ExemplarImages.size(); i++)
		cvReleaseImage(&m_ExemplarImages[i]);
	
	m_ExemplarImages.clear();
	m_ExemplarLabels.clear();
}

void CGaussianProcess::AddExemplar(IplImage *image, double label)
{
	IplImage *new_image = cvCloneImage(image);
	
	m_ExemplarImages.push_back(new_image);
	m_ExemplarLabels.push_back(label);
}

double CGaussianProcess::CalcCovariance(IplImage *img1, IplImage *img2, double scale, double sigma)
{
	double norm = cvNorm(img1, img2, CV_L2);
	
	return scale*exp(-(norm*norm)/(2*sigma*sigma));
}

void CGaussianProcess::GetCovarianceMatrix(CvMat *K, double noise, double scale, double sigma)
{
	int ex_num = m_ExemplarLabels.size();
	
	for(int i=0; i<ex_num; i++)
	{
		cvmSet(K, i, i, CalcCovariance(m_ExemplarImages[i], m_ExemplarImages[i], scale, sigma) + noise*noise);
		
		for(int j=i+1; j<ex_num; j++)
		{
			cvmSet(K, i, j, CalcCovariance(m_ExemplarImages[i], m_ExemplarImages[j], scale, sigma));
			cvmSet(K, j, i, CalcCovariance(m_ExemplarImages[i], m_ExemplarImages[j], scale, sigma));
		}
	}
}

void CGaussianProcess::CholeskyDecomp(CvMat *K, CvMat *L)
{
	assert(K->rows == K->cols);
	assert(K->rows == L->rows);
	assert(K->rows == L->cols);
	
	real_2d_array a;
	ae_int_t n = K->rows;
	
	a.setlength(n, n);
	for(int row=0; row<n; row++)
	{
		for(int col=0; col<n; col++)
		{
			a[row][col] = cvmGet(K, row, col);
		}
	}
	
	spdmatrixcholesky(a, n, m_CholeskyUpper);
	
	for(int row=0; row<n; row++)
	{
		for(int col=0; col<n; col++)
		{
			cvmSet(L, row, col, a(row,col));
		}
	}
}

void CGaussianProcess::CholeskySolve(CvMat *L, CvMat *y, CvMat *alpha)
{
	assert(L->rows == L->cols);
	assert(L->rows == y->rows);
	assert(L->rows == alpha->rows);
	assert(y->cols == alpha->cols);
	
	real_2d_array cha, b, x;
	ae_int_t n = L->rows;
	ae_int_t m = y->cols;
	ae_int_t info;
	densesolverreport rep;
	
	cha.setlength(n, n);
	b.setlength(n, m);
	x.setlength(n, m);
	for(int row=0; row<n; row++)
	{
		for(int col=0; col<n; col++)
		{
			cha[row][col] = cvmGet(L, row, col);
		}
		
		for(int col=0; col<m; col++)
		{
			b[row][col] = cvmGet(y, row, col);
		}
	}
	
	spdmatrixcholeskysolvem(cha, n, m_CholeskyUpper, b, m, info, rep, x);
	
	for(int row=0; row<n; row++)
	{
		for(int col=0; col<m; col++)
		{
			cvmSet(alpha, row, col, x(row,col));
		}
	}
}

void CGaussianProcess::UpdateGPMatrices(double noise, double scale, double sigma)
{
	int ex_num = m_ExemplarLabels.size();
	
	CvMat *K = cvCreateMat(ex_num, ex_num, CV_64FC1);
	GetCovarianceMatrix(K, noise, scale, sigma);
	
	CholeskyDecomp(K, m_GP_L);
	CholeskySolve(m_GP_L, m_GP_y, m_GP_Alpha);
	
	cvReleaseMat(&K);
}

double CGaussianProcess::GetLOOLPP(double noise, double scale, double sigma)
{
	int ex_num = m_ExemplarLabels.size();
	
	/*----- set up -----*/
	CvMat *K, *L, *E, *Alpha, *Kinv;
	
	K = cvCreateMat(ex_num, ex_num, CV_64FC1);
	L = cvCreateMat(ex_num, ex_num, CV_64FC1);
	E = cvCreateMat(ex_num, ex_num, CV_64FC1);
	Alpha = cvCreateMat(ex_num, 1, CV_64FC1);
	Kinv = cvCreateMat(ex_num, ex_num, CV_64FC1);
	
	GetCovarianceMatrix(K, noise, scale, sigma);
	
	CholeskyDecomp(K, L);
	CholeskySolve(L, m_GP_y, Alpha);
	
	cvSetIdentity(E);
	CholeskySolve(L, E, Kinv);
	/*----- set up -----*/
	
	double sum_lpp = 0.0;
	
	for(int i=0; i<ex_num; i++)
	{
		double v = 1.0/cvmGet(Kinv, i, i);
		double d = cvmGet(Alpha, i, 0)/cvmGet(Kinv, i, i);
		sum_lpp -= 0.5*(log(v) + pow(d,2)/v + log(2.0*PI));
	}
	
	cvReleaseMat(&K);
	cvReleaseMat(&L);
	cvReleaseMat(&E);
	cvReleaseMat(&Alpha);
	cvReleaseMat(&Kinv);
	
	return sum_lpp;
}

void CGaussianProcess::GetDerLOOLPP(double noise, double scale, double sigma, double *d_noise, double *d_scale, double *d_sigma)
{
	int ex_num = m_ExemplarLabels.size();
	
	/*----- set up -----*/
	CvMat *K, *L, *E, *Kinv, *dKda, *dKdb, *dKds, *Za, *Zb, *Zs, *ZaK, *ZbK, *ZsK;
	CvMat *A, *ZaA, *ZbA, *ZsA;
	
	K = cvCreateMat(ex_num, ex_num, CV_64FC1);
	L = cvCreateMat(ex_num, ex_num, CV_64FC1);
	E = cvCreateMat(ex_num, ex_num, CV_64FC1);
	A = cvCreateMat(ex_num, 1, CV_64FC1);
	ZaA = cvCreateMat(ex_num, 1, CV_64FC1);
	ZbA = cvCreateMat(ex_num, 1, CV_64FC1);
	ZsA = cvCreateMat(ex_num, 1, CV_64FC1);
	Kinv = cvCreateMat(ex_num, ex_num, CV_64FC1);
	dKda = cvCreateMat(ex_num, ex_num, CV_64FC1);
	dKdb = cvCreateMat(ex_num, ex_num, CV_64FC1);
	dKds = cvCreateMat(ex_num, ex_num, CV_64FC1);
	Za = cvCreateMat(ex_num, ex_num, CV_64FC1);
	Zb = cvCreateMat(ex_num, ex_num, CV_64FC1);
	Zs = cvCreateMat(ex_num, ex_num, CV_64FC1);
	ZaK = cvCreateMat(ex_num, ex_num, CV_64FC1);
	ZbK = cvCreateMat(ex_num, ex_num, CV_64FC1);
	ZsK = cvCreateMat(ex_num, ex_num, CV_64FC1);
	
	GetCovarianceMatrix(K, noise, scale, sigma);
	
	CholeskyDecomp(K, L);
	CholeskySolve(L, m_GP_y, A);
	
	cvSetIdentity(E);
	CholeskySolve(L, E, Kinv);
	
	// partial derivatives
	cvSetZero(dKdb);
	for(int i=0; i<ex_num; i++)
	{
		cvmSet(dKda, i, i, 1.0);
		cvmSet(dKdb, i, i, 2*noise);
		cvmSet(dKds, i, i, 0.0);
		
		for(int j=i+1; j<ex_num; j++)
		{
			double norm = cvNorm(m_ExemplarImages[i], m_ExemplarImages[j], CV_L2);
			double a_temp = exp(-pow(norm,2)/(2*pow(sigma,2)));
			double s_temp = (scale*pow(norm,2)*exp(-pow(norm,2)/(2*pow(sigma,2))))/pow(sigma,3);
			
			cvmSet(dKda, i, j, a_temp);
			cvmSet(dKda, j, i, a_temp);
			
			cvmSet(dKds, i, j, s_temp);
			cvmSet(dKds, j, i, s_temp);
		}
	}
	
	cvMatMul(Kinv, dKda, Za);
	cvMatMul(Kinv, dKdb, Zb);
	cvMatMul(Kinv, dKds, Zs);
	
	cvMatMul(Za, A, ZaA);
	cvMatMul(Zb, A, ZbA);
	cvMatMul(Zs, A, ZsA);
	
	cvMatMul(Za, Kinv, ZaK);
	cvMatMul(Zb, Kinv, ZbK);
	cvMatMul(Zs, Kinv, ZsK);
	/*----- set up -----*/
	
	for(int i=0; i<ex_num; i++)
	{
		double Kii = cvmGet(Kinv, i, i);
		double a = cvmGet(A, i, 0);
		
		*d_scale += (a*cvmGet(ZaA,i,0) - 0.5*(1.0+pow(a,2)/Kii)*cvmGet(ZaK,i,i))/Kii;
		*d_noise  += (a*cvmGet(ZbA,i,0) - 0.5*(1.0+pow(a,2)/Kii)*cvmGet(ZbK,i,i))/Kii;
		*d_sigma += (a*cvmGet(ZsA,i,0) - 0.5*(1.0+pow(a,2)/Kii)*cvmGet(ZsK,i,i))/Kii;
	}
	
	cvReleaseMat(&K);
	cvReleaseMat(&L);
	cvReleaseMat(&E);
	cvReleaseMat(&Kinv);
	cvReleaseMat(&A);
	cvReleaseMat(&ZaA);
	cvReleaseMat(&ZbA);
	cvReleaseMat(&ZsA);
	cvReleaseMat(&dKda);
	cvReleaseMat(&dKdb);
	cvReleaseMat(&dKds);
	cvReleaseMat(&Za);
	cvReleaseMat(&Zb);
	cvReleaseMat(&Zs);
	cvReleaseMat(&ZaK);
	cvReleaseMat(&ZbK);
	cvReleaseMat(&ZsK);
}

// allocate basic matrices
void CGaussianProcess::SetUp()
{
	assert(m_ExemplarImages.size() == m_ExemplarLabels.size());
	int ex_num = m_ExemplarLabels.size();
	
	//cout << "Total " << ex_num << " points" << endl;
	
	m_Mean = 0.0;
	for(int i=0; i<ex_num; i++)
	{
		m_Mean += m_ExemplarLabels[i];
	}
	m_Mean /= ex_num;
	
	m_Scale = 1.0;
	
	if(m_GP_y != NULL) cvReleaseMat(&m_GP_y);
	if(m_GP_L != NULL) cvReleaseMat(&m_GP_L);
	if(m_GP_Alpha != NULL) cvReleaseMat(&m_GP_Alpha);
	
	m_GP_y = cvCreateMat(ex_num, 1, CV_64FC1);
	for(int i=0; i<ex_num; i++)
	{
		cvmSet(m_GP_y, i, 0, (m_ExemplarLabels[i] - m_Mean)/m_Scale);
	}
	
	m_GP_L = cvCreateMat(ex_num, ex_num, CV_64FC1);
	m_GP_Alpha = cvCreateMat(ex_num, 1, CV_64FC1);
}

void CGaussianProcess::SetUp(double mean, double scale)
{
	assert(m_ExemplarImages.size() == m_ExemplarLabels.size());
	int ex_num = m_ExemplarLabels.size();
	
	//cout << "Total " << ex_num << " points" << endl;
	
	m_Mean = mean;
	m_Scale = scale;
	
	if(m_GP_y != NULL) cvReleaseMat(&m_GP_y);
	if(m_GP_L != NULL) cvReleaseMat(&m_GP_L);
	if(m_GP_Alpha != NULL) cvReleaseMat(&m_GP_Alpha);
	
	m_GP_y = cvCreateMat(ex_num, 1, CV_64FC1);
	for(int i=0; i<ex_num; i++)
	{
		cvmSet(m_GP_y, i, 0, (m_ExemplarLabels[i] - m_Mean)/m_Scale);
	}
	
	m_GP_L = cvCreateMat(ex_num, ex_num, CV_64FC1);
	m_GP_Alpha = cvCreateMat(ex_num, 1, CV_64FC1);
}

double CGaussianProcess::GetMean(IplImage *testimage, double scale, double sigma)
{
	assert(m_ExemplarImages.size() == m_ExemplarLabels.size());
	int ex_num = m_ExemplarLabels.size();
	
	IplImage *new_testimage = cvCloneImage(testimage);
	
	CvMat *KK = cvCreateMat(1, ex_num, CV_64FC1);
	
	for(int i=0; i<ex_num; i++)
	{
		cvmSet(KK, 0, i, CalcCovariance(m_ExemplarImages[i], new_testimage, scale, sigma));
	}
	
	CvMat *f = cvCreateMat(1, 1, CV_64FC1);
	cvMatMul(KK, m_GP_Alpha, f);
	
	double mean = m_Scale*cvmGet(f, 0, 0) + m_Mean;
	
	cvReleaseImage(&new_testimage);
	cvReleaseMat(&KK);
	cvReleaseMat(&f);
	
	return mean;
}
