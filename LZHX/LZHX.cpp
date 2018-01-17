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
#include "Globals.h"
#include "Types.h"
#include "Utils.h"
#include "BitStream.h"
#include "Huffman.h"
#include "LZ.h"


// namespaces
using namespace std;
using namespace std::experimental::filesystem::v1;

namespace LZHX {

class LZHX {
private:
    QWord total_input;
    QWord total_output;
    string curr_f_name;
    clock_t c_begin;
    CodecBuffer *cdc_bffrs;
    CodecStream  cdc_strm;
    CodecInterface *lz_cdc, *hf_cdc;
    CodecCallbackInterface *cdc_cllbck;
    CodecSettings *sttgs;

    // function for counting working buffer size
    int outBlkSize(int inBlkSize) { return inBlkSize * 2; }
    
    // init archiving
    bool init() {
        c_begin = clock();
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
    // hashing function
    void updateHash(char *buf, int size) { 
        for (int i = 0; i < size; i++) {
            f_hash ^= buf[i];
            f_hash *= 0x1000193;
        }
    }
public:
    // read/write with hashing
    void initHash()   {  f_hash = 0x811C9DC5; }
    void readAndHash (ifstream &ifile, char *buf, int size) {
         ifile.read(buf, size);
         updateHash(buf, (int)ifile.gcount());
    }
    void writeAndHash(ofstream &ofile, char *buf, int size) {
        updateHash (buf, size); ofile.write(buf, size);  }

private:
    int key_pos, key_size;
    bool do_encrypt;
    string ekey;
    DWord encrypted_hash;

    // encryption functions 
    Byte encryptByte(Byte b) {
        char c = ekey[key_pos++ % key_size];
        return b ^ c ^ (key_pos * 3) ^ (c * 5);
    }
    Byte decryptByte(Byte b) {
        return encryptByte(b);
    }
public:
    // init encryption
    void initEncryption(bool do_encrypt, ifstream *arch, ofstream *arch2) {
        this->do_encrypt = do_encrypt;
        this->key_pos    = 0;
        this->key_size   = int(ekey.length());
        if (do_encrypt) {

            // encrypt hashed password
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

            // compare hashed password with one stored in archive
            if (arch != nullptr) {
                DWord ehc(0);
                arch->read((char*)&ehc, sizeof(DWord));
                if (ehc != encrypted_hash) { throw string(S_ERR_WPAS); }

            // write hashed password to archive
            } else if (arch2 != nullptr) {
                arch2->write((char*)&encrypted_hash, sizeof(DWord));
            }
        }
    }
    // read and decrypt
    void readAndDecrypt(ifstream &ifile, char *buf, int size) {
        ifile.read(buf, size);
        if (do_encrypt) {
            for (int i = 0; i < ifile.gcount(); i++)
                buf[i] = decryptByte(buf[i]);
        }
    }
    // encrypt and write
    void encryptAndWrite(ofstream &ofile, char *buf, int size) {
        if (do_encrypt) {
            for (int i = 0; i < size; i++)
                buf[i] = encryptByte(buf[i]);
        }
        ofile.write(buf, size);
    }

public:

    // compress file
    int compressFile(ifstream &ifile, ofstream &ofile) {
        int tot_in(0), tot_out(0), cc(0), temp_s(0);
        CodecBuffer *empty_bf, *lz_bf, *hf_bf;

        // init compression
        if (!init()) return false;
        lz_cdc->initStream(&cdc_strm);
        hf_cdc->initStream(&cdc_strm);

        while(ifile.good())
        {
            // find empty buffer and read raw data into it
            empty_bf = cdc_strm.find(CBT_EMPTY);
            readAndHash(ifile, (char*)empty_bf->mem, sttgs->byte_blk_cap);
            empty_bf->size = (int)ifile.gcount(); empty_bf->type = CBT_RAW;
            tot_in += empty_bf->size;

            // compress block with LZ
            lz_cdc->compressBlock();

            // compress 4 LZ streams with huffman
            lz_bf = cdc_strm.find(CBT_LZ);
            while (lz_bf) {

                // compress
                hf_cdc->compressBlock();
                hf_bf = cdc_strm.find(CBT_HF);
                temp_s = hf_bf->size;

                // write compressed block size and compressed data
                encryptAndWrite(ofile, (char*)&temp_s, sizeof(int));
                encryptAndWrite(ofile, (char*)hf_bf->mem, hf_bf->size);

                // update info
                tot_out += sizeof(int) + hf_bf->size;
                hf_bf->type = CBT_EMPTY;
                lz_bf = cdc_strm.find(CBT_LZ);
            }

            // callback
            if (cdc_cllbck != nullptr && !(cc++ % 12))
                cdc_cllbck->compressCallback(lz_cdc->getTotalIn(), hf_cdc->getTotalOut(),
                    cdc_strm.stream_size, curr_f_name.c_str());
        }

        // final callback
        if (cdc_cllbck != nullptr)
            cdc_cllbck->compressCallback(tot_in, tot_out,
                cdc_strm.stream_size, curr_f_name.c_str());

        // update info
        total_input  += tot_in;
        total_output += tot_out;
        return tot_out;
    }

    // decompress file
    int decompressFile(ifstream &ifile, ofstream &ofile) {
        int tot_in(0), tot_out(0), cc(0);
        CodecBuffer *empty_bf, *raw_bf;

        // init
        if (!init()) return false;
        hf_cdc->initStream(&cdc_strm);
        lz_cdc->initStream(&cdc_strm);

        while (tot_in < cdc_strm.stream_size) {

            // read 4 blocks
            for (int i = 0; i < 4; i++) {
                empty_bf = cdc_strm.find(CBT_EMPTY);

                // read and decrypt block
                readAndDecrypt(ifile, (char*)&empty_bf->size, sizeof(int));
                readAndDecrypt(ifile, (char*)empty_bf->mem, empty_bf->size);
                tot_in     += sizeof(int) + empty_bf->size;
                empty_bf->type = CBT_HF;

                // decompress each block compressed with huffman algorithm
                hf_cdc->decompressBlock();
            }

            // decompress LZ 4 blocks into one
            lz_cdc->decompressBlock();
            raw_bf = cdc_strm.find(CBT_RAW);

            // write into file and hash block
            writeAndHash(ofile, (char*)raw_bf->mem, raw_bf->size);
            tot_out += raw_bf->size;
            raw_bf->type = CBT_EMPTY;

            // callback
            if (cdc_cllbck != nullptr && !(cc++ % 12))
                cdc_cllbck->decompressCallback(hf_cdc->getTotalIn(),
                    lz_cdc->getTotalOut(), cdc_strm.stream_size, curr_f_name.c_str());
        }

        // final callback
        if (cdc_cllbck != nullptr)
            cdc_cllbck->decompressCallback(tot_in, tot_out,
                cdc_strm.stream_size, curr_f_name.c_str());

        // update info
        total_input += tot_in;
        total_output += tot_out;
        return tot_in;
    }

private:
    // write archive header
    void writeHeader(ofstream &ofile, DWord a_fcnt, DWord a_flgs,
        QWord a_unc_size, QWord a_cmp_size) {
        ArchiveHeader ah;
        ah.a_fcnt = a_fcnt; ah.a_flgs = a_flgs; ah.a_sig2 = sig2;
        ah.a_cmp_size = a_cmp_size; ah.a_unc_size = a_unc_size;
        memcpy(ah.a_sig, sig, sizeof(sig));
        ofile.write((char*)&ah, sizeof(ah));
    }

    // read archive header
    bool readHeader(ifstream &ifile, DWord *a_fcnt, DWord *a_flgs,
        QWord *a_unc_size, QWord *a_cmp_size) {
        ArchiveHeader ah;
        ifile.read((char*)&ah, sizeof(ah));
        if (ifile.gcount() !=  sizeof(ah)) return false;
        if (memcmp(ah.a_sig, sig, sizeof(sig)) == 0 && ah.a_sig2 == sig2) {
            *a_fcnt = ah.a_fcnt;
            *a_flgs = ah.a_flgs;
            *a_unc_size = ah.a_unc_size;
            *a_cmp_size = ah.a_cmp_size;
            return true;
        } else {
            *a_fcnt     = *a_flgs     = 0;
            *a_unc_size = *a_cmp_size = 0;
            return false;
        }
    }

public:

    // add single file/folder to archive
    bool archiveAddFile(ofstream &arch, string &f, bool dir = false) {
        int h_pos, e_pos;
        ifstream ifile;
        FileHeader fh;
        memset(&fh, 0, sizeof(FileHeader));
        
        // fill header with info about file
        if (dir) fh.f_flags = FF_DIR;
        fh.f_nm_cnt = DWord(f.length());
        fh.f_attr = getFileAttributes(f.c_str());
        getFileTime(f.c_str(), &fh.f_cr_time, &fh.f_la_time, &fh.f_lw_time, dir);

        // remember header position and write header
        h_pos = int(arch.tellp());
        arch.write((char*)&fh, sizeof(FileHeader));
        arch.write((char*)f.c_str(), fh.f_nm_cnt);

        // for directory we finish on writing header
        if (!dir) {

            // open output file
            ifile.open(f, ios::binary); if (!ifile.is_open()) return false;

            // update file header
            cdc_strm.stream_size = fh.f_dcm_size = DWord(file_size(f));
            curr_f_name = path(f).filename().string();
            cdc_cllbck->init(); initHash();

            // compress file
            fh.f_cmp_size = compressFile(ifile, arch);
            fh.f_cnt_hsh = f_hash;

            cout << endl;
 
            // rewrite header
            e_pos = int(arch.tellp());
            arch.seekp(h_pos);
            arch.write((char*)&fh, sizeof(FileHeader));
            arch.seekp(e_pos);

            // close
            if (ifile.is_open()) ifile.close();
        }
        return true;
    }

    // create archive from directory or file
    bool archiveCreate(string &dir_name, string &arch_name) {
        DWord f_cnt(0), f_flgs(0);
        int b_pos(0);
        ofstream arch;
        path dir(dir_name);

        // ask for password
        setConsoleTextNormal();
        cout << S_PASS1 << endl << endl << " ";
        getline(cin, ekey, '\n');
        cout << endl;
        
        // open archive
        arch.open(arch_name, ios::binary); if (!arch.is_open()) return false;
    
        //remember position in file where we're going to write archive header
        b_pos = int(arch.tellp());

        // write header
        if (!ekey.empty()) f_flgs |= AF_ENCRYPT;
        writeHeader(arch, f_cnt, f_flgs, 0, 0);
        initEncryption(!ekey.empty(), nullptr, &arch);
        
        // directory
        if (is_directory(dir)) {
            dir = dir.filename();

            // add every file and folder in directory
            for (auto& itm : recursive_directory_iterator(dir)) {
                string &f = ((string&)((path&)itm).string());

                // file
                if (is_regular_file(f)) {
                    if (!archiveAddFile(arch, f))  return false;
                    f_cnt++;

                // directory
                } else if (is_directory(f)) {
                    if (!archiveAddFile(arch, f, true))
                        return false;
                    f_cnt++;
                }
            }

        // file
        } else if(is_regular_file(dir)) {

            // just add one file to archive
            string &f = (string&)dir.filename().string();
            if (!archiveAddFile(arch, f))
                return false;
            f_cnt = 1;
        } else { return false; }

        // go back and write archive header again
        arch.seekp(b_pos);
        writeHeader(arch, f_cnt, f_flgs, total_input, total_output);

        // close
        if (arch.is_open()) arch.close();
        return true;
    }

    // archive extracting with option to only list files stored in archive
    bool archiveExtract(string &arch_name, string &dir, bool list) {
        QWord a_unc_size(0), a_cmp_size(0);
        DWord f_cnt(0), f_flags(0);
        ifstream arch;

        setConsoleTextNormal();

        // read header
        arch.open(arch_name, ios::binary); if (!arch.is_open()) return false;
        if (!readHeader(arch, &f_cnt, &f_flags, &a_unc_size, &a_cmp_size)) return false;

        // ask for password if archive is encrypted
        if (f_flags & AF_ENCRYPT) {
            cout << S_PASS2 << endl << endl << " ";
            getline(cin, ekey, '\n');
            cout << endl;
        }

        initEncryption(f_flags & AF_ENCRYPT, &arch, nullptr);

        // create text file if we want to only list files
        ofstream flist;
        if (list) {
            flist.open(dir);

            // and write header
            flist << S_LST_AR << path(arch_name).filename() << endl;
            flist << S_LST_FC << f_cnt << endl;
            flist << S_LST_US << a_unc_size/1024 << " kB" << endl << endl;
            flist << S_LST_HD << endl;
        }

        for (int i = 0; i < int(f_cnt); i++) {
            // read file header
            FileHeader fh;
            string f_name;
            arch.read((char*)&fh, sizeof(FileHeader));

            // read file name from archive
            for (int j = 0; j < int(fh.f_nm_cnt); j++) f_name += (char)arch.get();

            // if we want to list only files...
            if (list) {
                // directory
                if (fh.f_flags & FF_DIR)
                {
                    for (int w = 0; w < 39; w++) flist << ' ';
                    flist << f_name << endl;
                }
                // file
                else {
                    std::stringstream sh;
                    sh << uppercase << hex << setfill('0') << setw(8) << fh.f_cnt_hsh;
                    flist << setw(10) << sh.str() << " "
                        << setw(15) <<  fh.f_cmp_size / 1024 << " kB "
                        << setw(15) <<  fh.f_dcm_size / 1024 << " kB ";
                    flist << f_name << endl;
                }

                // go to next file in archive
                arch.seekg(fh.f_cmp_size, ios::cur);

            // extracting archive
            } else {

                // create output directory
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

                // extract file
                if (!(fh.f_flags & FF_DIR)) {
                    ofstream ofile;
                    ofile.open(p, ios::binary);
                    cdc_strm.stream_size = fh.f_cmp_size;
                    cdc_cllbck->init(); initHash();
                    curr_f_name = p.filename().string();

                    // try {} catch() for wrong password exception
                    try {
                        decompressFile(arch, ofile);
                    } catch (string &s) {

                        // remove file if we created one
                        if (ofile.is_open()) ofile.close();
                        remove(p);
                        throw s;
                    }

                    // check if hash from archive is the same as counted one
                    if (f_hash != fh.f_cnt_hsh)
                        cout << endl << S_ERR_HASH << endl;

                    cout << endl;
                    if (ofile.is_open()) ofile.close();
                }

                // set file times and attributes
                setFileAttributes((char const *)(p.string().c_str()), fh.f_attr);
                setFileTime((char const *)(p.string().c_str()),
                    fh.f_cr_time, fh.f_la_time, fh.f_lw_time, (bool)(fh.f_flags & FF_DIR));
            }
        }
        if (arch.is_open())  arch.close();
        if (flist.is_open()) flist.close();
        return true;
    }

    // create output filename based on input
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

    // detect if input is a file, folder or archive 
    void detectInput(string &&name, bool list) {
        string oname;
        bool act_cmp = true;

        setConsoleTextRed();

        if (is_directory(name)) {
            // input is directory to compress
            createUniqueName(name, &oname, ext);
            cout << S_COMP << path(name).filename() << " -> "
                 << path(oname).filename() << endl << endl;
            archiveCreate(name, oname);
        } else if(is_regular_file(name)) {
            QWord nul;

            // try to read header
            ifstream ifile(name, ios::binary);
            bool is_arch = ifile.is_open();
            if (is_arch)
                is_arch = readHeader(ifile, (DWord*)&nul, (DWord*)&nul, &nul, &nul);
            if (is_arch) {

                // input is archove
                ifile.close();
                act_cmp = false;

                // create file name for file list or for output folder
                if (!list) createUniqueName(name, &oname, "");
                else createUniqueName(name, &oname, "txt");
                cout << S_DECOMP << path(name).filename() << " -> "
                     << path(oname).filename() << endl << endl;
                archiveExtract(name, oname, list);
            } else {

                // input is  file to compress
                ifile.close();
                createUniqueName(name, &oname, ext);
                cout << S_COMP << path(name).filename() << " -> "
                     << path(oname).filename() << endl << endl;
                archiveCreate(name, oname);
            }
        } else {
            return;
        }

        setConsoleTextRed();

        // print  summary
        clock_t c_time = clock();
        double sec = double(c_time - c_begin) / CLOCKS_PER_SEC;
        int w1(9), w2(9), w3(4);

        // compression summary with ratio
        if (act_cmp) {
            w1 = 6; w2 = 6; w3 = 5;
            cout << endl << " Ratio: "
                << setw(10) << (float)total_output / (float)total_input * 100.0 << "% |";

        // decompression
        } else {
            cout << endl;
            for (int k = 0; k < 15; k++) cout << " ";
        }

        // input output sizes
        cout << " Size: " << setw(w1) << (total_input / 1024) << "kB -> "
             << setw(w2) << (total_output / 1024) << "kB | Time: "
             << setw(w3) << sec << "s "
             << endl << endl << " ";
        consoleWait();
    }
};

// file compression/decompression progress print
class ConsoleCodecCallback : public CodecCallbackInterface {
private:
    clock_t c_begin;
public:
    void init() { c_begin = clock(); }
    bool compressCallback(int in_size, int out_size, int stream_size, const char *f_name) {
        string fn;
        if (f_name == nullptr) fn = "";
        else {
            fn = f_name;
            fn.resize(28, ' ');
        }
        clock_t c_time = clock();
        double sec = double(c_time - c_begin) / CLOCKS_PER_SEC;
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
        return compressCallback(in_size, out_size, stream_size, f_name); 
    }
};

// console app wraper
class ConsoleApplication {
public:
    int run(int argc, char const *argv[]) {
        // set console title + write program info
        setConsoleTitle(S_TITLE);
        setConsoleTextRed();
        cout << S_INF1 << endl;
        setConsoleTextNormal();
        cout << S_INF2 << endl;

        // app takes only 1 argument
        if (argc > 1) {
            CodecSettings        sttgs;
            sttgs.Set(16, 16, 2, 8, 16, 3, 3);
            sttgs.byte_lkp_hsh = 5;
            LZHX                 lzhx(&sttgs);
            LZ                   lz  (&sttgs);
            Huffman              huffman;
            ConsoleCodecCallback callback;
            lzhx.setCodec   (&huffman);
            lzhx.setCodec   (&lz);
            lzhx.setCallback(&callback);
            bool list = false;
            if (argc > 2) list = (bool)(argv[2][1] == 'l' || argv[2][1] == 'L');
            lzhx.detectInput(string(argv[1]), list);
        } else {
            // print usage info
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

// main()
int main(int argc, char const *argv[]) {
    LZHX::ConsoleApplication app;
    try { return app.run(argc, argv);
    } catch (string &msg)  { cout << msg << endl;
    } catch (exception &e) { cout << LZHX::S_ERR_EX   << e.what() << endl;
    } catch (...) {          cout << LZHX::S_ERR_UNEX << endl; }
    LZHX::consoleWait();
    return 1;
}
