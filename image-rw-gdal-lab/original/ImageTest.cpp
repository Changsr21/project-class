// ImageTest.cpp : 定义控制台应用程序的入口点。

#include "stdafx.h"
#include "string.h"
#include <io.h>

#include "gdal_priv.h"
#include "ogr_srs_api.h"
#include "ogr_spatialref.h"

#include <iostream>   
#include <iomanip> 

using namespace std;

#define MAX_IMAGE_BAND 512

typedef struct
{
	//波段数
	unsigned int bandCount;
	//图像像素宽
	unsigned int width;
	//图像像素高
	unsigned int height;
	//每像素量化bit数
	unsigned int pixelBit;
	//字节数据区
	unsigned char* pData[MAX_IMAGE_BAND];
	//数据类型
	GDALDataType gdalDataType;
}ImageData;

bool Img_ReadFile(const char* chImgFileName, ImageData* pImageData)
{
 	if (_access(chImgFileName, 0) != 0) 	{ printf("input image file not exist");	 return false; 	}

	GDALDataset* poDataset = NULL;

	poDataset = (GDALDataset*)GDALOpen(chImgFileName, GA_ReadOnly);
	if (poDataset == NULL) { printf("open image file error!"); return false; }
		
	pImageData->width = GDALGetRasterXSize(poDataset);
	pImageData->height = GDALGetRasterYSize(poDataset);

	pImageData->bandCount = GDALGetRasterCount(poDataset);

	GDALRasterBandH hRasterBand = GDALGetRasterBand(poDataset, 1);
	GDALDataType gdalDataType = GDALGetRasterDataType(hRasterBand);
	pImageData->pixelBit = GDALGetDataTypeSize(gdalDataType);
	pImageData->gdalDataType = gdalDataType;

	if (pImageData->bandCount > MAX_IMAGE_BAND)	{ printf("image band count > MAX_IMAGE_BAND");	return false;}

	unsigned int nBytes = pImageData->pixelBit / 8;
	for (unsigned int i = 0; i < pImageData->bandCount; i++)
	{
		pImageData->pData[i] = new unsigned char[pImageData->width * pImageData->height * nBytes];
		memset(pImageData->pData[i], 0, sizeof(unsigned char) * pImageData->width * pImageData->height * nBytes);
	}

	for (unsigned int i = 0; i < pImageData->bandCount; i++)
	{
		CPLErr err = poDataset->GetRasterBand(i + 1)->RasterIO(GF_Read, 0, 0, pImageData->width, pImageData->height,
			pImageData->pData[i], pImageData->width, pImageData->height, gdalDataType, 0, 0);
		if (err == CE_Failure){	printf("read image data error"); return false;}
	}

	GDALClose(poDataset);

	return true;
}

bool Img_WriteFile(ImageData *pImageData, const char* chImgFileName)
{
	CPLSetConfigOption("GDAL_FILENAME_IS_UTF8", "NO");

	GDALDriver *pMemDriver = NULL;
	pMemDriver = GetGDALDriverManager()->GetDriverByName("GTIFF");
	if (pMemDriver == NULL) return false;

	GDALDataType gdalDataType = pImageData->gdalDataType;

	GDALDataset * pMemDataSet = pMemDriver->Create(chImgFileName, pImageData->width, pImageData->height, pImageData->bandCount, gdalDataType, NULL);
	if (pMemDataSet == NULL) { printf("create image file error");  return false; }
	
	GDALRasterBand *hRasterBand = NULL;
	for (unsigned int i = 1; i <= pImageData->bandCount; i++)
	{
		hRasterBand = pMemDataSet->GetRasterBand(i);
		CPLErr err = hRasterBand->RasterIO(GF_Write, 0, 0, pImageData->width, pImageData->height, pImageData->pData[i - 1], pImageData->width, pImageData->height, gdalDataType, 0, 0);
		if (err == CE_Failure){ printf("write image data error"); return false; }
	}

	GDALClose(pMemDataSet);

	return true;
}

bool Img_InitialDataBuf(ImageData* pImageData, unsigned int nWidth, unsigned int nHeight, unsigned int nBandCount, GDALDataType gdalDataType)
{
	if (nBandCount > MAX_IMAGE_BAND) { printf("image band count > MAX_IMAGE_BAND");	return false; }

	pImageData->width = nWidth;
	pImageData->height = nHeight;
	pImageData->bandCount = nBandCount;
	pImageData->gdalDataType = gdalDataType;

	if (gdalDataType == GDT_Byte) pImageData->pixelBit = 8;
	else if (gdalDataType == GDT_UInt16) pImageData->pixelBit = 16;
	else {	printf("not supported data type"); return  false; }

	unsigned int nBytes = pImageData->pixelBit / 8;
	for (unsigned int i = 0; i < nBandCount; i++)
	{
		pImageData->pData[i] = new unsigned char[nWidth * nHeight * nBytes];
		memset(pImageData->pData[i], 0, sizeof(unsigned char)*nWidth*nHeight*nBytes);
	}

	return true;
}

unsigned short Img_GetPixel(ImageData *pImageData, unsigned int x, unsigned int y, unsigned int band)
{
	unsigned short nPixel = 0;
	if (pImageData->gdalDataType == GDT_Byte)
	{
		nPixel = pImageData->pData[band][y*pImageData->width + x];
	}
	else if (pImageData->gdalDataType == GDT_UInt16)
	{
		unsigned short *temp = (unsigned short*)(pImageData->pData[band] + y*pImageData->width * 2 + x * 2);
		nPixel = *temp;
	}
	else
	{
		printf("not supported data type");
	}
	return nPixel;
}

bool Img_SetPixel(ImageData *pImageData, unsigned int x, unsigned int y, unsigned int band, unsigned short nPixel)
{
	if (pImageData->gdalDataType == GDT_Byte)
	{
		unsigned char temp1 = nPixel & 0xFF;
		pImageData->pData[band][y*pImageData->width + x] = temp1;
	}
	else if (pImageData->gdalDataType == GDT_UInt16)
	{
		unsigned char *chTemp = (unsigned char*)&nPixel;
		memcpy(pImageData->pData[band] + y*pImageData->width * 2 + x * 2, chTemp, 2);
	}
	else
	{
		printf("not supported data type"); return false;
	}

	return true;
}

bool Img_ReleaseDataBuf(ImageData* pImageData)
{
	for (unsigned int i = 0; i < pImageData->bandCount; i++)
	{
		if (pImageData->pData[i] != NULL) delete pImageData->pData[i];
	}
	return true;
}

int main(int argc, char* argv[])
{
	GDALAllRegister();

	string strImgFile = "F:\\test.tif";
	ImageData image;
	if(!Img_ReadFile(strImgFile.c_str(), &image)) return 0;

	string strImgFile1 = "F:\\test1.tif";
	if (!Img_WriteFile(&image, strImgFile1.c_str())) return 0;

	ImageData image2;
	if (!Img_InitialDataBuf(&image2, image.width, image.height, image.bandCount, image.gdalDataType)) return 0;
	for (unsigned int b = 0; b < image.bandCount; b++)
	{
		for (unsigned int w = 0; w < image.width; w++)
		{
			for (unsigned int h = 0; h < image.height; h++)
			{
				unsigned short pixel = Img_GetPixel(&image, w, h, b);
				Img_SetPixel(&image2, w, h, b, pixel);
			}
		}
	}
	string strImgFile2 = "F:\\test2.tif";
	if (!Img_WriteFile(&image2, strImgFile2.c_str())) return 0;

	Img_ReleaseDataBuf(&image);
	Img_ReleaseDataBuf(&image2);

	return 1;
}

