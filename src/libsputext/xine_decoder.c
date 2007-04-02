/*
 * Copyright (C) 2000-2004 the xine project
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
 * $Id: xine_decoder.c,v 1.99 2007/02/20 01:04:07 dgp85 Exp $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>

#define LOG_MODULE "libsputext"
#define LOG_VERBOSE
/*
#define LOG
*/

#include "buffer.h"
#include "xine_internal.h"
#include "xineutils.h"
#include "osd.h"

#define SUB_MAX_TEXT  5      /* lines */
#define SUB_BUFSIZE   256    /* chars per line */


typedef enum {
  SUBTITLE_SIZE_TINY = 0,
  SUBTITLE_SIZE_SMALL,
  SUBTITLE_SIZE_NORMAL,
  SUBTITLE_SIZE_LARGE,
  SUBTITLE_SIZE_VERY_LARGE,
  SUBTITLE_SIZE_HUGE,

  SUBTITLE_SIZE_NUM        /* number of values in enum */
} subtitle_size;

#define FONTNAME_SIZE 100

typedef struct sputext_class_s {
  spu_decoder_class_t class;

  subtitle_size      subtitle_size;   /* size of subtitles */
  int                vertical_offset;
  char               font[FONTNAME_SIZE]; /* subtitle font */
#ifdef HAVE_FT2
  char               font_ft[FILENAME_MAX]; /* subtitle font */
  int                use_font_ft;     /* use Freetype */
#endif
  char              *src_encoding;    /* encoding of subtitle file */
  int                use_unscaled;    /* use unscaled OSD if possible */

  xine_t            *xine;

} sputext_class_t;


typedef struct sputext_decoder_s {
  spu_decoder_t      spu_decoder;

  sputext_class_t   *class;
  xine_stream_t     *stream;

  int                ogm;
  int                lines;
  char               text[SUB_MAX_TEXT][SUB_BUFSIZE];

  /* below 3 variables are the same from class. use to detect
   * when something changes.
   */
  subtitle_size      subtitle_size;   /* size of subtitles */
  int                vertical_offset;
  char               font[FILENAME_MAX]; /* subtitle font */
  char              *buf_encoding;    /* encoding of subtitle buffer */

  int                width;          /* frame width                */
  int                height;         /* frame height               */
  int                font_size;
  int                line_height;
  int                started;
  int                finished;

  osd_renderer_t    *renderer;
  osd_object_t      *osd;

  int64_t            img_duration;
  int64_t            last_subtitle_end; /* no new subtitle before this vpts */
  int                unscaled;          /* use unscaled OSD */
  
  int                last_lines;        /* number of lines of the previous subtitle */
} sputext_decoder_t;

static inline char *get_font (sputext_class_t *class)
{
#ifdef HAVE_FT2
  return class->use_font_ft ? class->font_ft : class->font;
#else
  return class->font;
#endif
}

static void update_font_size (sputext_decoder_t *this, int force_update) {
  static int sizes[SUBTITLE_SIZE_NUM] = { 16, 20, 24, 32, 48, 64 };

  int  y;

  if ((this->subtitle_size != this->class->subtitle_size) ||
      (this->vertical_offset != this->class->vertical_offset) ||
      force_update) {
  
    this->subtitle_size = this->class->subtitle_size;
    this->vertical_offset = this->class->vertical_offset;
    this->last_lines = 0;

    this->font_size = sizes[this->class->subtitle_size];

    this->line_height = this->font_size + 10;

    y = this->height - (SUB_MAX_TEXT * this->line_height) - 5;

    if(((y - this->class->vertical_offset) >= 0) && ((y - this->class->vertical_offset) <= this->height))
      y -= this->class->vertical_offset;

    if( this->osd )
      this->renderer->free_object (this->osd);

    lprintf("new osd object, width %d, height %d*%d\n", this->width, SUB_MAX_TEXT, this->line_height);
    this->osd = this->renderer->new_object (this->renderer, 
                                            this->width,
                                            SUB_MAX_TEXT * this->line_height);

    this->renderer->set_font (this->osd, get_font (this->class), this->font_size);
    this->renderer->set_position (this->osd, 0, y);
  }
}

static void update_output_size (sputext_decoder_t *this) {
  int unscaled;

  unscaled = this->class->use_unscaled &&
             (this->stream->video_out->get_capabilities(this->stream->video_out) &
              VO_CAP_UNSCALED_OVERLAY);

  if( unscaled != this->unscaled ) {
    this->unscaled = unscaled;
    this->width = 0; /* force update */
  }

  /* initialize decoder if needed */
  if( this->unscaled ) {
    if( this->width != this->stream->video_out->get_property(this->stream->video_out, 
                                                             VO_PROP_WINDOW_WIDTH) ||
        this->height != this->stream->video_out->get_property(this->stream->video_out, 
                                                             VO_PROP_WINDOW_HEIGHT) ||
        !this->img_duration || !this->osd ) {

      int width = 0, height = 0; /* dummy */
        
      this->stream->video_out->status(this->stream->video_out, NULL,
                                      &width, &height, &this->img_duration );
      if( width && height ) {

        this->width = this->stream->video_out->get_property(this->stream->video_out,
                                                             VO_PROP_WINDOW_WIDTH);
        this->height = this->stream->video_out->get_property(this->stream->video_out,
                                                             VO_PROP_WINDOW_HEIGHT);

        if(!this->osd || (this->width && this->height)) {
          this->renderer = this->stream->osd_renderer;
          
          update_font_size (this, 1);
        }
      }
    }
  } else {
    if( !this->width || !this->height || !this->img_duration || !this->osd ) {
        
      this->width = 0;
      this->height = 0;
      
      this->stream->video_out->status(this->stream->video_out, NULL,
                                      &this->width, &this->height, &this->img_duration );
                                      
      if(!this->osd || ( this->width && this->height)) {
        this->renderer = this->stream->osd_renderer;
        
        update_font_size (this, 1);
      }
    }
  }
}

static int parse_utf8_size(unsigned char *c)
{
  if ( c[0]<0x80 )
      return 1;
  
  if( c[1]==0 )
    return 1;
  if ( (c[0]>=0xC2 && c[0]<=0xDF) && (c[1]>=0x80 && c[1]<=0xBF) )
    return 2;
  
  if( c[2]==0 )
    return 2;	
  else if ( c[0]==0xE0 && (c[1]>=0xA0 && c[1]<=0xBF) && (c[2]>=0x80 && c[1]<=0xBF) )
    return 3;
  else if ( (c[0]>=0xE1 && c[0]<=0xEC) && (c[1]>=0x80 && c[1]<=0xBF) && (c[2]>=0x80 && c[1]<=0xBF) )
    return 3;
  else if ( c[0]==0xED && (c[1]>=0x80 && c[1]<=0x9F) && (c[2]>=0x80 && c[1]<=0xBF) )
    return 3;
  else if ( c[0]==0xEF && (c[1]>=0xA4 && c[1]<=0xBF) && (c[2]>=0x80 && c[1]<=0xBF) )
    return 3;
  else
    return 1;
}

static int ogm_get_width(sputext_decoder_t *this, char* text) {
  int i=0,width=0,w,dummy;
  char letter[5]={0, 0, 0, 0, 0};
  int shift, isutf8 = 0;
  char *encoding = (this->buf_encoding)?this->buf_encoding:
                                        this->class->src_encoding;
  if( strcmp(encoding, "utf-8") == 0 )
    isutf8 = 1;
    
  while (i<=strlen(text)) {
    switch (text[i]) {
    case '<':
      if (!strncmp("<b>", text+i, 3)) {
	/*Do somethink to enable BOLD typeface*/
	i=i+3;
	break;
      } else if (!strncmp("</b>", text+i, 3)) {
	/*Do somethink to disable BOLD typeface*/
	i=i+4;
	break;
      } else if (!strncmp("<i>", text+i, 3)) {	
	/*Do somethink to enable italics typeface*/
	i=i+3;
	break;
      } else if (!strncmp("</i>", text+i, 3)) {
	/*Do somethink to disable italics typeface*/
	i=i+4;
	break;
      } else if (!strncmp("<font>", text+i, 3)) {	
	/*Do somethink to disable typing
	  fixme - no teststreams*/
	i=i+6;
	break;
      } else if (!strncmp("</font>", text+i, 3)) {
	/*Do somethink to enable typing
	  fixme - no teststreams*/
	i=i+7;
	break;
      } 
default:
      if ( isutf8 )
        shift = parse_utf8_size(&text[i]);
      else
        shift = 1;
      memcpy(letter,&text[i],shift);
      letter[shift]=0;
      
      this->renderer->get_text_size(this->osd, letter, &w, &dummy);
      width=width+w;
      i+=shift;
    }
  }

  return width;
}

static void ogm_render_line(sputext_decoder_t *this, int x, int y, char* text) {
  int i=0,w,dummy;
  char letter[5]={0, 0, 0, 0, 0};
  int shift, isutf8 = 0;
  char *encoding = (this->buf_encoding)?this->buf_encoding:
                                        this->class->src_encoding;
  if( strcmp(encoding, "utf-8") == 0 )
    isutf8 = 1;

  while (i<=strlen(text)) {
    switch (text[i]) {
    case '<':
      if (!strncmp("<b>", text+i, 3)) {
	/*Do somethink to enable BOLD typeface*/
	i=i+3;
	break;
      } else if (!strncmp("</b>", text+i, 3)) {
	/*Do somethink to disable BOLD typeface*/
	i=i+4;
	break;
      } else if (!strncmp("<i>", text+i, 3)) {	
	/*Do somethink to enable italics typeface*/
	i=i+3;
	break;
      } else if (!strncmp("</i>", text+i, 3)) {
	/*Do somethink to disable italics typeface*/
	i=i+4;
	break;
      } else if (!strncmp("<font>", text+i, 3)) {	
	/*Do somethink to disable typing
	  fixme - no teststreams*/
	i=i+6;
	break;
      } else if (!strncmp("</font>", text+i, 3)) {
	/*Do somethink to enable typing
	  fixme - no teststreams*/
	i=i+7;
	break;
      } 
    default:
      if ( isutf8 )
        shift = parse_utf8_size(&text[i]);
      else
        shift = 1;
      memcpy(letter,&text[i],shift);
      letter[shift]=0;
      
      this->renderer->render_text(this->osd, x, y, letter, OSD_TEXT1);
      this->renderer->get_text_size(this->osd, letter, &w, &dummy);
      x=x+w;
      i+=shift;
    }
  }
}

static void draw_subtitle(sputext_decoder_t *this, int64_t sub_start, int64_t sub_end ) {
  
  int line, y;
  int font_size;
  char *font;

  _x_assert(this->renderer != NULL);
  if ( ! this->renderer )
    return;

  update_font_size(this, 0);
  
  font = get_font (this->class);
  if( strcmp(this->font, font) ) {
    strncpy(this->font, font, FILENAME_MAX);
    this->font[FILENAME_MAX - 1] = '\0';
    this->renderer->set_font (this->osd, font, this->font_size);
  }

  font_size = this->font_size;
  if (this->buf_encoding)
    this->renderer->set_encoding(this->osd, this->buf_encoding);
  else
    this->renderer->set_encoding(this->osd, this->class->src_encoding);

  for (line = 0; line < this->lines; line++) /* first, check lenghts and word-wrap if needed */
  {
    int w, h;
    if( this->ogm )
      w = ogm_get_width( this, this->text[line]);
    else
      this->renderer->get_text_size( this->osd, this->text[line], &w, &h);
    if( w > this->width ) { /* line is too long */
      int chunks=(int)(w/this->width)+(w%this->width?1:0);
      if( this->lines+chunks <= SUB_MAX_TEXT && chunks>1 ) { /* try adding newlines while keeping existing ones */
        int a;
        xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,"Partial subtitle line splitting in %i chunks\n",chunks);
        for(a=this->lines-1;a>=0;a--) {
          if(a>line) /* lines after the too-long one */
            memcpy(this->text[a+chunks-1],this->text[a],SUB_BUFSIZE);
          else if(a==line) { /* line to be splitted */
            int b,len=strlen(this->text[line]);
            char *p=this->text[line];
            for(b=0;b<chunks;b++) {
              char *c;
              if(b==chunks-1) { /* if we are reading the last chunk, copy it completly */
                strncpy(this->text[line+b],p,SUB_BUFSIZE);
                this->text[line+b][SUB_BUFSIZE - 1] = '\0';
              } else {
                for(c=p+(int)(len/chunks)+(len%chunks?1:0);*c!=' ' && c>p && c!='\0';c--);
                if(*c==' ') {
                  *c='\0';
                  if(b) { /* we are reading something that has to be moved to another line */
                    strncpy(this->text[line+b],p,SUB_BUFSIZE);
                    this->text[line+b][SUB_BUFSIZE - 1] = '\0';
                  }
                  p=c+1;
                }
              }
            }
          }
        }
        this->lines+=chunks-1;
      } else { /* regenerate all the lines to find something that better fits */
        char buf[SUB_BUFSIZE*SUB_MAX_TEXT];
        int a,w,h,chunks;
        buf[0]='\0';
        for(a=0;a<this->lines;a++) {
          if(a) {
            int len=strlen(buf);
            buf[len]=' ';
            buf[len+1]='\0';
          }
          strcat(buf,this->text[a]);
        }
        if( this->ogm )
          w = ogm_get_width( this, buf);
        else
          this->renderer->get_text_size( this->osd, buf, &w, &h);
        chunks=(int)(w/this->width)+(w%this->width?1:0);
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "Complete subtitle line splitting in %i chunks\n",chunks);
        if(chunks<=SUB_MAX_TEXT) {/* if the length is over than SUB_MAX_TEXT*this->width nothing can be done */
          int b,len=strlen(buf);
          char *p=buf;
          for(b=0;b<chunks;b++) {
            char *c;
            if(b==chunks-1) { /* if we are reading the last chunk, copy it completly */
              strncpy(this->text[b],p,SUB_BUFSIZE);
              this->text[b][SUB_BUFSIZE - 1] = '\0';
            } else {
              for(c=p+(int)(len/chunks)+(len%chunks?1:0);*c!=' ' && c>p && c!='\0';c--);
              if(*c==' ') {
                *c='\0';
                strncpy(this->text[b],p,SUB_BUFSIZE);
                this->text[b][SUB_BUFSIZE - 1] = '\0';
                p=c+1;
              }
            }
          }
          this->lines=chunks;
        } else
          xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "Subtitle too long to be splited\n");
        line=this->lines;
      }
    }
  }

  font_size = this->font_size;
  if (this->buf_encoding)
    this->renderer->set_encoding(this->osd, this->buf_encoding);
  else
    this->renderer->set_encoding(this->osd, this->class->src_encoding);

  for (line = 0; line < this->lines; line++) /* first, check lenghts and word-wrap if needed */
  {
    int w, h;
    if( this->ogm )
      w = ogm_get_width( this, this->text[line]);
    else
      this->renderer->get_text_size( this->osd, this->text[line], &w, &h);
    if( w > this->width ) { /* line is too long */
      int chunks=(int)(w/this->width)+(w%this->width?1:0);
      if( this->lines+chunks <= SUB_MAX_TEXT && chunks>1 ) { /* try adding newlines while keeping existing ones */
        int a;
        xprintf(this->stream->xine, XINE_VERBOSITY_DEBUG,"Partial subtitle line splitting in %i chunks\n",chunks);
        for(a=this->lines-1;a>=0;a--) {
          if(a>line) /* lines after the too-long one */
            memcpy(this->text[a+chunks-1],this->text[a],SUB_BUFSIZE);
          else if(a==line) { /* line to be splitted */
            int b,len=strlen(this->text[line]);
            char *p=this->text[line];
            for(b=0;b<chunks;b++) {
              char *c;
              if(b==chunks-1) { /* if we are reading the last chunk, copy it completly */
                strncpy(this->text[line+b],p,SUB_BUFSIZE);
                this->text[line+b][SUB_BUFSIZE - 1] = '\0';
              } else {
                for(c=p+(int)(len/chunks)+(len%chunks?1:0);*c!=' ' && c>p && c!='\0';c--);
                if(*c==' ') {
                  *c='\0';
                  if(b) { /* we are reading something that has to be moved to another line */
                    strncpy(this->text[line+b],p,SUB_BUFSIZE);
                    this->text[line+b][SUB_BUFSIZE - 1] = '\0';
                  }
                  p=c+1;
                }
              }
            }
          }
        }
        this->lines+=chunks-1;
      } else { /* regenerate all the lines to find something that better fits */
        char buf[SUB_BUFSIZE*SUB_MAX_TEXT];
        int a,w,h,chunks;
        buf[0]='\0';
        for(a=0;a<this->lines;a++) {
          if(a) {
            int len=strlen(buf);
            buf[len]=' ';
            buf[len+1]='\0';
          }
          strcat(buf,this->text[a]);
        }
        if( this->ogm )
          w = ogm_get_width( this, buf);
        else
          this->renderer->get_text_size( this->osd, buf, &w, &h);
        chunks=(int)(w/this->width)+(w%this->width?1:0);
        xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "Complete subtitle line splitting in %i chunks\n",chunks);
        if(chunks<=SUB_MAX_TEXT) {/* if the length is over than SUB_MAX_TEXT*this->width nothing can be done */
          int b,len=strlen(buf);
          char *p=buf;
          for(b=0;b<chunks;b++) {
            char *c;
            if(b==chunks-1) { /* if we are reading the last chunk, copy it completly */
              strncpy(this->text[b],p,SUB_BUFSIZE);
              this->text[b][SUB_BUFSIZE - 1] = '\0';
            } else {
              for(c=p+(int)(len/chunks)+(len%chunks?1:0);*c!=' ' && c>p && c!='\0';c--);
              if(*c==' ') {
                *c='\0';
                strncpy(this->text[b],p,SUB_BUFSIZE);
                this->text[b][SUB_BUFSIZE - 1] = '\0';
                p=c+1;
              }
            }
          }
          this->lines=chunks;
        } else
          xprintf (this->stream->xine, XINE_VERBOSITY_DEBUG, "Subtitle too long to be splited\n");
        line=this->lines;
      }
    }
  }

  if (this->last_lines)
    this->renderer->filled_rect (this->osd, 0, this->line_height * (SUB_MAX_TEXT - this->last_lines),
                                 this->width - 1, this->line_height * SUB_MAX_TEXT - 1, 0);
  this->last_lines = this->lines;
  y = (SUB_MAX_TEXT - this->lines) * this->line_height;

  for (line = 0; line < this->lines; line++) {
    int w, h, x;
          
    while(1) {
      if( this->ogm )
        w = ogm_get_width( this, this->text[line]);
      else
        this->renderer->get_text_size( this->osd, this->text[line], 
                                       &w, &h);
      x = (this->width - w) / 2;
            
      if( w > this->width && font_size > 16 ) {
        font_size -= 4;
        this->renderer->set_font (this->osd, get_font (this->class), font_size);
      } else {
        break;
      }
    }
    
    if( this->ogm ) {
      ogm_render_line(this, x, y + line*this->line_height, this->text[line]);
    } else  {
      this->renderer->render_text (this->osd, x, y + line * this->line_height,
                                   this->text[line], OSD_TEXT1);
    }
  }
         
  if( font_size != this->font_size )
    this->renderer->set_font (this->osd, get_font (this->class), this->font_size);
  
  if( this->last_subtitle_end && sub_start < this->last_subtitle_end ) {
    sub_start = this->last_subtitle_end;
  }
  this->last_subtitle_end = sub_end;
          
  this->renderer->set_text_palette (this->osd, -1, OSD_TEXT1);
  
  if (this->unscaled)
    this->renderer->show_unscaled (this->osd, sub_start);
  else
    this->renderer->show (this->osd, sub_start);
  
  this->renderer->hide (this->osd, sub_end);
  
  lprintf ("scheduling subtitle >%s< at %"PRId64" until %"PRId64", current time is %"PRId64"\n",
	   this->text[0], sub_start, sub_end, 
	   this->stream->xine->clock->get_current_time (this->stream->xine->clock));
}


static void spudec_decode_data (spu_decoder_t *this_gen, buf_element_t *buf) {

  sputext_decoder_t *this = (sputext_decoder_t *) this_gen;
  int uses_time;
  int32_t start, end, diff;
  int64_t start_vpts, end_vpts;
  int64_t spu_offset;
  int i;
  uint32_t *val;
  char *str;
  extra_info_t extra_info;
  int master_status, slave_status;
  int vo_discard;

  /* filter unwanted streams */
  if (buf->decoder_flags & BUF_FLAG_HEADER) {
    return;
  }
  if (buf->decoder_flags & BUF_FLAG_PREVIEW)
    return;
  
  if ((this->stream->spu_channel & 0x1f) != (buf->type & 0x1f))
    return;

  if ( (buf->decoder_flags & BUF_FLAG_SPECIAL) &&
       (buf->decoder_info[1] == BUF_SPECIAL_CHARSET_ENCODING) )
    this->buf_encoding = buf->decoder_info_ptr[2];
  else
    this->buf_encoding = NULL;
    
  if( (buf->type & 0xFFFF0000) == BUF_SPU_OGM ) {

    this->ogm = 1;
    uses_time = 1;
    val = (uint32_t * )buf->content;
    start = *val++;
    end = *val++;
    str = (char *)val;

    if (!*str) return;
    /* Empty ogm packets (as created by ogmmux) clears out old messages. We already respect the end time. */
  
    this->lines = 0;
  
    i = 0;
    while (*str && (this->lines < SUB_MAX_TEXT) && (i < SUB_BUFSIZE)) {
      if (*str == '\r' || *str == '\n') {
        if (i) {
          this->text[ this->lines ][i] = 0;
          this->lines++;
          i = 0;
        }
      } else {
        this->text[ this->lines ][i] = *str;
        if (i < SUB_BUFSIZE-1)
          i++;
      }
      str++;
    }
    if (i == SUB_BUFSIZE)
      i--;
    
    if (i) {
      this->text[ this->lines ][i] = 0;
      this->lines++;
    }

  } else {

    this->ogm = 0;
    val = (uint32_t * )buf->content;
    
    this->lines = *val++;
    uses_time = *val++;
    start = *val++;
    end = *val++;
    str = (char *)val;
    for (i = 0; i < this->lines; i++, str += strlen(str) + 1) {
      strncpy( this->text[i], str, SUB_BUFSIZE - 1);
      this->text[i][SUB_BUFSIZE - 1] = '\0';
    }

  }
  
  xprintf(this->class->xine, XINE_VERBOSITY_DEBUG,
          "libsputext: decoder data [%s]\n", this->text[0]);
  xprintf(this->class->xine, XINE_VERBOSITY_DEBUG,
          "libsputext: mode %d timing %d->%d\n", uses_time, start, end);

  if( end <= start ) {
    xprintf(this->class->xine, XINE_VERBOSITY_DEBUG,
            "libsputext: discarding subtitle with invalid timing\n");
    return;
  }
  
  spu_offset = this->stream->master->metronom->get_option (this->stream->master->metronom,
                                                           METRONOM_SPU_OFFSET);
  if( uses_time ) {
    start += (spu_offset / 90);
    end += (spu_offset / 90);
  } else {
    if( this->osd && this->img_duration ) {
      start += spu_offset / this->img_duration;
      end += spu_offset / this->img_duration;
    }
  }
   
  while( !this->finished ) {
 
    master_status = xine_get_status (this->stream->master);
    slave_status = xine_get_status (this->stream);
    vo_discard = this->stream->video_out->get_property(this->stream->video_out, 
                                                       VO_PROP_DISCARD_FRAMES);

    _x_get_current_info (this->stream->master, &extra_info, sizeof(extra_info) );
   
    lprintf("master: %d slave: %d input_normpos: %d vo_discard: %d\n", 
      master_status, slave_status, extra_info.input_normpos, vo_discard);

    if( !this->started && (master_status == XINE_STATUS_PLAY &&
                           slave_status == XINE_STATUS_PLAY &&
                           extra_info.input_normpos) ) {
      lprintf("started\n");

      this->width = this->height = 0;
      this->started = 1;

      update_output_size( this );      
    }

    if( this->started ) {

      if( master_status != XINE_STATUS_PLAY || 
          slave_status != XINE_STATUS_PLAY ||
          vo_discard ) {
        lprintf("finished\n");
  
        this->width = this->height = 0;
        this->finished = 1;
        return;
      }

      if( this->osd ) {
        
        /* try to use frame number mode */
        if( !uses_time && extra_info.frame_number ) {
          
          diff = end - extra_info.frame_number;
          
          /* discard old subtitles */
          if( diff < 0 ) {
            xprintf(this->class->xine, XINE_VERBOSITY_DEBUG, 
                    "libsputext: discarding old subtitle\n");
            return;
          }
            
          diff = start - extra_info.frame_number;
          
          start_vpts = extra_info.vpts + diff * this->img_duration;
          end_vpts = start_vpts + (end-start) * this->img_duration;
          
        } else {
          
          if( !uses_time ) {
            start = start * this->img_duration / 90;
            end = end * this->img_duration / 90;
            uses_time = 1;
          }
          
          diff = end - extra_info.input_time;
          
          /* discard old subtitles */
          if( diff < 0 ) {
            xprintf(this->class->xine, XINE_VERBOSITY_DEBUG,
                    "libsputext: discarding old subtitle\n");
            return;
          }
            
          diff = start - extra_info.input_time;
          
          start_vpts = extra_info.vpts + diff * 90;
          end_vpts = start_vpts + (end-start) * 90;
        }
        
        _x_spu_decoder_sleep(this->stream, start_vpts);
        update_output_size( this );
        draw_subtitle(this, start_vpts, end_vpts);
        
        return;  
      }
    }

    if (_x_spu_decoder_sleep(this->stream, 0))
      xine_usec_sleep (50000);
    else
      return;
  }
}  


static void spudec_reset (spu_decoder_t *this_gen) {
  sputext_decoder_t *this = (sputext_decoder_t *) this_gen;
  
  lprintf("i guess we just seeked\n");
  this->width = this->height = 0;
  this->started = this->finished = 0;
  this->last_subtitle_end = 0;
}

static void spudec_discontinuity (spu_decoder_t *this_gen) {
  /* sputext_decoder_t *this = (sputext_decoder_t *) this_gen; */

}

static void spudec_dispose (spu_decoder_t *this_gen) {
  sputext_decoder_t *this = (sputext_decoder_t *) this_gen;
  
  if (this->osd) {
    this->renderer->free_object (this->osd);
    this->osd = NULL;
  }
  free(this);
}

static void update_vertical_offset(void *class_gen, xine_cfg_entry_t *entry)
{
  sputext_class_t *class = (sputext_class_t *)class_gen;

  class->vertical_offset = entry->num_value;
}

static void update_osd_font(void *class_gen, xine_cfg_entry_t *entry)
{
  sputext_class_t *class = (sputext_class_t *)class_gen;

  strncpy(class->font, entry->str_value, FONTNAME_SIZE);
  class->font[FONTNAME_SIZE - 1] = '\0';
  
  xprintf(class->xine, XINE_VERBOSITY_DEBUG, "libsputext: spu_font = %s\n", class->font );
}

#ifdef HAVE_FT2
static void update_osd_font_ft(void *class_gen, xine_cfg_entry_t *entry)
{
  sputext_class_t *class = (sputext_class_t *)class_gen;

  strncpy(class->font_ft, entry->str_value, FILENAME_MAX);
  class->font_ft[FILENAME_MAX - 1] = '\0';
  
  xprintf(class->xine, XINE_VERBOSITY_DEBUG, "libsputext: spu_font_ft = %s\n", class->font_ft);
}

static void update_osd_use_font_ft(void *class_gen, xine_cfg_entry_t *entry)
{
  sputext_class_t *class = (sputext_class_t *)class_gen;

  class->use_font_ft = entry->num_value;
  
  xprintf(class->xine, XINE_VERBOSITY_DEBUG, "libsputext: spu_use_font_ft = %d\n", class->use_font_ft);
}
#endif

static void update_subtitle_size(void *class_gen, xine_cfg_entry_t *entry)
{
  sputext_class_t *class = (sputext_class_t *)class_gen;

  class->subtitle_size = entry->num_value;
}

static void update_use_unscaled(void *class_gen, xine_cfg_entry_t *entry)
{
  sputext_class_t *class = (sputext_class_t *)class_gen;

  class->use_unscaled = entry->num_value;
}

static spu_decoder_t *sputext_class_open_plugin (spu_decoder_class_t *class_gen, xine_stream_t *stream) {

  sputext_class_t *class = (sputext_class_t *)class_gen;
  sputext_decoder_t *this ;

  this = (sputext_decoder_t *) xine_xmalloc (sizeof (sputext_decoder_t));

  this->spu_decoder.decode_data         = spudec_decode_data;
  this->spu_decoder.reset               = spudec_reset;
  this->spu_decoder.discontinuity       = spudec_discontinuity;
  this->spu_decoder.get_interact_info   = NULL;
  this->spu_decoder.set_button          = NULL;
  this->spu_decoder.dispose             = spudec_dispose;

  this->class  = class;
  this->stream = stream;

  return (spu_decoder_t *) this;
}

static void sputext_class_dispose (spu_decoder_class_t *class_gen) {
  sputext_class_t *this = (sputext_class_t *)class_gen;

  this->xine->config->unregister_callback(this->xine->config,
					  "subtitles.separate.src_encoding");
  this->xine->config->unregister_callback(this->xine->config,
					  "subtitles.separate.subtitle_size");
  this->xine->config->unregister_callback(this->xine->config,
					  "subtitles.separate.vertical_offset");
  this->xine->config->unregister_callback(this->xine->config,
					  "subtitles.separate.use_unscaled_osd");
  free (this);
}

static char *sputext_class_get_identifier (spu_decoder_class_t *this) {
  return "sputext";
}

static char *sputext_class_get_description (spu_decoder_class_t *this) {
  return "external subtitle decoder plugin";
}

static void update_src_encoding(void *class_gen, xine_cfg_entry_t *entry)
{
  sputext_class_t *class = (sputext_class_t *)class_gen;

  class->src_encoding = entry->str_value;
  xprintf(class->xine, XINE_VERBOSITY_DEBUG, "libsputext: spu_src_encoding = %s\n", class->src_encoding );
}

static void *init_spu_decoder_plugin (xine_t *xine, void *data) {

  static const char *subtitle_size_strings[] = { 
    "tiny", "small", "normal", "large", "very large", "huge", NULL 
  };
  sputext_class_t *this ;

  lprintf("init class\n");
  
  this = (sputext_class_t *) xine_xmalloc (sizeof (sputext_class_t));

  this->class.open_plugin      = sputext_class_open_plugin;
  this->class.get_identifier   = sputext_class_get_identifier;
  this->class.get_description  = sputext_class_get_description;
  this->class.dispose          = sputext_class_dispose;

  this->xine                   = xine;

  this->subtitle_size  = xine->config->register_enum(xine->config, 
			      "subtitles.separate.subtitle_size", 
			       1,
			       subtitle_size_strings,
			       _("subtitle size"),
			       _("You can adjust the subtitle size here. The setting will "
			         "be evaluated relative to the window size."),
			       0, update_subtitle_size, this);
  this->vertical_offset  = xine->config->register_num(xine->config,
			      "subtitles.separate.vertical_offset", 
			      0,
			      _("subtitle vertical offset"),
			      _("You can adjust the vertical position of the subtitle. "
			        "The setting will be evaluated relative to the window size."),
			      0, update_vertical_offset, this);
  strncpy(this->font, xine->config->register_string(xine->config,
				"subtitles.separate.font",
				"sans",
				_("font for subtitles"),
				_("A font from the xine font directory to be used for the "
				  "subtitle text."),
				10, update_osd_font, this), FONTNAME_SIZE);
  this->font[FONTNAME_SIZE - 1] = '\0';
#ifdef HAVE_FT2
  strncpy(this->font_ft, xine->config->register_filename(xine->config,
				"subtitles.separate.font_freetype",
				"", XINE_CONFIG_STRING_IS_FILENAME,
				_("font for subtitles"),
				_("An outline font file (e.g. a .ttf) to be used for the subtitle text."),
				10, update_osd_font_ft, this), FILENAME_MAX);
  this->font_ft[FILENAME_MAX - 1] = '\0';
  this->use_font_ft = xine->config->register_bool(xine->config,
				"subtitles.separate.font_use_freetype",
				0,
				_("whether to use a freetype font"),
				NULL,
				10, update_osd_use_font_ft, this);
#endif
  this->src_encoding  = xine->config->register_string(xine->config, 
				"subtitles.separate.src_encoding", 
				xine_guess_spu_encoding(),
				_("encoding of the subtitles"),
				_("The encoding of the subtitle text in the stream. This setting "
				  "is used to render non-ASCII characters correctly. If non-ASCII "
				  "characters are not displayed as you expect, ask the "
				  "creator of the subtitles what encoding was used."),
				10, update_src_encoding, this);
  this->use_unscaled  = xine->config->register_bool(xine->config,
			      "subtitles.separate.use_unscaled_osd",
			       1,
			       _("use unscaled OSD if possible"),
			       _("The unscaled OSD will be rendered independently of the video "
				 "frame and will always be sharp, even if the video is magnified. "
				 "This will look better, but does not work with all graphics "
				 "hardware. The alternative is the scaled OSD, which will become "
				 "blurry, if you enlarge a low resolution video to fullscreen, but "
				 "it works with all graphics cards."),
			       10, update_use_unscaled, this);

  return &this->class;
}


/* plugin catalog information */
static uint32_t supported_types[] = { BUF_SPU_TEXT, BUF_SPU_OGM, 0 };

static const decoder_info_t spudec_info = {
  supported_types,     /* supported types */
  1                    /* priority        */
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */  
  { PLUGIN_SPU_DECODER | PLUGIN_MUST_PRELOAD, 16, "sputext", XINE_VERSION_CODE, &spudec_info, &init_spu_decoder_plugin },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};
