/* 
 * Copyright (C) 2000-2002 the xine project
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
 * $Id: demux_avi.c,v 1.81 2002/04/29 23:31:59 jcdutton Exp $
 *
 * demultiplexer for avi streams
 *
 * part of the code is taken from 
 * avilib (C) 1999 Rainer Johanni <Rainer@Johanni.de>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "xine_internal.h"
#include "xineutils.h"
#include "demux.h"

#define	WINE_TYPEDEFS_ONLY
#include "libw32dll/wine/avifmt.h"
#include "libw32dll/wine/windef.h"
#include "libw32dll/wine/vfw.h"

#define MAX_AUDIO_STREAMS 8

#define VALID_ENDS "avi"

/* The following variable indicates the kind of error */

typedef struct
{
  long pos;
  long len;
  long flags;
} video_index_entry_t;

typedef struct
{
  long pos;
  long len;
  long tot;
} audio_index_entry_t;

typedef struct
{
  long   dwScale_audio, dwRate_audio;
  long   dwSampleSize;

  uint32_t audio_type;      /* BUF_AUDIO_xxx type */

  long   a_fmt;             /* Audio format, see #defines below */
  long   a_chans;           /* Audio channels, 0 for no audio */
  long   a_rate;            /* Rate in Hz */
  long   a_bits;            /* bits per audio sample */
  long   audio_strn;        /* Audio stream number */
  long   audio_bytes;       /* Total number of bytes of audio data */
  long   audio_chunks;      /* Chunks of audio data in the file */
  char   audio_tag[4];      /* Tag of audio data */
  long   audio_posc;        /* Audio position: chunk */
  long   audio_posb;        /* Audio position: byte within chunk */

  char   *wavex;
  int    wavex_len;

  audio_index_entry_t   *audio_index;

} avi_audio_t;

typedef struct
{
  long   width;             /* Width  of a video frame */
  long   height;            /* Height of a video frame */
  long   dwScale, dwRate;
  double fps;               /* Frames per second */
  
  char   compressor[8];     /* Type of compressor, 4 bytes + padding for 0 byte */
  long   video_strn;        /* Video stream number */
  long   video_frames;      /* Number of video frames */
  char   video_tag[4];      /* Tag of video data */
  long   video_posf;        /* Number of next frame to be read
			       (if index present) */
  long   video_posb;        /* Video position: byte within frame */
  

  avi_audio_t		*audio[MAX_AUDIO_STREAMS];
  int			n_audio;
  
  uint32_t video_type;      /* BUF_VIDEO_xxx type */

  long                   pos;      /* position in file */
  long                   n_idx;    /* number of index entries actually filled */
  long                   max_idx;  /* number of index entries actually allocated */
  unsigned char        (*idx)[16]; /* index entries (AVI idx1 tag) */
  video_index_entry_t   *video_index;
  BITMAPINFOHEADER       bih;
  off_t                  movi_start;
} avi_t;

typedef struct demux_avi_s {
  demux_plugin_t       demux_plugin;

  xine_t              *xine;

  config_values_t     *config;

  fifo_buffer_t       *audio_fifo;
  fifo_buffer_t       *video_fifo;

  input_plugin_t      *input;

  avi_t               *avi;

  pthread_t            thread;
  pthread_mutex_t      mutex;

  int                  status;

  int                  no_audio;
  int                  have_spu;

  uint32_t             video_step;
  uint32_t             AVI_errno; 

  int                  send_end_buffers;

  char                 last_mrl[1024];
} demux_avi_t ;

#define AVI_ERR_SIZELIM      1     /* The write of the data would exceed
                                      the maximum size of the AVI file.
                                      This is more a warning than an error
                                      since the file may be closed safely */

#define AVI_ERR_OPEN         2     /* Error opening the AVI file - wrong path
                                      name or file nor readable/writable */

#define AVI_ERR_READ         3     /* Error reading from AVI File */

#define AVI_ERR_WRITE        4     /* Error writing to AVI File,
                                      disk full ??? */

#define AVI_ERR_WRITE_INDEX  5     /* Could not write index to AVI file
                                      during close, file may still be
                                      usable */

#define AVI_ERR_CLOSE        6     /* Could not write header to AVI file
                                      or not truncate the file during close,
                                      file is most probably corrupted */

#define AVI_ERR_NOT_PERM     7     /* Operation not permitted:
                                      trying to read from a file open
                                      for writing or vice versa */

#define AVI_ERR_NO_MEM       8     /* malloc failed */

#define AVI_ERR_NO_AVI       9     /* Not an AVI file */

#define AVI_ERR_NO_HDRL     10     /* AVI file has no header list,
                                      corrupted ??? */

#define AVI_ERR_NO_MOVI     11     /* AVI file has no MOVI list,
                                      corrupted ??? */

#define AVI_ERR_NO_VIDS     12     /* AVI file contains no video data */

#define AVI_ERR_NO_IDX      13     /* The file has been opened with
                                      getIndex==0, but an operation has been
                                      performed that needs an index */

static unsigned long str2ulong(unsigned char *str)
{
  return ( str[0] | (str[1]<<8) | (str[2]<<16) | (str[3]<<24) );
}

static unsigned long str2ushort(unsigned char *str)
{
  return ( str[0] | (str[1]<<8) );
}

static void long2str(unsigned char *dst, int n)
{
  dst[0] = (n    )&0xff;
  dst[1] = (n>> 8)&0xff;
  dst[2] = (n>>16)&0xff;
  dst[3] = (n>>24)&0xff;
}

static void AVI_close(avi_t *AVI)
{
  int i;

  if(AVI->idx) free(AVI->idx);
  if(AVI->video_index) free(AVI->video_index);

  for(i=0; i<AVI->n_audio; i++) {
    if(AVI->audio[i]->audio_index) free(AVI->audio[i]->audio_index);
    if(AVI->audio[i]->wavex) free(AVI->audio[i]->wavex);
    free(AVI->audio[i]);
  }
  free(AVI);
}

#define ERR_EXIT(x)	\
do {			\
   this->AVI_errno = x; \
   free (AVI);  \
   return 0;		\
} while(0)

#define PAD_EVEN(x) ( ((x)+1) & ~1 )

static int avi_sampsize(avi_t *AVI, int track)
{
  int s;
  s = ((AVI->audio[track]->a_bits+7)/8)*AVI->audio[track]->a_chans;
  if (s==0) 
    s=1; /* avoid possible zero divisions */
  return s;
}

static int avi_add_index_entry(demux_avi_t *this, avi_t *AVI, unsigned char *tag, 
			       long flags, long pos, long len)
{
  void *ptr;

  if(AVI->n_idx>=AVI->max_idx) {
    ptr = realloc((void *)AVI->idx,(AVI->max_idx+4096)*16);
    if(ptr == 0) {
      this->AVI_errno = AVI_ERR_NO_MEM;
      return -1;
    }
    AVI->max_idx += 4096;
    AVI->idx = (unsigned char((*)[16]) ) ptr;
  }
  
  /* Add index entry */

  memcpy(AVI->idx[AVI->n_idx],tag,4);
  long2str(AVI->idx[AVI->n_idx]+ 4,flags);
  long2str(AVI->idx[AVI->n_idx]+ 8,pos);
  long2str(AVI->idx[AVI->n_idx]+12,len);

  /* Update counter */

  AVI->n_idx++;

  return 0;
}

static void gen_index_show_progress (demux_avi_t *this, int percent) {

  char str[60];

  sprintf (str, "recons. index %3d%%", percent);

  this->xine->osd_renderer->filled_rect (this->xine->osd, 0, 0, 299, 99, 0);
  this->xine->osd_renderer->render_text (this->xine->osd, 5, 30, str, OSD_TEXT1);
  this->xine->osd_renderer->show (this->xine->osd, 0);
  

}

static avi_t *AVI_init(demux_avi_t *this)  {

  avi_t *AVI;
  long i, n, idx_type;
  unsigned char *hdrl_data;
  long hdrl_len=0;
  long nvi, nai[MAX_AUDIO_STREAMS], ioff;
  long tot;
  int lasttag = 0;
  int vids_strh_seen = 0;
  int vids_strf_seen = 0;
  int auds_strh_seen = 0;
  int auds_strf_seen = 0;
  int num_stream = 0;
  char data[256];
  off_t file_length;

  /* Create avi_t structure */

  AVI = (avi_t *) xine_xmalloc(sizeof(avi_t));
  if(AVI==NULL) {
    this->AVI_errno = AVI_ERR_NO_MEM;
    return 0;
  }
  memset((void *)AVI,0,sizeof(avi_t));

  /* Read first 12 bytes and check that this is an AVI file */

  this->input->seek(this->input, 0, SEEK_SET);
  if( this->input->read(this->input, data,12) != 12 ) ERR_EXIT(AVI_ERR_READ) ;
						
  if( strncasecmp(data  ,"RIFF",4) !=0 ||
      strncasecmp(data+8,"AVI ",4) !=0 ) 
    ERR_EXIT(AVI_ERR_NO_AVI) ;
  /* Go through the AVI file and extract the header list,
     the start position of the 'movi' list and an optionally
     present idx1 tag */
  
  hdrl_data = NULL;
  
  while(1) {

    if (this->input->read(this->input, data,8) != 8 ) 
      break; /* We assume it's EOF */
    
    n = str2ulong(data+4);
    n = PAD_EVEN(n);
    
    if(strncasecmp(data,"LIST",4) == 0) {
      if( this->input->read(this->input, data,4) != 4 ) ERR_EXIT(AVI_ERR_READ);
      n -= 4;

      if(strncasecmp(data,"hdrl",4) == 0) {

	hdrl_len = n;
	hdrl_data = (unsigned char *) xine_xmalloc(n);
	if(hdrl_data==0) 
	  ERR_EXIT(AVI_ERR_NO_MEM);
	if (this->input->read(this->input, hdrl_data,n) != n ) 
	  ERR_EXIT(AVI_ERR_READ);

      } else if(strncasecmp(data,"movi",4) == 0)  {

	AVI->movi_start = this->input->seek(this->input, 0,SEEK_CUR);
	this->input->seek(this->input, n, SEEK_CUR);
      }	else
	  this->input->seek(this->input, n, SEEK_CUR);

    } else if(strncasecmp(data,"idx1",4) == 0 || 
              strncasecmp(data,"iddx",4) == 0) {

      /* n must be a multiple of 16, but the reading does not
	 break if this is not the case */
      
      AVI->n_idx = AVI->max_idx = n/16;
      AVI->idx = (unsigned  char((*)[16]) ) xine_xmalloc(n);
      if (AVI->idx==0) 
	ERR_EXIT(AVI_ERR_NO_MEM);

      if (this->input->read(this->input, (char *)AVI->idx, n) != n ) 
      {
        xine_log (this->xine, XINE_LOG_FORMAT, _("demux_avi: avi index is broken\n"));
	free (AVI->idx);	/* Index is broken, reconstruct */
	AVI->idx = NULL;
	AVI->n_idx = AVI->max_idx = 0;
	break; /* EOF */
      }

    } else
      this->input->seek(this->input, n, SEEK_CUR);

  }
  
  if(!hdrl_data) ERR_EXIT(AVI_ERR_NO_HDRL) ;
  if(!AVI->movi_start) ERR_EXIT(AVI_ERR_NO_MOVI) ;

  /* Interpret the header list */
  
  for (i=0;i<hdrl_len;) {
    /* List tags are completly ignored */
      
    if (strncasecmp(hdrl_data+i,"LIST",4)==0) { 
      i+= 12; 
      continue; 
    }
    
    n = str2ulong(hdrl_data+i+4);
    n = PAD_EVEN(n);
    
    /* Interpret the tag and its args */
    
    if(strncasecmp(hdrl_data+i,"strh",4)==0) {

      i += 8;
      if(strncasecmp(hdrl_data+i,"vids",4) == 0 && !vids_strh_seen) {

	memcpy(AVI->compressor,hdrl_data+i+4,4);
	AVI->compressor[4] = 0;
	AVI->dwScale = str2ulong(hdrl_data+i+20);
	AVI->dwRate  = str2ulong(hdrl_data+i+24);
	
	if(AVI->dwScale!=0)
	  AVI->fps = (double)AVI->dwRate/(double)AVI->dwScale;

	this->video_step = (long) (90000.0 / AVI->fps);
	
	AVI->video_frames    = str2ulong(hdrl_data+i+32);
	AVI->video_strn = num_stream;
	vids_strh_seen = 1;
	lasttag = 1; /* vids */
      } else if (strncasecmp (hdrl_data+i,"auds",4) ==0 /* && ! auds_strh_seen*/) {
	if(AVI->n_audio < MAX_AUDIO_STREAMS) {
	  avi_audio_t *a = (avi_audio_t *) xine_xmalloc(sizeof(avi_audio_t));
	  if(a==NULL) {
	    this->AVI_errno = AVI_ERR_NO_MEM;
	    return 0;
	  }
	  memset((void *)a,0,sizeof(avi_audio_t));
	  AVI->audio[AVI->n_audio] = a;

	  a->audio_bytes   = str2ulong(hdrl_data+i+32)*avi_sampsize(AVI, AVI->n_audio);
	  a->audio_strn    = num_stream;
	  a->dwScale_audio = str2ulong(hdrl_data+i+20);
	  a->dwRate_audio  = str2ulong(hdrl_data+i+24);
	  a->dwSampleSize  = str2ulong(hdrl_data+i+44);
	  auds_strh_seen = 1;
	  lasttag = 2; /* auds */
	  AVI->n_audio++;
	}
      } else
	lasttag = 0;
      num_stream++;
    } else if(strncasecmp(hdrl_data+i,"strf",4)==0) {
      i += 8;
      if(lasttag == 1) {
	/* printf ("size : %d\n",sizeof(AVI->bih)); */
	memcpy (&AVI->bih, hdrl_data+i, sizeof(AVI->bih));
	/* stream_read(demuxer->stream,(char*) &avi_header.bih,MIN(size2,sizeof(avi_header.bih))); */
	AVI->width  = str2ulong(hdrl_data+i+4);
	AVI->height = str2ulong(hdrl_data+i+8);
	
	/*
	  printf ("size : %d x %d (%d x %d)\n", AVI->width, AVI->height, AVI->bih.biWidth, AVI->bih.biHeight);
	  printf("  biCompression %d='%.4s'\n", AVI->bih.biCompression, 
	  &AVI->bih.biCompression);
	*/
	vids_strf_seen = 1;

      } else if(lasttag == 2) {

	AVI->audio[AVI->n_audio-1]->wavex=malloc(n);
	memcpy(AVI->audio[AVI->n_audio-1]->wavex, hdrl_data+i, n);
	AVI->audio[AVI->n_audio-1]->wavex_len=n;
	AVI->audio[AVI->n_audio-1]->a_fmt   = str2ushort(hdrl_data+i  );
	AVI->audio[AVI->n_audio-1]->a_chans = str2ushort(hdrl_data+i+2);
	AVI->audio[AVI->n_audio-1]->a_rate  = str2ulong (hdrl_data+i+4);
	AVI->audio[AVI->n_audio-1]->a_bits  = str2ushort(hdrl_data+i+14);
	
	auds_strf_seen = 1;
      }
      lasttag = 0;
    } else {
      i += 8;
      lasttag = 0;
    }
    
    i += n;
  }
  
  if( hdrl_data )
    free( hdrl_data );
  hdrl_data = NULL;
      
  /* somehow ffmpeg doesn't specify the number of frames here */
  /* if (!vids_strh_seen || !vids_strf_seen || AVI->video_frames==0) { */
  if (!vids_strh_seen || !vids_strf_seen) 
    ERR_EXIT(AVI_ERR_NO_VIDS); 

  
  AVI->video_tag[0] = AVI->video_strn/10 + '0';
  AVI->video_tag[1] = AVI->video_strn%10 + '0';
  AVI->video_tag[2] = 'd';
  AVI->video_tag[3] = 'b';


  for(i = 0; i < AVI->n_audio; i++) {
    /* Audio tag is set to "99wb" if no audio present */
    if(!AVI->audio[i]->a_chans) AVI->audio[i]->audio_strn = 99;

    AVI->audio[i]->audio_tag[0] = AVI->audio[i]->audio_strn/10 + '0';
    AVI->audio[i]->audio_tag[1] = AVI->audio[i]->audio_strn%10 + '0';
    AVI->audio[i]->audio_tag[2] = 'w';
    AVI->audio[i]->audio_tag[3] = 'b';
  }

  this->input->seek(this->input, AVI->movi_start, SEEK_SET);

  /* if the file has an idx1, check if this is relative
     to the start of the file or to the start of the movi list */

  idx_type = 0;

  if(AVI->idx) {
    long pos, len;

    /* Search the first videoframe in the idx1 and look where
       it is in the file */
    
    for(i=0;i<AVI->n_idx;i++)
      if( strncasecmp(AVI->idx[i],AVI->video_tag,3)==0 ) break;
    if (i>=AVI->n_idx) {
      ERR_EXIT(AVI_ERR_NO_VIDS);
    }
    
    pos = str2ulong(AVI->idx[i]+ 8);
    len = str2ulong(AVI->idx[i]+12);
    
    this->input->seek(this->input, pos, SEEK_SET);
    if(this->input->read(this->input, data, 8)!=8) ERR_EXIT(AVI_ERR_READ) ;
    
    if( strncasecmp(data,AVI->idx[i],4)==0 && str2ulong(data+4)==len ) {
      idx_type = 1; /* Index from start of file */
      
    } else {
      
      this->input->seek(this->input, pos+AVI->movi_start-4, SEEK_SET);
      if(this->input->read(this->input, data, 8)!=8) 
	ERR_EXIT(AVI_ERR_READ) ;
      if( strncasecmp(data,AVI->idx[i],4)==0 && str2ulong(data+4)==len ) {
	idx_type = 2; /* Index from start of movi list */
      }
    }
    /* idx_type remains 0 if neither of the two tests above succeeds */
  }
  
  if(idx_type == 0) {
    /* we must search through the file to get the index */
    
    this->input->seek(this->input, AVI->movi_start, SEEK_SET);
    
    AVI->n_idx = 0;
    i=0;
    
    printf ("demux_avi: reconstructing index"); fflush (stdout);
    
    gen_index_show_progress (this, 0);
    file_length = this->input->get_length (this->input);

    while(1) {
      if( this->input->read(this->input, data,8) != 8 ) 
	break;
      n = str2ulong(data+4);
      
      i++;
      if (i>1000) {
	off_t pos;

	pos = this->input->get_current_pos (this->input);
	gen_index_show_progress (this, 100*pos/file_length);

	printf (".");
	i = 0; fflush (stdout);
      }
      
      /* The movi list may contain sub-lists, ignore them */
      
      if(strncasecmp(data,"LIST",4)==0) {
	this->input->seek(this->input, 4,SEEK_CUR);
	continue;
      }
      
      /* Check if we got a tag ##db, ##dc or ##wb */
      
      if( ( (data[2]=='d' || data[2]=='D') &&
	    (data[3]=='b' || data[3]=='B' || data[3]=='c' || data[3]=='C') )
	  || ( (data[2]=='w' || data[2]=='W') &&
	       (data[3]=='b' || data[3]=='B') ) ) {
	avi_add_index_entry(this, AVI, data, AVIIF_KEYFRAME, this->input->seek(this->input, 0, SEEK_CUR)-8, n);
      }
      
      this->input->seek(this->input, PAD_EVEN(n), SEEK_CUR);

    }
    printf ("\ndemux_avi: index recostruction done.\n");
    this->xine->osd_renderer->hide (this->xine->osd, 0);
    idx_type = 1;
  }

  /* Now generate the video index and audio index arrays */
  
  nvi = 0;
  memset((void *)nai,0,sizeof(long) * MAX_AUDIO_STREAMS);

  for(i=0;i<AVI->n_idx;i++) {
    if(strncasecmp(AVI->idx[i],AVI->video_tag,3) == 0) nvi++;
    for(n = 0; n < AVI->n_audio; n++)
      if(strncasecmp(AVI->idx[i],AVI->audio[n]->audio_tag,4) == 0) nai[n]++;
  }

  AVI->video_frames = nvi;
   for(n = 0; n < AVI->n_audio; n++)
    AVI->audio[n]->audio_chunks = nai[n];

  if (AVI->video_frames==0) {
    ERR_EXIT(AVI_ERR_NO_VIDS) ;
  }
    

  AVI->video_index = (video_index_entry_t *) xine_xmalloc(nvi*sizeof(video_index_entry_t));
  if(AVI->video_index==0) ERR_EXIT(AVI_ERR_NO_MEM) ;

  for(n = 0; n < AVI->n_audio; n++)
    if(AVI->audio[n]->audio_chunks) {
      AVI->audio[n]->audio_index = (audio_index_entry_t *) xine_xmalloc(nai[n] * sizeof(audio_index_entry_t));
      if(AVI->audio[n]->audio_index==0) ERR_EXIT(AVI_ERR_NO_MEM) ;
    }
  
  nvi = 0;
  memset((void *)nai,0,sizeof(long) * MAX_AUDIO_STREAMS);
  tot = 0;
  ioff = idx_type == 1 ? 8 : AVI->movi_start+4;

  for(i=0;i<AVI->n_idx;i++) {
    if(strncasecmp(AVI->idx[i],AVI->video_tag,3) == 0)	{
      AVI->video_index[nvi].pos   = str2ulong(AVI->idx[i]+ 8)+ioff;
      AVI->video_index[nvi].len   = str2ulong(AVI->idx[i]+12);
      AVI->video_index[nvi].flags = str2ulong(AVI->idx[i]+ 4);
      nvi++;
    }
    for(n = 0; n < AVI->n_audio; n++)
      if(strncasecmp(AVI->idx[i],AVI->audio[n]->audio_tag,4) == 0) {
	AVI->audio[n]->audio_index[nai[n]].pos = str2ulong(AVI->idx[i]+ 8)+ioff;
	AVI->audio[n]->audio_index[nai[n]].len = str2ulong(AVI->idx[i]+12);
	AVI->audio[n]->audio_index[nai[n]].tot = tot;
	AVI->audio[n]->audio_bytes += AVI->audio[n]->audio_index[nai[n]].len;
	tot += AVI->audio[n]->audio_index[nai[n]].len;
	nai[n]++;
      }
  }


  /* Reposition the file */

  this->input->seek(this->input, AVI->movi_start, SEEK_SET);
  AVI->video_posf = 0;
  AVI->video_posb = 0;

  return AVI;
}

static void AVI_seek_start(avi_t *AVI)
{
  int i;

  AVI->video_posf = 0;
  AVI->video_posb = 0;

  for(i = 0; i < AVI->n_audio; i++) {
    AVI->audio[i]->audio_posc = 0;
    AVI->audio[i]->audio_posb = 0;
  }
}

static long AVI_read_audio(demux_avi_t *this, avi_audio_t *AVI_A, char *audbuf, 
			   long bytes, int *buf_flags) {

  long nr, pos, left, todo;

  if(!AVI_A->audio_index)  { 
    this->AVI_errno = AVI_ERR_NO_IDX;   return -1; 
  }

  nr = 0; /* total number of bytes read */

  /* printf ("avi audio package len: %d\n", AVI_A->audio_index[AVI_A->audio_posc].len); */


  while(bytes>0) {
    left = AVI_A->audio_index[AVI_A->audio_posc].len - AVI_A->audio_posb;
    if(left==0) {
      AVI_A->audio_posc++;
      AVI_A->audio_posb = 0;
      if (nr>0) {
	*buf_flags = BUF_FLAG_FRAME_END;
	return nr;
      }
      left = AVI_A->audio_index[AVI_A->audio_posc].len - AVI_A->audio_posb;
    }
    if(bytes<left)
      todo = bytes;
    else
      todo = left;
    pos = AVI_A->audio_index[AVI_A->audio_posc].pos + AVI_A->audio_posb;
    /* printf ("demux_avi: read audio from %d\n", pos); */
    if (this->input->seek (this->input, pos, SEEK_SET)<0)
      return -1;
    if (this->input->read(this->input, audbuf+nr,todo) != todo) {
      this->AVI_errno = AVI_ERR_READ;
      *buf_flags = 0;
      return -1;
    }
    bytes -= todo;
    nr    += todo;
    AVI_A->audio_posb += todo;
  }

  left = AVI_A->audio_index[AVI_A->audio_posc].len - AVI_A->audio_posb;
  if (left==0)
    *buf_flags = BUF_FLAG_FRAME_END;
  else
    *buf_flags = 0;

  return nr;
}

static long AVI_read_video(demux_avi_t *this, avi_t *AVI, char *vidbuf, 
			   long bytes, int *buf_flags) {

  long nr, pos, left, todo;

  if (!AVI->video_index) { 
    this->AVI_errno = AVI_ERR_NO_IDX;   
    return -1; 
  }

  nr = 0; /* total number of bytes read */

  while(bytes>0) {
    
    left = AVI->video_index[AVI->video_posf].len - AVI->video_posb;
    
    if(left==0) {
      AVI->video_posf++;
      AVI->video_posb = 0;
      if (nr>0) {
	*buf_flags = BUF_FLAG_FRAME_END;
	return nr;
      }
      left = AVI->video_index[AVI->video_posf].len - AVI->video_posb;
    }
    if(bytes<left)
      todo = bytes;
    else
      todo = left;
    pos = AVI->video_index[AVI->video_posf].pos + AVI->video_posb;
    /* printf ("demux_avi: read video from %d\n", pos); */
    if (this->input->seek (this->input, pos, SEEK_SET)<0) 
      return -1;
    if (this->input->read(this->input, vidbuf+nr,todo) != todo) {
      this->AVI_errno = AVI_ERR_READ;
      *buf_flags = 0;
      return -1;
    }
    bytes -= todo;
    nr    += todo;
    AVI->video_posb += todo;
  }

  left = AVI->video_index[AVI->video_posf].len - AVI->video_posb;
  if (left==0)
    *buf_flags = BUF_FLAG_FRAME_END;
  else
    *buf_flags = 0;
	 
  return nr;
}

static int64_t get_audio_pts (demux_avi_t *this, int track, long posc, long posb) {

  if (this->avi->audio[track]->dwSampleSize==0)
    return posc * (double) this->avi->audio[track]->dwScale_audio / 
      this->avi->audio[track]->dwRate_audio * 90000.0;
  else 
    return (this->avi->audio[track]->audio_index[posc].tot+posb)/ 
      this->avi->audio[track]->dwSampleSize * (double) this->avi->audio[track]->dwScale_audio / 
      this->avi->audio[track]->dwRate_audio * 90000.0;
}

static int64_t get_video_pts (demux_avi_t *this, long pos) {
  return pos * (double) this->avi->dwScale / this->avi->dwRate * 90000.0;
}


static int demux_avi_next (demux_avi_t *this) {

  int            i;
  buf_element_t *buf = NULL;
  int64_t        audio_pts, video_pts;
  int            do_read_video = (this->avi->n_audio == 0);

  if (this->avi->video_frames <= this->avi->video_posf)
    return 0;

  for (i=0; i < this->avi->n_audio; i++)
    if (!this->no_audio && (this->avi->audio[i]->audio_chunks <= this->avi->audio[i]->audio_posc))
      return 0;


  video_pts = get_video_pts (this, this->avi->video_posf);

  for (i=0; i < this->avi->n_audio; i++) {
    avi_audio_t *audio = this->avi->audio[i];

    audio_pts = get_audio_pts (this, i, audio->audio_posc, audio->audio_posb);
    if (!this->no_audio && (audio_pts < video_pts)) {
      buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
      buf->content = buf->mem;

      /* read audio */

      buf->pts    = audio_pts;
      buf->size   = AVI_read_audio (this, audio, buf->mem, 2048, &buf->decoder_flags);

      if (buf->size<0) {
	buf->free_buffer (buf);
	return 0;
      }

      buf->input_pos  = 0;
      buf->input_time = 0;

      buf->type = audio->audio_type | i;
      buf->decoder_info[1] = audio->a_rate; /* audio Rate */
      buf->decoder_info[2] = audio->a_bits; /* audio bits */
      buf->decoder_info[3] = audio->a_chans; /* audio channels */
      
      if(this->audio_fifo) {
	this->audio_fifo->put (this->audio_fifo, buf);
      } else {
	buf->free_buffer (buf);
      }
    } else
      do_read_video = 1;
  }

  if (do_read_video) {

    buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
    buf->content = buf->mem;

    /* read video */

    buf->pts        = video_pts;
    buf->size       = AVI_read_video (this, this->avi, buf->mem, 2048, &buf->decoder_flags);
    buf->type       = this->avi->video_type;
    
    buf->input_time = video_pts / 90000;
    buf->input_pos  = this->input->get_current_pos(this->input);

    if (buf->size<0) {
      buf->free_buffer (buf);
      return 0;
    }

    /*
      printf ("demux_avi: adding buf %d to video fifo, decoder_info[0]: %d\n", 
      buf, buf->decoder_info[0]);
    */

    this->video_fifo->put (this->video_fifo, buf);

    /*
     * send packages to inform & drive text spu decoder
     */

    if (this->have_spu && (buf->decoder_flags & BUF_FLAG_FRAME_END)) {
      buf_element_t *buf;
      buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
      
      buf->decoder_flags = BUF_FLAG_FRAME_END;
      buf->type          = BUF_SPU_TEXT;
      buf->pts           = video_pts;
    
      buf->decoder_info[1] = this->avi->video_posf;
      
      this->video_fifo->put (this->video_fifo, buf);
    }
  }

  if( buf )
    return (buf->size>0);
  else
    return 0;
}

static void *demux_avi_loop (void *this_gen) {

  buf_element_t *buf = NULL;
  demux_avi_t *this = (demux_avi_t *) this_gen;

  this->send_end_buffers = 1;

  while(1) {
    
    pthread_mutex_lock( &this->mutex );
    
    if( this->status != DEMUX_OK)
      break;
    
    if (!demux_avi_next(this))
      this->status = DEMUX_FINISHED;
    
    pthread_mutex_unlock( &this->mutex );
  
  }

  if (this->send_end_buffers) {
    buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
    buf->type            = BUF_CONTROL_END;
    buf->decoder_flags   = BUF_FLAG_END_STREAM;
    this->video_fifo->put (this->video_fifo, buf);
    
    if(this->audio_fifo) {
      buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
      buf->type            = BUF_CONTROL_END;
      buf->decoder_flags   = BUF_FLAG_END_STREAM;
      this->audio_fifo->put (this->audio_fifo, buf);
    }
  }

  printf ("demux_avi: demux loop finished.\n");

  pthread_mutex_unlock( &this->mutex );
  pthread_exit(NULL);

  return NULL;
}

static void demux_avi_stop (demux_plugin_t *this_gen) {
  
  demux_avi_t   *this = (demux_avi_t *) this_gen;
  buf_element_t *buf;
  void *p;
  
  pthread_mutex_lock( &this->mutex );

  if (this->status != DEMUX_OK) {
    printf ("demux_avi: stop...ignored\n");
    pthread_mutex_unlock( &this->mutex );
    return;
  }

  this->send_end_buffers = 0;
  this->status = DEMUX_FINISHED;
  
  pthread_mutex_unlock( &this->mutex );
  pthread_join (this->thread, &p);

  xine_flush_engine(this->xine);
  /*
    AVI_close (this->avi);
    this->avi = NULL;
  */

  buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
  buf->type            = BUF_CONTROL_END;
  buf->decoder_flags   = BUF_FLAG_END_USER; /* user finished */
  this->video_fifo->put (this->video_fifo, buf);

  if(this->audio_fifo) {
    buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
    buf->type            = BUF_CONTROL_END;
    buf->decoder_flags   = BUF_FLAG_END_USER; /* user finished */
    this->audio_fifo->put (this->audio_fifo, buf);
  }
  
}

static void demux_avi_close (demux_plugin_t *this_gen) {
  demux_avi_t *this = (demux_avi_t *) this_gen;

  if (this->avi)  
    AVI_close (this->avi);

  free(this);
}

static int demux_avi_get_status (demux_plugin_t *this_gen) {
  demux_avi_t *this = (demux_avi_t *) this_gen;
  return this->status;
}

static void demux_avi_start (demux_plugin_t *this_gen,
			     fifo_buffer_t *video_fifo, 
			     fifo_buffer_t *audio_fifo,
			     off_t start_pos, int start_time) {
  int i;
  buf_element_t  *buf;
  demux_avi_t    *this = (demux_avi_t *) this_gen;
  uint32_t        video_pts = 0;
  int             err;
  unsigned char  *sub;
  int starting;
  
  pthread_mutex_lock( &this->mutex );

  starting = (this->status != DEMUX_OK);
  this->status = DEMUX_OK;
  
  if( starting ) {
    this->audio_fifo   = audio_fifo;
    this->video_fifo   = video_fifo;
  
    xine_log (this->xine, XINE_LOG_FORMAT, _("demux_avi: video format = %s\n"),
	      this->avi->compressor);
    xine_log (this->xine, XINE_LOG_FORMAT, _("demux_avi: video frame size %ld x %ld\n"),
	      this->avi->width, this->avi->height);
    for(i=0; i < this->avi->n_audio; i++)
      xine_log (this->xine, XINE_LOG_FORMAT, _("demux_avi: audio format[%d] = 0x%lx\n"),
	        i, this->avi->audio[i]->a_fmt);
    this->no_audio = 0;
  
    for(i=0; i < this->avi->n_audio; i++) {
      this->avi->audio[i]->audio_type = formattag_to_buf_audio (this->avi->audio[i]->a_fmt);
   
      if( !this->avi->audio[i]->audio_type ) {
        xine_log (this->xine, XINE_LOG_FORMAT, _("demux_avi: unknown audio type 0x%lx\n"), 
		  this->avi->audio[i]->a_fmt);
        this->no_audio  = 1;
        this->avi->audio[i]->audio_type     = BUF_CONTROL_NOP;
      }
      else
        xine_log (this->xine, XINE_LOG_FORMAT, _("demux_avi: audio type %s (wFormatTag 0x%x)\n"),
		  buf_audio_name(this->avi->audio[i]->audio_type),
		  (int)this->avi->audio[i]->a_fmt);
    }
  }
  
  AVI_seek_start (this->avi);
  
  /*
   * seek to start pos / time
   */

  printf ("demux_avi: start pos is %lld, start time is %d\n", start_pos, start_time);

  /* seek video */
  if (start_pos) {
    while ( (this->avi->video_index[this->avi->video_posf].pos < start_pos)
	    || !(this->avi->video_index[this->avi->video_posf].flags & AVIIF_KEYFRAME) ) {
      this->avi->video_posf++;
      if (this->avi->video_posf>this->avi->video_frames) {
	this->status = DEMUX_FINISHED;

	printf ("demux_avi: video seek to start failed\n");
	break;
      }
    }
    
    video_pts = get_video_pts (this, this->avi->video_posf); 

  } else if (start_time) {

    video_pts = start_time * 90000;

    while ( (get_video_pts (this, this->avi->video_posf) < video_pts)
	    || !(this->avi->video_index[this->avi->video_posf].flags & AVIIF_KEYFRAME) ) {
      this->avi->video_posf++;
      if (this->avi->video_posf>this->avi->video_frames) {
	this->status = DEMUX_FINISHED;

	printf ("demux_avi: video seek to start failed\n");
	break;
      }
    }

    video_pts = get_video_pts (this, this->avi->video_posf);

  }

  /* seek audio */
  if (!this->no_audio && this->status == DEMUX_OK) {
    for(i=0; i < this->avi->n_audio; i++) {
      while (get_audio_pts (this, i, this->avi->audio[i]->audio_posc, 0) < video_pts) {
	this->avi->audio[i]->audio_posc++;
	if (this->avi->audio[i]->audio_posc>this->avi->audio[i]->audio_chunks) {
	  this->status = DEMUX_FINISHED;
	  
	  printf ("demux_avi: audio seek to start failed\n");
	  break;
	}
      }
    }
  }

  /* 
   * send start buffers
   */
  if( starting && this->status == DEMUX_OK ) {
    buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
    buf->type          = BUF_CONTROL_START;
    buf->decoder_flags = 0;
    this->video_fifo->put (this->video_fifo, buf);

    if(this->audio_fifo) {
      buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
      buf->type          = BUF_CONTROL_START;
      buf->decoder_flags = 0;
      this->audio_fifo->put (this->audio_fifo, buf);
    }
  } 
  else {
    xine_flush_engine(this->xine);
  } 

  if( this->status == DEMUX_OK )
  {
    buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
    buf->type          = BUF_CONTROL_NEWPTS;
    buf->disc_off      = video_pts;
    this->video_fifo->put (this->video_fifo, buf);

    if(this->audio_fifo) {
      buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
      buf->type          = BUF_CONTROL_NEWPTS;
      buf->disc_off      = video_pts;
      this->audio_fifo->put (this->audio_fifo, buf);
    }
  }
  
  if( starting && this->status == DEMUX_OK ) {
    buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
    buf->content = buf->mem;
    buf->decoder_flags = BUF_FLAG_HEADER;
    buf->decoder_info[1] = this->video_step;
    memcpy (buf->content, &this->avi->bih, sizeof (this->avi->bih));
    buf->size = sizeof (this->avi->bih);

    this->avi->video_type = fourcc_to_buf_video((void*)&this->avi->bih.biCompression);
    
    if( !this->avi->video_type )
      this->avi->video_type = fourcc_to_buf_video((void*)&this->avi->compressor);
  
    if ( !this->avi->video_type ) {
      xine_log (this->xine, XINE_LOG_FORMAT, _("demux_avi: unknown video codec '%.4s'\n"),
	        (char*)&this->avi->bih.biCompression);
      buf->free_buffer (buf);
 
      this->status = DEMUX_FINISHED;
    }
    else {
      buf->type = this->avi->video_type;
      xine_log (this->xine, XINE_LOG_FORMAT, _("demux_avi: video codec is '%s'\n"), 
	        buf_video_name(buf->type));

      this->video_fifo->put (this->video_fifo, buf);
  
      if(this->audio_fifo) {
        for(i=0; i<this->avi->n_audio; i++) {
          avi_audio_t *a = this->avi->audio[i];

          buf = this->audio_fifo->buffer_pool_alloc (this->audio_fifo);
          buf->content = buf->mem;
          buf->decoder_flags = BUF_FLAG_HEADER;
          memcpy (buf->content, a->wavex, a->wavex_len);
          buf->size = a->wavex_len;
          buf->type = a->audio_type | i;
          buf->decoder_info[0] = 0; /* first package, containing wavex */
          buf->decoder_info[1] = a->a_rate; /* Audio Rate */
          buf->decoder_info[2] = a->a_bits; /* Audio bits */
          buf->decoder_info[3] = a->a_chans; /* Audio bits */
          this->audio_fifo->put (this->audio_fifo, buf);
        }
      }
  
      /* 
       * send external spu file pointer, if present
       */

      if (this->input->get_optional_data (this->input, &sub, INPUT_OPTIONAL_DATA_TEXTSPU0)) {

        buf = this->video_fifo->buffer_pool_alloc (this->video_fifo);
        buf->content = sub;

        buf->type = BUF_SPU_TEXT;
    
        buf->decoder_flags   = BUF_FLAG_HEADER;
        buf->decoder_info[1] = this->avi->width;
        buf->decoder_info[2] = this->avi->height;

        this->video_fifo->put (this->video_fifo, buf);

        this->have_spu = 1;

        printf ("demux_avi: text subtitle file available\n");
  
      } else
        this->have_spu = 0;
 
      if ((err = pthread_create (&this->thread, NULL, demux_avi_loop, this)) != 0) {
        printf ("demux_avi: can't create new thread (%s)\n",
	        strerror(err));
        abort();
      }
    }
  } 
  
  pthread_mutex_unlock( &this->mutex );
  
  if( !starting && this->status != DEMUX_OK ) {
    void *p;
    pthread_join (this->thread, &p);
  }
}


static void demux_avi_seek (demux_plugin_t *this_gen,
			     off_t start_pos, int start_time) {
  demux_avi_t *this = (demux_avi_t *) this_gen;

	demux_avi_start (this_gen, this->video_fifo, this->audio_fifo,
			 start_pos, start_time);
}

static int demux_avi_open(demux_plugin_t *this_gen, 
			  input_plugin_t *input, int stage) {

  demux_avi_t *this = (demux_avi_t *) this_gen;

  switch(stage) {

  case STAGE_BY_CONTENT: {
    if (input->get_blocksize(input))
      return DEMUX_CANNOT_HANDLE;

    if (!(input->get_capabilities(input) & INPUT_CAP_SEEKABLE))
      return DEMUX_CANNOT_HANDLE;

    input->seek(input, 0, SEEK_SET);
    
    this->input = input;

    if (strncmp(this->last_mrl, input->get_mrl (input), 1024)) {
      if (this->avi)
	AVI_close (this->avi);
      this->avi = AVI_init (this);
    }

    if (this->avi) {

      printf ("demux_avi: %ld frames\n", this->avi->video_frames);

      strncpy(this->last_mrl, input->get_mrl (input), 1024);

      return DEMUX_CAN_HANDLE;
    } else 
      printf ("demux_avi: AVI_init failed (AVI_errno: %d)\n", this->AVI_errno);

    return DEMUX_CANNOT_HANDLE;
  }
  break;
  
  case STAGE_BY_EXTENSION: {
    char *ending, *mrl;
    char *m, *valid_ends;

    mrl = input->get_mrl (input);
    
    ending = strrchr(mrl, '.');
    
    if(ending) {
      xine_strdupa(valid_ends, (this->config->register_string(this->config,
							      "mrl.ends_avi", VALID_ENDS,
							      "valid mrls ending for avi demuxer",
							      NULL, NULL, NULL)));
      while((m = xine_strsep(&valid_ends, ",")) != NULL) { 
	
	while(*m == ' ' || *m == '\t') m++;
	
	if(!strcasecmp((ending + 1), m)) {

	  this->input = input;
	  
	  if (strncmp(this->last_mrl, input->get_mrl (input), 1024)) {
	    if (this->avi)
	      AVI_close (this->avi);
	    this->avi = AVI_init (this);
	  }
	  
	  if (this->avi) {
	    strncpy(this->last_mrl, input->get_mrl (input), 1024);
	    return DEMUX_CAN_HANDLE;
	  } else {
	    printf ("demux_avi: AVI_init failed (AVI_errno: %d)\n", 
		    this->AVI_errno);
	    return DEMUX_CANNOT_HANDLE;
	  }
	}
      }
    }
    return DEMUX_CANNOT_HANDLE;
  }
  break;
  
  default:
    return DEMUX_CANNOT_HANDLE;
    break;
  }
  
  return DEMUX_CANNOT_HANDLE;
}

static char *demux_avi_get_id(void) {
  return "AVI";
}

static char *demux_avi_get_mimetypes(void) {
  return "video/msvideo: avi: AVI animation;"
         "video/x-msvideo: avi: AVI animation;";
}

static int demux_avi_get_stream_length (demux_plugin_t *this_gen) {

  demux_avi_t *this = (demux_avi_t *) this_gen;

  if (this->avi) {
    return get_video_pts(this, this->avi->video_frames) / 90000 ;
  } 

  return 0;
}

demux_plugin_t *init_demuxer_plugin(int iface, xine_t *xine) {

  demux_avi_t     *this;

  if (iface != 7) {
    xine_log (xine, XINE_LOG_PLUGIN,
	      _("demux_avi: this plugin doesn't support plugin API version %d.\n"
		"demux_avi: this means there's a version mismatch between xine and this "
		"demux_avi: demuxer plugin.\nInstalling current demuxer plugins should help.\n"),
	      iface);
    return NULL;
  }

  this         = xine_xmalloc (sizeof (demux_avi_t));
  this->config = xine->config;
  this->xine   = xine;

  (void*) this->config->register_string(this->config,
					"mrl.ends_avi", VALID_ENDS,
					"valid mrls ending for avi demuxer",
					NULL, NULL, NULL);    

  this->demux_plugin.interface_version = DEMUXER_PLUGIN_IFACE_VERSION;
  this->demux_plugin.open              = demux_avi_open;
  this->demux_plugin.start             = demux_avi_start;
  this->demux_plugin.seek              = demux_avi_seek;
  this->demux_plugin.stop              = demux_avi_stop;
  this->demux_plugin.close             = demux_avi_close;
  this->demux_plugin.get_status        = demux_avi_get_status;
  this->demux_plugin.get_identifier    = demux_avi_get_id;
  this->demux_plugin.get_stream_length = demux_avi_get_stream_length;
  this->demux_plugin.get_mimetypes     = demux_avi_get_mimetypes;
  
  this->status = DEMUX_FINISHED;
  pthread_mutex_init( &this->mutex, NULL );
    
  return (demux_plugin_t *) this;
}
