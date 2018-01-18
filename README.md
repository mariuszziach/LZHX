# LZHX

LZHX is a very easy to use file archiver based on Lempel-Ziv and Huffman algorithms. To check integrity of files, program uses a FNV hashing algorithm and for encryption modified XOR cipher.

![LZHX](http://ziach.pl/gif.gif)

Program will automatically recognize whether the given parameter is an archive for decompression or a file/folder for compression. It will also prevent overwriting files by creating unique names for outputed files and folders if needed.

Compression time and ratio

LZHX is able to compress and encrypt 1.36 GB folder (mostly with text files) into 469 MB archive in 4 minutes 25 seconds. Decompression and decryption of the same archive took 3 minutes 15 seconds. Compression ratio is usually worse than in for example ZIP archives but LZHX is often faster and is able encrypt archive.
