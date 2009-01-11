/// \file musepack_internal.h
/// Definitions and structures used only internally by the libmusepack.

#ifndef _musepack_internal_h
#define _musepack_internal_h

enum {
    MPC_DECODER_SYNTH_DELAY = 481
};

#ifndef swap32
/// Big/little endian 32 bit byte swapping routine.
static inline
mpc_uint32_t swap32(mpc_uint32_t val) {
    const unsigned char* src = (const unsigned char*)&val;
    return 
        (mpc_uint32_t)src[0] | 
        ((mpc_uint32_t)src[1] << 8) | ((mpc_uint32_t)src[2] << 16) | ((mpc_uint32_t)src[3] << 24);
}
#endif

/// Searches for a ID3v2-tag and reads the length (in bytes) of it.
/// \param reader supplying raw stream data
/// \return size of tag, in bytes
/// \return -1 on errors of any kind
mpc_int32_t JumpID3v2(mpc_reader* fp);

/// helper functions used by multiple files
mpc_uint32_t random_int(mpc_decoder *d); // in synth_filter.c
void mpc_decoder_initialisiere_quantisierungstabellen(mpc_decoder *d, double scale_factor);
void mpc_decoder_synthese_filter_float(mpc_decoder *d, MPC_SAMPLE_FORMAT* OutData);

#endif // _musepack_internal_h

