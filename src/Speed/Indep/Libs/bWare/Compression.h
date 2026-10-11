#ifndef BWARE_COMPRESSION_H
#define BWARE_COMPRESSION_H

// JDLZ block: "JDLZ", version byte 2, then uncompressed size at +8, total size at +0xc.
int JdlzDecompress(int *source, unsigned char *output);
// Extra prefix bits of the Huffman decoder, 0x8650f8 (zero in retail data).
extern int huffExtraBits;

unsigned int HuffDecode(const unsigned char *input, unsigned char *output);
int HuffDecompress(int *source, unsigned char *output);
int RawDecompress(int *source, unsigned char *output);
int bIsCompressed(unsigned int *source);
int bDecompress(unsigned int *source, unsigned char *output);

#endif
