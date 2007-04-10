/// \file requant.h
/// Requantization function definitions.

#ifndef _musepack_requant_h
#define _musepack_requant_h_

#include "musepack/musepack.h"

/* C O N S T A N T S */
extern const mpc_uint32_t Res_bit [18];         // bits per sample for chosen quantizer
extern const MPC_SAMPLE_FORMAT __Cc    [1 + 18];     // coefficients for requantization
extern const mpc_int32_t          __Dc    [1 + 18];     // offset for requantization

#define Cc      (__Cc + 1)
#define Dc      (__Dc + 1)

#endif // _musepack_requant_h_
