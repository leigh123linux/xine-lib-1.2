/* 
 * Copyright (C) 2000-2003 the xine project
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 * $Id: audio_decoder.c,v 1.49 2006/07/10 22:08:30 dgp85 Exp $
 *
 * thin layer to use real binary-only codecs in xine
 *
 * code inspired by work from Florian Schneider for the MPlayer Project 
 */


#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#ifdef __x86_64__
  #include <elf.h>
#endif

#define LOG_MODULE "real_audio_decoder"
#define LOG_VERBOSE
/*
#define LOG
*/

#include "bswap.h"
#include "xine_internal.h"
#include "video_out.h"
#include "buffer.h"
#include "xineutils.h"

typedef struct {
  audio_decoder_class_t   decoder_class;

  /* empty so far */
} real_class_t;

typedef struct realdec_decoder_s {
  audio_decoder_t  audio_decoder;

  real_class_t    *cls;

  xine_stream_t   *stream;

  void            *ra_handle;

  unsigned long  (*raCloseCodec)(void*);
  unsigned long  (*raDecode)(void*, char*,unsigned long,char*,unsigned int*,long);
  unsigned long  (*raFlush)(unsigned long,unsigned long,unsigned long);
  unsigned long  (*raFreeDecoder)(void*);
  void*          (*raGetFlavorProperty)(void*,unsigned long,unsigned long,int*);
  unsigned long  (*raInitDecoder)(void*, void*);
  unsigned long  (*raOpenCodec2)(void*);
  unsigned long  (*raSetFlavor)(void*,unsigned long);
  void           (*raSetDLLAccessPath)(char*);
  void           (*raSetPwd)(char*,char*);

  void            *context;

  int              sps, w, h;
  int              block_align;

  uint8_t         *frame_buffer;
  uint8_t         *frame_reordered;
  int              frame_size;
  int              frame_num_bytes;

  int              sample_size;

  uint64_t         pts;

  int              output_open;
  
  int              decoder_ok;

} realdec_decoder_t;

typedef struct {
    int    samplerate;
    short  bits;
    short  channels;
    int    unk1;
    int    subpacket_size;
    int    coded_frame_size;
    int    codec_data_length;
    void  *extras;
} ra_init_t;

void *__builtin_new(unsigned long size);
void __builtin_delete (void *foo);
void *__builtin_vec_new(unsigned long size);
void __builtin_vec_delete(void *mem);
void __pure_virtual(void);


void *__builtin_new(unsigned long size) {
  return malloc(size);
}

void __builtin_delete (void *foo) {
  /* printf ("libareal: __builtin_delete called\n"); */
  free (foo);
}

#ifdef __x86_64__
/* (gb) quick-n-dirty check to be run natively */
static int is_x86_64_object_(FILE *f)
{
  Elf64_Ehdr *hdr = malloc(sizeof(Elf64_Ehdr));
  if (hdr == NULL)
	return 0;

  if (fseek(f, 0, SEEK_SET) != 0) {
	free(hdr);
	return 0;
  }

  if (fread(hdr, sizeof(Elf64_Ehdr), 1, f) != 1) {
	free(hdr);
	return 0;
  }

  if (hdr->e_ident[EI_MAG0] != ELFMAG0 ||
	  hdr->e_ident[EI_MAG1] != ELFMAG1 ||
	  hdr->e_ident[EI_MAG2] != ELFMAG2 ||
	  hdr->e_ident[EI_MAG3] != ELFMAG3) {
	free(hdr);
	return 0;
  }

  return hdr->e_machine == EM_X86_64;
}

static inline int is_x86_64_object(const char *filename)
{
  FILE *f;
  int ret;

  if ((f = fopen(filename, "r")) == NULL)
	return 0;

  ret = is_x86_64_object_(f);
  fclose(f);
  return ret;
}
#endif

static int load_syms_linux (realdec_decoder_t *this, char *codec_name,
			    const char *alt_codec_name) {

  cfg_entry_t* entry = this->stream->xine->config->lookup_entry(
			 this->stream->xine->config, "decoder.external.real_codecs_path");
  char path[1024];
  struct stat sb;

  snprintf (path, sizeof(path), "%s/%s", entry->str_value, codec_name);
  if (stat(path, &sb))
    snprintf (path, sizeof(path), "%s/%s", entry->str_value, alt_codec_name);
    
#ifdef __x86_64__
  /* check whether it's a real x86-64 library */
  if (!is_x86_64_object(path))
	return 0;
#endif

  lprintf ("(audio) opening shared obj '%s'\n", path);

  this->ra_handle = dlopen (path, RTLD_LAZY);

  if (!this->ra_handle) {
    xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "libareal: error: %s\n", dlerror());
    _x_message(this->stream, XINE_MSG_LIBRARY_LOAD_ERROR,
                 codec_name, NULL);
    return 0;
  }
  
  this->raCloseCodec        = dlsym (this->ra_handle, "RACloseCodec");
  this->raDecode            = dlsym (this->ra_handle, "RADecode");
  this->raFlush             = dlsym (this->ra_handle, "RAFlush");
  this->raFreeDecoder       = dlsym (this->ra_handle, "RAFreeDecoder");
  this->raGetFlavorProperty = dlsym (this->ra_handle, "RAGetFlavorProperty");
  this->raOpenCodec2        = dlsym (this->ra_handle, "RAOpenCodec2");
  this->raInitDecoder       = dlsym (this->ra_handle, "RAInitDecoder");
  this->raSetFlavor         = dlsym (this->ra_handle, "RASetFlavor");
  this->raSetDLLAccessPath  = dlsym (this->ra_handle, "SetDLLAccessPath");
  this->raSetPwd            = dlsym (this->ra_handle, "RASetPwd"); /* optional, used by SIPR */

  if (!this->raCloseCodec || !this->raDecode || !this->raFlush || !this->raFreeDecoder ||
      !this->raGetFlavorProperty || !this->raOpenCodec2 || !this->raSetFlavor ||
      /*!raSetDLLAccessPath ||*/ !this->raInitDecoder){
    xprintf (this->stream->xine, XINE_VERBOSITY_LOG, 
	     _("libareal: (audio) Cannot resolve symbols - incompatible dll: %s\n"), path);
    return 0;
  }

  if (this->raSetDLLAccessPath){

    char path[1024];

    snprintf(path, sizeof(path) - 2, "DT_Codecs=%s", entry->str_value);
    if (path[strlen(path)-1]!='/'){
      path[strlen(path)+1]=0;
      path[strlen(path)]='/';
    }
    path[strlen(path)+1]=0;

    this->raSetDLLAccessPath(path);
  }

  lprintf ("audio decoder loaded successfully\n");

  return 1;
}

static int init_codec (realdec_decoder_t *this, buf_element_t *buf) {

  int   version, result ;
  int   samples_per_sec, bits_per_sample, num_channels;
  int   subpacket_size, coded_frame_size, codec_data_length;
  int   coded_frame_size2, data_len, flavor;
  int   mode;
  void *extras;
  
  /*
   * extract header data
   */

  version = BE_16 (buf->content+4);

  lprintf ("header buffer detected, header version %d\n", version);
#ifdef LOG
  xine_hexdump (buf->content, buf->size);
#endif
    
  flavor           = BE_16 (buf->content+22);
  coded_frame_size = BE_32 (buf->content+24);
  codec_data_length= BE_16 (buf->content+40);
  coded_frame_size2= BE_16 (buf->content+42);
  subpacket_size   = BE_16 (buf->content+44);
    
  this->sps        = subpacket_size;
  this->w          = coded_frame_size2;
  this->h          = codec_data_length;

  if (version == 4) {
    samples_per_sec = BE_16 (buf->content+48);
    bits_per_sample = BE_16 (buf->content+52);
    num_channels    = BE_16 (buf->content+54);

    /* FIXME: */
    if (buf->type==BUF_AUDIO_COOK) {
      
      xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, 
	       "libareal: audio header version 4 for COOK audio not supported.\n");
      return 0;
    }
    data_len        = 0; /* FIXME: COOK audio needs this */
    extras          = buf->content+71;

  } else {
    samples_per_sec = BE_16 (buf->content+54);
    bits_per_sample = BE_16 (buf->content+58);
    num_channels    = BE_16 (buf->content+60);
    data_len        = BE_32 (buf->content+74);
    extras          = buf->content+78;
  }

  this->block_align= coded_frame_size2;

  lprintf ("0x%04x 0x%04x 0x%04x 0x%04x data_len 0x%04x\n",
	   subpacket_size, coded_frame_size, codec_data_length, 
	   coded_frame_size2, data_len);
  lprintf ("%d samples/sec, %d bits/sample, %d channels\n",
	   samples_per_sec, bits_per_sample, num_channels);

  /* load codec, resolv symbols */

  switch (buf->type) {
  case BUF_AUDIO_COOK:
    _x_meta_info_set_utf8(this->stream, XINE_META_INFO_AUDIOCODEC, "Cook");
    if (!load_syms_linux (this, "cook.so", "cook.so.6.0"))
      return 0;
    break;
    
  case BUF_AUDIO_ATRK:
    _x_meta_info_set_utf8(this->stream, XINE_META_INFO_AUDIOCODEC, "Atrac");
    if (!load_syms_linux (this, "atrc.so", "atrc.so.6.0"))
      return 0;
    this->block_align = 384;
    break;

  case BUF_AUDIO_14_4:
    _x_meta_info_set_utf8(this->stream, XINE_META_INFO_AUDIOCODEC, "Real 14.4");
    if (!load_syms_linux (this, "14_4.so", "14_4.so.6.0"))
      return 0;
    break;

  case BUF_AUDIO_28_8:
    _x_meta_info_set_utf8(this->stream, XINE_META_INFO_AUDIOCODEC, "Real 28.8");
    if (!load_syms_linux (this, "28_8.so", "28_8.so.6.0"))
      return 0;
    break;

  case BUF_AUDIO_SIPRO:
    _x_meta_info_set_utf8(this->stream, XINE_META_INFO_AUDIOCODEC, "Sipro");
    if (!load_syms_linux (this, "sipr.so", "sipr.so.6.0"))
      return 0;
    /* this->block_align = 19; */
    break;

  default:
    xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, 
	     "libareal: error, i don't handle buf type 0x%08x\n", buf->type);
    return 0;
  }

  /*
   * init codec
   */

  result = this->raOpenCodec2 (&this->context);
  if (result) {
    xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "libareal: error in raOpenCodec2: %d\n", result);
    return 0;
  }

  { 
    ra_init_t init_data;

    init_data.samplerate = samples_per_sec;
    init_data.bits = bits_per_sample;
    init_data.channels = num_channels;
    init_data.unk1 = 100; /* ??? */
    init_data.subpacket_size = subpacket_size; /* subpacket size */
    init_data.coded_frame_size = coded_frame_size; /* coded frame size */
    init_data.codec_data_length = data_len; /* codec data length */
    init_data.extras = extras; /* extras */

#ifdef LOG
    printf ("libareal: init_data:\n");
    xine_hexdump ((char *) &init_data, sizeof (ra_init_t));
    printf ("libareal: extras :\n");
    xine_hexdump (init_data.extras, data_len);
#endif
     
    result = this->raInitDecoder (this->context, &init_data);
    if(result){
      xprintf (this->stream->xine, XINE_VERBOSITY_LOG, 
	       _("libareal: decoder init failed, error code: 0x%x\n"), result);
      return 0;
    }
  }

  if (this->raSetPwd){
    /* used by 'SIPR' */
    this->raSetPwd (this->context, "Ardubancel Quazanga"); /* set password... lol. */
    lprintf ("password set\n");
  }

  result = this->raSetFlavor (this->context, flavor);
  if (result){
    xprintf (this->stream->xine, XINE_VERBOSITY_LOG,
	     _("libareal: decoder flavor setup failed, error code: 0x%x\n"), result);
    return 0;
  }

  /*
   * alloc buffers for data reordering
   */

  if (this->sps) {

    this->frame_size      = this->w/this->sps*this->h*this->sps;
    this->frame_buffer    = xine_xmalloc (this->frame_size);
    this->frame_reordered = xine_xmalloc (this->frame_size);
    this->frame_num_bytes = 0;

  } else {

    this->frame_size      = this->w*this->h;
    this->frame_buffer    = xine_xmalloc (this->frame_size);
    this->frame_reordered = this->frame_buffer;
    this->frame_num_bytes = 0;

  }

  /*
   * open audio output
   */

  switch (num_channels) {
  case 1:
    mode = AO_CAP_MODE_MONO;
    break;
  case 2:
    mode = AO_CAP_MODE_STEREO;
    break;
  default:
    xprintf (this->stream->xine, XINE_VERBOSITY_LOG,
	     _("libareal: oups, real can do more than 2 channels ?\n"));
    return 0;
  }

  this->stream->audio_out->open(this->stream->audio_out, 
				this->stream,
				bits_per_sample,
				samples_per_sec,
				mode) ;

  this->output_open = 1;

  this->sample_size = num_channels * (bits_per_sample>>3);

  return 1;
}

static unsigned char sipr_swaps[38][2]={
    {0,63},{1,22},{2,44},{3,90},{5,81},{7,31},{8,86},{9,58},{10,36},{12,68},
    {13,39},{14,73},{15,53},{16,69},{17,57},{19,88},{20,34},{21,71},{24,46},
    {25,94},{26,54},{28,75},{29,50},{32,70},{33,92},{35,74},{38,85},{40,56},
    {42,87},{43,65},{45,59},{48,79},{49,93},{51,89},{55,95},{61,76},{67,83},
    {77,80} };

static void realdec_decode_data (audio_decoder_t *this_gen, buf_element_t *buf) {
  realdec_decoder_t *this = (realdec_decoder_t *) this_gen;

  lprintf ("decode_data %d bytes, flags=0x%08x, pts=%lld ...\n", 
	   buf->size, buf->decoder_flags, buf->pts);

  if (buf->decoder_flags & BUF_FLAG_PREVIEW) {

    /* real_find_sequence_header (&this->real, buf->content, buf->content + buf->size);*/

  } else if (buf->decoder_flags & BUF_FLAG_HEADER) {

    this->decoder_ok = init_codec (this, buf) ;
    if( !this->decoder_ok )
      _x_stream_info_set(this->stream, XINE_STREAM_INFO_AUDIO_HANDLED, 0);

  } else if( this->decoder_ok ) {

    int size;

    lprintf ("content buffer detected, %d bytes\n", buf->size);

    if (buf->pts && !this->pts)
      this->pts = buf->pts;

    size = buf->size;

    while (size) {

      int needed;

      needed = this->frame_size - this->frame_num_bytes;

      if (needed>size) {
	
	memcpy (this->frame_buffer+this->frame_num_bytes, buf->content, size);
	this->frame_num_bytes += size;

	lprintf ("buffering %d/%d bytes\n", this->frame_num_bytes, this->frame_size);

	size = 0;

      } else {

	int result;
	int len     =-1;
	int n;
	int sps     = this->sps;
	int w       = this->w;
	int h       = this->h;
	audio_buffer_t *audio_buffer;

	lprintf ("buffering %d bytes\n", needed);

	memcpy (this->frame_buffer+this->frame_num_bytes, buf->content, needed);

	size -= needed;
	this->frame_num_bytes = 0;

	lprintf ("frame completed. reordering...\n");
	lprintf ("bs=%d  sps=%d  w=%d h=%d \n",/*sh->wf->nBlockAlign*/-1,sps,w,h);

	if (!sps) {

	  int            j,n;
	  int            bs=h*w*2/96; /* nibbles per subpacket */
	  unsigned char *p=this->frame_buffer;
	  
	  /* 'sipr' way */
	  /* demux_read_data(sh->ds, p, h*w); */
	  for (n=0;n<38;n++){
	    int i=bs*sipr_swaps[n][0];
	    int o=bs*sipr_swaps[n][1];
	    /* swap nibbles of block 'i' with 'o'      TODO: optimize */
	    for (j=0;j<bs;j++) {
	      int x=(i&1) ? (p[(i>>1)]>>4) : (p[(i>>1)]&15);
	      int y=(o&1) ? (p[(o>>1)]>>4) : (p[(o>>1)]&15);
	      if (o&1) 
		p[(o>>1)]=(p[(o>>1)]&0x0F)|(x<<4);
	      else  
		p[(o>>1)]=(p[(o>>1)]&0xF0)|x;

	      if (i&1) 
		p[(i>>1)]=(p[(i>>1)]&0x0F)|(y<<4);
	      else  
		p[(i>>1)]=(p[(i>>1)]&0xF0)|y;

	      ++i;
	      ++o;
	    }
	  }
	  /*
	  sh->a_in_buffer_size=
	    sh->a_in_buffer_len=w*h;
	  */

	} else {
	  int      x, y;
	  uint8_t *s;
	  
	  /*  'cook' way */
	  
	  w /= sps; s = this->frame_buffer;
	  
	  for (y=0; y<h; y++)
	    
	    for (x=0; x<w; x++) {
	      
	      lprintf ("x=%d, y=%d, off %d\n",
		       x, y, sps*(h*x+((h+1)/2)*(y&1)+(y>>1)));
	      
	      memcpy (this->frame_reordered+sps*(h*x+((h+1)/2)*(y&1)+(y>>1)),
		      s, sps);
	      s+=sps;
	      
	      /* demux_read_data(sh->ds, sh->a_in_buffer+sps*(h*x+((h+1)/2)*(y&1)+(y>>1)), 
		 sps); */
	      
	    }
	  /*
	    sh->a_in_buffer_size=
	    sh->a_in_buffer_len=w*h*sps;
	  */
	}

#ifdef LOG
	xine_hexdump (this->frame_reordered, buf->size);
#endif
  
	n = 0;
	while (n<this->frame_size) {

	  audio_buffer = this->stream->audio_out->get_buffer (this->stream->audio_out);

	  result = this->raDecode (this->context, 
				   this->frame_reordered+n,
				   this->block_align,
				   (char *) audio_buffer->mem, &len, -1);

	  lprintf ("raDecode result %d, len=%d\n", result, len);

	  audio_buffer->vpts       = this->pts; 

	  this->pts = 0;

	  audio_buffer->num_frames = len/this->sample_size;;
	  
	  this->stream->audio_out->put_buffer (this->stream->audio_out, 
					       audio_buffer, this->stream);
	  n+=this->block_align;
	}
      }
    }
  }

  lprintf ("decode_data...done\n");
}

static void realdec_reset (audio_decoder_t *this_gen) {
  realdec_decoder_t *this = (realdec_decoder_t *) this_gen;
  
  this->frame_num_bytes = 0;
}

static void realdec_discontinuity (audio_decoder_t *this_gen) {
  realdec_decoder_t *this = (realdec_decoder_t *) this_gen;
  
  this->pts = 0;
}

static void realdec_dispose (audio_decoder_t *this_gen) {

  realdec_decoder_t *this = (realdec_decoder_t *) this_gen;

  lprintf ("dispose\n");

  if (this->context)
    this->raCloseCodec (this->context);

#if 0
  printf ("libareal: FreeDecoder...\n");

  if (this->context)
    this->raFreeDecoder (this->context);
#endif

  lprintf ("dlclose...\n");

  if (this->ra_handle)
    dlclose (this->ra_handle);

  if (this->output_open)
     this->stream->audio_out->close (this->stream->audio_out, this->stream);

  if (this->frame_buffer)
    free (this->frame_buffer);

  free (this);

  lprintf ("dispose done\n");
}

static audio_decoder_t *open_plugin (audio_decoder_class_t *class_gen, 
				     xine_stream_t *stream) {

  real_class_t      *cls = (real_class_t *) class_gen;
  realdec_decoder_t *this ;

  this = (realdec_decoder_t *) xine_xmalloc (sizeof (realdec_decoder_t));

  this->audio_decoder.decode_data         = realdec_decode_data;
  this->audio_decoder.reset               = realdec_reset;
  this->audio_decoder.discontinuity       = realdec_discontinuity;
  this->audio_decoder.dispose             = realdec_dispose;
  this->stream                            = stream;
  this->cls                               = cls;

  this->output_open = 0;

  return &this->audio_decoder;
}

/*
 * real plugin class
 */

static char *get_identifier (audio_decoder_class_t *this) {
  return "realadec";
}

static char *get_description (audio_decoder_class_t *this) {
  return "real binary-only codec based audio decoder plugin";
}

static void dispose_class (audio_decoder_class_t *this) {
  free (this);
}

/*
 * some fake functions to make real codecs happy 
 */
void *__builtin_vec_new(unsigned long size) EXPORTED;
void __builtin_vec_delete(void *mem) EXPORTED;
void __pure_virtual(void) EXPORTED;

void *__builtin_vec_new(unsigned long size) {
  return malloc(size);
}
void __builtin_vec_delete(void *mem) {
  free(mem);
}
void __pure_virtual(void) {
  lprintf("libareal: FATAL: __pure_virtual() called!\n");
  /*      exit(1); */
}

/*
 * real audio codec loader
 */

static void *init_class (xine_t *xine, void *data) {

  real_class_t       *this;
  config_values_t    *config = xine->config;
  char               *real_codec_path;
  char               *default_real_codec_path = "";
  struct stat s;

  this = (real_class_t *) xine_xmalloc (sizeof (real_class_t));

  this->decoder_class.open_plugin     = open_plugin;
  this->decoder_class.get_identifier  = get_identifier;
  this->decoder_class.get_description = get_description;
  this->decoder_class.dispose         = dispose_class;

  /* try some auto-detection */

  if (!stat ("/usr/local/RealPlayer8/Codecs/drv3.so.6.0", &s)) 
    default_real_codec_path = "/usr/local/RealPlayer8/Codecs";
  if (!stat ("/usr/RealPlayer8/Codecs/drv3.so.6.0", &s)) 
    default_real_codec_path = "/usr/RealPlayer8/Codecs";
  if (!stat ("/usr/lib/RealPlayer8/Codecs/drv3.so.6.0", &s)) 
    default_real_codec_path = "/usr/lib/RealPlayer8/Codecs";
  if (!stat ("/opt/RealPlayer8/Codecs/drv3.so.6.0", &s)) 
    default_real_codec_path = "/opt/RealPlayer8/Codecs";
  if (!stat ("/usr/lib/RealPlayer9/users/Real/Codecs/drv3.so.6.0", &s)) 
    default_real_codec_path = "/usr/lib/RealPlayer9/users/Real/Codecs";
  if (!stat ("/usr/lib/RealPlayer10/codecs/drvc.so", &s)) 
    default_real_codec_path = "/usr/lib/RealPlayer10/codecs";
  if (!stat ("/usr/lib64/RealPlayer8/Codecs/drv3.so.6.0", &s)) 
    default_real_codec_path = "/usr/lib64/RealPlayer8/Codecs";
  if (!stat ("/usr/lib64/RealPlayer9/users/Real/Codecs/drv3.so.6.0", &s)) 
    default_real_codec_path = "/usr/lib64/RealPlayer9/users/Real/Codecs";
  if (!stat ("/usr/lib64/RealPlayer10/codecs/drvc.so", &s)) 
    default_real_codec_path = "/usr/lib64/RealPlayer10/codecs";
  if (!stat ("/usr/lib/codecs/drv3.so.6.0", &s)) 
    default_real_codec_path = "/usr/lib/codecs";
  if (!stat ("/usr/lib/win32/drv3.so.6.0", &s)) 
    default_real_codec_path = "/usr/lib/win32";
  
  real_codec_path = config->register_string (config, "decoder.external.real_codecs_path", 
					     default_real_codec_path,
					     _("path to RealPlayer codecs"),
					     _("If you have RealPlayer installed, specify the path "
					       "to its codec directory here. You can easily find "
					       "the codec directory by looking for a file named "
					       "\"drv3.so.6.0\" in it. If xine can find the RealPlayer "
					       "codecs, it will use them to decode RealPlayer content "
					       "for you. Consult the xine FAQ for more information on "
					       "how to install the codecs."),
					     10, NULL, this);
  
  lprintf ("real codec path : %s\n",  real_codec_path);

  return this;
}

/*
 * exported plugin catalog entry
 */

static uint32_t audio_types[] = { 
  BUF_AUDIO_COOK, BUF_AUDIO_ATRK, /* BUF_AUDIO_14_4, BUF_AUDIO_28_8, */ BUF_AUDIO_SIPRO, 0
 };

static const decoder_info_t dec_info_audio = {
  audio_types,         /* supported types */
  5                    /* priority        */
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */  
  { PLUGIN_AUDIO_DECODER | PLUGIN_MUST_PRELOAD, 15, "realadec", XINE_VERSION_CODE, &dec_info_audio, init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
