// ImageTest.cpp —— 图像读写上机实践（Linux / 跨平台移植版）
// 任务：读取图像 test.tif，将每个像素的灰度值减 10（若小于 0 则为 0），输出图像 test1.tif
// 基于课程提供的 ImageTest_VS2013_64 工程（GDAL）改写，保留 ImageData 结构与
// Img_ReadFile / Img_WriteFile / Img_InitialDataBuf / Img_GetPixel / Img_SetPixel / Img_ReleaseDataBuf 接口。

#include <cstdio>
#include <cstring>
#include <string>
#include <chrono>
#include <iostream>

#ifdef _WIN32
#include <io.h>
#define ACCESS _access
#else
#include <unistd.h>
#define ACCESS access
#endif

#include "gdal_priv.h"
#include "cpl_conv.h"

using namespace std;

#define MAX_IMAGE_BAND 512

typedef struct
{
	unsigned int bandCount;               // 波段数
	unsigned int width;                   // 图像像素宽
	unsigned int height;                  // 图像像素高
	unsigned int pixelBit;                // 每像素量化 bit 数
	unsigned char* pData[MAX_IMAGE_BAND]; // 字节数据区（每个波段一块）
	GDALDataType gdalDataType;            // 数据类型
} ImageData;

static void Img_Zero(ImageData* p)
{
	p->bandCount = p->width = p->height = p->pixelBit = 0;
	for (int i = 0; i < MAX_IMAGE_BAND; i++) p->pData[i] = NULL;
	p->gdalDataType = GDT_Unknown;
}

bool Img_ReadFile(const char* chImgFileName, ImageData* pImageData)
{
	if (ACCESS(chImgFileName, 0) != 0) { printf("input image file not exist: %s\n", chImgFileName); return false; }

	GDALDataset* poDataset = (GDALDataset*)GDALOpen(chImgFileName, GA_ReadOnly);
	if (poDataset == NULL) { printf("open image file error!\n"); return false; }

	pImageData->width = poDataset->GetRasterXSize();
	pImageData->height = poDataset->GetRasterYSize();
	pImageData->bandCount = poDataset->GetRasterCount();

	GDALDataType gdalDataType = poDataset->GetRasterBand(1)->GetRasterDataType();
	pImageData->pixelBit = GDALGetDataTypeSizeBits(gdalDataType);
	pImageData->gdalDataType = gdalDataType;

	if (pImageData->bandCount > MAX_IMAGE_BAND) { printf("image band count > MAX_IMAGE_BAND\n"); GDALClose(poDataset); return false; }
	if (gdalDataType != GDT_Byte && gdalDataType != GDT_UInt16) { printf("not supported data type\n"); GDALClose(poDataset); return false; }

	size_t nBytes = pImageData->pixelBit / 8;
	size_t bufSize = (size_t)pImageData->width * pImageData->height * nBytes;
	for (unsigned int i = 0; i < pImageData->bandCount; i++)
	{
		pImageData->pData[i] = new unsigned char[bufSize];
		memset(pImageData->pData[i], 0, bufSize);
	}

	for (unsigned int i = 0; i < pImageData->bandCount; i++)
	{
		CPLErr err = poDataset->GetRasterBand(i + 1)->RasterIO(GF_Read, 0, 0, pImageData->width, pImageData->height,
			pImageData->pData[i], pImageData->width, pImageData->height, gdalDataType, 0, 0);
		if (err == CE_Failure) { printf("read image data error\n"); GDALClose(poDataset); return false; }
	}

	GDALClose(poDataset);
	return true;
}

bool Img_WriteFile(ImageData* pImageData, const char* chImgFileName)
{
	GDALDriver* pDriver = GetGDALDriverManager()->GetDriverByName("GTiff");
	if (pDriver == NULL) return false;

	// 与输入 test.tif 保持一致：PackBits 无损压缩、像素交叉存储
	char** papszOptions = NULL;
	papszOptions = CSLSetNameValue(papszOptions, "COMPRESS", "PACKBITS");
	papszOptions = CSLSetNameValue(papszOptions, "INTERLEAVE", "PIXEL");

	GDALDataset* pDataSet = pDriver->Create(chImgFileName, pImageData->width, pImageData->height,
		pImageData->bandCount, pImageData->gdalDataType, papszOptions);
	CSLDestroy(papszOptions);
	if (pDataSet == NULL) { printf("create image file error\n"); return false; }

	for (unsigned int i = 1; i <= pImageData->bandCount; i++)
	{
		GDALRasterBand* hRasterBand = pDataSet->GetRasterBand(i);
		CPLErr err = hRasterBand->RasterIO(GF_Write, 0, 0, pImageData->width, pImageData->height,
			pImageData->pData[i - 1], pImageData->width, pImageData->height, pImageData->gdalDataType, 0, 0);
		if (err == CE_Failure) { printf("write image data error\n"); GDALClose(pDataSet); return false; }
	}

	GDALClose(pDataSet);
	return true;
}

bool Img_InitialDataBuf(ImageData* pImageData, unsigned int nWidth, unsigned int nHeight, unsigned int nBandCount, GDALDataType gdalDataType)
{
	if (nBandCount > MAX_IMAGE_BAND) { printf("image band count > MAX_IMAGE_BAND\n"); return false; }

	pImageData->width = nWidth;
	pImageData->height = nHeight;
	pImageData->bandCount = nBandCount;
	pImageData->gdalDataType = gdalDataType;

	if (gdalDataType == GDT_Byte) pImageData->pixelBit = 8;
	else if (gdalDataType == GDT_UInt16) pImageData->pixelBit = 16;
	else { printf("not supported data type\n"); return false; }

	size_t bufSize = (size_t)nWidth * nHeight * (pImageData->pixelBit / 8);
	for (unsigned int i = 0; i < nBandCount; i++)
	{
		pImageData->pData[i] = new unsigned char[bufSize];
		memset(pImageData->pData[i], 0, bufSize);
	}
	return true;
}

unsigned short Img_GetPixel(ImageData* pImageData, unsigned int x, unsigned int y, unsigned int band)
{
	size_t idx = (size_t)y * pImageData->width + x;
	if (pImageData->gdalDataType == GDT_Byte) return pImageData->pData[band][idx];
	if (pImageData->gdalDataType == GDT_UInt16)
	{
		unsigned short v;
		memcpy(&v, pImageData->pData[band] + idx * 2, 2);
		return v;
	}
	printf("not supported data type\n");
	return 0;
}

bool Img_SetPixel(ImageData* pImageData, unsigned int x, unsigned int y, unsigned int band, unsigned short nPixel)
{
	size_t idx = (size_t)y * pImageData->width + x;
	if (pImageData->gdalDataType == GDT_Byte)
		pImageData->pData[band][idx] = (unsigned char)(nPixel & 0xFF);
	else if (pImageData->gdalDataType == GDT_UInt16)
		memcpy(pImageData->pData[band] + idx * 2, &nPixel, 2);
	else { printf("not supported data type\n"); return false; }
	return true;
}

bool Img_ReleaseDataBuf(ImageData* pImageData)
{
	for (unsigned int i = 0; i < pImageData->bandCount; i++)
	{
		delete[] pImageData->pData[i];   // 原工程使用 delete（与 new[] 不匹配），此处修正为 delete[]
		pImageData->pData[i] = NULL;
	}
	return true;
}

int main(int argc, char* argv[])
{
	// 用法：ImageTest [输入图像=test.tif] [输出图像=test1.tif] [减去的值=10]
	string strInFile  = argc > 1 ? argv[1] : "test.tif";
	string strOutFile = argc > 2 ? argv[2] : "test1.tif";
	int nSub          = argc > 3 ? atoi(argv[3]) : 10;

	GDALAllRegister();
	auto t0 = chrono::steady_clock::now();

	ImageData image;  Img_Zero(&image);
	if (!Img_ReadFile(strInFile.c_str(), &image)) return 1;
	auto t1 = chrono::steady_clock::now();

	printf("input : %s\n", strInFile.c_str());
	printf("size  : %u x %u, bands = %u, bits/pixel = %u (%s)\n", image.width, image.height,
		image.bandCount, image.pixelBit, GDALGetDataTypeName(image.gdalDataType));

	ImageData image1; Img_Zero(&image1);
	if (!Img_InitialDataBuf(&image1, image.width, image.height, image.bandCount, image.gdalDataType)) return 1;

	// 核心处理：每个像素（每个波段）灰度值减 nSub，小于 0 则置 0
	unsigned long long nClamped = 0;
	for (unsigned int b = 0; b < image.bandCount; b++)
		for (unsigned int y = 0; y < image.height; y++)       // 按行优先遍历，访存连续
			for (unsigned int x = 0; x < image.width; x++)
			{
				int v = (int)Img_GetPixel(&image, x, y, b) - nSub;
				if (v < 0) { v = 0; nClamped++; }
				Img_SetPixel(&image1, x, y, b, (unsigned short)v);
			}
	auto t2 = chrono::steady_clock::now();

	if (!Img_WriteFile(&image1, strOutFile.c_str())) return 1;
	auto t3 = chrono::steady_clock::now();

	auto ms = [](chrono::steady_clock::duration d) { return chrono::duration<double, milli>(d).count(); };
	printf("output: %s (each pixel value - %d, clamped to 0)\n", strOutFile.c_str(), nSub);
	printf("pixels clamped to 0: %llu\n", nClamped);
	printf("time  : read %.1f ms, process %.1f ms, write %.1f ms\n", ms(t1 - t0), ms(t2 - t1), ms(t3 - t2));

	Img_ReleaseDataBuf(&image);
	Img_ReleaseDataBuf(&image1);
	return 0;
}
