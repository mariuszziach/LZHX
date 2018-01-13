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

// czyszczenie warto�ci
void LZMatch::clear() {
    pos = 0; // pozycja dopasowania
    len = 0; // d�ugo�� dopasowania
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
// konstruktor alokuje pami�� dla mapy i dla bufora cyklicznego
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
// przypisanie bufora z danymi wej�ciowymi do obiektu
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
// funkcja dodaje do s�ownika element na podstawie
// podanej pozycji we wcze�niej zdefiniowanym buforze
//
void LZMatchFinder::insert(int pos) {

    // nie mo�emy doda� elementu do s�ownika
    // poniewa� pozycja w buforze nie pozwala na stworzenie
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

    // element bufora cyklicznego kt�ry trzeba wyczy�ci�
    LZDictionaryNode *empty = cyclic_buf + cyclic_iterator++;

    // czyszczenie tego elementu
    empty->clear();

    // pierwszy element mapy s�ownika kopiujemy do 
    // wlasnie wyczyszczonego elementu bufora cyklicznego
    // co przesunie go na drug� pozycje w s�owniku

    empty->pos = current->pos;

    empty->next = current->next;
    if (empty->next) {
        empty->next->prev = empty;
    }
    empty->prev = current;
    empty->valid = true;

    // a na pierwsz� pozycj� w s�owniku wstawiamy nowy element
    current->pos = lz_buf->getPos();
    current->next = empty;
    current->prev = nullptr;
    current->valid = true;
}

//
// funkcja wyszukuje najlepsze dopasowanie do ci�gu znak�w
// znajduj�cych si� na podanej pozycji we wcze�niej zdefiniowanym buforze
//
LZMatch *LZMatchFinder::find(int pos) {

    // czy�cimy wcze�niejsze dopasowanie
    best_match->clear();

    // sprawdzamy czy nie znajdujemy si� na samym pocz�tku lub na samym ko�cu bufora
    // je�eli tak to zwracamy brak dopasowania (best_match->len = 0)
    if (buf_size - pos <= (int)(cdc_sttgs->byte_lkp_hsh) || pos == 0) return best_match;

    // tworzymy i wybieramy klucz dla wyszukiwanego ciagu znak�w
    int index = hash(buf + pos);
    LZDictionaryNode *current = hash_map + index;

    // �eby unikn�� czasoch�onnego przeszukiwania s�ownika
    // ograniczamy je przy pomocy kontroli ilo�ci powt�rze� p�tli
    int runs = 0;

    // dop�ki w s�owniku s� pasuj�ce elementy i nie przekroczyli�my limitu wyszukiwa�
    while (current && (runs++ < 32)) {

        // element jest prawdziwym elementem
        if (current->valid) {

            int i = 0;

            // zliczamy ilo�� identycznych znak�w
            // pasuj�cych do aktualnie wyszukiwanego ci�gu znak�w
            while (buf[pos + i] == lz_buf->getByte(current->pos + i)) {
                if (pos + i >= buf_size - 1 || current->pos + i >= lz_buf->size - 1) {
                    break;
                }

                // ograniczamy d�ugo�� powt�rzenia do 255 �eby�my
                // mogli je zapisa� przy pomocy jednego bajta
                if (i >= 0xFF)
                    break;
                i++;
            }

            // znale�li�my d�u�sze dopasowanie? 
            if (best_match->len < i) {
                best_match->pos = current->pos;
                best_match->len = i;
            }
        }


        // nast�pny element w s�owniku
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

    // aktualna implementacja algorytmu przy ka�dym nowym bloku danych
    // resetuje s�ownik i tworzy nowy
    lz_mf->assignBuffer(in, in_size, lz_buf);
    o[1] += write32To8Buf(out[1] + o[1], in_size);

    int i = 0;

    // przetwa�amy ca�e dane wyj�ciowe
    while (i < in_size) {

        // wyszukujemy dopasowanie
        if (i + 8 > in_size) {
            lz_match->len = 0;
        }
        else {
            lz_match = lz_mf->find(i);
        }

        // je�eli dopasowanie wystarczaj�co d�ugie wypisujemy je do danych wyj�ciowych
        // jako wska�nik (pozycja i d�ugo��) poprzedzony bitem 1
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
            // je�eli dopasowanie by�o za kr�tkie
            // wypisujemy bit 0 i aktualny znak
            out[0][o[0]++] = (Byte)(0);
            out[3][o[3]++] = (Byte)(in[i]); // d�ugo�� - 1 bajt

                                            // i dodajemy go do s�ownika
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

    // zwracamy wielko�� skompresowanych danych
    return out_size;
}

//
// funkcja dekompresuje blok danych o podanej d�ugo�ci
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

        // odczytujemy bit informuj�cy czy b�dziemy odczytywa� wska�nik (pozycje i d�ugo��)
        // czy pojedy�czy znak
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

            // kopiujemy pojedy�czy bajt
            lz_buf->putByte(in[3][i[3]]);
            out[o++] = in[3][i[3]++];
        }
    }

    // zliczamy przetworzone bajty
    total_in += i[0] + i[1] + i[2] + i[3];
    total_out += dec_size;

    // zwracamy wielko�� zapisanych danych
    cb_out->size = dec_size;
    return dec_size;
}
