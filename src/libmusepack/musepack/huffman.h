/// \file huffman.h
/// Data structures and functions for huffman coding.

#ifndef _musepack_huffman_h_
#define _musepack_huffman_h_

#include "musepack/config_types.h"
#include "musepack/decoder.h"

struct mpc_decoder_t; // forward declare to break circular dependencies

/// Huffman table entry.
typedef struct huffman_type_t {
    mpc_uint32_t  Code;
    mpc_uint32_t  Length;
    mpc_int32_t   Value;
} HuffmanTyp;

//! \brief Sorts huffman-tables by codeword.
//!
//! offset resulting value.
//! \param elements
//! \param Table table to sort
//! \param offset offset of resulting sort
void
mpc_decoder_resort_huff_tables(
    const mpc_uint32_t elements, HuffmanTyp *Table, const mpc_int32_t offset);

/// Initializes sv6 huffman decoding structures.
void mpc_decoder_init_huffman_sv6(struct mpc_decoder_t *d);

/// Initializes sv6 huffman decoding tables.
void mpc_decoder_init_huffman_sv6_tables(struct mpc_decoder_t *d);

/// Initializes sv7 huffman decoding structures.
void mpc_decoder_init_huffman_sv7(struct mpc_decoder_t *d);

/// Initializes sv7 huffman decoding tables.
void mpc_decoder_init_huffman_sv7_tables(struct mpc_decoder_t *d);

#endif // _musepack_huffman_h_
