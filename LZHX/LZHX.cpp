/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

// stl
#include <filesystem>
#include <iostream>
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

// strings
char const S_INF[] = "Lempel-Ziv-Huffman file compressor\n"\
"author: mariusz.ziach@gmail.com\n";
char const S_USAGE[]    = "usage: lzhx.exe -<c | d> <input> <output>\n";
char const S_OPT_CMPS[] = "-c";
char const S_ERR_FOPN[] = "file open error";

using namespace std;
using namespace std::experimental::filesystem::v1;

namespace LZHX {

struct ArchiveHeader {
    Byte  f_sig[4];   // LZHX
    DWord f_sig2;     // 0xFFFFFFFB
    DWord f_cnt;      // num of files
    DWord f_flgs;     // flags
    QWord a_unc_size; // archive uncompressed size
    QWord a_cmp_size; // archive compressed size
};

struct FileHeader {
    // compressed and decompressed sizes
    DWord f_cmp_size;
    DWord f_dcm_size;

    // creation, last acces and write times
    QWord f_cr_time;
    QWord f_la_time;
    QWord f_lw_time;
    DWord f_attr;    // file attributes
    DWord f_nm_cnt;  // file name count
    DWord f_cnt_hsh; // file hash sum
};

class LZHX {
private:
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
        //cdc_strm.stream_size = 0;
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

    int compressFile(ifstream &ifile, ofstream &ofile) {
        int tot_out(0);

        if (!init()) return false;
        //cdc_strm.stream_size = getFSize(ifile);

        lz_cdc->initStream(&cdc_strm);
        hf_cdc->initStream(&cdc_strm);

        int cc = 0;
        while(ifile.good())
        {
            CodecBuffer *empty_bf = cdc_strm.find(CBT_EMPTY);
            ifile.read((char*)empty_bf->mem, sttgs->byte_blk_cap);
            empty_bf->size = (int)ifile.gcount(); empty_bf->type = CBT_RAW;

            lz_cdc->compressBlock();

            CodecBuffer *lz_bf = cdc_strm.find(CBT_LZ);
            while (lz_bf) {

                hf_cdc->compressBlock();
                CodecBuffer *hf_bf = cdc_strm.find(CBT_HF);
                ofile.write((char*)&hf_bf->size, sizeof(int));
                ofile.write((char*)hf_bf->mem, hf_bf->size);
                tot_out += sizeof(int) + hf_bf->size;
                hf_bf->type = CBT_EMPTY;

                lz_bf = cdc_strm.find(CBT_LZ);
            }

            if (cdc_cllbck != nullptr && !(cc++ % 5))
                cdc_cllbck->compressCallback(lz_cdc->getTotalIn(), hf_cdc->getTotalOut(), cdc_strm.stream_size);
        }

        if (cdc_cllbck != nullptr) 
            cdc_cllbck->compressCallback(lz_cdc->getTotalIn(), hf_cdc->getTotalOut(), cdc_strm.stream_size);

        return tot_out;
    }

    bool decompressFile(ifstream &ifile, ofstream &ofile) {
        int tot_in(0);
       
        if (!init()) return false;

        hf_cdc->initStream(&cdc_strm);
        lz_cdc->initStream(&cdc_strm);

        int cc = 0;
        while(tot_in < cdc_strm.stream_size) {
            
            for (int i = 0; i < 4; i++) {
                
                CodecBuffer *empty = cdc_strm.find(CBT_EMPTY);

                ifile.read((char*)&empty->size, sizeof(int));
                ifile.read((char*)empty->mem, empty->size);

                tot_in += sizeof(int) + empty->size;

                empty->type = CBT_HF;
                hf_cdc->decompressBlock();
            }

            lz_cdc->decompressBlock();

            CodecBuffer *raw = cdc_strm.find(CBT_RAW);
            ofile.write((char*)raw->mem, raw->size);
            raw->type = CBT_EMPTY;

            if (cdc_cllbck != nullptr && !(cc++ % 5)) 
                cdc_cllbck->decompressCallback(hf_cdc->getTotalIn(),
                lz_cdc->getTotalOut(), cdc_strm.stream_size);
        }

        if (cdc_cllbck != nullptr)
            cdc_cllbck->decompressCallback(hf_cdc->getTotalIn(), lz_cdc->getTotalOut(), cdc_strm.stream_size);

        return tot_in;
    }

private:
    Byte  const sig[4] = { 'L','Z','H','X' };
    char  const ext[5] = "lzhx";
    DWord const sig2   = 0xFFFFFFFB;

    void writeHeader(ofstream &ofile, DWord f_cnt, DWord f_flgs) {
        ArchiveHeader ah;
        ah.f_cnt = f_cnt; ah.f_flgs = f_flgs; ah.f_sig2 = sig2;
        memcpy(ah.f_sig, sig, sizeof(sig));
        ofile.write((char*)&ah, sizeof(ah));
    }

    bool readHeader(ifstream &ifile, DWord *f_cnt, DWord *f_flgs) {
        ArchiveHeader ah;
        ifile.read((char*)&ah, sizeof(ah));
        if (ifile.gcount() !=  sizeof(ah)) return false;
        if (memcmp(ah.f_sig, sig, sizeof(sig)) == 0 && ah.f_sig2 == sig2) {
            *f_cnt  = ah.f_cnt;
            *f_flgs = ah.f_flgs;
            return true;
        }
        else {
            *f_cnt = *f_flgs = 0;
            return false;
        }
    }

public:
    bool archiveAddFile(ofstream &arch, string &f) {
        ///////////////////////
        cout << path(f).filename() << endl;

        FileHeader fh; memset(&fh, 0, sizeof(FileHeader));
        cdc_strm.stream_size = fh.f_dcm_size = DWord(file_size(f));
        fh.f_nm_cnt = f.length();

        int h_pos = int(arch.tellp());

        /* TODO: more file information */

        arch.write((char*)&fh, sizeof(FileHeader));
        arch.write((char*)f.c_str(), fh.f_nm_cnt);

        ifstream ifile;
        ifile.open(f, ios::binary); if (!ifile.is_open()) return false;
        fh.f_cmp_size = compressFile(ifile, arch);

        int e_pos = int(arch.tellp());
        arch.seekp(h_pos);
        arch.write((char*)&fh, sizeof(FileHeader));

        arch.seekp(e_pos);

        if (ifile.is_open()) ifile.close();
        return true;
    }

    bool archiveCreate(string &dir_name, string &arch_name) {
        /* TODO: prevent overwrite archive file name */
        /* TODO: add empty folders */

        ofstream arch;
        arch.open(arch_name, ios::binary); if (!arch.is_open()) return false;
    
        int b_pos = int(arch.tellp());

        DWord f_cnt(0), f_flgs(0);
        writeHeader(arch, f_cnt, f_flgs);

        path dir(dir_name);

        if (is_directory(dir)) {
            dir = dir.filename();
            
            for (auto& itm : recursive_directory_iterator(dir)) {
                string &f = ((string&)((path&)itm).string());
                if (is_regular_file(f)) {

                    if (!archiveAddFile(arch, f))
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
        writeHeader(arch, f_cnt, f_flgs);

        if (arch.is_open()) arch.close();
        return true;
    }

    bool archiveExtract(string &arch_name, string &dir) {
        /* TODO: create root dir */
        /* TODO: use dir variable  */
        DWord f_cnt, f_flags;
        ifstream arch;

        arch.open(arch_name, ios::binary); if (!arch.is_open()) return false;
        if (!readHeader(arch, &f_cnt, &f_flags)) return false;

        for (int i = 0; i < int(f_cnt); i++) {
            FileHeader fh;
            string f_name;
            arch.read((char*)&fh, sizeof(FileHeader));

            for (int j = 0; j < int(fh.f_nm_cnt); j++) f_name += (char)arch.get();

            path p(f_name);
            if (p.has_parent_path()) {
                path newp(dir);
                for (auto it = p.begin(); it != p.end(); it++)
                    if (it != p.begin()) newp.append(*it);
                p = newp;
                create_directories(p.parent_path());
            } else {
                path base(f_name), pxt = p.extension();
                base.replace_extension("");
                while (exists(p)) {
                    p = base.concat("0");
                    p.replace_extension(pxt);
                }
            }

            ofstream ofile;
            ofile.open(p, ios::binary);
            cdc_strm.stream_size = fh.f_cmp_size;

            ////////////////////////////////
            cout << p.filename() << endl;
            decompressFile(arch, ofile);

            if (ofile.is_open()) ofile.close();
        }

        if (arch.is_open()) arch.close();
        return true;
    }

    void createUniqueName(string &name, string *out, bool wext = true) {
        path pn(name), base(name);
        string xt (wext ? ext : "");
        base.replace_extension("");
        pn.replace_extension(xt);
        while (exists(pn)) {
            pn = base.concat("0");
            pn.replace_extension(xt);
        }
        *out = pn.string();
    }

    bool detectInput(string &&name) {
        string oname;

        if (is_directory(name)) {
            /* compress directory */
            createUniqueName(name, &oname);
            cout << "compress directory" << endl << name << endl << oname << endl;
            archiveCreate(name, oname);
            
        } else if(is_regular_file(name)) {
            DWord nul;
            ifstream ifile(name, ios::binary);
            if (!ifile.is_open()) return false;
            if (readHeader(ifile, &nul, &nul)) {
                ifile.close();
                /* decompress archive */
                
                createUniqueName(name, &oname, false);
                cout << "decompress archive" << endl << name << endl << oname << endl;
                archiveExtract(name, oname);
                
            } else {
                ifile.close();
                /* compress one file */
                createUniqueName(name, &oname);
                cout << "compress file" << endl << name << endl << oname << endl;
                archiveCreate(name, oname);
                
            }
            
        } else {
            return false;
        }
        return false;
    }
};

class ConsoleCodecCallback : public CodecCallbackInterface {
private:
    clock_t begin;
public:
    void clockStart() { begin = clock(); }
    bool compressCallback(int in_size, int out_size, int stream_size) {
        clock_t time = clock();
        double sec = double(time - begin) / CLOCKS_PER_SEC;
        cout << "\r" << (in_size)  / 1024 << " kB -> " 
                     << (out_size) / 1024 << " kB | " 
                     << ((int)((in_size / (float)stream_size) * 100)) << "% | "
                     << sec << " sec\t";
        return true;
    }
    bool decompressCallback(int in_size, int out_size, int stream_size) {
        return compressCallback(in_size, out_size, stream_size); }
};

class ConsoleApplication {
public:
    int run(int argc, char const *argv[]) {

        cout << S_INF << endl;

        if (argc > 1) {
            CodecSettings        sttgs;
            sttgs.Set(16, 16, 2, 8, 16, 3/*<-3*/, 3);
            sttgs.byte_lkp_hsh = 5;

            LZHX                 lzhx(&sttgs);
            LZ                   lz(&sttgs);
            Huffman              huffman;
            ConsoleCodecCallback callback;

            lzhx.setCodec(&huffman);
            lzhx.setCodec(&lz);
            lzhx.setCallback(&callback);

            callback.clockStart();

            lzhx.detectInput(string(argv[1]));
        } else { cout << S_INF << endl << S_USAGE << endl; }
        return 0;
    }
};
}

int main(int argc, char const *argv[]) {
    LZHX::ConsoleApplication app;
    try { return app.run(argc, argv);
    } catch (string &msg)  { cout << msg << endl;
    } catch (exception &e) { cout << "exception: " << e.what() << endl;
    } catch (...) {          cout << "unknown exception" << endl; }
    return 1;
}
