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
#include <sstream>
#include <iomanip>

// windows
#include <windows.h>
#include <conio.h>

// LZHX
#include "Utils.h"

using namespace LZHX;
using namespace std;

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
    cout << S_CLOSE;
    _getch();
}
void LZHX::consoleEndLine() {
    cout << endl;
}
void  LZHX::consoleWriteEndLine(char const *msg) {
    cout << msg << endl;
}
void LZHX::consoleAskPassword1(string &pass) {
    setConsoleTextNormal();
    cout << S_PASS1 << endl << endl << " ";
    getline(cin, pass, '\n');
    consoleEndLine();
}
void LZHX::consoleAskPassword2(string &pass) {
    setConsoleTextNormal();
    cout << S_PASS2 << endl << endl << " ";
    getline(cin, pass, '\n');
    consoleEndLine();
}
void LZHX::consoleCompWrite(const char *f_name1,
    const char *f_name2) {
    cout << S_COMP << f_name1 << " -> "
        << f_name2 << endl << endl;
}
void LZHX::consoleDecompWrite(const char *f_name1,
    const char *f_name2) {
    cout << S_DECOMP << f_name1 << " -> "
        << f_name2 << endl << endl;
}
void LZHX::consoleListWrite(const char *f_name1,
    const char *f_name2) {
    cout << S_LIST << f_name1 << " -> "
        << f_name2 << endl << endl;
}
void LZHX::consolePrintProgress(const char *f_name, int pr,
    float sec, int in_s, int out_s) {
    string fn;
    if (f_name == nullptr){
        fn = S_EMPTY;
    } else {
        fn = f_name;
        fn.resize(28, ' ');
    }
    cout << "\r"
         << setw(5) << pr  << "% | "
         << setw(9) << sec << "s | "
         << setw(9) << (in_s)  / 1024 << "kB -> "
         << setw(9) << (out_s) / 1024 << "kB | "
         << fn;
}
void LZHX::consoleSummaryWrite(QWord tot_in, QWord tot_out, float sec, bool cmp) {
    int w1(9), w2(9), w3(4);
    if (cmp) {
        w1 = 6; w2 = 6; w3 = 5;
        cout << endl << " Ratio: "
            << setw(10) << (float)tot_out / (float)tot_in * 100.0 << "% |";
    } else {
        cout << endl;
        for (int k = 0; k < 15; k++) cout << " ";
    }
    cout << " Size: " 
        << setw(w1) << (tot_in / 1024)  << "kB -> "
        << setw(w2) << (tot_out / 1024) << "kB | Time: "
        << setw(w3) << sec << "s "
        << endl << endl << " ";
}

// file list output
void LZHX::fileListWriteHeader(ofstream &ofs, DWord a_cnt,
                               QWord a_unc, const char *a_name) {
    ofs << S_LST_AR << a_name << endl;
    ofs << S_LST_FC << a_cnt << endl;
    ofs << S_LST_US << a_unc / 1024 << " kB" << endl << endl;
    consoleEndLine();
}
void LZHX::fileListWriteFile(ofstream &ofs, const char *f_name,
                             LZHX::FileHeader *fh) {
    if (fh->f_flags & FF_DIR)
    {
        for (int w = 0; w < 39; w++) ofs << ' ';
        ofs << f_name << endl;
    } else {
        std::stringstream sh;
        sh << uppercase << hex << std::setfill('0') << std::setw(8) << fh->f_cnt_hsh;
        ofs << std::setw(10) << sh.str() << " "
            << std::setw(15) << fh->f_cmp_size / 1024 << " kB "
            << std::setw(15) << fh->f_dcm_size / 1024 << " kB ";
        ofs << f_name << endl;
    }
}

// file attributes
DWord LZHX::getFileAttributes(char const *f_name) {
    return GetFileAttributes(f_name);}
bool LZHX::setFileAttributes(char const *f_name, DWord attr) {
    return (bool)SetFileAttributes(f_name, attr); }

// file time
void LZHX::getFileTime(char const *f_name, QWord *fcr, QWord *fla, QWord *lwr, bool dir) {
    FILETIME ft1, ft2, ft3;
    HANDLE hf = CreateFile(f_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        (dir ? (FILE_ATTRIBUTE_DIRECTORY | FILE_FLAG_BACKUP_SEMANTICS) : FILE_ATTRIBUTE_NORMAL), NULL);
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
    HANDLE hf = CreateFile(f_name, GENERIC_ALL,  FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,  OPEN_EXISTING,
        (dir ? (FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_DIRECTORY) : FILE_ATTRIBUTE_NORMAL), NULL);
    SetFileTime(hf, &ft1, &ft2, &ft3);
    CloseHandle(hf);
}

// suffix gen
string LZHX::suffixGen(int &i) {
    stringstream ss;
    ss << i++;
    return ss.str();
}
