/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////


#ifndef LZHX_LZ_H
#define LZHX_LZ_H

// LZHX
#include "Types.h"
#include "BitStream.h"

namespace LZHX {

class LZDictionaryBuffer {
public:
    Byte * arr;
    int pos, size, cap;

    LZDictionaryBuffer(int cap = 0xFFFF);
    ~LZDictionaryBuffer();

    Byte *putByte(Byte val);
    Byte getByte(int p);
    int getPos();
    int convPos(bool absToRel, int pos);

    //int absToRel(int abs);
    //int relToAbs(int rel);
};

//
// pozycja znalezionego dopasowania
//
class LZMatch {
public:
    int pos;
    int len;

    LZMatch();
    void clear();
    void copy(LZMatch *lzm);
};

//
// element słownika LZ
// defacto jest to element listy dwukierunkowej
//
class LZDictionaryNode {
public:

    // zmienna przechowująca informacje na temat pozycji w buforze
    int pos;

    // następny element w liście
    LZDictionaryNode *next;

    // poprzedni element w liście
    LZDictionaryNode *prev;

    // element jest prawdziwy?
    bool valid;

    LZDictionaryNode();
    void clear();
};


//
// implementacja słownika LZ
// słownik skonstruowany jest z mapy przechowującej informacje na temat
// dotychczas przetworzonych danych w której każdy element jest listą dwukiernukową
// której to z kolei elementy są elementami bufora cyklicznego dla zachowania
// stałej wielkości słownika
//
class LZMatchFinder {
private:

    CodecSettings *cdc_sttgs;

    LZDictionaryBuffer *lz_buf;

    // wskaźnik bufora na podstawie którego będzie tworzony słownik oraz jego rozmiar
    Byte *buf;

    // tablica mieszająca używana do stworzenia mapy słownika
    // przetworzonych już danych
    LZDictionaryNode *hash_map;

    // dzięki użyciu bofora cyklicznego rozmiar słownika jest stały (aktualnie 64kB)  
    // bez potrzeby ciągłego alokowania pamięci
    LZDictionaryNode *cyclic_buf;
    int cyclic_iterator, buf_size;

    // najlepsze dopasowanie
    LZMatch *best_match;

public:

    //
    // konstruktor alokuje pamięć dla mapy i dla bufora cyklicznego
    // oraz inicjuje zmienne
    //
    LZMatchFinder(CodecSettings *cdc_sttgs);
    ~LZMatchFinder();
    void assignBuffer(Byte *buf, int buf_size, LZDictionaryBuffer *lz_buf);
    int hash(Byte *in);
    void insert(int pos);
    LZMatch *find(int pos);
};

//
// implementacja algorytmu LZ
//
class LZ : public CodecInterface {
private:

    // dane dla funkcji monitorującej
    int total_in;
    int total_out;
    int stream_size;

    CodecStream *codec_stream;
    CodecSettings *cdc_sttgs;

    // obsługa strumienia bitów
    BitStream    *bit_stream;

    // słownik
    LZMatchFinder      *lz_mf;
    LZMatch            *lz_match;
    LZDictionaryBuffer *lz_buf;

public:

    LZ(CodecSettings *cdc_sttgs);
    ~LZ();
    CodecType getCodecType();
    int getTotalIn();
    int getTotalOut();
    void initStream(CodecStream *codec_stream);
    int compressBlock();
    int decompressBlock();
};

} // namespace

#endif // LZHX_LZ_H
