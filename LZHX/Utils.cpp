/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

// c
#include <cassert>
#include <stdlib.h>

// stl
#include <fstream>
#include <iostream>

// windows
#include <windows.h>
#include <conio.h>

#include "Utils.h"

using namespace LZHX;

// get file size
int LZHX::getFSize(std::ifstream &ifs) {
	ifs.seekg(0, ifs.end);
	int fs = (int)ifs.tellg();
	ifs.seekg(0, ifs.beg);
	return fs;
}

// reading/write integers to byte buffer
int LZHX::write32To8Buf(Byte *buf, DWord i) {
	buf[0] = (Byte)((i) & 0xFF);
	buf[1] = (Byte)((i >> 8) & 0xFF);
	buf[2] = (Byte)((i >> 16) & 0xFF);
	buf[3] = (Byte)((i >> 24) & 0xFF);
	return sizeof(DWord);
}
int LZHX::write16To8Buf(Byte *buf, Word  i) {
	buf[0] = (Byte)((i) & 0xFF);
	buf[1] = (Byte)((i >> 8) & 0xFF);
	return sizeof(Word);
}
DWord LZHX::read32From8Buf(Byte *buf) {
    DWord i  = (DWord)(buf[0]);
	i |= (DWord)(buf[1]) << 8;
	i |= (DWord)(buf[2]) << 16;
	i |= (DWord)(buf[3]) << 24;
	return i;
}
Word LZHX::read16From8Buf(Byte *buf) {
    Word i  = (Word)(buf[0]);
	i |= (Word)(buf[1]) << 8;
	return i;
}

// console stuff
void LZHX::setConsoleTextRed() {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
        FOREGROUND_RED);
}
void LZHX::setConsoleTextNormal() {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}
void LZHX::setConsoleTitle(char const *tit) { SetConsoleTitle(tit); }
void LZHX::consoleWait() {
    setConsoleTextNormal();
    std::cout << S_CLOSE;
    _getch();
}

// file attributes
DWord LZHX::getFileAttributes(char const *f_name) {
    return GetFileAttributes(f_name);}
bool LZHX::setFileAttributes(char const *f_name, DWord attr) {
    return (bool)SetFileAttributes(f_name, attr); }

// file time
void LZHX::getFileTime(char const *f_name, QWord *fcr, QWord *fla, QWord *lwr, bool dir) {
    FILETIME ft1, ft2, ft3;
    HANDLE hf = CreateFile(f_name, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, (dir ? (FILE_ATTRIBUTE_DIRECTORY | FILE_FLAG_BACKUP_SEMANTICS) : FILE_ATTRIBUTE_NORMAL), NULL);
    GetFileTime(hf, &ft1, &ft2, &ft3);
    CloseHandle(hf);
    *fcr = QWord(ft1.dwHighDateTime) << 32 | ft1.dwLowDateTime;
    *fla = QWord(ft2.dwHighDateTime) << 32 | ft2.dwLowDateTime;
    *lwr = QWord(ft3.dwHighDateTime) << 32 | ft3.dwLowDateTime;
}
void LZHX::setFileTime(char const *f_name, QWord fcr, QWord fla, QWord lwr, bool dir) {
    FILETIME ft1, ft2, ft3;
    ft1.dwHighDateTime = (fcr >> 32); ft1.dwLowDateTime = fcr & 0xFFFFFFFF;
    ft2.dwHighDateTime = (fla >> 32); ft2.dwLowDateTime = fla & 0xFFFFFFFF;
    ft3.dwHighDateTime = (lwr >> 32); ft3.dwLowDateTime = lwr & 0xFFFFFFFF;
    HANDLE hf = CreateFile(f_name, GENERIC_ALL,  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
        OPEN_EXISTING, (dir ? (FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_DIRECTORY) : FILE_ATTRIBUTE_NORMAL), NULL);
    SetFileTime(hf, &ft1, &ft2, &ft3);
    CloseHandle(hf);
}
