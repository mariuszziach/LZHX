/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

//stl
#include <iostream>
#include <sstream>

// c
#include <cassert>

// LHZX
#include "LZ.h"
#include "Utils.h"

using namespace LZHX;
using namespace std;

LZDictionaryBuffer::LZDictionaryBuffer(int cap) {
    this->cap = cap;
    arr = new Byte[cap];
    assert(arr);
    pos = 0;
    size = 0;
    for (int i = 0; i < cap; i++) {
        arr[i] = 0;
    }
}

LZDictionaryBuffer::~LZDictionaryBuffer() {
    if (arr) delete[] arr;
}

Byte *LZDictionaryBuffer::putByte(Byte val) {
    if (pos >= cap) {
        pos = 0;
    }
    if (size < cap) {
        size++;
    }
    arr[pos] = val;
    Byte *r = &arr[pos];
    pos++;
    return r;
}

int LZDictionaryBuffer::convPos(bool absToRel, int pos) {
    int ret = 0;
    if (absToRel) {
        if (this->pos >= pos) {
            ret = this->pos - pos;
        }
        else {
            ret = (this->cap - pos) + this->pos;
        }
    }
    else {
        if (this->pos >= pos) {
            ret = this->pos - pos;
        }
        else {
            ret = this->cap - (pos - this->pos);
        }
    }
    return ret;
}

Byte LZDictionaryBuffer::getByte(int p) {
    return arr[p];
}

int LZDictionaryBuffer::getPos() {
    return pos;
}

LZMatch::LZMatch() {
    clear();
}

// czyszczenie wartoœci
void LZMatch::clear() {
    pos = 0; // pozycja dopasowania
    len = 0; // d³ugoœæ dopasowania
}

// kopiowanie
void LZMatch::copy(LZMatch *lzm) {
    assert(lzm);
    pos = lzm->pos;
    len = lzm->len;
}

//
// konstruktor
//
LZDictionaryNode::LZDictionaryNode() {
    next = nullptr;
    prev = nullptr;
    clear();
}

// czyszczenie elementu
void LZDictionaryNode::clear() {
    if (next != nullptr) {
        next->prev = nullptr;
    }

    if (prev != nullptr) {
        prev->next = nullptr;
    }

    pos = 0;
    //p = nullptr;
    next = nullptr;
    prev = nullptr;
    valid = false;
}

//
// konstruktor alokuje pamiêæ dla mapy i dla bufora cyklicznego
// oraz inicjuje zmienne
//
LZMatchFinder::LZMatchFinder(CodecSettings *cdc_sttgs) {
    this->lz_buf   = nullptr;
    this->buf      = nullptr;
    this->buf_size = 0;

    DUMP(cdc_sttgs->byte_lkp_cap); DUMP(cdc_sttgs->byte_mtch_pos); DUMP(cdc_sttgs->byte_lkp_hsh);
    DUMP(cdc_sttgs->byte_bffr_cnt); DUMP(cdc_sttgs->byte_blk_cap); DUMP(cdc_sttgs->byte_mtch_len);

    this->hash_map   = new LZDictionaryNode[cdc_sttgs->byte_lkp_cap ];  
    this->cyclic_buf = new LZDictionaryNode[cdc_sttgs->byte_mtch_pos];
    this->best_match = new LZMatch; 

    assert(this->hash_map); assert(this->best_match); assert(this->cyclic_buf);

    for (int i = 0; i < (int)cdc_sttgs->byte_lkp_cap;  i++) hash_map  [i].clear();
    for (int i = 0; i < (int)cdc_sttgs->byte_mtch_pos; i++) cyclic_buf[i].clear();

    this->cdc_sttgs       = cdc_sttgs;
    this->cyclic_iterator = 0;
}

//
// destruktor
//
LZMatchFinder::~LZMatchFinder() {
    delete[] this->hash_map;
    delete[] this->cyclic_buf;
    delete this->best_match;
}

//
// przypisanie bufora z danymi wejœciowymi do obiektu
//
void LZMatchFinder::assignBuffer(Byte *buf, int buf_size, LZDictionaryBuffer *lz_buf) {
    this->buf = buf;
    this->buf_size = buf_size;
    this->lz_buf = lz_buf;
}

int LZMatchFinder::hash(Byte *in) {
    DWord hash = 0x811C9DC5;
    for (int i = 0; i < int(cdc_sttgs->byte_lkp_hsh); i++) {
        hash ^= in[i];
        hash *= 0x1000193;
    }
    return hash & cdc_sttgs->mask_mtch_pos;
}

// 
// funkcja dodaje do s³ownika element na podstawie
// podanej pozycji we wczeœniej zdefiniowanym buforze
//
void LZMatchFinder::insert(int pos) {

    // nie mo¿emy dodaæ elementu do s³ownika
    // poniewa¿ pozycja w buforze nie pozwala na stworzenie
    // klucza mapy (hashu)

    if (buf_size - pos <= (int)cdc_sttgs->byte_lkp_hsh /*|| lz_buf->pos == 0 || lz_buf->pos > lz_buf->size*/) {
        return;
    }

    // zerowanie pozycji bufora cyklicznego
    if (cyclic_iterator >= 0xffff)
        cyclic_iterator = 0;

    // klucz mapy (hash)
    int index = hash(buf + pos);

    // aktualny element mapy
    LZDictionaryNode *current = hash_map + index;

    // element bufora cyklicznego który trzeba wyczyœciæ
    LZDictionaryNode *empty = cyclic_buf + cyclic_iterator++;

    // czyszczenie tego elementu
    empty->clear();

    // pierwszy element mapy s³ownika kopiujemy do 
    // wlasnie wyczyszczonego elementu bufora cyklicznego
    // co przesunie go na drug¹ pozycje w s³owniku

    empty->pos = current->pos;

    empty->next = current->next;
    if (empty->next) {
        empty->next->prev = empty;
    }
    empty->prev = current;
    empty->valid = true;

    // a na pierwsz¹ pozycjê w s³owniku wstawiamy nowy element
    current->pos = lz_buf->getPos();
    current->next = empty;
    current->prev = nullptr;
    current->valid = true;
}

//
// funkcja wyszukuje najlepsze dopasowanie do ci¹gu znaków
// znajduj¹cych siê na podanej pozycji we wczeœniej zdefiniowanym buforze
//
LZMatch *LZMatchFinder::find(int pos) {

    // czyœcimy wczeœniejsze dopasowanie
    best_match->clear();

    // sprawdzamy czy nie znajdujemy siê na samym pocz¹tku lub na samym koñcu bufora
    // je¿eli tak to zwracamy brak dopasowania (best_match->len = 0)
    if (buf_size - pos <= (int)(cdc_sttgs->byte_lkp_hsh) || pos == 0) return best_match;

    // tworzymy i wybieramy klucz dla wyszukiwanego ciagu znaków
    int index = hash(buf + pos);
    LZDictionaryNode *current = hash_map + index;

    // ¿eby unikn¹æ czasoch³onnego przeszukiwania s³ownika
    // ograniczamy je przy pomocy kontroli iloœci powtórzeñ pêtli
    int runs = 0;

    // dopóki w s³owniku s¹ pasuj¹ce elementy i nie przekroczyliœmy limitu wyszukiwañ
    while (current && (runs++ < 32)) {

        // element jest prawdziwym elementem
        if (current->valid) {

            int i = 0;

            // zliczamy iloœæ identycznych znaków
            // pasuj¹cych do aktualnie wyszukiwanego ci¹gu znaków
            while (buf[pos + i] == lz_buf->getByte(current->pos + i)) {
                if (pos + i >= buf_size - 1 || current->pos + i >= lz_buf->size - 1) {
                    break;
                }

                // ograniczamy d³ugoœæ powtórzenia do 255 ¿ebyœmy
                // mogli je zapisaæ przy pomocy jednego bajta
                if (i >= 0xFF)
                    break;
                i++;
            }

            // znaleŸliœmy d³u¿sze dopasowanie? 
            if (best_match->len < i) {
                best_match->pos = current->pos;
                best_match->len = i;
            }
        }


        // nastêpny element w s³owniku
        current = current->next;
    }

    // zwracamy najlepsze znalezione dopasowanie
    return best_match;
}


LZ::LZ(CodecSettings *cdc_sttgs) {
    bit_stream = new BitStream; 
    lz_mf      = new LZMatchFinder(cdc_sttgs);
    lz_buf     = new LZDictionaryBuffer(cdc_sttgs->byte_mtch_pos);
    lz_match   = nullptr;

    this->cdc_sttgs = cdc_sttgs;
}

LZ::~LZ() {
    if (bit_stream) delete bit_stream;
    if (lz_mf)      delete lz_mf;
    if (lz_buf)     delete lz_buf;
}

CodecType LZ::getCodecType() { return CT_LZ; }
int LZ::getTotalIn() { return total_in; }
int LZ::getTotalOut() { return total_out; }

void LZ::initStream(CodecStream *codec_stream) {
    this->codec_stream = codec_stream;
    this->total_in     = 0;
    this->total_out    = 0;
    this->stream_size  = stream_size;
}

int LZ::compressBlock() {
    assert(lz_mf);

    CodecBuffer *cb_in, *cb_out[4];
    Byte *in, *out[4]; int o[4];

    cb_in = codec_stream->find(CBT_RAW);  assert(cb_in);
    in    = cb_in->mem;

    for (int i = 0; i < 4; i++) {
        o[i] = 0;

        cb_out[i] = codec_stream->find(CBT_EMPTY); assert(cb_out[i]);
        cb_out[i]->type = CBT_LZ;

        out[i] = cb_out[i]->mem;
        out[i][o[i]++] = i;
    }

    cb_in->type = CBT_EMPTY;

    int in_size = cb_in->size;

    // aktualna implementacja algorytmu przy ka¿dym nowym bloku danych
    // resetuje s³ownik i tworzy nowy
    lz_mf->assignBuffer(in, in_size, lz_buf);
    o[1] += write32To8Buf(out[1] + o[1], in_size);

    int i = 0;

    // przetwa¿amy ca³e dane wyjœciowe
    while (i < in_size) {

        // wyszukujemy dopasowanie
        if (i + 8 > in_size) {
            lz_match->len = 0;
        }
        else {
            lz_match = lz_mf->find(i);
        }

        // je¿eli dopasowanie wystarczaj¹co d³ugie wypisujemy je do danych wyjœciowych
        // jako wskaŸnik (pozycja i d³ugoœæ) poprzedzony bitem 1
        if (lz_match->len > 3 && (lz_match->len + lz_match->pos + 8) < in_size) {

            assert(lz_match->len <= 0xFF);
            assert(lz_match->pos <= 0xFFFF);
            assert(lz_match->len + lz_match->pos <= 0xFFFF);

            out[0][o[0]++] = (Byte)(1); // control
            o[1] += write16To8Buf(out[1] + o[1], lz_buf->convPos(true, lz_match->pos)); // pos
            out[2][o[2]++] = (Byte)(lz_match->len); // len

            while (lz_match->len--) {
                lz_mf->insert(i);
                lz_buf->putByte(in[i++]);
            }

        }
        else {
            // je¿eli dopasowanie by³o za krótkie
            // wypisujemy bit 0 i aktualny znak
            out[0][o[0]++] = (Byte)(0);
            out[3][o[3]++] = (Byte)(in[i]); // d³ugoœæ - 1 bajt

                                            // i dodajemy go do s³ownika
            lz_mf->insert(i);
            lz_buf->putByte(in[i++]);

        }

    }
    int out_size = 0;
    for (int i = 0; i < 4; i++) {
        out_size += o[i];
        cb_out[i]->size = o[i];
    }

    // zliczamy przetworzone bajty
    total_in += in_size;
    total_out += out_size;

    //DUMP(in_size);  DUMP(out_size);

    // zwracamy wielkoœæ skompresowanych danych
    return out_size;
}

//
// funkcja dekompresuje blok danych o podanej d³ugoœci
//
int LZ::decompressBlock() {
    CodecBuffer *cb_out = codec_stream->find(CBT_EMPTY); assert(cb_out);
    Byte *out = cb_out->mem;

    CodecBuffer *cb_in[4];
    Byte *temp_in[4], *in[4]; int i[4];

    for (int j = 0; j < 4; j++) {
        cb_in[j] = codec_stream->find(CBT_LZ); assert(cb_in[j]);
        cb_in[j]->type = CBT_EMPTY;
        temp_in[j] = cb_in[j]->mem;
        in[j] = nullptr;
        i[j] = 1;
    }

    cb_out->type = CBT_RAW;

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (temp_in[i][0] == j) {
                in[j] = temp_in[i];
            }
        }
    }

    assert(in[0] && in[1] && in[2] && in[3]);

    int dec_size = read32From8Buf(in[1] + i[1]); i[1] += sizeof(DWord);

    // koniec bufora wyjsciowego
    for (int o = 0; o < dec_size; ) {

        // odczytujemy bit informuj¹cy czy bêdziemy odczytywaæ wskaŸnik (pozycje i d³ugoœæ)
        // czy pojedyñczy znak
        if (in[0][i[0]++]) {
            int pos, len;

            pos = read16From8Buf(in[1] + i[1]); i[1] += sizeof(Word);
            pos = lz_buf->convPos(false, pos);
            len = in[2][i[2]++];

            assert(pos < 0xFFFF);
            assert(len <= 0xFF);
            assert(pos + len <= 0xFFFF);

            // kopiujemy znaki z podanej pozycji
            Byte bytesToPut[256];
            for (int j = 0; j < len; j++) {
                Byte b = lz_buf->getByte(pos + j);
                bytesToPut[j] = b;

                out[o++] = b;
            }
            for (int j = 0; j < len; j++) {
                lz_buf->putByte(bytesToPut[j]);
            }
        }
        else {

            // kopiujemy pojedyñczy bajt
            lz_buf->putByte(in[3][i[3]]);
            out[o++] = in[3][i[3]++];
        }
    }

    // zliczamy przetworzone bajty
    total_in += i[0] + i[1] + i[2] + i[3];
    total_out += dec_size;

    // zwracamy wielkoœæ zapisanych danych
    cb_out->size = dec_size;
    return dec_size;
}
