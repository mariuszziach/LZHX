/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

#ifndef LZHX_HUFFMAN_H
#define LZHX_HUFFMAN_H

// LZHX
#include "Types.h"
#include "BitStream.h"

namespace LZHX {

// klasa reprezentująca drzewo Huffmana
class HuffmanTree {

public:
    // dzieci
	HuffmanTree *left;
	HuffmanTree *right;

    // zmienna przechowująca symbol (bajt/znak)
    int symbol;
    
    // częstotliwość występowania symbolu
	int freq;

	HuffmanTree();
	void clear();
	bool isLeaf();
};

// klasa reprezentująca kod bitowy symbolu stworzonego na podstawie drzewa
class HuffmanCode {
public:

    // kod bitowy (bity)
	int code;
    
    // ilość bitów w kodzie
	int bit_count;

	void clear();
	HuffmanCode();
};

//
// klasa reprezentuje pare symboli tworzących węzeł w drzewie
//
class HuffmanSymbolPair {

public:
	HuffmanTree *first;
	HuffmanTree *second;

	HuffmanSymbolPair();
};

//
// implementacja algorytmu Huffmana kompresująca strumień danych
// w niezależnych od siebie blokach
//
class Huffman : public CodecInterface {
private:

    // rozmiar alfabetu
	int alphabet_size;
    
    // zamiast alokować każdy potrzebny element drzewa osobno
    // tworzymy tablice równą długości 2 * wielkość alfabetu
    // z pustymni wierzchołkami
	int nodes_array_size;

    // dane dla funkcji monitorującej
    int total_in;
    int total_out;
    int stream_size;

	CodecStream *codec_stream;

    // puste węzły drzewa z których pierwsze (dla alfabetu 8 bitowego - 256)
    // zawierają informacje o częstotliwości występowania symboli
    // natomiast kolejne będą węzłami drzewa Huffmana
	HuffmanTree  *nodes;

    // puste węzły drzewa
	HuffmanTree  *parents;

    // wierzchołek drzewa
	HuffmanTree  *root;

    // tablica z kodami symboli
	HuffmanCode  *codes;

    // obsługa strumienia bitów
	BitStream    *bit_stream;

private:
	void reset();
	void countFrequencies(Byte *buf, int in_size);
	void findLowestFreqSymbolPair(HuffmanSymbolPair &sp);
	void buildTree();
	void makeCodes(HuffmanTree *node, int code, int bit_count);
	void writeTree(HuffmanTree *node);
	HuffmanTree *readTree(HuffmanTree *node);
	int decodeSymbol(HuffmanTree *node);

public:
	Huffman(int alphabet_size = 256);
	~Huffman();
	CodecType getCodecType();
	int getTotalIn();
	int getTotalOut();
	void initStream(CodecStream *codec_stream);
	int compressBlock();
	int decompressBlock();
};

} // namespace

#endif // LZHX_HUFFMAN_H
