
#include "bmpq.h"
#include <tchar.h>

void main(int argc, char** argv)
{
	BitmapSaver bs;
	TCHAR fname[MAX_PATH];
	
	if (argc > 1) {
		size_t ch_con;
		mbstowcs_s(&ch_con, fname, MAX_PATH, argv[1], _TRUNCATE);

		_tcprintf_s(_T("Open file %s...\r\n"), fname);
		bs.convertBMP(fname);
	}
}