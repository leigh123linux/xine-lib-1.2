#ifndef __CONFIG_TYPES_H__
#define __CONFIG_TYPES_H__

#include "os_types.h"

#ifdef WORDS_BIGENDIAN
  #undef MPC_LITTLE_ENDIAN
#else
  #define MPC_LITTLE_ENDIAN
#endif

typedef unsigned char mpc_bool_t;
#define TRUE  1
#define FALSE 0

/* these are filled in by configure */
typedef int16_t mpc_int16_t;
typedef uint16_t mpc_uint16_t;
typedef int32_t mpc_int32_t;
typedef uint32_t mpc_uint32_t;
typedef int64_t mpc_int64_t;

#endif
