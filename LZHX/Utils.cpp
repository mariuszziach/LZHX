/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

// stl
#include <fstream>
#include <iostream>

// windows
#include <windows.h>
#include <conio.h>

#include "Utils.h"

using namespace LZHX;

int LZHX::getFSize(std::ifstream &ifs) {
	ifs.seekg(0, ifs.end);
	int fs = (int)ifs.tellg();
	ifs.seekg(0, ifs.beg);
	return fs;
}
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
void LZHX::setConsoleTextRed() {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
        FOREGROUND_RED);
}
void LZHX::setConsoleTextNormal() {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}
void LZHX::setConsoleTitle(char const *tit) {
    SetConsoleTitle(tit);
}
void LZHX::consoleWait() {
    setConsoleTextNormal();
    std::cout << " Press anything to close program...";
    _getch();
}

DWord LZHX::getFileAttributes(char const *f_name) {
    return GetFileAttributes(f_name);
}

bool LZHX::setFileAttributes(char const *f_name, DWord attr) {
    return (bool)SetFileAttributes(f_name, attr);
}
