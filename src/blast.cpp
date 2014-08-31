/* blast.c
 * Copyright (C) 2003 Mark Adler
 * For conditions of distribution and use, see copyright notice in blast.h
 * version 1.1, 16 Feb 2003
 *
 * blast.c decompresses data compressed by the PKWare Compression Library.
 * This function provides functionality similar to the explode() function of
 * the PKWare library, hence the name "blast".
 *
 * This decompressor is based on the excellent format description provided by
 * Ben Rudiak-Gould in comp.compression on August 13, 2001.  Interestingly, the
 * example Ben provided in the post is incorrect.  The distance 110001 should
 * instead be 111000.  When corrected, the example byte stream becomes:
 *
 *    00 04 82 24 25 8f 80 7f
 *
 * which decompresses to "AIAIAIAIAIAIA" (without the quotes).
 */

/*
 * Change history:
 *
 * 1.0  12 Feb 2003     - First version
 * 1.1  16 Feb 2003     - Fixed distance check for > 4 GB uncompressed data
 */

#include "blast.h"              /* prototype for blast() */

/* input and output state */
struct state {
    /* input state */
    blast_in infun;             /* input function provided by user */
    void *inhow;                /* opaque information passed to infun() */
    unsigned char *in;          /* next input location */
    unsigned left;              /* available input at in */
    int total;                  /* how much to read */
    int bitbuf;                 /* bit buffer */
    int bitcnt;                 /* number of bits in bit buffer */

    /* input limit error return state for bits() and decode() */
    jmp_buf env;

    /* output state */
    blast_out outfun;           /* output function provided by user */
    void *outhow;               /* opaque information passed to outfun() */
    unsigned next;              /* index of next write location in out[] */
    int first;                  /* true to check distances (for first 4K) */
    unsigned char out[MAXWIN];  /* output buffer and sliding window */
};

/*
 * Return need bits from the input stream.  This always leaves less than
 * eight bits in the buffer.  bits() works properly for need == 0.
 *
 * Format notes:
 *
 * - Bits are stored in bytes from the least significant bit to the most
 *   significant bit.  Therefore bits are dropped from the bottom of the bit
 *   buffer, using shift right, and new bytes are appended to the top of the
 *   bit buffer, using shift left.
 */
int Blast::bits(int need)
{
    int val;            /* bit accumulator */

    /* load at least need bits into val */
    val = m_bitbuf;
    while (m_bitcnt < need) {
        if (m_left == 0) {
            m_left = m_infun(m_inhow, &(m_in), m_total);
            if (m_left == 0) longjmp(m_env, 1);       /* out of input */
        }
        val |= (int)(*(m_in)++) << m_bitcnt;          /* load eight bits */
        m_left--;
        m_bitcnt += 8;
    }

    /* drop need bits and update buffer, always zero to seven bits left */
    m_bitbuf = val >> need;
    m_bitcnt -= need;

    /* return need bits, zeroing the bits above that */
    return val & ((1 << need) - 1);
}




/*
 * Decode a code from the stream s using huffman table h.  Return the symbol or
 * a negative value if there is an error.  If all of the lengths are zero, i.e.
 * an empty code, or if the code is incomplete and an invalid code is received,
 * then -9 is returned after reading MAXBITS bits.
 *
 * Format notes:
 *
 * - The codes as stored in the compressed data are bit-reversed relative to
 *   a simple integer ordering of codes of the same lengths.  Hence below the
 *   bits are pulled from the compressed data one at a time and used to
 *   build the code value reversed from what is in the stream in order to
 *   permit simple integer comparisons for decoding.
 *
 * - The first code for the shortest length is all ones.  Subsequent codes of
 *   the same length are simply integer decrements of the previous code.  When
 *   moving up a length, a one bit is appended to the code.  For a complete
 *   code, the last code of the longest length will be all zeros.  To support
 *   this ordering, the bits pulled during decoding are inverted to apply the
 *   more "natural" ordering starting with all zeros and incrementing.
 */
int Blast::decode(huffman& huff)
{
    int len;            /* current number of bits in code */
    int code;           /* len bits being decoded */
    int first;          /* first code of length len */
    int count;          /* number of codes of length len */
    int index;          /* index of first code of length len in symbol table */
    int bitbuf;         /* bits from stream */
    int left;           /* bits left in next or left to process */
    short *next;        /* next number of codes */

    bitbuf = m_bitbuf;
    left = m_bitcnt;
    code = first = index = 0;
    len = 1;
    next = huff.count + 1;
    while (1) {
        while (left--) {
            code |= (bitbuf & 1) ^ 1;   /* invert code */
            bitbuf >>= 1;
            count = *next++;
            if (code < first + count) { /* if length len, return symbol */
                m_bitbuf = bitbuf;
                m_bitcnt = (m_bitcnt - len) & 7;
                return huff.symbol[index + (code - first)];
            }
            index += count;             /* else update for next length */
            first += count;
            first <<= 1;
            code <<= 1;
            len++;
        }
        left = (MAXBITS+1) - len;
        if (left == 0) break;
        if (m_left == 0) {
            m_left = m_infun(m_inhow, &(m_in), m_total);
            if (m_left == 0) longjmp(m_env, 1);       /* out of input */
        }
        bitbuf = *(m_in)++;
        m_left--;
        if (left > 8) left = 8;
    }
    return -9;                          /* ran out of codes */
}

/*
 * Given a list of repeated code lengths rep[0..n-1], where each byte is a
 * count (high four bits + 1) and a code length (low four bits), generate the
 * list of code lengths.  This compaction reduces the size of the object code.
 * Then given the list of code lengths length[0..n-1] representing a canonical
 * Huffman code for n symbols, construct the tables required to decode those
 * codes.  Those tables are the number of codes of each length, and the symbols
 * sorted by length, retaining their original order within each length.  The
 * return value is zero for a complete code set, negative for an over-
 * subscribed code set, and positive for an incomplete code set.  The tables
 * can be used if the return value is zero or positive, but they cannot be used
 * if the return value is negative.  If the return value is zero, it is not
 * possible for decode() using that table to return an error--any stream of
 * enough bits will resolve to a symbol.  If the return value is positive, then
 * it is possible for decode() using that table to return an error for received
 * codes past the end of the incomplete lengths.
 */
int Blast::construct(huffman& huff, const unsigned char *rep, int n)
{
    int symbol;         /* current symbol when stepping through length[] */
    int len;            /* current length when stepping through huff.count[] */
    int left;           /* number of possible codes left of current length */
    short offs[MAXBITS+1];      /* offsets in symbol table for each length */
    short length[256];  /* code lengths */

    /* convert compact repeat counts into symbol bit length list */
    symbol = 0;
    do {
        len = *rep++;
        left = (len >> 4) + 1;
        len &= 15;
        do {
            length[symbol++] = len;
        } while (--left);
    } while (--n);
    n = symbol;

    /* count number of codes of each length */
    for (len = 0; len <= MAXBITS; len++)
        huff.count[len] = 0;
    for (symbol = 0; symbol < n; symbol++)
        (huff.count[length[symbol]])++;   /* assumes lengths are within bounds */
    if (huff.count[0] == n)               /* no codes! */
        return 0;                       /* complete, but decode() will fail */

    /* check for an over-subscribed or incomplete set of lengths */
    left = 1;                           /* one possible code of zero length */
    for (len = 1; len <= MAXBITS; len++) {
        left <<= 1;                     /* one more bit, double codes left */
        left -= huff.count[len];          /* deduct count from possible codes */
        if (left < 0) return left;      /* over-subscribed--return negative */
    }                                   /* left > 0 means incomplete */

    /* generate offsets into symbol table for each length for sorting */
    offs[1] = 0;
    for (len = 1; len < MAXBITS; len++)
        offs[len + 1] = offs[len] + huff.count[len];

    /*
     * put symbols in table sorted by length, by symbol order within each
     * length
     */
    for (symbol = 0; symbol < n; symbol++)
        if (length[symbol] != 0)
            huff.symbol[offs[length[symbol]]++] = symbol;

    /* return zero for complete set, positive for incomplete set */
    return left;
}

/*
 * Decode PKWare Compression Library stream.
 *
 * Format notes:
 *
 * - First byte is 0 if literals are uncoded or 1 if they are coded.  Second
 *   byte is 4, 5, or 6 for the number of extra bits in the distance code.
 *   This is the base-2 logarithm of the dictionary size minus six.
 *
 * - Compressed data is a combination of literals and length/distance pairs
 *   terminated by an end code.  Literals are either Huffman coded or
 *   uncoded bytes.  A length/distance pair is a coded length followed by a
 *   coded distance to represent a string that occurs earlier in the
 *   uncompressed data that occurs again at the current location.
 *
 * - A bit preceding a literal or length/distance pair indicates which comes
 *   next, 0 for literals, 1 for length/distance.
 *
 * - If literals are uncoded, then the next eight bits are the literal, in the
 *   normal bit order in th stream, i.e. no bit-reversal is needed. Similarly,
 *   no bit reversal is needed for either the length extra bits or the distance
 *   extra bits.
 *
 * - Literal bytes are simply written to the output.  A length/distance pair is
 *   an instruction to copy previously uncompressed bytes to the output.  The
 *   copy is from distance bytes back in the output stream, copying for length
 *   bytes.
 *
 * - Distances pointing before the beginning of the output data are not
 *   permitted.
 *
 * - Overlapped copies, where the length is greater than the distance, are
 *   allowed and common.  For example, a distance of one and a length of 518
 *   simply copies the last byte 518 times.  A distance of four and a length of
 *   twelve copies the last four bytes three times.  A simple forward copy
 *   ignoring whether the length is greater than the distance or not implements
 *   this correctly.
 */
int Blast::decomp()
{
    int lit;            /* true if literals are coded */
    int dict;           /* log2(dictionary size) - 6 */
    int symbol;         /* decoded symbol, extra bits for distance */
    int len;            /* length for copy */
    int dist;           /* distance for copy */
    int copy;           /* copy counter */
    unsigned char *from, *to;   /* copy pointers */
    static int virgin = 1;                              /* build tables once */
    static short litcnt[MAXBITS+1], litsym[256];        /* litcode memory */
    static short lencnt[MAXBITS+1], lensym[16];         /* lencode memory */
    static short distcnt[MAXBITS+1], distsym[64];       /* distcode memory */
    huffman litcode = {litcnt, litsym};   /* length code */
    huffman lencode = {lencnt, lensym};   /* length code */
    huffman distcode = {distcnt, distsym};/* distance code */
        /* bit lengths of literal codes */
    static const unsigned char litlen[] = {
        11, 124, 8, 7, 28, 7, 188, 13, 76, 4, 10, 8, 12, 10, 12, 10, 8, 23, 8,
        9, 7, 6, 7, 8, 7, 6, 55, 8, 23, 24, 12, 11, 7, 9, 11, 12, 6, 7, 22, 5,
        7, 24, 6, 11, 9, 6, 7, 22, 7, 11, 38, 7, 9, 8, 25, 11, 8, 11, 9, 12,
        8, 12, 5, 38, 5, 38, 5, 11, 7, 5, 6, 21, 6, 10, 53, 8, 7, 24, 10, 27,
        44, 253, 253, 253, 252, 252, 252, 13, 12, 45, 12, 45, 12, 61, 12, 45,
        44, 173};
        /* bit lengths of length codes 0..15 */
    static const unsigned char lenlen[] = {2, 35, 36, 53, 38, 23};
        /* bit lengths of distance codes 0..63 */
    static const unsigned char distlen[] = {2, 20, 53, 230, 247, 151, 248};
    static const short base[16] = {     /* base for length codes */
        3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264};
    static const char extra[16] = {     /* extra bits for length codes */
        0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8};

    /* set up decoding tables (once--might not be thread-safe) */
    if (virgin) {
        construct(&litcode, litlen, sizeof(litlen));
        construct(&lencode, lenlen, sizeof(lenlen));
        construct(&distcode, distlen, sizeof(distlen));
        virgin = 0;
    }

    /* read header */
    lit = bits(s, 8);
    if (lit > 1) return -1;
    dict = bits(s, 8);
    if (dict < 4 || dict > 6) return -2;

    /* decode literals and length/distance pairs */
    do {
        if (bits(s, 1)) {
            /* get length */
            symbol = decode(s, &lencode);
            len = base[symbol] + bits(s, extra[symbol]);
            if (len == 519) break;              /* end code */

            /* get distance */
            symbol = len == 2 ? 2 : dict;
            dist = decode(s, &distcode) << symbol;
            dist += bits(s, symbol);
            dist++;
            if (m_first && dist > m_next)
                return -3;              /* distance too far back */

            /* copy length bytes from distance bytes back */
            do {
                to = m_out + m_next;
                from = to - dist;
                copy = MAXWIN;
                if (m_next < dist) {
                    from += copy;
                    copy = dist;
                }
                copy -= m_next;
                if (copy > len) copy = len;
                len -= copy;
                m_next += copy;
                do {
                    *to++ = *from++;
                } while (--copy);
                if (m_next == MAXWIN) {
                    if (m_outfun(m_outhow, m_out, m_next)) return 1;
                    m_next = 0;
                    m_first = 0;
                }
            } while (len != 0);
        }
        else {
            /* get literal and write it */
            symbol = lit ? decode(s, &litcode) : bits(s, 8);
            m_out[m_next++] = symbol;
            if (m_next == MAXWIN) {
                if (m_outfun(m_outhow, m_out, m_next)) return 1;
                m_next = 0;
                m_first = 0;
            }
        }
    } while (1);
    return 0;
}

/* See comments in blast.h */
Blast::Blast()
{
    init();
}

void Blast::init(){
    /* initialize input state */
    //m_infun = infun;
    //m_inhow = inhow;
    m_left = 0;
    //m_total = len;
    m_bitbuf = 0;
    m_bitcnt = 0;

    /* initialize output state */
    //m_outfun = outfun;
    //m_outhow = outhow;
    m_next = 0;
    m_first = 1;

    /* return if bits() or decode() tries to read past available input */
    if (setjmp(m_env) != 0)             /* if came back here via longjmp(), */
        m_err = 2;                        /*  then skip decomp(), return error */
    else
        m_err = decomp();               /* decompress */

    /* write any leftover output and update the error code if needed */
    /*if (err != 1 && m_next && m_outfun(m_outhow, m_out, m_next) && err == 0)
        err = 1;
    return err;*/
}

#ifdef TEST
/* Example of how to use blast() */
#include <stdio.h>
#include <stdlib.h>

#define CHUNK 16384

local unsigned inf(void *how, unsigned char **buf)
{
    static unsigned char hold[CHUNK];

    *buf = hold;
    return fread(hold, 1, CHUNK, (FILE *)how);
}

local int outf(void *how, unsigned char *buf, unsigned len)
{
    return fwrite(buf, 1, len, (FILE *)how) != len;
}

/* Decompress a PKWare Compression Library stream from stdin to stdout */
int main(void)
{
    int ret, n;

    /* decompress to stdout */
    ret = blast(inf, stdin, outf, stdout);
    if (ret != 0) fprintf(stderr, "blast error: %d\n", ret);

    /* see if there are any leftover bytes */
    n = 0;
    while (getchar() != EOF) n++;
    if (n) fprintf(stderr, "blast warning: %d unused bytes of input\n", n);

    /* return blast() error code */
    return ret;
}
#endif