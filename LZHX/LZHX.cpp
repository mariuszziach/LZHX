/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

// stl
#include <filesystem>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <string>
#include <memory>
#include <vector>

// c
#include <cassert>
#include <cstdlib>
#include <ctime>

// LZHX
#include "Types.h"
#include "Utils.h"
#include "BitStream.h"
#include "Huffman.h"
#include "LZ.h"

// namespaces
using namespace std;
using namespace std::experimental::filesystem::v1;

namespace LZHX {

// strings
char const S_TITLE[]    = "LZHX File Archiver";
char const S_INF1 []    = "\n LZHX\n";
char const S_INF2 []    = " Info       : File archiver based on Lempel-Ziv and Huffman algorithms.\n"
                          "              To check the integrity  of  files, the program uses a FNV\n"
                          "              hashing algorithm and for encryption modified XOR cipher.\n\n"
                          " Author     : mariusz.ziach@gmail.com\n"
                          " Date       : 2018\n"
                          " Version    : 1.0\n";
char const S_USAGE1[]   = " Usage: LZHX.exe <file/folder/archive to compress/decompress>\n";
char const S_USAGE2[]   = "  The program will automatically recognize whether the given parameter\n"
                          "  is an archive  for  decompression or a file/folder  for  compression.\n"
                          "  It  will also prevent overwriting files by creating unique names for\n"
                          "  outputed files and folders if needed.\n ";
char const S_ERR_FOPN[] = " File error.\n";
char const S_ERR_EX  [] = " Exception: ";
char const S_ERR_UNEX[] = " Unknown exception.\n";
char const S_ERR_HASH[] = " Error - different file hashes.\n";
char const S_ERR_WPAS[] = " Wrong password.\n";
char const S_PASS1 []   = " Type password if you want to encrypt this archive or just press enter:";
char const S_PASS2 []   = " Archive is encrypted. Type password:";
char const S_COMP  []   = " Compress   : ";
char const S_DECOMP[]   = " Decompress : ";
char const S_LST_AR[]   = "Archive                   : ";
char const S_LST_FC[]   = "Files/folders in archive  : ";
char const S_LST_US[]   = "Uncompressed archive size : ";
char const S_LST_HD[]   = "  FNV Hash        Compressed       Uncompressed  Name";

Byte  const sig[4] = { 'L','Z','H','X' };
char  const ext[5] = "lzhx";
DWord const sig2   = 0xFFFFFFFB;

enum AH_FLAGS { AF_ENCRYPT = 0x1 };

struct ArchiveHeader {
    Byte  f_sig[4];   // LZHX
    DWord f_sig2;     // 0xFFFFFFFB
    DWord f_cnt;      // num of files
    DWord f_flgs;     // flags
    QWord a_unc_size; // archive uncompressed size
    QWord a_cmp_size; // archive compressed size
};

enum FH_FLAGS { FF_DIR = 0x1 };

struct FileHeader {
    Byte  f_flags;    // flags
    DWord f_cmp_size; // compressed and decompressed sizes
    DWord f_dcm_size;
    QWord f_cr_time;  // creation, last acces and write times
    QWord f_la_time;
    QWord f_lw_time;
    DWord f_attr;     // file attributes
    DWord f_nm_cnt;   // file name bytes count
    DWord f_cnt_hsh;  // FNV hash
};

class LZHX {
private:
    QWord total_input;
    QWord total_output;
    string curr_f_name;
    CodecBuffer *cdc_bffrs;
    CodecStream  cdc_strm;
    CodecInterface *lz_cdc, *hf_cdc;
    CodecCallbackInterface *cdc_cllbck;
    CodecSettings *sttgs;
    int outBlkSize(int inBlkSize) { return inBlkSize * 2; }
    bool init() {
        cdc_strm.buf_count = sttgs->byte_bffr_cnt;
        cdc_strm.buf_stack = cdc_bffrs;
        for (int i = 0; i < int(sttgs->byte_bffr_cnt); i++) {
            cdc_bffrs[i].size = 0;
            cdc_bffrs[i].type = CBT_EMPTY;
        }
        return true;
    }
public:
    LZHX(CodecSettings *sttgs) {
        cdc_bffrs = new CodecBuffer[sttgs->byte_bffr_cnt];

        for (int i = 0; i < int(sttgs->byte_bffr_cnt); i++) {
            cdc_bffrs[i].cap = outBlkSize(sttgs->byte_blk_cap);
            cdc_bffrs[i].mem = new Byte[cdc_bffrs[i].cap];
            cdc_bffrs[i].type = CBT_EMPTY;
            cdc_bffrs[i].size = 0;
        }

        this->sttgs = sttgs;

        lz_cdc = hf_cdc = nullptr;
        cdc_cllbck      = nullptr;
        curr_f_name     = "";
        total_input = total_output = 0;
        
    }
    ~LZHX() {
        for (int i = 0; i < int(sttgs->byte_bffr_cnt); i++) {
            delete [] cdc_bffrs[i].mem;
        }
        delete[] cdc_bffrs;
    }
    void setCodec(CodecInterface *codec) {
        if (codec == nullptr) return;
        switch (codec->getCodecType()) {
            case CT_LZ: lz_cdc = codec; break;
            case CT_HF: hf_cdc = codec; break;
        }
    }
    void setCallback(CodecCallbackInterface  *codec_callback) {
        this->cdc_cllbck = codec_callback; }

private:
    DWord f_hash;
    void updateHash(char *buf, int size) { 
        for (int i = 0; i < size; i++) {
            f_hash ^= buf[i];
            f_hash *= 0x1000193;
        }
    }
public:
    void initHash()   {  f_hash = 0x811C9DC5; }
    void readAndHash(ifstream &ifile, char *buf, int size) {
         ifile.read(buf, size);
         updateHash(buf, (int)ifile.gcount());
    }
    void writeAndHash(ofstream &ofile, char *buf, int size) {
        updateHash (buf, size); ofile.write(buf, size);  }

private:
    bool do_encrypt;// , enc_hash;
    string ekey;
    int key_pos, key_size;
    clock_t begin;
    DWord encrypted_hash;

    Byte encryptByte(Byte b) {
        char c = ekey[key_pos++ % key_size];
        return b ^ c ^ (key_pos * 3) ^ (c * 5);
    }
    Byte decryptByte(Byte b) {
        char c = ekey[key_pos++ % key_size];
        return b ^ c ^ (key_pos * 3) ^ (c * 5);
    }
public:
    void initEncryption(bool do_encrypt, ifstream *arch, ofstream *arch2) {
        begin = clock();

        this->do_encrypt = do_encrypt;
        this->key_pos    = 0;
        this->key_size   = int(ekey.length());
        if (do_encrypt) {
            DWord hash = 0x811C9DC5;
            encrypted_hash = 0;
            if (!ekey.empty()) {
                int i;
                for (i = 0; i < this->key_size; i++) {
                    hash ^= ekey[i];
                    hash *= 0x1000193;
                }
                i = 0;
                encrypted_hash |= ekey[i++ % this->key_size];
                encrypted_hash |= ekey[i++ % this->key_size] << 8;
                encrypted_hash |= ekey[i++ % this->key_size] << 16;
                encrypted_hash |= ekey[i++ % this->key_size] << 24;
                encrypted_hash = encrypted_hash ^ hash;
            }
            if (arch != nullptr) {
                DWord ehc(0);
                arch->read((char*)&ehc, sizeof(DWord));
                if (ehc != encrypted_hash) { throw string(S_ERR_WPAS); }
            }
            else if (arch2 != nullptr) {
                arch2->write((char*)&encrypted_hash, sizeof(DWord));
            }
        }
    }
    void readAndDecrypt(ifstream &ifile, char *buf, int size) {
        ifile.read(buf, size);
        if (do_encrypt) {
            for (int i = 0; i < ifile.gcount(); i++)
                buf[i] = decryptByte(buf[i]);
        }
    }
    void encryptAndWrite(ofstream &ofile, char *buf, int size) {
        if (do_encrypt) {
            for (int i = 0; i < size; i++)
                buf[i] = encryptByte(buf[i]);
        }
        ofile.write(buf, size);
    }
public:
    int compressFile(ifstream &ifile, ofstream &ofile) {
        int tot_in(0), tot_out(0);

        if (!init()) return false;

        lz_cdc->initStream(&cdc_strm);
        hf_cdc->initStream(&cdc_strm);

        int cc = 0;
        while(ifile.good())
        {
            CodecBuffer *empty_bf = cdc_strm.find(CBT_EMPTY);
            readAndHash(ifile, (char*)empty_bf->mem, sttgs->byte_blk_cap);
            empty_bf->size = (int)ifile.gcount(); empty_bf->type = CBT_RAW;
            tot_in += empty_bf->size;

            lz_cdc->compressBlock();

            CodecBuffer *lz_bf = cdc_strm.find(CBT_LZ);
            while (lz_bf) {
                hf_cdc->compressBlock();
                CodecBuffer *hf_bf = cdc_strm.find(CBT_HF);
                int temp_s = hf_bf->size;

                encryptAndWrite(ofile, (char*)&temp_s, sizeof(int));
                encryptAndWrite(ofile, (char*)hf_bf->mem, hf_bf->size);

                tot_out += sizeof(int) + hf_bf->size;
                hf_bf->type = CBT_EMPTY;
                lz_bf = cdc_strm.find(CBT_LZ);
            }

            if (cdc_cllbck != nullptr && !(cc++ % 12))
                cdc_cllbck->compressCallback(lz_cdc->getTotalIn(), hf_cdc->getTotalOut(),
                    cdc_strm.stream_size, curr_f_name.c_str());
        }

        if (cdc_cllbck != nullptr)
            cdc_cllbck->compressCallback(tot_in, tot_out,
                cdc_strm.stream_size, curr_f_name.c_str());

        total_input  += tot_in;
        total_output += tot_out;

        return tot_out;
    }

    int decompressFile(ifstream &ifile, ofstream &ofile) {
        int tot_in(0), tot_out(0);

        if (!init()) return false;

        hf_cdc->initStream(&cdc_strm);
        lz_cdc->initStream(&cdc_strm);

        int cc = 0;
        while (tot_in < cdc_strm.stream_size) {

            for (int i = 0; i < 4; i++) {
                CodecBuffer *empty = cdc_strm.find(CBT_EMPTY);
                readAndDecrypt(ifile, (char*)&empty->size, sizeof(int));
                readAndDecrypt(ifile, (char*)empty->mem, empty->size);
                tot_in += sizeof(int) + empty->size;
                empty->type = CBT_HF;
                hf_cdc->decompressBlock();
            }

            lz_cdc->decompressBlock();
            CodecBuffer *raw = cdc_strm.find(CBT_RAW);
            writeAndHash(ofile, (char*)raw->mem, raw->size);

            tot_out += raw->size;
            raw->type = CBT_EMPTY;

            if (cdc_cllbck != nullptr && !(cc++ % 12))
                cdc_cllbck->decompressCallback(hf_cdc->getTotalIn(),
                    lz_cdc->getTotalOut(), cdc_strm.stream_size, curr_f_name.c_str());
        }

        if (cdc_cllbck != nullptr)
            cdc_cllbck->decompressCallback(tot_in, tot_out,
                cdc_strm.stream_size, curr_f_name.c_str());

        total_input += tot_in;
        total_output += tot_out;

        return tot_in;
    }

private:
    void writeHeader(ofstream &ofile, DWord f_cnt, DWord f_flgs,
        QWord a_unc_size, QWord a_cmp_size) {

        ArchiveHeader ah;
        ah.f_cnt = f_cnt; ah.f_flgs = f_flgs; ah.f_sig2 = sig2;
        ah.a_cmp_size = a_cmp_size; ah.a_unc_size = a_unc_size;
        memcpy(ah.f_sig, sig, sizeof(sig));
        ofile.write((char*)&ah, sizeof(ah));
    }
    bool readHeader(ifstream &ifile, DWord *f_cnt, DWord *f_flgs,
        QWord *a_unc_size, QWord *a_cmp_size) {

        ArchiveHeader ah;
        ifile.read((char*)&ah, sizeof(ah));
        if (ifile.gcount() !=  sizeof(ah)) return false;
        if (memcmp(ah.f_sig, sig, sizeof(sig)) == 0 && ah.f_sig2 == sig2) {
            *f_cnt  = ah.f_cnt;
            *f_flgs = ah.f_flgs;
            *a_unc_size = ah.a_unc_size;
            *a_cmp_size = ah.a_cmp_size;
            return true;
        } else {
            *f_cnt = *f_flgs = 0;
            *a_unc_size = *a_cmp_size = 0;
            return false;
        }
    }

public:
    bool archiveAddFile(ofstream &arch, string &f, bool dir = false) {
        ifstream ifile;
        FileHeader fh;
        memset(&fh, 0, sizeof(FileHeader));
        
        if (dir) fh.f_flags = FF_DIR;

        fh.f_nm_cnt = DWord(f.length());
        fh.f_attr = getFileAttributes(f.c_str());
        getFileTime(f.c_str(), &fh.f_cr_time, &fh.f_la_time, &fh.f_lw_time, dir);

        int h_pos = int(arch.tellp());

        arch.write((char*)&fh, sizeof(FileHeader));
        arch.write((char*)f.c_str(), fh.f_nm_cnt);

        if (!dir) {
            ifile.open(f, ios::binary); if (!ifile.is_open()) return false;

            cdc_strm.stream_size = fh.f_dcm_size = DWord(file_size(f));
            curr_f_name = path(f).filename().string();

            cdc_cllbck->init(); initHash();
            fh.f_cmp_size = compressFile(ifile, arch);
            fh.f_cnt_hsh = f_hash;

            cout << endl;
 
            int e_pos = int(arch.tellp());
            arch.seekp(h_pos);
            arch.write((char*)&fh, sizeof(FileHeader));
            arch.seekp(e_pos);

            if (ifile.is_open()) ifile.close();
        }
        return true;
    }

    bool archiveCreate(string &dir_name, string &arch_name) {
        setConsoleTextNormal();

        cout << S_PASS1 << endl << endl << " ";
        getline(cin, ekey, '\n');
        cout << endl;
        
        ofstream arch;
        arch.open(arch_name, ios::binary); if (!arch.is_open()) return false;
    
        int b_pos = int(arch.tellp());

        DWord f_cnt(0), f_flgs(0);
        if (!ekey.empty()) f_flgs |= AF_ENCRYPT;
        writeHeader(arch, f_cnt, f_flgs, 0, 0);

        initEncryption(!ekey.empty(), nullptr, &arch);

        path dir(dir_name);

        if (is_directory(dir)) {
            dir = dir.filename();
            for (auto& itm : recursive_directory_iterator(dir)) {
                string &f = ((string&)((path&)itm).string());
                if (is_regular_file(f)) {

                    if (!archiveAddFile(arch, f))
                        return false;

                    f_cnt++;
                } if (is_directory(f)) {
                    if (!archiveAddFile(arch, f, true))
                        return false;
                    f_cnt++;
                }
            }

        } else if(is_regular_file(dir)) {
            string &f = (string&)dir.filename().string();
            if (!archiveAddFile(arch, f))
                return false;
            f_cnt = 1;
        } else { return false; }

        arch.seekp(b_pos);
 
        writeHeader(arch, f_cnt, f_flgs, total_input, total_output);

        if (arch.is_open()) arch.close();
        return true;
    }

    bool archiveExtract(string &arch_name, string &dir, bool list) {
        setConsoleTextNormal();

        QWord a_unc_size(0), a_cmp_size(0);
        DWord f_cnt, f_flags;
        ifstream arch;

        arch.open(arch_name, ios::binary); if (!arch.is_open()) return false;
        if (!readHeader(arch, &f_cnt, &f_flags, &a_unc_size, &a_cmp_size)) return false;

        if (f_flags & AF_ENCRYPT) {
            cout << S_PASS2 << endl << endl << " ";
            getline(cin, ekey, '\n');
            cout << endl;
        }

        initEncryption(f_flags & AF_ENCRYPT, &arch, nullptr);

        ofstream flist;
        if (list) {
            flist.open(dir);
            flist << S_LST_AR << path(arch_name).filename() << endl;
            flist << S_LST_FC << f_cnt << endl;
            flist << S_LST_US << a_unc_size/1024 << " kB" << endl << endl;
            flist << S_LST_HD << endl;
        }

        for (int i = 0; i < int(f_cnt); i++) {

            FileHeader fh;
            string f_name;
            arch.read((char*)&fh, sizeof(FileHeader));

            for (int j = 0; j < int(fh.f_nm_cnt); j++) f_name += (char)arch.get();

            if (list) {
                if (fh.f_flags & FF_DIR)
                {
                    for (int w = 0; w < 39; w++) flist << ' ';
                    flist << f_name << endl;
                }
                else {
                    std::stringstream sh;
                    sh << uppercase << hex << setfill('0') << setw(8) << fh.f_cnt_hsh;

                    flist << setw(10) << sh.str() << " "
                        << setw(15) <<  fh.f_cmp_size / 1024 << " kB "
                        << setw(15) <<  fh.f_dcm_size / 1024 << " kB ";
                    flist << f_name << endl;
                }

                arch.seekg(fh.f_cmp_size, ios::cur);
                continue;
            }

            path p(f_name);
            if (p.has_parent_path()) {
                path newp(dir);
                for (auto it = p.begin(); it != p.end(); it++)
                    if (it != p.begin()) newp.append(*it);
                p = newp;
                if (fh.f_flags& FF_DIR) create_directories(p);
                else create_directories(p.parent_path());

            } else {
                path base(f_name), pxt = p.extension();
                base.replace_extension("");
                while (exists(p)) {
                    p = base.concat("0");
                    p.replace_extension(pxt);
                }
            }

            if (!(fh.f_flags & FF_DIR)) {
                ofstream ofile;
                ofile.open(p, ios::binary);
                cdc_strm.stream_size = fh.f_cmp_size;

                cdc_cllbck->init(); initHash();
                curr_f_name = p.filename().string();

                try {
                    decompressFile(arch, ofile);
                } catch (string &s) {
                    if (ofile.is_open()) ofile.close();
                    remove(p);
                    throw s;
                }

                if (f_hash != fh.f_cnt_hsh) 
                    cout << endl << S_ERR_HASH << endl;

                cout << endl;
                if (ofile.is_open()) ofile.close();
            }

            setFileAttributes((char const *)(p.string().c_str()), fh.f_attr);
            setFileTime((char const *)(p.string().c_str()),
                fh.f_cr_time, fh.f_la_time, fh.f_lw_time, (bool)(fh.f_flags & FF_DIR));
        }

        if (arch.is_open())  arch.close();
        if (flist.is_open()) flist.close();
        return true;
    }

    void createUniqueName(string &name, string *out, char const *next) {
        path pn(name), base(name);
        base.replace_extension("");
        pn.replace_extension(next);
        while (exists(pn)) {
            pn = base.concat("0");
            pn.replace_extension(next);
        }
        *out = pn.string();
    }

    bool detectInput(string &&name, bool list) {
        string oname;
        bool act_cmp = true;
        

        setConsoleTextRed();

        if (is_directory(name)) {
            createUniqueName(name, &oname, ext);
            cout << S_COMP << path(name).filename() << " -> "
                 << path(oname).filename() << endl << endl;
            archiveCreate(name, oname);
        } else if(is_regular_file(name)) {
            QWord nul;
            ifstream ifile(name, ios::binary);
            if (!ifile.is_open()) return false;
            if (readHeader(ifile, (DWord*)&nul, (DWord*)&nul, &nul, &nul)) {
                ifile.close();
                act_cmp = false;
                if (!list) createUniqueName(name, &oname, "");
                else createUniqueName(name, &oname, "txt");
                cout << S_DECOMP << path(name).filename() << " -> "
                     << path(oname).filename() << endl << endl;
                archiveExtract(name, oname, list);
            } else {
                ifile.close();
                createUniqueName(name, &oname, ext);
                cout << S_COMP << path(name).filename() << " -> "
                     << path(oname).filename() << endl << endl;
                archiveCreate(name, oname);
            }
        } else {
            return false;
        }

        setConsoleTextRed();

        clock_t time = clock();
        double sec = double(time - begin) / CLOCKS_PER_SEC;
        int w1(9), w2(9), w3(4);
        if (act_cmp) {
            w1 = 6; w2 = 6; w3 = 5;
            cout << endl << " Ratio: "
                << setw(10) << (float)total_output / (float)total_input * 100.0 << "% |";
        } else {
            cout << endl;
            for (int k = 0; k < 15; k++) cout << " ";
        }
        cout << " Size: " << setw(w1) << (total_input / 1024) << "kB -> "
             << setw(w2) << (total_output / 1024) << "kB | Time: "
             << setw(w3) << sec << "s "
             << endl << endl << " ";
        consoleWait();

        return false;
    }
};

class ConsoleCodecCallback : public CodecCallbackInterface {
private:
    clock_t begin;
public:
    void init() { begin = clock(); }
    bool compressCallback(int in_size, int out_size, int stream_size, const char *f_name) {
        string fn;
        if (f_name == nullptr) fn = "";
        else {
            fn = f_name;
            fn.resize(28, ' ');
        }
        clock_t time = clock();
        double sec = double(time - begin) / CLOCKS_PER_SEC;
        int pr = ((int)((in_size / (float)stream_size) * 100));
        if (pr < 0) pr = 100;
        cout << "\r" 
             << setw(5) << pr  << "% | "
             << setw(9) << sec << "s | "
             << setw(9) << (in_size)   / 1024 << "kB -> "
             << setw(9) << (out_size)  / 1024 << "kB | "
             << fn;
        return true;
    }
    bool decompressCallback(int in_size, int out_size, int stream_size, const char *f_name) {
        return compressCallback(in_size, out_size, stream_size, f_name); }
};

class ConsoleApplication {
public:
    int run(int argc, char const *argv[]) {
        setConsoleTitle(S_TITLE);
        setConsoleTextRed();
        cout << S_INF1 << endl;
        setConsoleTextNormal();
        cout << S_INF2 << endl;
        if (argc > 1) {
            CodecSettings        sttgs;
            sttgs.Set(16, 16, 2, 8, 16, 3, 3);
            sttgs.byte_lkp_hsh = 5;
            LZHX                 lzhx(&sttgs);
            LZ                   lz(&sttgs);
            Huffman              huffman;
            ConsoleCodecCallback callback;
            lzhx.setCodec(&huffman);
            lzhx.setCodec(&lz);
            lzhx.setCallback(&callback);
            bool list = false;
            if (argc > 2) list = (bool)(argv[2][1] == 'l' || argv[2][1] == 'L');
            lzhx.detectInput(string(argv[1]), list);
        } else {
            setConsoleTextRed();
            cout << S_USAGE1 << endl;

            setConsoleTextNormal();
            cout << S_USAGE2 << endl;
            consoleWait();
        }
        return 0;
    }
};
}

int main(int argc, char const *argv[]) {
    
    LZHX::ConsoleApplication app;
    try { return app.run(argc, argv);
    } catch (string &msg)  { cout << msg << endl;
    } catch (exception &e) { cout << LZHX::S_ERR_EX   << e.what() << endl;
    } catch (...) {          cout << LZHX::S_ERR_UNEX << endl; }
    LZHX::consoleWait();
    return 1;
}
