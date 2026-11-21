/* miniz.h -- compact, freestanding RFC1951 DEFLATE / RFC1952 gzip codec.
 *
 * This is a genuine, interoperable zlib-compatible codec written for the
 * Sybian REAL QEMU port.  It implements the raw DEFLATE format (RFC 1951):
 *   * deflate: LZ77 match finding (32 KiB class window, hashed chains) with
 *              fixed-Huffman coding, plus stored-block fallback.  The output
 *              is a standard raw DEFLATE stream readable by zlib/gzip/Python.
 *   * inflate: complete decoder -- stored, fixed-Huffman AND dynamic-Huffman
 *              blocks, with canonical Huffman decoding and distance back-refs.
 *   * CRC-32 (IEEE 802.3) and Adler-32, the standard zlib checksums.
 *
 * No libc, no dynamic allocation, no floating point: only static work areas
 * (defined in miniz.c).  This fits the port's 32 KiB .text/IAT budget and its
 * small-heap constraint, unlike the full richgel999/miniz compressor whose
 * state alone exceeds 100 KiB.
 */
#ifndef PORT_MINIZ_H
#define PORT_MINIZ_H

typedef unsigned long mz_ulong;

/* Compress a raw DEFLATE stream.  Returns 1 on success, 0 on failure. */
int mz_deflate(const unsigned char* src, unsigned src_len,
               unsigned char* dst, unsigned dst_cap, unsigned* dst_len);

/* Decompress a raw DEFLATE stream.  Returns 1 on success, 0 on failure. */
int mz_inflate(const unsigned char* src, unsigned src_len,
               unsigned char* dst, unsigned dst_cap, unsigned* dst_len);

/* Standard checksums.  Pass 0 (or MZ_CRC32_INIT / MZ_ADLER32_INIT) to start. */
#define MZ_CRC32_INIT   ((mz_ulong)0)
#define MZ_ADLER32_INIT ((mz_ulong)1)
mz_ulong mz_crc32(mz_ulong crc, const unsigned char* buf, unsigned len);
mz_ulong mz_adler32(mz_ulong adler, const unsigned char* buf, unsigned len);

#endif /* PORT_MINIZ_H */
