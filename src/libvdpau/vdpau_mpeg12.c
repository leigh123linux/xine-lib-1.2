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
 * vdpau_mpeg12.c, a mpeg1/2 video stream parser using VDPAU hardware decoder
 *
 */

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



#define sequence_header_code    0xb3
#define sequence_error_code     0xb4
#define sequence_end_code       0xb7
#define group_start_code        0xb8
#define extension_start_code    0xb5
#define user_data_start_code    0xb2
#define picture_start_code      0x00
#define begin_slice_start_code  0x01
#define end_slice_start_code    0xaf

#define I_FRAME   1
#define P_FRAME   2
#define B_FRAME   3

#define WANT_HEADER 1
#define WANT_EXT    2
#define WANT_SLICE  3



/* default intra quant matrix, in zig-zag order */
static const uint8_t default_intra_quantizer_matrix[64] = {
    8,
    16, 16,
    19, 16, 19,
    22, 22, 22, 22,
    22, 22, 26, 24, 26,
    27, 27, 27, 26, 26, 26,
    26, 27, 27, 27, 29, 29, 29,
    34, 34, 34, 29, 29, 29, 27, 27,
    29, 29, 32, 32, 34, 34, 37,
    38, 37, 35, 35, 34, 35,
    38, 38, 40, 40, 40,
    48, 48, 46, 46,
    56, 56, 58,
    69, 69,
    83
};


typedef struct {
  VdpPictureInfoMPEG1Or2  vdp_infos;
  int                     slices_count;
  uint8_t                 *slices;
  int                     slices_size;
  int                     slices_pos;

  int64_t                pts;

  int                     state;
} picture_t;



typedef struct {
  uint32_t    coded_width;
  uint32_t    coded_height;
  uint32_t    display_width;
  uint32_t    display_height;
  uint64_t    video_step; /* frame duration in pts units */
  double      ratio;

  uint8_t     intra_quant_matrix[64];
  uint8_t     non_intra_quant_matrix[64];

  int         have_header;

  uint8_t     *buf; /* accumulate data */
  int         bufseek;
  uint32_t    bufsize;
  uint32_t    bufpos;
  int         start;

  picture_t   picture;
  vo_frame_t  *forward_ref;

  int64_t    pts;
} sequence_t;


typedef struct {
  video_decoder_class_t   decoder_class;
} vdpau_mpeg12_class_t;


typedef struct foovideo_decoder_s {
  video_decoder_t         video_decoder;  /* parent video decoder structure */

  vdpau_mpeg12_class_t    *class;
  xine_stream_t           *stream;

  sequence_t              sequence;

  VdpDecoder  decoder;

} vdpau_mpeg12_decoder_t;



static void reset_picture( picture_t *pic )
{
  pic->slices_count = 0;
  pic->slices_pos = 0;
  pic->pts = 0;
  pic->state = WANT_HEADER;
}



static void init_picture( picture_t *pic )
{
  pic->slices_size = 2048;
  pic->slices = (uint8_t*)malloc(pic->slices_size);
  reset_picture( pic );
}



static void reset_sequence( sequence_t *sequence )
{
  sequence->have_header = 0;
  sequence->bufpos = 0;
  sequence->bufseek = 0;
  sequence->start = -1;
  xine_fast_memcpy( sequence->intra_quant_matrix, &default_intra_quantizer_matrix, 64 );
  memset( sequence->non_intra_quant_matrix, 16, 64 );
  if ( sequence->forward_ref )
    sequence->forward_ref->free( sequence->forward_ref );
  sequence->forward_ref = 0;
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



static void sequence_header( sequence_t *sequence, uint8_t *buf, int len )
{
  int i, j, off=0;
  sequence->coded_width = get_bits( buf,0,12 );
  sequence->coded_height = get_bits( buf,12,12 );
  switch ( get_bits( buf+3,0,4 ) ) {
    case 2: sequence->ratio = 4.0/3.0; break;
    case 3: sequence->ratio = 16.0/9.0; break;
    case 4: sequence->ratio = 2.21; break;
  }
  //printf( "frame_rate_code: %d\n", get_bits( buf+3,4,4 ) );
  //printf( "bit_rate_value: %d\n", get_bits( buf+4,0,18 ) );
  //printf( "marker_bit: %d\n", get_bits( buf+6,2,1 ) );
  //printf( "vbv_buffer_size_value: %d\n", get_bits( buf+6,3,10 ) );
  //printf( "constrained_parameters_flag: %d\n", get_bits( buf+7,5,1 ) );
  i = get_bits( buf+7,6,1 );
  if ( i ) {
    for ( j=0; j<64; ++j ) {
      sequence->intra_quant_matrix[j] = get_bits( buf+7+j,7,8 );
    }
    off = 64;
  }
  //printf( "load_intra_quantizer_matrix: %d\n", i );
  i = get_bits( buf+7+off,7,1 );
    //printf( "load_non_intra_quantizer_matrix: %d\n", i );
  if ( i ) {
    for ( j=0; j<64; ++j ) {
      sequence->non_intra_quant_matrix[j] = get_bits( buf+8+off+j,0,8 );
    }
  }
  sequence->have_header = 1;
}



static void picture_header( sequence_t *sequence, uint8_t *buf, int len )
{
  int i = get_bits( buf,10,3 );
  printf( "!!!!!!!!!!!!!!! picture type : %d !!!!!!!!!!!!!!!!!!!!!!!!!!\n", i );
  if ( (i!=I_FRAME && i!=P_FRAME) || sequence->picture.state!=WANT_HEADER )
    return;
  reset_picture( &sequence->picture );
  sequence->picture.vdp_infos.picture_coding_type = i;
  sequence->picture.vdp_infos.forward_reference = VDP_INVALID_HANDLE;
  sequence->picture.vdp_infos.backward_reference = VDP_INVALID_HANDLE;
  sequence->picture.vdp_infos.full_pel_forward_vector = 0;
  sequence->picture.vdp_infos.full_pel_backward_vector = 0;
  sequence->picture.state = WANT_EXT;
  //printf( "temporal_reference: %d\n", get_bits( buf,0,10 ) );
  //printf( "picture_coding_type: %d\n", get_bits( buf,10,3 ) );
}



static void sequence_extension( uint8_t *buf, int len )
{
  printf( "extension_start_code_identifier: %d\n", get_bits( buf,0,4 ) );
  printf( "profile_and_level_indication: %d\n", get_bits( buf,4,8 ) );
  printf( "progressive_sequence: %d\n", get_bits( buf,12,1 ) );
  printf( "chroma_format: %d\n", get_bits( buf,13,2 ) );
  printf( "horizontal_size_extension: %d\n", get_bits( buf,15,2 ) );
  printf( "vertical_size_extension: %d\n", get_bits( buf,17,2 ) );
  printf( "bit_rate_extension: %d\n", get_bits( buf,19,12 ) );
  printf( "marker_bit: %d\n", get_bits( buf,31,1 ) );
  printf( "vbv_buffer_size_extension: %d\n", get_bits( buf+4,0,8 ) );
  printf( "low_delay: %d\n", get_bits( buf+5,0,1 ) );
  printf( "frame_rate_extension_n: %d\n", get_bits( buf+5,1,2 ) );
  printf( "frame_rate_extension_d: %d\n", get_bits( buf+5,3,5 ) );
}



static void picture_coding_extension( sequence_t *sequence, uint8_t *buf, int len )
{
  if ( sequence->picture.state!=WANT_EXT )
    return;
  sequence->picture.vdp_infos.f_code[0][0] = get_bits( buf,4,4 );
  sequence->picture.vdp_infos.f_code[0][1] = get_bits( buf,8,4 );
  sequence->picture.vdp_infos.f_code[1][0] = get_bits( buf,12,4 );
  sequence->picture.vdp_infos.f_code[1][1] = get_bits( buf,16,4 );
  //printf( "extension_start_code_identifier: %d\n", get_bits( buf,0,4 ) );
  //printf( "f_code_0_0: %d\n", get_bits( buf,4,4 ) );
  //printf( "f_code_0_1: %d\n", get_bits( buf,8,4 ) );
  //printf( "f_code_1_0: %d\n", get_bits( buf,12,4 ) );
  //printf( "f_code_1_1: %d\n", get_bits( buf,16,4 ) );
  sequence->picture.vdp_infos.intra_dc_precision = get_bits( buf,20,2 );
  //printf( "intra_dc_precision: %d\n", get_bits( buf,20,2 ) );
  sequence->picture.vdp_infos.picture_structure = get_bits( buf,22,2 );
  //printf( "picture_structure: %d\n", get_bits( buf,22,2 ) );
  sequence->picture.vdp_infos.top_field_first = get_bits( buf,24,1 );
  //printf( "top_field_first: %d\n", get_bits( buf,24,1 ) );
  sequence->picture.vdp_infos.frame_pred_frame_dct = get_bits( buf,25,1 );
  //printf( "frame_pred_frame_dct: %d\n", get_bits( buf,25,1 ) );
  sequence->picture.vdp_infos.concealment_motion_vectors = get_bits( buf,26,1 );
  //printf( "concealment_motion_vectors: %d\n", get_bits( buf,26,1 ) );
  sequence->picture.vdp_infos.q_scale_type = get_bits( buf,27,1 );
  //printf( "q_scale_type: %d\n", get_bits( buf,27,1 ) );
  sequence->picture.vdp_infos.intra_vlc_format = get_bits( buf,28,1 );
  //printf( "intra_vlc_format: %d\n", get_bits( buf,28,1 ) );
  sequence->picture.vdp_infos.alternate_scan = get_bits( buf,29,1 );
  //printf( "alternate_scan: %d\n", get_bits( buf,29,1 ) );
  //printf( "repeat_first_field: %d\n", get_bits( buf,30,1 ) );
  //printf( "chroma_420_type: %d\n", get_bits( buf,31,1 ) );
  //printf( "progressive_frame: %d\n", get_bits( buf,32,1 ) );
  sequence->picture.state = WANT_SLICE;
}



static void copy_slice( sequence_t *sequence, uint8_t *buf, int len )
{
  int size = sequence->picture.slices_pos+len;
  if ( sequence->picture.slices_size < size ) {
    sequence->picture.slices_size = size+1024;
    sequence->picture.slices = realloc( sequence->picture.slices, sequence->picture.slices_size );
  }
  xine_fast_memcpy( sequence->picture.slices+sequence->picture.slices_pos, buf, len );
  sequence->picture.slices_pos += len;
  sequence->picture.slices_count++;
}



static void quant_matrix_extension( uint8_t *buf, int len )
{
}



static int parse_code( sequence_t *sequence, uint8_t *buf, int len )
{
  if ( !sequence->have_header && buf[3]!=sequence_header_code )
    return 0;

  if ( (buf[3] >= begin_slice_start_code) && (buf[3] <= end_slice_start_code) ) {
    if ( sequence->picture.state==WANT_SLICE )
      copy_slice( sequence, buf, len );
    return 0;
    //printf( " ----------- slice_start_code: %d\n", i+3 );
  }
  else if ( sequence->picture.state==WANT_SLICE ) {
    /* no more slices, decode */
    return 1;
  }

  switch ( buf[3] ) {
    case sequence_header_code:
      printf( " ----------- sequence_header_code\n" );
      sequence_header( sequence, buf+4, len-4 );
      break;
    case extension_start_code: {
      switch ( get_bits( buf+4,0,4 ) ) {
        case 1:
          printf( " ----------- sequence_extension_start_code\n" );
          sequence_extension( buf+4, len-4 );
          break;
        case 3:
          printf( " ----------- quant_matrix_extension_start_code\n" );
          quant_matrix_extension( buf+4, len-4 );
          break;
        case 8:
          printf( " ----------- picture_coding_extension_start_code\n" );
          picture_coding_extension( sequence, buf+4, len-4 );
          break;
      }
      break;
      }
    case user_data_start_code:
      printf( " ----------- user_data_start_code\n" );
      break;
    case group_start_code:
      printf( " ----------- group_start_code\n" );
      break;
    case picture_start_code:
      printf( " ----------- picture_start_code\n" );
      //slice_count = 0;
      picture_header( sequence, buf+4, len-4 );
      break;
    case sequence_error_code:
      printf( " ----------- sequence_error_code\n" );
      break;
    case sequence_end_code:
      printf( " ----------- sequence_end_code\n" );
      break;
  }
  return 0;
}



static void decode_picture( vdpau_mpeg12_decoder_t *vd )
{
  sequence_t *seq = (sequence_t*)&vd->sequence;
  picture_t *pic = (picture_t*)&seq->picture;

  if ( seq->forward_ref && pic->vdp_infos.picture_coding_type==P_FRAME ) {
    vdpau_accel_t *back_accel = (vdpau_accel_t*)seq->forward_ref->accel_data;
    pic->vdp_infos.forward_reference = back_accel->surface;
  }
  pic->vdp_infos.slice_count = pic->slices_count;
  xine_fast_memcpy( &pic->vdp_infos.intra_quantizer_matrix, &seq->intra_quant_matrix, 64 );
  xine_fast_memcpy( &pic->vdp_infos.non_intra_quantizer_matrix, &seq->non_intra_quant_matrix, 64 );
  pic->state = WANT_HEADER;
  vo_frame_t *img = vd->stream->video_out->get_frame( vd->stream->video_out, seq->coded_width, seq->coded_height,
                                                      seq->ratio, XINE_IMGFMT_VDPAU, VO_BOTH_FIELDS);
  vdpau_accel_t *accel = (vdpau_accel_t*)img->accel_data;
  VdpStatus st;
  if ( vd->decoder==VDP_INVALID_HANDLE ) {
    st = accel->vdp_decoder_create( accel->vdp_device, VDP_DECODER_PROFILE_MPEG2_MAIN, seq->coded_width, seq->coded_height, &vd->decoder);
    if ( st!=VDP_STATUS_OK )
      printf( "vdpau_mpeg12: failed to create decoder !! %s\n", accel->vdp_get_error_string( st ) );
  }
  if ( accel->surface==VDP_INVALID_HANDLE ) {
    st = accel->vdp_video_surface_create( accel->vdp_device, VDP_CHROMA_TYPE_420, seq->coded_width, seq->coded_height, &accel->surface);
    if ( st!=VDP_STATUS_OK )
      printf( "vdpau_mpeg12: failed to create surface !! %s\n", accel->vdp_get_error_string( st ) );
  }
  VdpBitstreamBuffer vbit;
  vbit.struct_version = VDP_BITSTREAM_BUFFER_VERSION;
  vbit.bitstream = seq->picture.slices;
  vbit.bitstream_bytes = seq->picture.slices_pos;
  st = accel->vdp_decoder_render( vd->decoder, accel->surface, (VdpPictureInfo*)&pic->vdp_infos, 1, &vbit );
  if ( st!=VDP_STATUS_OK )
    printf( "vdpau_mpeg12: decoder failed : %d!! %s\n", st, accel->vdp_get_error_string( st ) );
  else
    printf( "vdpau_mpeg12: DECODER SUCCESS !! \n" );

  //img->duration = 3600;
  img->pts = seq->pts;
  img->bad_frame = 0;
  img->draw( img, vd->stream );

  if ( seq->forward_ref )
    seq->forward_ref->free( seq->forward_ref );
  seq->forward_ref = img;
}



/*
 * This function receives a buffer of data from the demuxer layer and
 * figures out how to handle it based on its header flags.
 */
static void vdpau_mpeg12_decode_data (video_decoder_t *this_gen, buf_element_t *buf) {

  vdpau_mpeg12_decoder_t *this = (vdpau_mpeg12_decoder_t *) this_gen;
  sequence_t *seq = (sequence_t*)&this->sequence;

  /* a video decoder does not care about this flag (?) */
  if (buf->decoder_flags & BUF_FLAG_PREVIEW)
    return;

  if (buf->decoder_flags & BUF_FLAG_FRAMERATE) {
    //this->video_step = buf->decoder_info[0];
    //_x_stream_info_set(this->stream, XINE_STREAM_INFO_FRAME_DURATION, this->video_step);
  }

  if ( buf->pts ) {
    seq->pts = buf->pts;
    //printf("vdpau_mpeg12_decode_data: new pts : %lld\n", buf->pts );
  }

  int size = seq->bufpos+buf->size;
  if ( seq->bufsize < size ) {
    seq->bufsize = size+1024;
    seq->buf = realloc( seq->buf, seq->bufsize );
    printf("sequence buffer realloced = %d\n", seq->bufsize );
  }
  xine_fast_memcpy( seq->buf+seq->bufpos, buf->content, buf->size );
  seq->bufpos += buf->size;

  while ( seq->bufseek < seq->bufpos-4 ) {
    uint8_t *buf = seq->buf+seq->bufseek;
    if ( buf[0]==0 && buf[1]==0 && buf[2]==1 ) {
      if ( seq->start<0 ) {
        seq->start = seq->bufseek;
      }
      else {
        if ( parse_code( seq, seq->buf+seq->start, seq->bufseek-seq->start ) ) {
          decode_picture( this );
          //seq->pts += 3600;
          parse_code( seq, seq->buf+seq->start, seq->bufseek-seq->start );
        }
        uint8_t *tmp = (uint8_t*)malloc(seq->bufsize);
        xine_fast_memcpy( tmp, seq->buf+seq->bufseek, seq->bufpos-seq->bufseek );
        seq->bufpos -= seq->bufseek;
        seq->start = -1;
        seq->bufseek = -1;
        free( seq->buf );
        seq->buf = tmp;
      }
    }
    ++seq->bufseek;
  }

}

/*
 * This function is called when xine needs to flush the system.
 */
static void vdpau_mpeg12_flush (video_decoder_t *this_gen) {
}

/*
 * This function resets the video decoder.
 */
static void vdpau_mpeg12_reset (video_decoder_t *this_gen) {
  vdpau_mpeg12_decoder_t *this = (vdpau_mpeg12_decoder_t *) this_gen;

  //this->size = 0;
}

/*
 * The decoder should forget any stored pts values here.
 */
static void vdpau_mpeg12_discontinuity (video_decoder_t *this_gen) {
  vdpau_mpeg12_decoder_t *this = (vdpau_mpeg12_decoder_t *) this_gen;

}

/*
 * This function frees the video decoder instance allocated to the decoder.
 */
static void vdpau_mpeg12_dispose (video_decoder_t *this_gen) {

  vdpau_mpeg12_decoder_t *this = (vdpau_mpeg12_decoder_t *) this_gen;

  /*if (this->buf) {
    free (this->buf);
    this->buf = NULL;
  }

  if (this->decoder_ok) {
    this->decoder_ok = 0;
    this->stream->video_out->close(this->stream->video_out, this->stream);
  }*/

  free( this->sequence.picture.slices );
  free( this->sequence.buf );
  free( this_gen );
}

/*
 * This function allocates, initializes, and returns a private video
 * decoder structure.
 */
static video_decoder_t *open_plugin (video_decoder_class_t *class_gen, xine_stream_t *stream) {

  vdpau_mpeg12_decoder_t  *this ;

  /* the videoout must be vdpau-capable to support this decoder */
  if ( !(stream->video_driver->get_capabilities(stream->video_driver) & VO_CAP_VDPAU_MPEG12) )
    return NULL;

  this = (vdpau_mpeg12_decoder_t *) calloc(1, sizeof(vdpau_mpeg12_decoder_t));

  this->video_decoder.decode_data         = vdpau_mpeg12_decode_data;
  this->video_decoder.flush               = vdpau_mpeg12_flush;
  this->video_decoder.reset               = vdpau_mpeg12_reset;
  this->video_decoder.discontinuity       = vdpau_mpeg12_discontinuity;
  this->video_decoder.dispose             = vdpau_mpeg12_dispose;

  this->stream                            = stream;
  this->class                             = (vdpau_mpeg12_class_t *) class_gen;

  this->sequence.bufsize = 1024;
  this->sequence.buf = (uint8_t*)malloc(this->sequence.bufsize);
  this->sequence.forward_ref = 0;
  reset_sequence( &this->sequence );

  init_picture( &this->sequence.picture );

  this->decoder = VDP_INVALID_HANDLE;

  return &this->video_decoder;
}

/*
 * This function returns a brief string that describes (usually with the
 * decoder's most basic name) the video decoder plugin.
 */
static char *get_identifier (video_decoder_class_t *this) {
  return "vdpau_mpeg12";
}

/*
 * This function returns a slightly longer string describing the video
 * decoder plugin.
 */
static char *get_description (video_decoder_class_t *this) {
  return "vdpau_mpeg12: mpeg1/2 decoder plugin using VDPAU hardware decoding.\n"
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

  vdpau_mpeg12_class_t *this;

  this = (vdpau_mpeg12_class_t *) calloc(1, sizeof(vdpau_mpeg12_class_t));

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
  BUF_VIDEO_MPEG,
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
  { PLUGIN_VIDEO_DECODER, 18, "vdpau_mpeg12", XINE_VERSION_CODE, &dec_info_video, init_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
