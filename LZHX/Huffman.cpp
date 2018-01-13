/////////////////////////////////////////
// Lempel-Ziv-Huffman File Compressor  //
// author: mariusz.ziach@gmail.com     //
// date  : 2018                        //
/////////////////////////////////////////

#include "Huffman.h"
#include <cassert>
#include <iostream>

using namespace std;
using namespace LZHX;

HuffmanTree::HuffmanTree() {
	clear();
}

// 
// funkcja czy�ci w�ze� drzewa
//
void HuffmanTree::clear() {
	symbol = 0;
	freq = 0;
	left = nullptr;
	right = nullptr;
}

//
// funkcja sprawdza czy w�ze� jest li�ciem
//
bool HuffmanTree::isLeaf() {
	if (left == nullptr && right == nullptr) {
		return true;
	}
	return false;
}

void HuffmanCode::clear() {
	code = 0;
	bit_count = 0;
}

//
// konstruktor
// 
HuffmanCode::HuffmanCode() {
	clear();
}

HuffmanSymbolPair::HuffmanSymbolPair() {
	first = nullptr;
	second = nullptr;
}

//
// funkcja resetuje w�z�y drzewa, tablice kod�w bitowych symboli,
// wierzcho�ek drzewa i wska�nik na puste wierzcho�ki drzewa (parents)
//
void Huffman::reset() {

	// zerujemy w�z�y drzewa
	for (int i = 0; i < alphabet_size * 2; i++) {
		nodes[i].clear();
		nodes[i].symbol = i;
	}

	// zerujemy tablice kod�w
	for (int i = 0; i < alphabet_size; i++) {
		codes[i].clear();
	}

	root = nullptr;
	parents = nodes + alphabet_size;
}

//
// obliczamy cz�stotliwo�� wyst�powania poszczeg�lnych symboli
//
void Huffman::countFrequencies(Byte *buf, int in_size) {
	for (int i = 0; i < in_size; i++) {
		nodes[buf[i]].freq++;
	}
}

//
// funkcja wyszukuje 2 najrzadziej wyst�puj�ce symbole w tablicy
//
void Huffman::findLowestFreqSymbolPair(HuffmanSymbolPair &sp) {

	sp.first = nullptr;
	sp.second = nullptr;

	int i = 0;
	while (i < nodes_array_size) {

		HuffmanTree *currentNode = nodes + i;

		if (currentNode->freq > 0) {

			// nie znale�li�my jeszcze pierwszego elementu
			if (sp.first == nullptr) { sp.first = currentNode; }

			// nie znale�li�my drugiego elementu
			else if (sp.second == nullptr) { sp.second = currentNode; }

			// znale�li�my element mniejszy od poprzednio znalezionego
			// pierwszego symbolu
			else if (sp.first->freq  > currentNode->freq) { sp.first = currentNode; }

			// element mniejszy od drugiego poprzednio znalezionego symbolu
			else if (sp.second->freq > currentNode->freq) { sp.second = currentNode; }
		}
		i++;
	}
}

//
// funkcja buduje drzewo Huffmana, na podstawie cz�stoliwo�ci
// wyst�powania poszczeg�lnych symboli
//
void Huffman::buildTree() {

	while (true) {

		HuffmanSymbolPair sp;
		findLowestFreqSymbolPair(sp);

		// je�eli znale�li�my tylko pierwszy element
		// a drugiego ju� nie  
		// to znaczy, �e w tablicy zostal juz tylko wierzcho�ek
		if (sp.second == nullptr) {

			//assert(sp.first != nullptr);
			root = sp.first;
			break;

		}
		else {

			// je�eli znale�li�my oba elementy
			// to tworzymy w�ze� drzewa zawieraj�cy
			// sum� cz�stotliwo�ci ich wyst�powania
			// a jako dziecmi b�d� w�a�nie znalezione elementy
			parents->freq = sp.first->freq + sp.second->freq;
			parents->right = sp.first;
			parents->left = sp.second;

			// "usuwamy" elementy z tablicy w�z��w
			sp.first->freq = 0;
			sp.second->freq = 0;
			parents++;
		}
	}
}

//
// funkcja tworzy kody bitowe na podstawie drzewa
//
void Huffman::makeCodes(HuffmanTree *node, int code, int bit_count) {

	// je�eli w�ze� nie ma dzieci, przypisujemy kod bitowy do symbolu
	if (node->isLeaf()) {
		//assert(node->symbol < 256);
		//assert(bit_count < 32);

		codes[node->symbol].code = code;
		codes[node->symbol].bit_count = bit_count;
	}

	// idziemy w prawo i dodajemy bit 1 do kodu
	if (node->right != nullptr)
	{
		makeCodes(node->right, code | (1 << bit_count), bit_count + 1);
	}

	// idziemy w lewo i dodajemy bit 0 do kodu
	if (node->left != nullptr)
	{
		makeCodes(node->left, code, bit_count + 1);
	}
}

//
// funkcja zapisuje drzewo do bufora za pom�c� strumienia bit�w
//
void Huffman::writeTree(HuffmanTree *node) {

	// li��
	if (node->isLeaf()) {

		// zapisujemy do do bufora bit 1 i symbol
		bit_stream->writeBit(1);
		bit_stream->writeBits(node->symbol, 8);

		// w�ze�
	}
	else {

		// zapisujemy do bufora bit 0
		bit_stream->writeBit(0);
	}

	// zapisujemy prawe dziecko
	if (node->right != nullptr) { writeTree(node->right); }

	// zapisujemy lewe dziecko
	if (node->left != nullptr) { writeTree(node->left); }
}

// 
// funkcja odczytuje drzewo z bufora
//
HuffmanTree *Huffman::readTree(HuffmanTree *node) {

	// odczytujemy bit
	int bit = bit_stream->readBit();

	// 1 = li��
	if (bit == 1) {

		// odczytujemy symbol
		node->symbol = bit_stream->readBits(8);
		node->right = nullptr;
		node->left = nullptr;
		node->freq = 0;

		// zwracamy nast�pny pusty element w tablicy w�z��w
		return node;

		// 0 = w�ze�
	}
	else if (bit == 0) {

		// przypisujemy puste elementy z tablicy w�z��w
		// do lewego i prawego dziecka...
		node->right = node + 1;
		node->left = readTree(node->right) + 1;


		// ...i zwracamy pusty element
		return readTree(node->left);
	}

	return nullptr;
}

//
// funkcja dekoduje symbol odczytuj�c bity ze strumienia, na podstawie
// odczytanego wcze�niej drzewa
//
int Huffman::decodeSymbol(HuffmanTree *node) {

	// li�� = odczytujemy i zwracamy symbol
	if (node->isLeaf()) {

		//assert(node->symbol < 256);
		return node->symbol;
	}

	// w�ze� = szukamy dalej
	int bit = bit_stream->readBit();

	if      (bit == 1) { return decodeSymbol(node->right); }
	else if (bit == 0) { return decodeSymbol(node->left);  }
	else                 return 0;
}

//
// konstruktor - alokacja pami�ci
//
Huffman::Huffman(int alphabet_size) {
	// wielko�� alfabetu
	this->alphabet_size = alphabet_size;

	// wielko�� tablicy z w�z�ami
	nodes_array_size = alphabet_size * 2;

	// alokacja pamieci
	nodes = new HuffmanTree[nodes_array_size];
	//assert(nodes);

	codes = new HuffmanCode[alphabet_size];
	//assert(codes);

	bit_stream = new BitStream;
	//assert(bit_stream);
}

//
// destruktor - zwalnianie pami�ci
//
Huffman::~Huffman() {
	delete[] nodes;
	delete[] codes;
	delete bit_stream;
}

//
// typ kodeka
//
CodecType Huffman::getCodecType() {
	return CT_HF;
}

int Huffman::getTotalIn() {
	return total_in;
}

int Huffman::getTotalOut() {
	return total_out;
}

//
// inicjacja strumienia
//
void Huffman::initStream(CodecStream *codec_stream) {
	/*
	assert(in);
	assert(out);
	assert(in_max_size > 0);
	assert(out_max_size > 0);
	*/
	// zmienne
	/*this->in = in;
	this->out = out;
	this->in_max_size = in_max_size;
	this->out_max_size = out_max_size;*/
	this->codec_stream = codec_stream;

	this->total_in = 0;
	this->total_out = 0;
	this->stream_size = stream_size;
}

//
// funkcja kompresuje pojedynczy blok danych o podanej d�ugo�ci.
//
int Huffman::compressBlock() {
    CodecBuffer *cb_in, *cb_out;
    Byte *in, *out;

    cb_in        = codec_stream->find(CBT_LZ);
	cb_out       = codec_stream->find(CBT_EMPTY); 
    cb_out->type = CBT_HF;

    in           = cb_in->mem;
	out          = cb_out->mem;

	cb_in->type  = CBT_EMPTY;

	int in_size  = cb_in->size;
	int out_cap  = cb_out->cap;

	reset();

	// przypisujemy bufor wyj�ciowy do klasy obs�uguj�cej
	// odczyt i zapis bitowy 
	bit_stream->assignBuffer(out);

	// zapisujemy w buforze rozmiar danych wej�ciowych
	bit_stream->writeBits(in_size, 32);

	// zliczamy cz�stotliwo�� wyst�powania symboli
	countFrequencies(in, in_size);

	// budujemy drzewo
	buildTree();

	// na podstawie drzewa tworzymy kody bitowe symboli
	makeCodes(root, 0, 0);

	// zapisujemy drzewo Huffmana do bufora
	writeTree(root);

	for (int i = 0; i < in_size; i++) {

		//assert(bit_stream->getBytePos() < out_cap);

		// aktualny kod bitowy symbolu
		HuffmanCode *currentCode = codes + in[i];

		// zapisujemy do bufora
		bit_stream->writeBits(currentCode->code, currentCode->bit_count);
	}

	// domykamy bufor
	while (bit_stream->getBitPos() > 0)
		bit_stream->writeBit(0);

	// zliczamy przetworzone bajty
	total_in  += in_size;
	total_out += bit_stream->getBytePos();

	cb_out->size = bit_stream->getBytePos();

	return cb_out->size;
}

//
// funkcja dekompresuje pojedynczy blok
//
int Huffman::decompressBlock() {
	//assert (codec_stream->in.size > 4);
	//assert(total_in < codec_stream->stream_size);
	CodecBuffer *cb_in = codec_stream->find(CBT_HF); //assert(cb_in);
	Byte *in = cb_in->mem;

	CodecBuffer *cb_out = codec_stream->find(CBT_EMPTY);  cb_out->type = CBT_LZ; //assert(cb_out);
	Byte *out = cb_out->mem;

	cb_in->type = CBT_EMPTY;

	int in_size = cb_in->size;
	int out_cap = cb_out->cap;

	// resetujemy dekompresor
	reset();

	// przypisujemy bufor wej�ciowy
	bit_stream->assignBuffer(in);

	// odczytujemy rozmiar danych wyjsciowych
	int dec_size = bit_stream->readBits(32);
	//assert(dec_size <= codec_stream->in.capacity);

	// odczytujemy drzewo Huffmana
	readTree(nodes);

	// koniec bufora wyjsciowego
	for (int o = 0; o < dec_size; o++) {
		 
		//assert(i < codec_stream->in.capacity);
		//assert(i < codec_stream->in.capacity);

		// wypisujemy do bufora zdekodowany bajt
		out[o] = decodeSymbol(nodes);
	}

	// zliczamy przetworzone bajty
	total_in += in_size;
	total_out += dec_size;
	
	cb_out->size = dec_size;

	return dec_size;
}
