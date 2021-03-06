#include "pch.h"
#include <iostream>
#include <string>
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <tchar.h>
#include <shlobj.h>

inline int GetFilePointer(HANDLE FileHandle)
{
	return SetFilePointer(FileHandle, 0, 0, FILE_CURRENT);
}

extern bool SaveBMPFile(LPCWSTR filename, HBITMAP bitmap, HDC bitmapDC, int width, int height)
{
	bool Success = false;
	HDC SurfDC = NULL;        // GDI-compatible device context for the surface
	HBITMAP OffscrBmp = NULL; // bitmap that is converted to a DIB
	HDC OffscrDC = NULL;      // offscreen DC that we can select OffscrBmp into
	LPBITMAPINFO lpbi = NULL; // bitmap format info; used by GetDIBits
	LPVOID lpvBits = NULL;    // pointer to bitmap bits array
	HANDLE BmpFile = INVALID_HANDLE_VALUE;    // destination .bmp file
	BITMAPFILEHEADER bmfh;  // .bmp file header

	if ((OffscrBmp = CreateCompatibleBitmap(bitmapDC, width, height)) == NULL)
		return false;

	if ((OffscrDC = CreateCompatibleDC(bitmapDC)) == NULL)
		return false;

	HBITMAP OldBmp = (HBITMAP)SelectObject(OffscrDC, OffscrBmp);

	BitBlt(OffscrDC, 0, 0, width, height, bitmapDC, 0, 0, SRCCOPY);

	if ((lpbi = (LPBITMAPINFO)(new char[sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD)])) == NULL)
		return false;

	ZeroMemory(&lpbi->bmiHeader, sizeof(BITMAPINFOHEADER));
	lpbi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

	SelectObject(OffscrDC, OldBmp);
	if (!GetDIBits(OffscrDC, OffscrBmp, 0, height, NULL, lpbi, DIB_RGB_COLORS))
		return false;

	if ((lpvBits = new char[lpbi->bmiHeader.biSizeImage]) == NULL)
		return false;

	if (!GetDIBits(OffscrDC, OffscrBmp, 0, height, lpvBits, lpbi, DIB_RGB_COLORS))
		return false;

	if ((BmpFile = CreateFile(filename,
		GENERIC_WRITE,
		0, NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL)) == INVALID_HANDLE_VALUE)

		return false;

	DWORD Written;

	// Write a file header to the file:
	bmfh.bfType = 19778; 
	// bmfh.bfSize = ???       
	bmfh.bfReserved1 = bmfh.bfReserved2 = 0;
	// bmfh.bfOffBits = ??? 
	if (!WriteFile(BmpFile, &bmfh, sizeof(bmfh), &Written, NULL))
		return false;

	if (Written < sizeof(bmfh))
		return false;

	// Write BITMAPINFOHEADER to the file:
	if (!WriteFile(BmpFile, &lpbi->bmiHeader, sizeof(BITMAPINFOHEADER), &Written, NULL))
		return false;

	if (Written < sizeof(BITMAPINFOHEADER))
		return false;

	// Calculate size of palette:
	int PalEntries;
	// 16-bit or 32-bit bitmaps require bit masks:
	if (lpbi->bmiHeader.biCompression == BI_BITFIELDS)
		PalEntries = 3;
	else
		// bitmap is palettized?
		PalEntries = (lpbi->bmiHeader.biBitCount <= 8) ?
		// 2^biBitCount palette entries max.:
		(int)(1 << lpbi->bmiHeader.biBitCount)
		// bitmap is TrueColor -> no palette:
		: 0;
	// If biClrUsed use only biClrUsed palette entries:
	if (lpbi->bmiHeader.biClrUsed)
		PalEntries = lpbi->bmiHeader.biClrUsed;

	// Write palette to the file:
	if (PalEntries) {
		if (!WriteFile(BmpFile, &lpbi->bmiColors, PalEntries * sizeof(RGBQUAD), &Written, NULL))
			return false;

		if (Written < PalEntries * sizeof(RGBQUAD))
			return false;
	}

	// The current position in the file (at the beginning of the bitmap bits)
	// will be saved to the BITMAPFILEHEADER:
	bmfh.bfOffBits = GetFilePointer(BmpFile);

	// Write bitmap bits to the file:
	if (!WriteFile(BmpFile, lpvBits, lpbi->bmiHeader.biSizeImage, &Written, NULL))
		return false;

	if (Written < lpbi->bmiHeader.biSizeImage)
		return false;

	// The current pos. in the file is the final file size and will be saved:
	bmfh.bfSize = GetFilePointer(BmpFile);

	// We have all the info for the file header. Save the updated version:
	SetFilePointer(BmpFile, 0, 0, FILE_BEGIN);
	if (!WriteFile(BmpFile, &bmfh, sizeof(bmfh), &Written, NULL))
		return false;

	if (Written < sizeof(bmfh))
		return false;

	return true;
}

bool ScreenCapture(int x, int y, int width, int height, LPCWSTR filename)
{
	// get a DC compat. w/ the screen
	HDC hDc = CreateCompatibleDC(0);

	// make a bmp in memory to store the capture in
	HBITMAP hBmp = CreateCompatibleBitmap(GetDC(0), width, height);

	// join em up
	SelectObject(hDc, hBmp);

	// copy from the screen to my bitmap
	BitBlt(hDc, 0, 0, width, height, GetDC(0), x, y, SRCCOPY);

	// save my bitmap
	bool ret = SaveBMPFile(filename, hBmp, hDc, width, height);

	// free the bitmap memory
	DeleteObject(hBmp);

	return ret;
}

std::string getCurrentDateAndTime()
{
	std::string sTimeString;
	SYSTEMTIME lt;

	GetLocalTime(&lt);

	sTimeString += std::to_string(lt.wYear) + "-" + std::to_string(lt.wMonth) + "-" + std::to_string(lt.wDay)
		+ " " + std::to_string(lt.wHour) + "-" + std::to_string(lt.wMinute) + "-" + std::to_string(lt.wSecond);

	return sTimeString;
}

std::wstring s2ws(const std::string& s)
{
	int len;
	int slength = (int)s.length() + 1;
	len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf);
	delete[] buf;
	return r;
}

LPSTR getDesktopDirectory()
{
	static char path[MAX_PATH + 1];

	if (SHGetSpecialFolderPathA(HWND_DESKTOP, path, CSIDL_DESKTOP, FALSE)) {
		return path;
	}
}

int main()
{
	std::string sDesktopPath(getDesktopDirectory());
	std::string screenCapFilename = sDesktopPath + "\\Screenshot-" + getCurrentDateAndTime() + ".bmp";
	std::wstring sTemp = s2ws(screenCapFilename);
	LPCWSTR wScreenCapFilename = sTemp.c_str();

	ScreenCapture(0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), wScreenCapFilename);
}