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


//==================================================================================
// �p���禡
//==================================================================================
// ���] ImgData �j�p
static inline void ImgData_resize(basic_ImgData &dst, int newW, int newH, int bits) {
	dst.raw_img.resize(newW*newH*3);
	dst.width = newW;
	dst.height = newH;
	dst.bits = bits;
};
static inline void ImgData_resize(const basic_ImgData& src, basic_ImgData &dst) {
	dst.raw_img.resize(src.width*src.height*3);
	dst.width = src.width;
	dst.height = src.height;
	dst.bits = src.bits;
};
// ��X bmp
static inline void ImgData_write(basic_ImgData &dst, string name) {
	Raw2Img::raw2bmp(name, dst.raw_img, dst.width, dst.height);
};
// Ū��bmp
static inline void ImgData_read(basic_ImgData &src, std::string name) {
	Raw2Img::read_bmp(src.raw_img, name, &src.width, &src.height, &src.bits);
}



//==================================================================================
// ��g�ഫ
//==================================================================================
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
void WarpPerspective_cut(const basic_ImgData &src, basic_ImgData &dst, 
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
	int dstH = srcH;
	// ��l�� dst
	ImgData_resize(dst, dstW, dstH, src.bits);
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



//==================================================================================
// �V�X
//==================================================================================
// ��ҲV�X
void AlphaBlend(basic_ImgData& matchImg, 
	const basic_ImgData& imgL, const basic_ImgData& imgR) {
	// R �ϥ��ɤW�h
	matchImg=imgR;
	// ��ҲV�X
	int i, j, start, end;
#pragma omp parallel for private(i, j, start, end)
	for(j = 0; j < (int)imgL.height; j++) {
		start = imgL.width;
		end = imgL.width;
		for(i = 0; i <= (int)(imgL.width-1); i++) {
			if( (imgR.raw_img[j*imgR.width*3 + i*3+0] == 0 and 
				imgR.raw_img[j*imgR.width*3 + i*3+1] == 0 and
				imgR.raw_img[j*imgR.width*3 + i*3+2] == 0)
				)
			{
				// �o�̭n�ɭ�� L ��.
				matchImg.raw_img[j*matchImg.width*3 + i*3+0] = 
					imgL.raw_img[j*imgL.width*3 + i*3+0];
				matchImg.raw_img[j*matchImg.width*3 + i*3+1] = 
					imgL.raw_img[j*imgL.width*3 + i*3+1];
				matchImg.raw_img[j*matchImg.width*3 + i*3+2] = 
					imgL.raw_img[j*imgL.width*3 + i*3+2];
			} else {
				if(imgL.raw_img[j*imgL.width*3 + i*3+0] != 0 or
					imgL.raw_img[j*imgL.width*3 + i*3+1] != 0 or
					imgL.raw_img[j*imgL.width*3 + i*3+2] != 0)
				{
					// �o�̬O���|�B.
					if(start==end) {
						start=i; // �����_�Y
					}
					if(start<end) {
						float len = end-start;
						float ratioR = (i-start)/len;
						float ratioL = 1.0 - ratioR;
						matchImg.raw_img[j*matchImg.width*3 + i*3+0] = (unsigned char)//100;
							(imgL.raw_img[j*imgL.width*3 + i*3+0]*ratioL + 
							imgR.raw_img[j*imgR.width*3 + i*3+0]*ratioR);
						matchImg.raw_img[j*matchImg.width*3 + i*3+1] = (unsigned char)//0;
							(imgL.raw_img[j*imgL.width*3 + i*3+1]*ratioL + 
							imgR.raw_img[j*imgR.width*3 + i*3+1]*ratioR);
						matchImg.raw_img[j*matchImg.width*3 + i*3+2] = (unsigned char)//0;
							(imgL.raw_img[j*imgL.width*3 + i*3+2]*ratioL + 
							imgR.raw_img[j*imgR.width*3 + i*3+2]*ratioR);
					}
				}
			}
		}
	}
}
void PasteBlend(basic_ImgData& matchImg, 
	const basic_ImgData& imgL, const basic_ImgData& imgR) {
	// R �ϥ��ɤW�h
	matchImg=imgR;
	// ��ҲV�X
	int i, j, start, end;
#pragma omp parallel for private(i, j, start, end)
	for(j = 0; j < (int)imgL.height; j++) {
		start = imgL.width;
		end = imgL.width;
		for(i = 0; i <= (int)(imgL.width-1); i++) {
			if( (imgR.raw_img[j*imgR.width*3 + i*3+0] == 0 and 
				imgR.raw_img[j*imgR.width*3 + i*3+1] == 0 and
				imgR.raw_img[j*imgR.width*3 + i*3+2] == 0)
				)
			{
				// �o�̭n�ɭ�� L ��.
				matchImg.raw_img[j*matchImg.width*3 + i*3+0] = 
					imgL.raw_img[j*imgL.width*3 + i*3+0];
				matchImg.raw_img[j*matchImg.width*3 + i*3+1] = 
					imgL.raw_img[j*imgL.width*3 + i*3+1];
				matchImg.raw_img[j*matchImg.width*3 + i*3+2] = 
					imgL.raw_img[j*imgL.width*3 + i*3+2];
			} else {
				if(imgL.raw_img[j*imgL.width*3 + i*3+0] != 0 or
					imgL.raw_img[j*imgL.width*3 + i*3+1] != 0 or
					imgL.raw_img[j*imgL.width*3 + i*3+2] != 0)
				{
					// �o�̬O���|�B.
					if(start==end) {
						start=i; // �����_�Y
					}
					if(start<end) {
						float len = end-start;
						float ratioR = (i-start)/len;
						float ratioL = 1.0 - ratioR;
						matchImg.raw_img[j*matchImg.width*3 + i*3+0] =
							imgL.raw_img[j*imgL.width*3 + i*3+0];
						matchImg.raw_img[j*matchImg.width*3 + i*3+1] =
							imgL.raw_img[j*imgL.width*3 + i*3+1];
						matchImg.raw_img[j*matchImg.width*3 + i*3+2] =
							imgL.raw_img[j*imgL.width*3 + i*3+2];
					}
				}
			}
		}
	}
}

// ����ϻP��v�x�}�_�X�Ϥ�
void WarpPers_Stitch(basic_ImgData& matchImg, 
	const basic_ImgData& imgL, const basic_ImgData& imgR, 
	const vector<double>& HomogMat)
{
	// �z����v
	basic_ImgData warpImg;
	WarpPerspective_cut(imgR, warpImg, HomogMat, 0);
	// �_�X�v��
	AlphaBlend(matchImg, imgL, warpImg);
}

//==================================================================================
// ���ը禡
//==================================================================================
// �z���ഫ
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
	basic_ImgData img1, img2;

	 // ����1
    /*const vector<double> HomogMat{
        0.708484   ,  0.00428145 , 245.901,
        -0.103356   ,  0.888676   , 31.6815,
        -0.000390072, -1.61619e-05, 1
    };
    ImgData_read(img1, "srcImg\\sc02.bmp");
    ImgData_read(img2, "srcImg\\sc03.bmp");*/

	// ����4
	/*const vector<double> HomogMat{
		0.7348744865670485, 0.0108833042626221, 243.6991395469133,
		-0.08927309477781344, 0.9070001281140213, 26.59162261058603,
		-0.0003535551339269338, -2.68855175895038e-06, 1
	};
	ImgData_read(img1, "srcImg\\sc02.bmp");
	ImgData_read(img2, "srcImg\\sc03.bmp");*/


    // ����2
    /*const vector<double> HomogMat{
		0.689226, -0.0855788, 285.784,
		-0.088683, 0.850725, 42.3368,
		-0.00043415, -0.000150003, 1,
    };
    ImgData_read(img1, "srcImg\\DSC_2940.bmp");
    ImgData_read(img2, "srcImg\\DSC_2941.bmp");*/
	
	// ����3
	/*const vector<double> HomogMat{
		0.9015176297750432, -0.07853590983276278, 590.1138974282143,
	0.01868191170472197, 0.8896358331521255, -6.256648356599711,
	-3.308436741865926e-05, -6.164729033096104e-05, 1
	};
	ImgData_read(img1, "srcImg\\ball_01.bmp");
	ImgData_read(img2, "srcImg\\ball_02.bmp");	*/
	// ����5
	/*const vector<double> HomogMat{
		0.9015176297750432, -0.07853590983276278, 590.1138974282143,
	0.01868191170472197, 0.8896358331521255, -6.256648356599711,
	-3.308436741865926e-05, -6.164729033096104e-05, 1
	};
	ImgData_read(img1, "srcImg\\ball_01.bmp");
	ImgData_read(img2, "srcImg\\ball_02.bmp");*/

	
	basic_ImgData warpImg2;
	//WarpPerspective(img2);
	WarpPerspective_cut(img2, warpImg2, HomogMat, 0);
	Raw2Img::raw2bmp("__WarpPers.bmp", warpImg2.raw_img, warpImg2.width, warpImg2.height, warpImg2.bits);




	// �V�X�v��
	basic_ImgData warpImg, matchImg;
	WarpPers_Stitch(matchImg, img1, img2, HomogMat);
	// ��X�v��
	string outName = "WarpPers_AlphaBlend.bmp";
	Raw2Img::raw2bmp(outName, matchImg.raw_img, matchImg.width, matchImg.height, matchImg.bits);
}



//==================================================================================
