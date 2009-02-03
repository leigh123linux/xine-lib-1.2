/*
 * Copyright (C) 2008 Christophe Thommeret <hftom@free.fr>
 *
 * This file is part of xine, a free video player.
 *
 * xine is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * xine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * vdpau_vc1.c, a vc1 video stream parser using VDPAU hardware decoder
 *
 */

//#define LOG
#define LOG_MODULE "vdpau_vc1"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "xine_internal.h"
#include "video_out.h"
#include "buffer.h"
#include "xineutils.h"
#include "accel_vdpau.h"

#include <vdpau/vdpau.h>

#define sequence_header_code    0x0f
#define sequence_end_code       0x0a
#define entry_point_code        0x0e
#define frame_start_code        0x0d
#define field_start_code        0x0c
#define slice_start_code        0x0b

#define PICTURE_FRAME            0
#define PICTURE_FRAME_INTERLACE  2
#define PICTURE_FIELD_INTERLACE  3

#define I_FRAME 1
#define P_FRAME 2
#define B_FRAME 3

#define WANT_HEADER 1
#define WANT_EXT    2
#define WANT_SLICE  3



typedef struct {
  VdpPictureInfoVC1       vdp_infos;
  int                     hrd_param_flag;
  int                     hrd_num_leaky_buckets;
  int                     type;
} picture_t;



typedef struct {
  uint32_t    coded_width;
  uint32_t    coded_height;

  uint64_t    video_step; /* frame duration in pts units */
  double      ratio;
  VdpDecoderProfile profile;

  int         have_header;

  uint8_t     *buf; /* accumulate data */
  uint32_t    bufsize;
  uint32_t    bufpos;

  picture_t   picture;
  vo_frame_t  *forward_ref;
  vo_frame_t  *backward_ref;

  int64_t    seq_pts;
  int64_t    cur_pts;

  vdpau_accel_t *accel_vdpau;

  int         vdp_runtime_nr;

} sequence_t;



typedef struct {
  video_decoder_class_t   decoder_class;
} vdpau_vc1_class_t;



typedef struct vdpau_vc1_decoder_s {
  video_decoder_t         video_decoder;  /* parent video decoder structure */

  vdpau_vc1_class_t    *class;
  xine_stream_t           *stream;

  sequence_t              sequence;

  VdpDecoder              decoder;
  VdpDecoderProfile       decoder_profile;
  uint32_t                decoder_width;
  uint32_t                decoder_height;

} vdpau_vc1_decoder_t;



static void reset_picture( picture_t *pic )
{
  lprintf( "reset_picture\n" );
  /*pic->vdp_infos.picture_structure = 0;
  pic->vdp_infos2.intra_dc_precision = pic->vdp_infos.intra_dc_precision = 0;
  pic->vdp_infos2.frame_pred_frame_dct = pic->vdp_infos.frame_pred_frame_dct = 1;
  pic->vdp_infos2.concealment_motion_vectors = pic->vdp_infos.concealment_motion_vectors = 0;
  pic->vdp_infos2.intra_vlc_format = pic->vdp_infos.intra_vlc_format = 0;
  pic->vdp_infos2.alternate_scan = pic->vdp_infos.alternate_scan = 0;
  pic->vdp_infos2.q_scale_type = pic->vdp_infos.q_scale_type = 0;
  pic->vdp_infos2.top_field_first = pic->vdp_infos.top_field_first = 0;
  pic->slices_count = 0;
  pic->slices_count2 = 0;
  pic->slices_pos = 0;
  pic->slices_pos_top = 0;
  pic->state = WANT_HEADER;*/
}



static void init_picture( picture_t *pic )
{
  reset_picture( pic );
}



static void reset_sequence( sequence_t *sequence )
{
  lprintf( "reset_sequence\n" );
  sequence->bufpos = 0;
  sequence->seq_pts = sequence->cur_pts = 0;
  if ( sequence->forward_ref )
    sequence->forward_ref->free( sequence->forward_ref );
  sequence->forward_ref = NULL;
  if ( sequence->backward_ref )
    sequence->backward_ref->free( sequence->backward_ref );
  sequence->backward_ref = NULL;
}



static void init_sequence( sequence_t *sequence )
{
  lprintf( "init_sequence\n" );
  sequence->have_header = 0;
  sequence->profile = VDP_DECODER_PROFILE_VC1_SIMPLE;
  sequence->ratio = 0;
  sequence->video_step = 0;
  sequence->picture.hrd_param_flag = 0;
  reset_sequence( sequence );
}



static uint32_t get_bits( uint8_t *b, int offbits, int nbits )
{
  int i, nbytes;
  uint32_t ret = 0;
  uint8_t *buf;

  buf = b+(offbits/8);
  offbits %=8;
  nbytes = (offbits+nbits)/8;
  if ( ((offbits+nbits)%8)>0 )
    nbytes++;
  for ( i=0; i<nbytes; i++ )
    ret += buf[i]<<((nbytes-i-1)*8);
  i = (4-nbytes)*8+offbits;
  ret = ((ret<<i)>>i)>>((nbytes*8)-nbits-offbits);

  return ret;
}



static void update_metadata( vdpau_vc1_decoder_t *this_gen )
{
  sequence_t *sequence = (sequence_t*)&this_gen->sequence;

  if ( !sequence->have_header ) {
    sequence->have_header = 1;
    _x_stream_info_set( this_gen->stream, XINE_STREAM_INFO_VIDEO_WIDTH, sequence->coded_width );
    _x_stream_info_set( this_gen->stream, XINE_STREAM_INFO_VIDEO_HEIGHT, sequence->coded_height );
    _x_stream_info_set( this_gen->stream, XINE_STREAM_INFO_VIDEO_RATIO, ((double)10000*sequence->ratio) );
    _x_stream_info_set( this_gen->stream, XINE_STREAM_INFO_FRAME_DURATION, sequence->video_step );
    _x_meta_info_set_utf8( this_gen->stream, XINE_META_INFO_VIDEOCODEC, "VC1/WMV9 (vdpau)" );
    xine_event_t event;
    xine_format_change_data_t data;
    event.type = XINE_EVENT_FRAME_FORMAT_CHANGE;
    event.stream = this_gen->stream;
    event.data = &data;
    event.data_length = sizeof(data);
    data.width = sequence->coded_width;
    data.height = sequence->coded_height;
    data.aspect = sequence->ratio;
    xine_event_send( this_gen->stream, &event );
  }
}



static void sequence_header_advanced( vdpau_vc1_decoder_t *this_gen, uint8_t *buf, int len )
{
  lprintf( "sequence_header_advanced\n" );
  sequence_t *sequence = (sequence_t*)&this_gen->sequence;

  if ( len < 5 )
    return;

  sequence->profile = VDP_DECODER_PROFILE_VC1_ADVANCED;
  lprintf("VDP_DECODER_PROFILE_VC1_ADVANCED\n");
  int off = 15;
  sequence->picture.vdp_infos.postprocflag = get_bits(buf,off++,1);
  sequence->coded_width = (get_bits(buf,off,12)+1)<<1;
  off += 12;
  sequence->coded_height = (get_bits(buf,off,12)+1)<<1;
  off += 12;
  ++off;
  sequence->picture.vdp_infos.interlace = get_bits(buf,off++,1);
  sequence->picture.vdp_infos.tfcntrflag = get_bits(buf,off++,1);
  sequence->picture.vdp_infos.finterpflag = get_bits(buf,off++,1);
  ++off;
  sequence->picture.vdp_infos.psf = get_bits(buf,off++,1);
  sequence->picture.vdp_infos.maxbframes = 7;
  if ( get_bits(buf,off++,1) ) {
    int w, h, ar=0;
    w = get_bits(buf,off,14)+1;
    off += 14;
    h = get_bits(buf,off,14)+1;
    off += 14;
    if ( get_bits(buf,off++,1) ) {
      ar = get_bits(buf,off,4);
      off += 4;
    }
    if ( ar==15 ) {
      w = get_bits(buf,off,8);
      off += 8;
      h = get_bits(buf,off,8);
      off += 8;
    }
    if ( get_bits(buf,off++,1) ) {
      if ( get_bits(buf,off++,1) )
        off += 16;
      else
        off += 12;
    }
    if ( get_bits(buf,off++,1) )
      off += 24;
  }
  sequence->picture.hrd_param_flag = get_bits(buf,off++,1);
  if ( sequence->picture.hrd_param_flag )
    sequence->picture.hrd_num_leaky_buckets = get_bits(buf,off,5);

  update_metadata( this_gen );
}



static void sequence_header( vdpau_vc1_decoder_t *this_gen, uint8_t *buf, int len )
{
  lprintf( "sequence_header\n" );
  sequence_t *sequence = (sequence_t*)&this_gen->sequence;

  if ( len < 4 )
    return;

  switch ( get_bits(buf,0,2) ) {
    case 0: sequence->profile = VDP_DECODER_PROFILE_VC1_SIMPLE; lprintf("VDP_DECODER_PROFILE_VC1_SIMPLE\n"); break;
    case 1: sequence->profile = VDP_DECODER_PROFILE_VC1_MAIN; lprintf("VDP_DECODER_PROFILE_VC1_MAIN\n"); break;
    case 3: return sequence_header_advanced( this_gen, buf, len ); break;
    default: return; /* illegal value, broken header? */
  }

  sequence->picture.vdp_infos.loopfilter = get_bits(buf,12,1);
  sequence->picture.vdp_infos.multires = get_bits(buf,14,1);
  sequence->picture.vdp_infos.fastuvmc = get_bits(buf,16,1);
  sequence->picture.vdp_infos.extended_mv = get_bits(buf,17,1);
  sequence->picture.vdp_infos.dquant = get_bits(buf,18,2);
  sequence->picture.vdp_infos.vstransform = get_bits(buf,20,1);
  sequence->picture.vdp_infos.overlap = get_bits(buf,22,1);
  sequence->picture.vdp_infos.syncmarker = get_bits(buf,23,1);
  sequence->picture.vdp_infos.rangered = get_bits(buf,24,1);
  sequence->picture.vdp_infos.maxbframes = get_bits(buf,25,3);
  sequence->picture.vdp_infos.quantizer = get_bits(buf,28,2);
  sequence->picture.vdp_infos.finterpflag = get_bits(buf,30,1);

  update_metadata( this_gen );
}



static void entry_point( vdpau_vc1_decoder_t *this_gen, uint8_t *buf, int len )
{
  lprintf( "entry_point\n" );
  sequence_t *sequence = (sequence_t*)&this_gen->sequence;
  int off=2;

  sequence->picture.vdp_infos.panscan_flag = get_bits(buf,off++,1);
  sequence->picture.vdp_infos.refdist_flag = get_bits(buf,off++,1);
  sequence->picture.vdp_infos.loopfilter = get_bits(buf,off++,1);
  sequence->picture.vdp_infos.fastuvmc = get_bits(buf,off++,1);
  sequence->picture.vdp_infos.extended_mv = get_bits(buf,off++,1);
  sequence->picture.vdp_infos.dquant = get_bits(buf,off,2);
  off += 2;
  sequence->picture.vdp_infos.vstransform = get_bits(buf,off++,1);
  sequence->picture.vdp_infos.overlap = get_bits(buf,off++,1);
  sequence->picture.vdp_infos.quantizer = get_bits(buf,off,2);
  off += 2;

  if ( sequence->picture.hrd_param_flag ) {
    int i;
    for ( i=0; i<sequence->picture.hrd_num_leaky_buckets; ++i )
      off += 8;
  }

  if ( get_bits(buf,off++,1) ) {
    sequence->coded_width = (get_bits(buf,off,12)+1)<<1;
    off += 12;
    sequence->coded_height = (get_bits(buf,off,12)+1)<<1;
    off += 12;
  }

  if ( sequence->picture.vdp_infos.extended_mv )
    sequence->picture.vdp_infos.extended_dmv = get_bits(buf,off++,1);

  sequence->picture.vdp_infos.range_mapy_flag = get_bits(buf,off++,1);
  if ( sequence->picture.vdp_infos.range_mapy_flag ) {
    sequence->picture.vdp_infos.range_mapy = get_bits(buf,off,3);
    off += 3;
  }
  sequence->picture.vdp_infos.range_mapuv_flag = get_bits(buf,off++,1);
  if ( sequence->picture.vdp_infos.range_mapuv_flag ) {
    sequence->picture.vdp_infos.range_mapuv = get_bits(buf,off,3);
    off += 3;
  }
}



static void picture_header( vdpau_vc1_decoder_t *this_gen, uint8_t *buf, int len )
{
  sequence_t *sequence = (sequence_t*)&this_gen->sequence;
  picture_t *pic = (picture_t*)&sequence->picture;
  VdpPictureInfoVC1 *info = &(sequence->picture.vdp_infos);

  int off=2;

  if ( info->finterpflag )
    ++off;
  if ( info->rangered )
    ++off;
  if ( !info->maxbframes ) {
    info->picture_type = get_bits( buf,off,1 );
    if ( info->picture_type )
      pic->type = P_FRAME;
    else
      pic->type = I_FRAME;
  }
  else {
    int ptype = get_bits( buf,off,1 );
    if ( ptype ) {
      info->picture_type = ptype;
      pic->type = P_FRAME;
      ++off;
    }
    else {
      info->picture_type = get_bits( buf,off,2 );
      if ( info->picture_type )
        pic->type = I_FRAME;
      else
        pic->type = B_FRAME;
      off += 2;
    }
  }
}



static void parse_header( vdpau_vc1_decoder_t *this_gen, uint8_t *buf, int len )
{
  sequence_t *sequence = (sequence_t*)&this_gen->sequence;
  int off=0;

  while ( off < (len-4) ) {
    uint8_t *buffer = buf+off;
    if ( buffer[0]==0 && buffer[1]==0 && buffer[2]==1 ) {
      switch ( buffer[3] ) {
        case sequence_header_code: sequence_header( this_gen, buf+off+4, len-off-4 ); break;
        case entry_point_code: entry_point( this_gen, buf+off+4, len-off-4 ); break;
      }
    }
    ++off;
  }
  if ( !sequence->have_header )
    sequence_header( this_gen, buf, len );
}



static void decode_render( vdpau_vc1_decoder_t *vd, vdpau_accel_t *accel )
{
  sequence_t *seq = (sequence_t*)&vd->sequence;
  picture_t *pic = (picture_t*)&seq->picture;

  VdpStatus st;
  if ( vd->decoder==VDP_INVALID_HANDLE || vd->decoder_profile!=seq->profile || vd->decoder_width!=seq->coded_width || vd->decoder_height!=seq->coded_height ) {
    if ( vd->decoder!=VDP_INVALID_HANDLE ) {
      accel->vdp_decoder_destroy( vd->decoder );
      vd->decoder = VDP_INVALID_HANDLE;
    }
    st = accel->vdp_decoder_create( accel->vdp_device, seq->profile, seq->coded_width, seq->coded_height, 2, &vd->decoder);
    if ( st!=VDP_STATUS_OK )
      lprintf( "failed to create decoder !! %s\n", accel->vdp_get_error_string( st ) );
    else {
      lprintf( "decoder created.\n" );
      vd->decoder_profile = seq->profile;
      vd->decoder_width = seq->coded_width;
      vd->decoder_height = seq->coded_height;
      seq->vdp_runtime_nr = accel->vdp_runtime_nr;
    }
  }

  VdpBitstreamBuffer vbit;
  vbit.struct_version = VDP_BITSTREAM_BUFFER_VERSION;
  vbit.bitstream = seq->buf;
  vbit.bitstream_bytes = seq->bufpos;
  st = accel->vdp_decoder_render( vd->decoder, accel->surface, (VdpPictureInfo*)&pic->vdp_infos, 1, &vbit );
  if ( st!=VDP_STATUS_OK )
    lprintf( "decoder failed : %d!! %s\n", st, accel->vdp_get_error_string( st ) );
  else {
    lprintf( "DECODER SUCCESS : slices=%d, slices_bytes=%d, current=%d, forwref:%d, backref:%d, pts:%lld\n",
              pic->vdp_infos.slice_count, vbit.bitstream_bytes, accel->surface, pic->vdp_infos.forward_reference, pic->vdp_infos.backward_reference, seq->seq_pts );
  }
}



static void decode_picture( vdpau_vc1_decoder_t *vd )
{
  sequence_t *seq = (sequence_t*)&vd->sequence;
  picture_t *pic = (picture_t*)&seq->picture;
  vdpau_accel_t *ref_accel;

  picture_header( vd, seq->buf, seq->bufpos );

  VdpPictureInfoVC1 *info = &(seq->picture.vdp_infos);
  printf("%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n\n", info->slice_count, info->picture_type, info->frame_coding_mode, info->postprocflag, info->pulldown, info->interlace, info->tfcntrflag, info->finterpflag, info->psf, info->dquant, info->panscan_flag, info->refdist_flag, info->quantizer, info->extended_mv, info->extended_dmv, info->overlap, info->vstransform, info->loopfilter, info->fastuvmc, info->range_mapy_flag, info->range_mapy, info->range_mapuv_flag, info->range_mapuv, info->multires, info->syncmarker, info->rangered, info->maxbframes, info->deblockEnable, info->pquant );

  pic->vdp_infos.forward_reference = VDP_INVALID_HANDLE;
  pic->vdp_infos.backward_reference = VDP_INVALID_HANDLE;

  if ( pic->type==P_FRAME ) {
    if ( seq->backward_ref ) {
      ref_accel = (vdpau_accel_t*)seq->backward_ref->accel_data;
      pic->vdp_infos.forward_reference = ref_accel->surface;
    }
    else
      return;
  }
  else if ( pic->type==B_FRAME ) {
    if ( seq->forward_ref ) {
      ref_accel = (vdpau_accel_t*)seq->forward_ref->accel_data;
      pic->vdp_infos.forward_reference = ref_accel->surface;
    }
    else
      return;
    if ( seq->backward_ref ) {
      ref_accel = (vdpau_accel_t*)seq->backward_ref->accel_data;
      pic->vdp_infos.backward_reference = ref_accel->surface;
    }
    else
      return;
  }

  vo_frame_t *img = vd->stream->video_out->get_frame( vd->stream->video_out, seq->coded_width, seq->coded_height,
                                                      seq->ratio, XINE_IMGFMT_VDPAU, VO_BOTH_FIELDS );
  vdpau_accel_t *accel = (vdpau_accel_t*)img->accel_data;
  if ( !seq->accel_vdpau )
    seq->accel_vdpau = accel;

  if( seq->vdp_runtime_nr != *(seq->accel_vdpau->current_vdp_runtime_nr) ) {
    seq->accel_vdpau = accel;
    if ( seq->forward_ref )
      seq->forward_ref->free( seq->forward_ref );
    seq->forward_ref = NULL;
    if ( seq->backward_ref )
      seq->backward_ref->free( seq->backward_ref );
    seq->backward_ref = NULL;
    vd->decoder = VDP_INVALID_HANDLE;
  }

  decode_render( vd, accel );

  img->pts = seq->seq_pts;
  img->bad_frame = 0;
  img->duration = seq->video_step;

  if ( pic->type!=B_FRAME ) {
    if ( pic->type==I_FRAME && !seq->backward_ref ) {
      img->pts = 0;
      img->draw( img, vd->stream );
      ++img->drawn;
    }
    if ( seq->forward_ref ) {
      seq->forward_ref->drawn = 0;
      seq->forward_ref->free( seq->forward_ref );
    }
    seq->forward_ref = seq->backward_ref;
    if ( seq->forward_ref && !seq->forward_ref->drawn ) {
      seq->forward_ref->draw( seq->forward_ref, vd->stream );
    }
    seq->backward_ref = img;
  }
  else {
    img->draw( img, vd->stream );
    img->free( img );
  }

  seq->seq_pts +=seq->video_step;
}



/*
 * This function receives a buffer of data from the demuxer layer and
 * figures out how to handle it based on its header flags.
 */
static void vdpau_vc1_decode_data (video_decoder_t *this_gen, buf_element_t *buf)
{
  vdpau_vc1_decoder_t *this = (vdpau_vc1_decoder_t *) this_gen;
  sequence_t *seq = (sequence_t*)&this->sequence;

  /* a video decoder does not care about this flag (?) */
  if (buf->decoder_flags & BUF_FLAG_PREVIEW) {
    lprintf("BUF_FLAG_PREVIEW\n");
    return;
  }

  if (buf->decoder_flags & BUF_FLAG_FRAMERATE) {
    lprintf("BUF_FLAG_FRAMERATE=%d\n", buf->decoder_info[0]);
    if ( buf->decoder_info[0] > 0 ) {
      this->sequence.video_step = buf->decoder_info[0];
      _x_stream_info_set(this->stream, XINE_STREAM_INFO_FRAME_DURATION, this->sequence.video_step);
    }
  }

  if (buf->decoder_flags & BUF_FLAG_HEADER) {
    lprintf("BUF_FLAG_HEADER\n");
  }

  if (buf->decoder_flags & BUF_FLAG_ASPECT) {
    lprintf("BUF_FLAG_ASPECT\n");
    seq->ratio = (double)buf->decoder_info[1]/(double)buf->decoder_info[2];
    lprintf("arx=%d ary=%d ratio=%f\n", buf->decoder_info[1], buf->decoder_info[2], seq->ratio);
  }

  if (buf->decoder_flags & BUF_FLAG_FRAME_START) {
    lprintf("BUF_FLAG_FRAME_START\n");
    seq->seq_pts = buf->pts;
  }

  if ( !buf->size )
    return;

  //printf("vdpau_vc1_decode_data: new pts : %lld\n", buf->pts );
  seq->cur_pts = buf->pts;

  if (buf->decoder_flags & BUF_FLAG_STDHEADER) {
    lprintf("BUF_FLAG_STDHEADER\n");
    xine_bmiheader *bih = (xine_bmiheader *) buf->content;
    int bs = sizeof( xine_bmiheader );
    seq->coded_width = bih->biWidth;
    seq->coded_height = bih->biHeight;
    lprintf( "width=%d height=%d\n", bih->biWidth, bih->biHeight );
    if ( buf->size > bs ) {
      parse_header( this, buf->content+bs, buf->size-bs );
    }
    int i;
    for ( i=0; i<buf->size; ++i )
      printf("%02X ", buf->content[i] );
    printf("\n\n");
    return;
  }

  int size = seq->bufpos+buf->size;
  if ( seq->bufsize < size ) {
    seq->bufsize = size+10000;
    seq->buf = realloc( seq->buf, seq->bufsize );
    lprintf("sequence buffer realloced = %d\n", seq->bufsize );
  }
  xine_fast_memcpy( seq->buf+seq->bufpos, buf->content, buf->size );
  seq->bufpos += buf->size;

  if (buf->decoder_flags & BUF_FLAG_FRAME_END) {
    lprintf("BUF_FLAG_FRAME_END\n");
    seq->picture.vdp_infos.slice_count = 1;
    decode_picture( this );
    seq->bufpos = 0;
  }


  /*int i;
  for ( i=0; i<buf->size; ++i )
    printf("%02X ", buf->content[i] );
  printf("\n\n");

  uint8_t *buffer = buf->content;
  for ( i=0; i<buf->size-4; ++i ) {
    if ( buffer[i]==0 && buffer[i+1]==0 && buffer[i+2]==1 )
      printf("start code\n");
  }*/
}


/*
 * This function is called when xine needs to flush the system.
 */
static void vdpau_vc1_flush (video_decoder_t *this_gen) {
  vdpau_vc1_decoder_t *this = (vdpau_vc1_decoder_t *) this_gen;

  lprintf( "vdpau_vc1_flush\n" );
}

/*
 * This function resets the video decoder.
 */
static void vdpau_vc1_reset (video_decoder_t *this_gen) {
  vdpau_vc1_decoder_t *this = (vdpau_vc1_decoder_t *) this_gen;

  lprintf( "vdpau_vc1_reset\n" );
  reset_sequence( &this->sequence );

  //this->size = 0;
}

/*
 * The decoder should forget any stored pts values here.
 */
static void vdpau_vc1_discontinuity (video_decoder_t *this_gen) {
  vdpau_vc1_decoder_t *this = (vdpau_vc1_decoder_t *) this_gen;

  lprintf( "vdpau_vc1_discontinuity\n" );
  //reset_sequence( &this->sequence );

}

/*
 * This function frees the video decoder instance allocated to the decoder.
 */
static void vdpau_vc1_dispose (video_decoder_t *this_gen) {

  vdpau_vc1_decoder_t *this = (vdpau_vc1_decoder_t *) this_gen;

  lprintf( "vdpau_vc1_dispose\n" );

  if ( this->decoder!=VDP_INVALID_HANDLE && this->sequence.accel_vdpau ) {
      this->sequence.accel_vdpau->vdp_decoder_destroy( this->decoder );
      this->decoder = VDP_INVALID_HANDLE;
    }

  reset_sequence( &this->sequence );

  this->stream->video_out->close( this->stream->video_out, this->stream );

  free( this->sequence.buf );
  free( this_gen );
}

/*
 * This function allocates, initializes, and returns a private video
 * decoder structure.
 */
static video_decoder_t *open_plugin (video_decoder_class_t *class_gen, xine_stream_t *stream) {

  vdpau_vc1_decoder_t  *this ;

  lprintf( "open_plugin\n" );

  /* the videoout must be vdpau-capable to support this decoder */
  if ( !(stream->video_driver->get_capabilities(stream->video_driver) & VO_CAP_VDPAU_VC1) )
    return NULL;

  /* now check if vdpau has free decoder resource */
  vo_frame_t *img = stream->video_out->get_frame( stream->video_out, 1920, 1080, 1, XINE_IMGFMT_VDPAU, VO_BOTH_FIELDS );
  vdpau_accel_t *accel = (vdpau_accel_t*)img->accel_data;
  img->free(img);
  VdpDecoder decoder;
  VdpStatus st = accel->vdp_decoder_create( accel->vdp_device, VDP_DECODER_PROFILE_VC1_MAIN, 1920, 1080, 2, &decoder );
  if ( st!=VDP_STATUS_OK ) {
    lprintf( "can't create vdpau decoder.\n" );
    return NULL;
  }

  accel->vdp_decoder_destroy( decoder );

  this = (vdpau_vc1_decoder_t *) calloc(1, sizeof(vdpau_vc1_decoder_t));

  this->video_decoder.decode_data         = vdpau_vc1_decode_data;
  this->video_decoder.flush               = vdpau_vc1_flush;
  this->video_decoder.reset               = vdpau_vc1_reset;
  this->video_decoder.discontinuity       = vdpau_vc1_discontinuity;
  this->video_decoder.dispose             = vdpau_vc1_dispose;

  this->stream                            = stream;
  this->class                             = (vdpau_vc1_class_t *) class_gen;

  this->sequence.bufsize = 10000;
  this->sequence.buf = (uint8_t*)malloc(this->sequence.bufsize);
  this->sequence.forward_ref = 0;
  this->sequence.backward_ref = 0;
  this->sequence.vdp_runtime_nr = 1;
  init_sequence( &this->sequence );

  init_picture( &this->sequence.picture );

  this->decoder = VDP_INVALID_HANDLE;
  this->sequence.accel_vdpau = NULL;

  (stream->video_out->open)(stream->video_out, stream);

  return &this->video_decoder;
}

/*
 * This function returns a brief string that describes (usually with the
 * decoder's most basic name) the video decoder plugin.
 */
static char *get_identifier (video_decoder_class_t *this) {
  return "vdpau_vc1";
}

/*
 * This function returns a slightly longer string describing the video
 * decoder plugin.
 */
static char *get_description (video_decoder_class_t *this) {
  return "vdpau_vc1: vc1 decoder plugin using VDPAU hardware decoding.\n"
    "Must be used along with video_out_vdpau.";
}

/*
 * This function frees the video decoder class and any other memory that was
 * allocated.
 */
static void dispose_class (video_decoder_class_t *this) {
  free (this);
}

/*
 * This function allocates a private video decoder class and initializes
 * the class's member functions.
 */
static void *init_plugin (xine_t *xine, void *data) {

  vdpau_vc1_class_t *this;

  this = (vdpau_vc1_class_t *) calloc(1, sizeof(vdpau_vc1_class_t));

  this->decoder_class.open_plugin     = open_plugin;
  this->decoder_class.get_identifier  = get_identifier;
  this->decoder_class.get_description = get_description;
  this->decoder_class.dispose         = dispose_class;

  return this;
}

/*
 * This is a list of all of the internal xine video buffer types that
 * this decoder is able to handle. Check src/xine-engine/buffer.h for a
 * list of valid buffer types (and add a new one if the one you need does
 * not exist). Terminate the list with a 0.
 */
static const uint32_t video_types[] = {
  BUF_VIDEO_VC1, BUF_VIDEO_WMV9,
  0
};

/*
 * This data structure combines the list of supported xine buffer types and
 * the priority that the plugin should be given with respect to other
 * plugins that handle the same buffer type. A plugin with priority (n+1)
 * will be used instead of a plugin with priority (n).
 */
static const decoder_info_t dec_info_video = {
  video_types,         /* supported types */
  8                    /* priority        */
};

/*
 * The plugin catalog entry. This is the only information that this plugin
 * will export to the public.
 */
const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* { type, API, "name", version, special_info, init_function } */
  { PLUGIN_VIDEO_DECODER, 18, "vdpau_vc1", XINE_VERSION_CODE, &dec_info_video, init_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
