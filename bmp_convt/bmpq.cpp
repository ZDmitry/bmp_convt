
#include "bmpq.h"
#include <tchar.h>
#include <iostream>

void LSPDebug(const TCHAR * fmt, ...)
{
	static const size_t max = 1024;
	va_list args;
	TCHAR * buffer = new TCHAR[max];

	static const TCHAR * suffix = _T(":[DBG]: ");
	static const size_t len = _tcslen(suffix);
	_tcscpy_s(buffer, max, suffix);

	TCHAR * message = buffer + len;
 
	va_start(args, fmt);
	_vsntprintf_s(message, max-len, max-len, fmt, args);
	va_end(args);

	_tcscat_s(buffer,max,_T("\r\n"));
 
	_tprintf_s(buffer);
    
	delete [] buffer;
}

BYTE* ConvertBMPToRGBBuffer ( BYTE* Buffer, int width, int height )
{
	if ( ( NULL == Buffer ) || ( width == 0 ) || ( height == 0 ) )
		return NULL;

	int padding = 0;
	int scanlinebytes = width * 3;
	while ( ( scanlinebytes + padding ) % 4 != 0 )
		padding++;

	int psw = scanlinebytes + padding;

	BYTE* newbuf = (BYTE*) LocalAlloc(LPTR, width*height*3);
	ZeroMemory(newbuf, width*height*3 );

	long bufpos = 0;   
	long newpos = 0;
	for ( int y = 0; y < height; y++ ) 
	{
		for ( int x = 0; x < 3 * width; x+=3 )
		{
			newpos = y * 3 * width + x;     
			bufpos = ( height - y - 1 ) * psw + x;

			newbuf[newpos] = Buffer[bufpos + 2];       
			newbuf[newpos + 1] = Buffer[bufpos+1]; 
			newbuf[newpos + 2] = Buffer[bufpos];     
		}
	}
	
	return newbuf;
}

BYTE* ConvertRGBToBMPBuffer ( BYTE* Buffer, int width, int height, 
	long* newsize )
{
	if ( ( NULL == Buffer ) || ( width == 0 ) || ( height == 0 ) )
		return NULL;

	int padding = 0;
	int scanlinebytes = width * 3;
	while ( ( scanlinebytes + padding ) % 4 != 0 ) 
		padding++;
	int psw = scanlinebytes + padding;

	*newsize = height * psw;
	BYTE* newbuf = (BYTE*) LocalAlloc(LPTR, *newsize);
	ZeroMemory(newbuf, *newsize );

	long bufpos = 0;   
	long newpos = 0;
	for ( int y = 0; y < height; y++ )
	{
		for ( int x = 0; x < 3 * width; x+=3 )
		{
			bufpos = y * 3 * width + x;              // position in original buffer
			newpos = ( height - y - 1 ) * psw + x;   // position in padded buffer
			newbuf[newpos] = Buffer[bufpos+2];       // swap r and b
			newbuf[newpos + 1] = Buffer[bufpos + 1]; // g stays
			newbuf[newpos + 2] = Buffer[bufpos];     // swap b and r
		}
	}

	return newbuf;
}

void BitmapSaver::convertBMP(const TCHAR *filename)
{
	DWORD bytesRead;

    BITMAPFILEHEADER bmpf;             //our bitmap file header
	ZeroMemory(&bmpf, sizeof(bmpf));
	BITMAPINFOHEADER bmpi;
	ZeroMemory(&bmpi, sizeof(bmpi));

    BYTE * bitmapImage = NULL;                 //store image data

	HANDLE filePtr = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	// Flip the biHeight member so that it denotes top-down bitmap 

	ReadFile(filePtr, &bmpf, sizeof(BITMAPFILEHEADER),&bytesRead,NULL);

    //verify that this is a bmp file by check bitmap id
    if (bmpf.bfType !=0x4D42)
    {
        CloseHandle(filePtr);
        return;
    }

    //read the bitmap info header
    ReadFile(filePtr, &bmpi, sizeof(BITMAPINFOHEADER), &bytesRead, NULL);

    //move file point to the begging of bitmap data
	SetFilePointer(filePtr, bmpf.bfOffBits, NULL, FILE_BEGIN); 

    //allocate enough memory for the bitmap image data
	bitmapImage = (BYTE *) LocalAlloc(LPTR, bmpi.biSizeImage);

    //verify memory allocation
    if (!bitmapImage)
    {
        LocalFree(bitmapImage);
        CloseHandle(filePtr);
        return;
    }

    //read in the bitmap image data
	ReadFile(filePtr, bitmapImage, bmpi.biSizeImage, &bytesRead, NULL);

    //make sure bitmap image data was read
    if (bitmapImage == NULL)
    {
        CloseHandle(filePtr);
        return;
    }

	TCHAR fname[MAX_PATH+1] = _T("conv.bmp");
	//LSSetFileName(filename, pOemPDEV->m_config.mOutputPath, dirname, pOemPDEV->m_currentFrame+1);
	//_sntprintf_s(filename, MAX_PATH, MAX_PATH, _T("%s\\%s\\%s_%04i.BMP"), pOemPDEV->m_config.mOutputPath, dirname, dirname, pOemPDEV->m_currentFrame+1);

	//bmpi.biHeight = bmpi.biHeight * -1;

	//BitmapSaver::saveAs8Bit(fname, &bmpi, bitmapImage, bmpi.biSizeImage);
	BitmapSaver::saveAs16Bit(fname, &bmpi, bitmapImage, bmpi.biSizeImage);

    //close file and return bitmap iamge data
    CloseHandle(filePtr);
	LocalFree(bitmapImage);
}

BYTE closestPaletteElement(BYTE elem)
{
	BYTE down = (elem/0x33)*0x33;
	BYTE up   = (elem/0x33 + 1)*0x33;
	BYTE downDist = elem - down;
	BYTE upDist  = up - elem;
	return downDist < upDist ? down : up;
}

BYTE paletteIndex(const RGBQUAD & color)
{
	RGBQUAD paletteColor = {
		closestPaletteElement(color.rgbBlue),
		closestPaletteElement(color.rgbGreen),
		closestPaletteElement(color.rgbRed),
		0
	};
	return (paletteColor.rgbRed / 0x33) * 36 
		+ (paletteColor.rgbGreen / 0x33) * 6 
		+ paletteColor.rgbBlue / 0x33;
}

void BitmapSaver::saveAs8Bit(const TCHAR *filename, BITMAPINFOHEADER *infoHeader, BYTE * data, int DataBufferSize)
{

	LSPDebug(_T("Convertin image from 24 bit into 8 bit bitmap\n"
		_T("size: %d x %d, buffer size is %d, saving to %s\n")
		), infoHeader->biWidth, infoHeader->biHeight, DataBufferSize, filename);
	if (infoHeader->biBitCount != 24) {
		LSPDebug(_T("Bitmap for converting isn't a 24 bit image!\n"));
		return;
	}
	BITMAPINFOHEADER bitmapinfo;
	ZeroMemory(&bitmapinfo, sizeof(bitmapinfo));
	bitmapinfo.biSize = sizeof(bitmapinfo);
	bitmapinfo.biWidth = infoHeader->biWidth;
	bitmapinfo.biHeight = infoHeader->biHeight  < 0 ? infoHeader->biHeight*-1 : infoHeader->biHeight;
	bitmapinfo.biPlanes = 1;
	bitmapinfo.biBitCount = 8;
	bitmapinfo.biCompression = BI_RGB;

    bitmapinfo.biXPelsPerMeter = infoHeader->biXPelsPerMeter;
	bitmapinfo.biYPelsPerMeter = infoHeader->biYPelsPerMeter;

	//std::vector<RGBQUAD> palette;
	const int PALETTE_SIZE = 216;
	RGBQUAD palette[256];
	int index = 0;
	//create palette (using "web safe colors")
	for (int r = 0; r < 256; r+= 0x33) {
		for (int g = 0; g < 256; g += 0x33) {
			for (int b = 0; b < 256; b += 0x33) {
				if (index >= PALETTE_SIZE) {
					LSPDebug(_T("Too many colors in palette o_O index is %d, color is %x %x %x\n"), index, r, g, b);
					break;
				}
				RGBQUAD color = {(BYTE)b, (BYTE)g, (BYTE)r, 0};
				palette[index] = color;				
				++index;
			}
		} 
	}
	for (int i=216; i<256; i++)
	{
		RGBQUAD color = { 0, 0, 0, 0};
		palette[index] = color;				
		++index;
	}
	LSPDebug(_T("Generating palette is done, there are %d colors"), index);
	bitmapinfo.biClrUsed = ( 1 << bitmapinfo.biBitCount );
	//FILE *out = _tfopen(filename, _T("wb"));
	HANDLE out = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!out) {
		LSPDebug(_T("Can't open file for writting! error code: %d"), GetLastError());
	}
	BITMAPFILEHEADER fileHeader;
	ZeroMemory(&fileHeader, sizeof(fileHeader));
	fileHeader.bfType = 0x4d42;
	fileHeader.bfOffBits = (DWORD) (sizeof(BITMAPFILEHEADER) + bitmapinfo.biSize + bitmapinfo.biClrUsed * sizeof (RGBQUAD));

	BYTE* src = ConvertBMPToRGBBuffer( data, bitmapinfo.biWidth, bitmapinfo.biHeight );

	if (out) {       //beginto write bitmap data to file		
		DWORD bytesWritten;

		int padding = 0;
		int scanlinebytes = bitmapinfo.biWidth; // * 3;

		while ( ( scanlinebytes + padding ) % 4 != 0 ) 
			padding++;
		LSPDebug(_T("Padding size: %d"), padding);

		int psw = scanlinebytes + padding;
		int paddedsize = bitmapinfo.biHeight * psw;

		BYTE* dst = (BYTE*) LocalAlloc(LPTR, paddedsize);
		ZeroMemory(dst, paddedsize );

		bitmapinfo.biSizeImage = paddedsize; //( ( bitmapinfo.biWidth * bitmapinfo.biBitCount + 31 ) & ~31 ) / 8 * bitmapinfo.biHeight;
		fileHeader.bfSize = (DWORD) (sizeof(BITMAPFILEHEADER) + bitmapinfo.biSize + bitmapinfo.biClrUsed * sizeof(RGBQUAD) + bitmapinfo.biSizeImage);

		WriteFile(out, &fileHeader, sizeof(fileHeader), &bytesWritten, NULL);
		if ( padding > 0) // infoHeader->biHeight < 0
			bitmapinfo.biHeight *= -1;
		WriteFile(out, &bitmapinfo,  sizeof(bitmapinfo), &bytesWritten, NULL);
		if ( padding > 0) 
			bitmapinfo.biHeight *= -1;
		WriteFile(out, palette, bitmapinfo.biClrUsed * sizeof (RGBQUAD), &bytesWritten, NULL);

		long bufpos = 0;   
		long newpos = 0;
		for ( int y = 0; y < bitmapinfo.biHeight; y++ )
		{
			int pixelIndex = 0;
			for ( int x = 0; x < 3 * bitmapinfo.biWidth; x+=3, pixelIndex++ )
			{
				bufpos = y * 3 * bitmapinfo.biWidth + x;                     // position in original buffer
				newpos = ( bitmapinfo.biHeight - y - 1 ) * psw + pixelIndex; // position in padded buffer

				RGBQUAD sourceColor = { src[bufpos+2], src[bufpos + 1], src[bufpos], 0 };
				dst[newpos] = paletteIndex(sourceColor);
			}
		}

		WriteFile(out, dst, bitmapinfo.biSizeImage, &bytesWritten, NULL);
		LocalFree(src);
		LocalFree(dst);

		CloseHandle(out);
	}
}

WORD mapTo5Bit(int value)
{
	return WORD(((double)value / 256)*32);
}

WORD convert24Color(const RGBQUAD & color)
{
	WORD res = 0;
	res = mapTo5Bit(color.rgbRed);
	res <<= 5;
	res |= mapTo5Bit(color.rgbGreen);
	res <<= 5;
	res |= mapTo5Bit(color.rgbBlue);
	return res;
}


void BitmapSaver::saveAs16Bit(const TCHAR * filename, BITMAPINFOHEADER *infoHeader, BYTE * data, int DataBufferSize)
{

	LSPDebug(_T("Convertin image from 24 bit into 16 bit bitmap\n"
		_T("size: %d x %d, buffer size is %d, saving to %s\n")
		), infoHeader->biWidth, infoHeader->biHeight, DataBufferSize, filename);
	if (infoHeader->biBitCount != 24) {
		LSPDebug(_T("Bitmap for converting isn't a 24 bit image!\n"));
		return;
	}
	BITMAPINFOHEADER bitmapinfo;
	ZeroMemory(&bitmapinfo, sizeof(bitmapinfo));
	bitmapinfo.biSize = sizeof(bitmapinfo);
	bitmapinfo.biWidth = infoHeader->biWidth;
	bitmapinfo.biHeight = infoHeader->biHeight  < 0 ? infoHeader->biHeight*-1 : infoHeader->biHeight;
	bitmapinfo.biPlanes = 1;
	bitmapinfo.biBitCount = 16;
	bitmapinfo.biCompression = BI_RGB;

	bitmapinfo.biXPelsPerMeter = infoHeader->biXPelsPerMeter;
	bitmapinfo.biYPelsPerMeter = infoHeader->biYPelsPerMeter;

	bitmapinfo.biClrUsed = 0;

	HANDLE out = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (!out) {
		LSPDebug(_T("Can't open file for writting! error code: %d"), GetLastError());
	}
	BITMAPFILEHEADER fileHeader;
	ZeroMemory(&fileHeader, sizeof(fileHeader));
	fileHeader.bfType = 0x4d42;
	fileHeader.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) + bitmapinfo.biSize + bitmapinfo.biClrUsed * sizeof (RGBQUAD);
	
	BYTE* src = ConvertBMPToRGBBuffer( data, bitmapinfo.biWidth, bitmapinfo.biHeight );

	if (out) {       //beginto write bitmap data to file		
		DWORD bytesWritten;

		int padding = 0;
		int scanlinebytes = bitmapinfo.biWidth * 2; // WORD = 2 BYTE 

		while ( ( scanlinebytes + padding ) % 4 != 0 ) 
			padding++;
		LSPDebug(_T("Padding size: %d\r\n"), padding);

		int psw = scanlinebytes + padding;
		int paddedsize = bitmapinfo.biHeight * psw * 2;

		WORD* dst = (WORD*) LocalAlloc(LPTR, paddedsize);
		ZeroMemory(dst, paddedsize );

		bitmapinfo.biSizeImage = paddedsize; //( ( bitmapinfo.biWidth * bitmapinfo.biBitCount + 31 ) & ~31 ) / 8 * bitmapinfo.biHeight;
		fileHeader.bfSize = (DWORD) (sizeof(BITMAPFILEHEADER) + bitmapinfo.biSize + bitmapinfo.biClrUsed * sizeof(RGBQUAD) + bitmapinfo.biSizeImage);

		WriteFile(out, &fileHeader, sizeof(fileHeader), &bytesWritten, NULL);
		if ( padding > 0) // infoHeader->biHeight < 0
			bitmapinfo.biHeight *= -1;
		WriteFile(out, &bitmapinfo,  sizeof(bitmapinfo), &bytesWritten, NULL);
		if ( padding > 0) 
			bitmapinfo.biHeight *= -1;
		//WriteFile(out, palette, bitmapinfo.biClrUsed * sizeof (RGBQUAD), &bytesWritten, NULL);

		long bufpos = 0;   
		long newpos = 0;
		for ( int y = 0; y < bitmapinfo.biHeight; y++ )
		{
			int pixelIndex = 0;
			for ( int x = 0; x < 3 * bitmapinfo.biWidth; x+=3, pixelIndex++ )
			{
				bufpos = y * 3 * bitmapinfo.biWidth + x;                         // position in original buffer
				newpos = ( bitmapinfo.biHeight - y - 1 ) * psw / 2 + pixelIndex; // position in padded buffer

				RGBQUAD sourceColor = { src[bufpos+2], src[bufpos + 1], src[bufpos], 0 };
				dst[newpos] = convert24Color(sourceColor);
			}
		}

		WriteFile(out, dst, bitmapinfo.biSizeImage, &bytesWritten, NULL);
		LocalFree(src);
		LocalFree(dst);

		CloseHandle(out);
	}

}
