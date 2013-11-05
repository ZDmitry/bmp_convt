#pragma once

#include <windows.h>

class BitmapSaver {	
public:  
	void convertBMP(const TCHAR * filename);
	void saveAs8Bit(const TCHAR * fileName, BITMAPINFOHEADER *infoHeader, BYTE * data, int DataBufferSize);
	void saveAs16Bit(const TCHAR * filename, BITMAPINFOHEADER *infoHeader, BYTE * data, int DataBufferSize);
};