#ifndef _XXMC_H
#define _XXMC_H

#include "accel_xvmc.h"

extern void mpeg2_xxmc_slice( mpeg2dec_t *mpeg2dec, picture_t *picture, 
			      int code, uint8_t *buffer); 
extern void mpeg2_xxmc_choose_coding(int decoder_format, picture_t *picture); 

extern void mpeg2_xxmc_vld_frame_complete(mpeg2dec_t *mpeg2dec, picture_t *picture, int code);

#endif
