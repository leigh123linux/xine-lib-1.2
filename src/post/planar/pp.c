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
 * $Id: pp.c,v 1.1 2003/11/07 18:37:34 miguelfreitas Exp $
 *
 * plugin for ffmpeg libpostprocess
 */

#include "xine_internal.h"
#include "post.h"
#include "xineutils.h"
#include "postprocess.h"
#include <pthread.h>

#define PP_STRING_SIZE 256 /* size of pp mode string (including all options) */

/* plugin class initialization function */
void *pp_init_plugin(xine_t *xine, void *);

typedef struct post_plugin_pp_s post_plugin_pp_t;

/*
 * this is the struct used by "parameters api" 
 */
typedef struct pp_parameters_s {

  int quality;
  char mode[PP_STRING_SIZE];

} pp_parameters_t;

/*
 * description of params struct
 */
START_PARAM_DESCR( pp_parameters_t )
PARAM_ITEM( POST_PARAM_TYPE_INT, quality, NULL, 0, PP_QUALITY_MAX, 0, 
            "postprocessing quality" )
PARAM_ITEM( POST_PARAM_TYPE_CHAR, mode, NULL, 0, 0, 0, 
            "mode string (overwrites all other options except quality)" )
END_PARAM_DESCR( param_descr )


/* plugin structure */
struct post_plugin_pp_s {
  post_plugin_t post;

  /* private data */
  xine_video_port_t *vo_port;
  xine_stream_t     *stream;
  int                frame_width;
  int                frame_height;
  int                frame_format;

  pp_parameters_t params;

  /* libpostproc specific stuff */
  int                pp_flags;
  pp_context_t      *pp_context;
  pp_mode_t         *pp_mode;

  pthread_mutex_t    lock;
};


static int set_parameters (xine_post_t *this_gen, void *param_gen) {
  post_plugin_pp_t *this = (post_plugin_pp_t *)this_gen;
  pp_parameters_t *param = (pp_parameters_t *)param_gen;

  pthread_mutex_lock (&this->lock);

  memcpy( &this->params, param, sizeof(pp_parameters_t) );

  pthread_mutex_unlock (&this->lock);

  return 1;
}

static int get_parameters (xine_post_t *this_gen, void *param_gen) {
  post_plugin_pp_t *this = (post_plugin_pp_t *)this_gen;
  pp_parameters_t *param = (pp_parameters_t *)param_gen;


  memcpy( param, &this->params, sizeof(pp_parameters_t) );

  return 1;
}
 
static xine_post_api_descr_t * get_param_descr (void) {
  return &param_descr;
}

static char * get_help (void) {
  char *help1 = 
         _("FFmpeg libpostprocess plugin.\n"
           "\n"
           "Parameters\n"
           "\n");
   
  char *help2 =
         _("\n"
           "* libpostprocess (C) Michael Niedermayer\n"
         );
  static char *help = NULL;

  if( !help ) {
    char *s;
    
    help = malloc( strlen(help1) + strlen(help2) + strlen(pp_help) + 1);
    strcpy(help, help1);
    strcat(help, pp_help);
    strcat(help, help2);
    
    /* tab is not correctly displayed in xine-ui */
    for( s = help; *s; s++ )
      if( *s == '\t' )
        *s = ' ';
  }
  return help;
}

static xine_post_api_t post_api = {
  set_parameters,
  get_parameters,
  get_param_descr,
  get_help,
};

typedef struct post_pp_out_s post_pp_out_t;
struct post_pp_out_s {
  xine_post_out_t  xine_out;

  post_plugin_pp_t *plugin;
};

/* plugin class functions */
static post_plugin_t *pp_open_plugin(post_class_t *class_gen, int inputs,
					 xine_audio_port_t **audio_target,
					 xine_video_port_t **video_target);
static char          *pp_get_identifier(post_class_t *class_gen);
static char          *pp_get_description(post_class_t *class_gen);
static void           pp_class_dispose(post_class_t *class_gen);

/* plugin instance functions */
static void           pp_dispose(post_plugin_t *this_gen);

/* rewire function */
static int            pp_rewire(xine_post_out_t *output, void *data);

/* replaced video_port functions */
static void           pp_open(xine_video_port_t *port_gen, xine_stream_t *stream);
static vo_frame_t    *pp_get_frame(xine_video_port_t *port_gen, uint32_t width, 
				       uint32_t height, double ratio, 
				       int format, int flags);
static void           pp_close(xine_video_port_t *port_gen, xine_stream_t *stream);

/* replaced vo_frame functions */
static int            pp_draw(vo_frame_t *frame, xine_stream_t *stream);


void *pp_init_plugin(xine_t *xine, void *data)
{
  post_class_t *class = (post_class_t *)malloc(sizeof(post_class_t));

  if (!class)
    return NULL;
  
  class->open_plugin     = pp_open_plugin;
  class->get_identifier  = pp_get_identifier;
  class->get_description = pp_get_description;
  class->dispose         = pp_class_dispose;

  return class;
}


static post_plugin_t *pp_open_plugin(post_class_t *class_gen, int inputs,
					 xine_audio_port_t **audio_target,
					 xine_video_port_t **video_target)
{
  post_plugin_pp_t *this = (post_plugin_pp_t *)malloc(sizeof(post_plugin_pp_t));
  xine_post_in_t            *input = (xine_post_in_t *)malloc(sizeof(xine_post_in_t));
  xine_post_in_t            *input_api = (xine_post_in_t *)malloc(sizeof(xine_post_in_t));
  post_pp_out_t    *output = (post_pp_out_t *)malloc(sizeof(post_pp_out_t));
  post_video_port_t *port;
  uint32_t cpu_caps;
  
  if (!this || !input || !input_api || !output || !video_target || !video_target[0]) {
    free(this);
    free(input);
    free(input_api);
    free(output);
    return NULL;
  }

  this->stream = NULL;

  this->params.quality = 3;
  strcpy(this->params.mode, "de");

  /* Detect what cpu accel we have */
  cpu_caps = xine_mm_accel();
  this->pp_flags = 0;
  if(cpu_caps & MM_ACCEL_X86_MMX)
    this->pp_flags |= PP_CPU_CAPS_MMX;
  if(cpu_caps & MM_ACCEL_X86_MMXEXT)
    this->pp_flags |= PP_CPU_CAPS_MMX2;
  if(cpu_caps & MM_ACCEL_X86_3DNOW)  
    this->pp_flags |= PP_CPU_CAPS_3DNOW;

  this->pp_mode = NULL;
  this->pp_context = NULL;

  pthread_mutex_init (&this->lock, NULL);
  
  port = post_intercept_video_port(&this->post, video_target[0]);
  /* replace with our own get_frame function */
  port->port.open         = pp_open;
  port->port.get_frame    = pp_get_frame;
  port->port.close        = pp_close;
  
  input->name = "video";
  input->type = XINE_POST_DATA_VIDEO;
  input->data = (xine_video_port_t *)&port->port;

  input_api->name = "parameters";
  input_api->type = XINE_POST_DATA_PARAMETERS;
  input_api->data = &post_api;

  output->xine_out.name   = "pped video";
  output->xine_out.type   = XINE_POST_DATA_VIDEO;
  output->xine_out.data   = (xine_video_port_t **)&port->original_port;
  output->xine_out.rewire = pp_rewire;
  output->plugin          = this;
  
  this->post.xine_post.audio_input    = (xine_audio_port_t **)malloc(sizeof(xine_audio_port_t *));
  this->post.xine_post.audio_input[0] = NULL;
  this->post.xine_post.video_input    = (xine_video_port_t **)malloc(sizeof(xine_video_port_t *) * 2);
  this->post.xine_post.video_input[0] = &port->port;
  this->post.xine_post.video_input[1] = NULL;
  
  this->post.input  = xine_list_new();
  this->post.output = xine_list_new();
  
  xine_list_append_content(this->post.input, input);
  xine_list_append_content(this->post.input, input_api);
  xine_list_append_content(this->post.output, output);
  
  this->post.dispose = pp_dispose;
  
  return &this->post;
}

static char *pp_get_identifier(post_class_t *class_gen)
{
  return "pp";
}

static char *pp_get_description(post_class_t *class_gen)
{
  return "plugin for ffmpeg libpostprocess";
}

static void pp_class_dispose(post_class_t *class_gen)
{
  free(class_gen);
}


static void pp_dispose(post_plugin_t *this_gen)
{
  post_plugin_pp_t *this = (post_plugin_pp_t *)this_gen;
  post_pp_out_t *output = (post_pp_out_t *)xine_list_first_content(this->post.output);
  xine_video_port_t *port = *(xine_video_port_t **)output->xine_out.data;

  if (this->stream)
    port->close(port, this->stream);

  if(this->pp_mode) {
    pp_free_mode(this->pp_mode);
    this->pp_mode = NULL;
  }
    
  if(this->pp_context) {
    pp_free_context(this->pp_context);
    this->pp_context = NULL;
  }

  free(this->post.xine_post.audio_input);
  free(this->post.xine_post.video_input);
  free(xine_list_first_content(this->post.input));
  free(xine_list_next_content(this->post.input));
  free(xine_list_first_content(this->post.output));
  xine_list_free(this->post.input);
  xine_list_free(this->post.output);
  free(this);
}


static int pp_rewire(xine_post_out_t *output_gen, void *data)
{
  post_pp_out_t *output = (post_pp_out_t *)output_gen;
  xine_video_port_t *old_port = *(xine_video_port_t **)output_gen->data;
  xine_video_port_t *new_port = (xine_video_port_t *)data;
  
  if (!data)
    return 0;

  if (output->plugin->stream) {
    /* register our stream at the new output port */
    old_port->close(old_port, output->plugin->stream);
    new_port->open(new_port, output->plugin->stream);
  }
  /* reconnect ourselves */
  *(xine_video_port_t **)output_gen->data = new_port;

  return 1;
}

static void pp_open(xine_video_port_t *port_gen, xine_stream_t *stream)
{
  post_video_port_t *port = (post_video_port_t *)port_gen;
  post_plugin_pp_t *this = (post_plugin_pp_t *)port->post;
  this->stream = stream;
  port->original_port->open(port->original_port, stream);
}

static vo_frame_t *pp_get_frame(xine_video_port_t *port_gen, uint32_t width, 
				    uint32_t height, double ratio, 
				    int format, int flags)
{
  post_video_port_t *port = (post_video_port_t *)port_gen;
  vo_frame_t        *frame;

  frame = port->original_port->get_frame(port->original_port,
    width, height, ratio, format, flags);

  if( format == XINE_IMGFMT_YV12 || format == XINE_IMGFMT_YUY2 ) {
    post_intercept_video_frame(frame, port);
    /* replace with our own draw function */
    frame->draw = pp_draw;
    /* decoders should not copy the frames, since they won't be displayed */
    frame->proc_slice = NULL;
    frame->proc_frame = NULL;
  }

  return frame;
}

static void pp_close(xine_video_port_t *port_gen, xine_stream_t *stream)
{
  post_video_port_t *port = (post_video_port_t *)port_gen;
  post_plugin_pp_t *this = (post_plugin_pp_t *)port->post;

  this->stream = NULL;
  
  port->original_port->close(port->original_port, stream);
}


static int pp_draw(vo_frame_t *frame, xine_stream_t *stream)
{
  post_video_port_t *port = (post_video_port_t *)frame->port;
  post_plugin_pp_t *this = (post_plugin_pp_t *)port->post;
  vo_frame_t *out_frame;
  int skip;
  int pp_flags;

  post_restore_video_frame(frame, port);

  if( !frame->bad_frame ) {

    out_frame = port->original_port->get_frame(port->original_port,
      frame->width, frame->height, frame->ratio, frame->format, frame->flags | VO_BOTH_FIELDS);
  
    extra_info_merge(out_frame->extra_info, frame->extra_info);
  
    out_frame->pts = frame->pts;
    out_frame->duration = frame->duration;

    pthread_mutex_lock (&this->lock);

    if( !this->pp_context || 
        this->frame_width != frame->width ||
        this->frame_height != frame->height ||
        this->frame_format != frame->format ) {

      this->frame_width = frame->width;
      this->frame_height = frame->height;
      this->frame_format = frame->format;
      pp_flags = this->pp_flags;

      if( this->frame_format == XINE_IMGFMT_YV12 )
        pp_flags |= PP_FORMAT_420;
      else
        pp_flags |= PP_FORMAT_422;

      if(this->pp_context)
        pp_free_context(this->pp_context);

      this->pp_context = pp_get_context(frame->width, frame->height, pp_flags);

      if(this->pp_mode) {
        pp_free_mode(this->pp_mode);
        this->pp_mode = NULL;
      }
    }

    if(!this->pp_mode)
      this->pp_mode = pp_get_mode_by_name_and_quality(this->params.mode, 
                                                      this->params.quality);

    if(this->pp_mode)
      pp_postprocess(frame->base, frame->pitches, 
                     out_frame->base, out_frame->pitches, 
                     (frame->width+7)&(~7), frame->height,
                     NULL, 0,
                     this->pp_mode, this->pp_context, 
                     0 /*this->av_frame->pict_type*/);

    pthread_mutex_unlock (&this->lock);

    if(this->pp_mode) {
      skip = out_frame->draw(out_frame, stream);
      frame->vpts = out_frame->vpts;
    } else {
      skip = frame->draw(frame, stream);
    }

    out_frame->free(out_frame);

  } else {
    skip = frame->draw(frame, stream);
  }

  
  return skip;
}
