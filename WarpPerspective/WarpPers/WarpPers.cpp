/*****************************************************************
Name :
Date : 2018/03/15
By   : CharlotteHonG
Final: 2018/03/16
*****************************************************************/
#include <iostream>
#include <vector>
#include <string>
#include <timer.hpp>
using namespace std;

#include "Raw2Img.hpp"
#include "Sharelib.hpp"

#include "WarpPers.hpp"



// ��J dst �y��, ���� scr ��X.
static void WarpPerspective_CoorTranfer_Inve(const vector<double>& HomogMat, double& x, double& y) {
	const double* H = HomogMat.data();
	const double i=x, j=y;

	x = (H[2] - H[8]*i) * (H[4] - H[7]*j) - 
		(H[1] - H[7]*i) * (H[5] - H[8]*j);
	y = (H[0] - H[6]*i) * (H[5] - H[8]*j) - 
		(H[2] - H[8]*i) * (H[3] - H[6]*j);

	double z = (H[1] - H[7]*i) * (H[3] - H[6]*j) - 
		(H[0] - H[6]*i) * (H[4] - H[7]*j);

	x /= z;
	y /= z;
}
// ��J scr �y��, �ഫ dst ��X.
static void WarpPerspective_CoorTranfer(const vector<double>& HomogMat, double& x, double& y) {
	const double* H = HomogMat.data();
	const double i=x, j=y;

	x = H[0]*i + H[1]*y +H[2];
	y = H[3]*i + H[4]*y +H[5];
	double z = H[6]*i + H[7]*y +H[8];

	x /= z;
	y /= z;

	//x=round(x);
	//y=round(y);
}

// �z���ഫ���I ��J(xy*4) ��X(dx, dy, minx, miny, maxx, maxy)
static vector<double> WarpPerspective_Corner(
	const vector<double>& HomogMat, size_t srcW, size_t srcH)
{
	vector<double> cn = {
		0, 0,  (double)(srcW-1.0), 0,
		0, (double)(srcH-1.0),   ((double)srcW-1.0), ((double)srcH-1.0)
	};
	// �z���ഫ
	for(size_t i = 0; i < 4; i++) {
		//cout << cn[i*2+0] << ", " << cn[i*2+1] << "----->";
		WarpPerspective_CoorTranfer(HomogMat, cn[i*2+0], cn[i*2+1]);
		cn[i*2+0] = round(cn[i*2+0]);
		cn[i*2+1] = round(cn[i*2+1]);
		//cout << cn[i*2+0] << ", " << cn[i*2+1] << endl;
	}
	int max, min;
	// �� x �̤j�̤p
	max=INT_MIN, min=INT_MAX;
	for(size_t i = 0; i < 4; i++) {
		if(cn[i*2] > max) max=cn[i*2+0];
		if(cn[i*2] < min) min=cn[i*2+0];
	} cn[0] = max-min, cn[2] = min, cn[4] = max;
	// �� y �̤j�̤p
	max=INT_MIN, min=INT_MAX;
	for(size_t i = 0; i < 4; i++) {
		if(cn[i*2 +1] > max) max=cn[i*2+1];
		if(cn[i*2 +1] < min) min=cn[i*2+1];
	} cn[1] = max-min, cn[3] = min, cn[5] = max;
	// �ϥN�G��M�䥿�T��
	double dstW, dstH, tarW=cn[4], tarH=cn[5];
	for(size_t i=0, bw=0, bh=0; i < 100; i++) {
		double w=tarW+i, h=tarH+i;
		WarpPerspective_CoorTranfer_Inve(HomogMat, w, h);
		if(w > srcW and bw == 0) {
			bw=1;
			dstW=cn[4]+i;
		}
		if(h > srcH and bh == 0) {
			bh=1;
			dstH=cn[5]+i;
		}
		if(bh==1 and bw==1) break;
	}
	//cout << "final=" << dstW << ", " << dstH << endl;
	cn[0] = dstW-cn[2], cn[1] = dstH-cn[3];

	return cn;
}

// �Ϲ��z���ഫ ImgRaw_basic ����
void WarpPerspective(const basic_ImgData &src, basic_ImgData &dst, 
	const vector<double> &H, bool clip=0)
{
	int srcW = src.width;
	int srcH = src.height;
	// ��o�ഫ��̤j���I
	vector<double> cn = WarpPerspective_Corner(H, srcW, srcH);
	// �_�I��m
	int miny=0, minx=0;
	if(clip==1) { miny = (int)-cn[3], minx = (int)-cn[2]; }
	// ���I��m
	int dstW = cn[0]+cn[2]+minx;
	int dstH = cn[1]+cn[3]+miny;
	// ��l�� dst
	dst.raw_img.resize(dstW * dstH * src.bits/8.0);
	dst.width = dstW;
	dst.height = dstH;
	dst.bits = src.bits;
	// �z����v
#pragma omp parallel
	for (int j = -miny; j < dstH-miny; ++j) {
		for (int i = -minx; i < dstW-minx; ++i){
			double x = i, y = j;
			WarpPerspective_CoorTranfer_Inve(H, x, y);
			if ((x <= (double)srcW-1.0 && x >= 0.0) and
				(y <= (double)srcH-1.0 && y >= 0.0))
			{
				unsigned char* p = &dst.raw_img[(j*dstW + i) *3];
				fast_Bilinear_rgb(p, src, y, x);
			}
		}
	}
}

void test1(string name, const vector<double>& HomogMat) {
	Timer t1;

	basic_ImgData img1, img2;
	Raw2Img::read_bmp(img1.raw_img, name, &img1.width, &img1.height, &img1.bits);
	t1.start();
	WarpPerspective(img1, img2, HomogMat, 0);
	t1.print(" WarpPerspective");

	Raw2Img::raw2bmp("WarpPers1.bmp", img2.raw_img, img2.width, img2.height, img2.bits);
}
// �z���ഫ�_�X�d��
void test_WarpPers_Stitch() {
	Timer t1;
	// �z���x�}
	const vector<double> HomogMat{
		0.708484   ,  0.00428145 , 245.901,
		-0.103356   ,  0.888676   , 31.6815,
		-0.000390072, -1.61619e-05, 1
	};

	// Ū���v��
	basic_ImgData img1, img2;
	Raw2Img::read_bmp(img1.raw_img, "srcImg\\sc02.bmp", &img1.width, &img1.height, &img1.bits);
	Raw2Img::read_bmp(img2.raw_img, "srcImg\\sc03.bmp", &img2.width, &img2.height, &img2.bits);

	// �z����v
	basic_ImgData warpImg, matchImg;
	WarpPerspective(img2, warpImg, HomogMat, 0);
	// �_�X�v��
	AlphaBlend(matchImg, img1, warpImg);

	// ��X�v��
	string outName = "WarpPers_AlphaBlend.bmp";
	Raw2Img::raw2bmp(outName, matchImg.raw_img, matchImg.width, matchImg.height, matchImg.bits);
}
void test_WarpPers_Stitch2(string name1, string name2) {
	Timer t1;
	// �z���x�}
	const vector<double> HomogMat{
		0.708484   ,  0.00428145 , 245.901,
		-0.103356   ,  0.888676   , 31.6815,
		-0.000390072, -1.61619e-05, 1
	};

	// Ū���v��
	basic_ImgData img1, img2;
	Raw2Img::read_bmp(img1.raw_img, name1, &img1.width, &img1.height, &img1.bits);
	Raw2Img::read_bmp(img2.raw_img, name2, &img2.width, &img2.height, &img2.bits);

	// �z����v
	basic_ImgData warpImg, matchImg;
	WarpPerspective(img2, warpImg, HomogMat, 0);
	// �_�X�v��
	AlphaBlend(matchImg, img1, warpImg);

	// ��X�v��
	string outName = "WarpPers_AlphaBlend.bmp";
	Raw2Img::raw2bmp(outName, matchImg.raw_img, matchImg.width, matchImg.height, matchImg.bits);
}