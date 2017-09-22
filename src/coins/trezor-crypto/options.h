/**
 * Copyright (c) 2013-2014 Pavol Rusnak
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __OPTIONS_H__
#define __OPTIONS_H__

// use precomputed Curve Points (some scalar multiples of curve base point G)
#define USE_PRECOMPUTED_CP 0

// use fast inverse method
#define USE_INVERSE_FAST 1

// support for printing bignum256 structures via printf
#define USE_BN_PRINT 0

// use deterministic signatures
#define USE_RFC6979 1

// implement BIP32 caching
#define USE_BIP32_CACHE 1
#define BIP32_CACHE_SIZE 3
#define BIP32_CACHE_MAXDEPTH 8

// support Ethereum operations
#define USE_ETHEREUM 0

// support Graphene operations (STEEM, BitShares)
#define USE_GRAPHENE 0

// support Keccak hashing
#define USE_KECCAK 1

#define MAX_ADDR_RAW_SIZE (4 + 40)
#define MAX_WIF_RAW_SIZE (4 + 32 + 1)
#define MAX_ADDR_SIZE (54)
#define MAX_WIF_SIZE (58)

#define MAX_BASE58_ENC 90
#define MAX_BASE58_DEC 120

#endif
