/*****************************************************************
Name :
Date : 2018/03/28
By   : CharlotteHonG
Final: 2018/03/28
*****************************************************************/
#include <iostream>
#include <vector>
#include <string>
#include <timer.hpp>
using namespace std;

#include "Raw2Img.hpp"
#include "Sharelib.hpp"
#include "WarpScale.hpp"
#include "GauBlur.hpp"

// �ӷ��ۦP���ҥ~
class file_same : public std::runtime_error {
public:
	file_same(const string& str): std::runtime_error(str) {}
};
// ��������
float gau_meth(size_t r, float p) {
	float two = 2.0;
	float num = exp(-pow(r, two) / (two*pow(p, two)));
	num /= sqrt(two*M_PI)*p;
	return num;
}
// �����x�} (mat_len defa=3)
vector<float> gau_matrix(float p, size_t mat_len) {
	vector<float> gau_mat;
	// �p��x�}����
	if (mat_len == 0) {
		//mat_len = (int)(((p - 0.8) / 0.3 + 1.0) * 2.0);// (�C��o��������)
		mat_len = (int)(round((p*6 + 1))) | 1; // (opencv������)
	}
	// �_�ƭץ�
	if (mat_len % 2 == 0) { ++mat_len; }
	// �@�������x�}
	gau_mat.resize(mat_len);
	float sum = 0;
	for (int i = 0, j = mat_len / 2; j < mat_len; ++i, ++j) {
		float temp;
		if (i) {
			temp = gau_meth(i, p);
			gau_mat[j] = temp;
			gau_mat[mat_len - j - 1] = temp;
			sum += temp += temp;
		}
		else {
			gau_mat[j] = gau_meth(i, p);
			sum += gau_mat[j];
		}
	}
	// �k�@��
	for (auto&& i : gau_mat) { i /= sum; }
	return gau_mat;
}

// �����ҽk
using types=float;
void GauBlur(const basic_ImgData& src, basic_ImgData& dst, float p, size_t mat_len)
{
	/*vector<unsigned char> raw_img = src.raw_img;
	Raw2Img::raw2gray(raw_img);*/

	size_t width  = src.width;
	size_t height = src.height;


	// �ӷ����i�ۦP
	if (&dst == &src) {
		throw file_same("## Erroe! in and out is same.");
	}
	vector<types> gau_mat = gau_matrix(p, mat_len);

	// ��l�� dst
	dst.raw_img.resize(width*height * src.bits/8.0);
	dst.width  = width;
	dst.height = height;
	dst.bits   = src.bits;


	// �w�s
	vector<types> img_gauX(width*height*3);

	// �����ҽk X �b
	const size_t r = gau_mat.size() / 2;

	int i, j, k;
#pragma omp parallel for private(i, j, k)
	for (j = 0; j < height; ++j) {
		for (i = 0; i < width; ++i) {
			double sumR = 0;
			double sumG = 0;
			double sumB = 0;
			for (k = 0; k < gau_mat.size(); ++k) {
				int idx = i-r + k;
				// idx�W�X��t�B�z
				if (idx < 0) {
					idx = 0;
				} else if (idx >(int)(width-1)) {
					idx = (width-1);
				}
				sumR += (double)src.raw_img[(j*width + idx)*3 + 0] * gau_mat[k];
				sumG += (double)src.raw_img[(j*width + idx)*3 + 1] * gau_mat[k];
				sumB += (double)src.raw_img[(j*width + idx)*3 + 2] * gau_mat[k];
			}
			img_gauX[(j*width + i)*3 + 0] = sumR;
			img_gauX[(j*width + i)*3 + 1] = sumG;
			img_gauX[(j*width + i)*3 + 2] = sumB;
		}
	}
	// �����ҽk Y �b
#pragma omp parallel for private(i, j, k)
	for (j = 0; j < height; ++j) {
		for (i = 0; i < width; ++i) {
			double sumR = 0;
			double sumG = 0;
			double sumB = 0;
			for (k = 0; k < gau_mat.size(); ++k) {
				int idx = j-r + k;
				// idx�W�X��t�B�z
				if (idx < 0) {
					idx = 0;
				} else if (idx > (int)(height-1)) {
					idx = (height-1);
				}
				sumR += img_gauX[(idx*width + i)*3 + 0] * gau_mat[k];
				sumG += img_gauX[(idx*width + i)*3 + 1] * gau_mat[k];
				sumB += img_gauX[(idx*width + i)*3 + 2] * gau_mat[k];

			}
			dst.raw_img[(j*width + i)*3 + 0] = sumR;
			dst.raw_img[(j*width + i)*3 + 1] = sumG;
			dst.raw_img[(j*width + i)*3 + 2] = sumB;
		}
	}
}

void test_GauBlur() {
	Timer t1;
	// Ū���v��
	basic_ImgData img1, img2;
	Raw2Img::read_bmp(img1.raw_img, "sc02.bmp", &img1.width, &img1.height, &img1.bits);
	
	t1.start();
	GauBlur(img1, img2, 1.6, 0);
	t1.print(" WarpScale");

	Raw2Img::raw2bmp("test_GauBlur.bmp", img2.raw_img, img2.width, img2.height, 24);
}