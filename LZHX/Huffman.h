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

class HuffmanTree {

public:
	HuffmanTree *left, *right;
    int symbol, freq;
	HuffmanTree();
	void clear();
	bool isLeaf();
};

class HuffmanCode {
public:
	int code, bit_count;
	void clear();
	HuffmanCode();
};

class HuffmanSymbolPair {
public:
	HuffmanTree *first;
	HuffmanTree *second;
	HuffmanSymbolPair();
};

class Huffman : public CodecInterface {
private:
	int alphabet_size;
	int nodes_array_size;
    int total_in, total_out;
    int stream_size;
	CodecStream *codec_stream;
	HuffmanTree  *nodes;
	HuffmanTree  *parents;
	HuffmanTree  *root;
	HuffmanCode  *codes;
	BitStream    *bit_stream;
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
