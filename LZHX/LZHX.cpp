/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

// stl
#include <iostream>
#include <fstream>
#include <string>
#include <memory>

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

namespace LZHX {

struct ArchiveHeader {
    Byte  f_sig[4]; // LZHX
    DWord f_sig2;   // 0xFFFFFFFB
    DWord f_cnt;    // num of files
    DWord f_flgs;   // flags
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
    char const sig[4] = { 'L','Z','H','X' };
    //int  const bffrs_cnt = 8;

    ifstream ifile;
    ofstream ofile;

    //shared_ptr<Byte> wrkng_bffrs[8];
    //shared_ptr<CodecBuffer[8]> cdc_bffrs;

    CodecBuffer *cdc_bffrs;
    CodecStream cdc_strm;
    CodecInterface *lz_cdc, *hf_cdc;
    CodecCallbackInterface *cdc_cllbck;

    CodecSettings *sttgs;

    inline int outBlkSize(int inBlkSize) {
        return inBlkSize * 2;
    }

    bool init(char const *ifname, char const *ofname) {
        ifile.open(ifname, ios::binary);
        ofile.open(ofname, ios::binary);
        if (!ifile.is_open() || !ofile.is_open()) throw string(S_ERR_FOPN);

        cdc_strm.buf_count = sttgs->byte_bffr_cnt;
        cdc_strm.buf_stack = cdc_bffrs;

        for (int i = 0; i < int(sttgs->byte_bffr_cnt); i++) {
            cdc_bffrs[i].size = 0;
            cdc_bffrs[i].type = CBT_EMPTY;
        }

        cdc_strm.stream_size = 0;
        return true;
    }
    void writeHeader() { ofile.write(sig, sizeof(sig)); }
    bool readHeader () {
        char s[sizeof(sig)] = {0,};
        ifile.read(s, sizeof(sig));
        for (int i = 0; i < sizeof(sig); i++) {
            if (s[i] != sig[i]) return false;
        }
        return true;
    }
    void clean() {
        if (ifile.is_open()) ifile.close();
        if (ofile.is_open()) ofile.close();
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
        clean();
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

    bool compressFile(char const *ifname, char const *ofname) {
        if (!init(ifname, ofname)) return false;

        cdc_strm.stream_size = getFSize(ifile);
        writeHeader();

        hf_cdc->initStream(&cdc_strm);
        lz_cdc->initStream(&cdc_strm);

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
                ofile.write((char*)&hf_bf->size, 4);
                ofile.write((char*)hf_bf->mem, hf_bf->size);
                hf_bf->type = CBT_EMPTY;

                lz_bf = cdc_strm.find(CBT_LZ);
            }

            if (cdc_cllbck != nullptr && !(cc++ % 5)) {
                if (!cdc_cllbck->compressCallback(lz_cdc->getTotalIn(), hf_cdc->getTotalOut(), cdc_strm.stream_size)) {
                    return false; } }
        }

        if (cdc_cllbck != nullptr) 
            return cdc_cllbck->compressCallback(cdc_strm.stream_size, (int)ofile.tellp(), cdc_strm.stream_size);

        clean();
        return true;
    }

    //
    // funkcja dekompresuje podany plik do nowego pliku o podanej nazwie
    //
    bool decompressFile(const char *in_file_name, const char *out_file_name) {
        cout << "decompressFIle" << endl;
        // otwieramy pliki, alokujemy pamięć
        if (!init(in_file_name, out_file_name))
            return false;

        //assert(statistical_codec != nullptr);
        //assert(transform_codec != nullptr);
        //assert(string_codec != nullptr);

        // wielkość skompresowanego pliku
        cdc_strm.stream_size = getFSize(ifile);

        // odczytujemy nagłówek
        if (!readHeader()) {
            //assert(false);
            return false;
        }

        // inicjujemy kodek
        hf_cdc->initStream(&cdc_strm);
        lz_cdc->initStream(&cdc_strm);

        int cc = 0;

        // dla całych danych wejściowych
        while(ifile.tellg() < cdc_strm.stream_size)
        {
            for (int i = 0; i < 4; i++) {

                CodecBuffer *empty = cdc_strm.find(CBT_EMPTY);
                //assert(empty);

                ifile.read((char*)&empty->size, 4);
                ifile.read((char*)empty->mem, empty->size);

                //empty->type = cbt_str;

                empty->type = CBT_HF;
                hf_cdc->decompressBlock();
            }

            lz_cdc->decompressBlock();

            CodecBuffer *raw = cdc_strm.find(CBT_RAW);
            ofile.write((char*)raw->mem, raw->size);
            raw->type = CBT_EMPTY;

            if (cdc_cllbck != nullptr && !(cc++ % 5)) {
                if (!cdc_cllbck->decompressCallback(hf_cdc->getTotalIn(),
                    lz_cdc->getTotalOut(),
                    cdc_strm.stream_size)) {
                    return 0;
                }
            }
        }

        // informujemy funkcje monitorującą o końcu dekompresji
        if (cdc_cllbck != nullptr) {
            return cdc_cllbck->decompressCallback(cdc_strm.stream_size, (int)ofile.tellp(), cdc_strm.stream_size);
        }

        clean();

        return true;
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

        cout << "\r" << (in_size)  / 1024 << " kB ->\t" 
                     << (out_size) / 1024 << " kB | \t" 
                     << ((int)((in_size / (float)stream_size) * 100)) << "% | \t"
                     << sec << " sec\t";
        return true;
    }

    bool decompressCallback(int in_size, int out_size, int stream_size) {
        return compressCallback(in_size, out_size, stream_size); }
};

class ConsoleApplication {
public:
    int run(int argc, char const *argv[]) {
        if (argc >= 4) {
            bool do_compress = (string(argv[1]) == string(S_OPT_CMPS));

            CodecSettings        sttgs;
            sttgs.Set(16, 16, 2, 8, 16, 3);

            LZHX                 lzhx(&sttgs);
            LZ                   lz(&sttgs);
            Huffman              huffman;
            ConsoleCodecCallback callback;

            lzhx.setCodec(&huffman);
            lzhx.setCodec(&lz);
            lzhx.setCallback(&callback);

            cout << S_INF << endl;

            callback.clockStart();

            if (do_compress)  lzhx.compressFile  (argv[2], argv[3]);
            else              lzhx.decompressFile(argv[2], argv[3]);
            cout << endl;

        } else  { cout << S_INF << endl << S_USAGE << endl; } 

        return 0;
    }
};
} 

int main(int argc, char const *argv[])
{
    LZHX::ConsoleApplication app;
    try { return app.run(argc, argv);
    } catch (string &msg)  { cout << msg << endl;
    } catch (exception &e) { cout << "exception: " << e.what() << endl;
    } catch (...) {          cout << "unknown exception" << endl; }
    return 1;
}
