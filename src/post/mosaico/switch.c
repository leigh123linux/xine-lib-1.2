/*
 * Copyright (C) 2000-2018 the xine project
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
 */

/*
 * simple switch video post plugin
 */

#include <pthread.h>

#define LOG_MODULE "switch"
#define LOG_VERBOSE
/*
#define LOG
*/

#include <xine/xine_internal.h>
#include <xine/post.h>

/* FIXME: This plugin needs to handle overlays as well. */


typedef struct switch_parameter_s {
  unsigned int select;
} switch_parameter_t;


START_PARAM_DESCR(switch_parameter_t)
PARAM_ITEM(POST_PARAM_TYPE_INT, select, NULL, 1, INT_MAX, 1,
  "the input source which will be passed through to the output")
END_PARAM_DESCR(switch_param_descr)

typedef struct post_switch_s post_switch_t;

/* plugin structure */
struct post_switch_s {
  post_plugin_t    post;

  int64_t          vpts_limit;
  pthread_cond_t   display_condition_changed;
  int64_t          skip_vpts;
  int              skip;
  pthread_mutex_t  mutex;
  unsigned int     source_count;
  unsigned int     selected_source;
};

/* parameter functions */

static xine_post_api_descr_t *switch_get_param_descr(void)
{
  return &switch_param_descr;
}

static int switch_set_parameters(xine_post_t *this_gen, const void *param_gen)
{
  post_switch_t *this = (post_switch_t *)this_gen;
  const switch_parameter_t *param = (const switch_parameter_t *)param_gen;

  if (param->select > this->source_count) return 0;
  pthread_mutex_lock(&this->mutex);
  this->selected_source = param->select;
  pthread_mutex_unlock(&this->mutex);
  pthread_cond_broadcast(&this->display_condition_changed);
  return 1;
}

static int switch_get_parameters(xine_post_t *this_gen, void *param_gen)
{
  post_switch_t *this = (post_switch_t *)this_gen;
  switch_parameter_t *param = (switch_parameter_t *)param_gen;

  param->select = this->selected_source;
  return 1;
}

static char *switch_get_help(void)
{
  return _("Switch can be used for fast switching between multiple inputs.\n"
           "\n"
           "Parameters\n"
           "  select: the number of the input which will be passed to the output\n");
}

/* replaced vo_frame functions */

static int switch_draw(vo_frame_t *frame, xine_stream_t *stream)
{
  post_video_port_t *port = (post_video_port_t *)frame->port;
  post_switch_t *this = (post_switch_t *)port->post;
  unsigned int source_num;
  int skip;

  for (source_num = 1; source_num <= this->source_count; source_num++)
    if (this->post.xine_post.video_input[source_num-1] == frame->port) break;
  _x_assert(source_num <= this->source_count);

  pthread_mutex_lock(&this->mutex);
  /* the original output will probably never see this frame again */
  _x_post_frame_u_turn(frame, stream);
  while (this->selected_source != source_num &&
      (frame->vpts > this->vpts_limit || !this->vpts_limit))
    /* we are too early */
    pthread_cond_wait(&this->display_condition_changed, &this->mutex);
  if (this->selected_source == source_num) {
    _x_post_frame_copy_down(frame, frame->next);
    skip = frame->next->draw(frame->next, XINE_ANON_STREAM);
    _x_post_frame_copy_up(frame, frame->next);
    this->vpts_limit = frame->vpts + frame->duration;
    if (skip) {
      this->skip      = skip;
      this->skip_vpts = frame->vpts;
    } else
      this->skip      = 0;
    pthread_mutex_unlock(&this->mutex);
    pthread_cond_broadcast(&this->display_condition_changed);
  } else {
    if (this->skip && frame->vpts <= this->skip_vpts)
      skip = this->skip;
    else
      skip = 0;
    pthread_mutex_unlock(&this->mutex);
  }

  return skip;
}

/* plugin instance functions */

static void switch_dispose(post_plugin_t *this_gen)
{
  post_switch_t *this = (post_switch_t *)this_gen;

  if (_x_post_dispose(this_gen)) {
    pthread_cond_destroy(&this->display_condition_changed);
    pthread_mutex_destroy(&this->mutex);
    free(this);
  }
}

static post_plugin_t *switch_open_plugin(post_class_t *class_gen, int inputs,
                                         xine_audio_port_t **audio_target,
                                         xine_video_port_t **video_target)
{
  post_switch_t     *this = calloc(1, sizeof(post_switch_t));
  post_in_t         *input;
  post_out_t        *output;
  post_video_port_t *port;
  int i;

  static const xine_post_api_t post_api = {
    .set_parameters  = switch_set_parameters,
    .get_parameters  = switch_get_parameters,
    .get_param_descr = switch_get_param_descr,
    .get_help        = switch_get_help,
  };
  static const xine_post_in_t params_input = {
    .name = "parameters",
    .type = XINE_POST_DATA_PARAMETERS,
    .data = (void *)&post_api,
  };

  lprintf("switch open\n");

  if (inputs < 2 || !this || !video_target || !video_target[0]) {
    free(this);
    return NULL;
  }

  (void)class_gen;
  (void)audio_target;

  _x_post_init(&this->post, 0, inputs);

  this->source_count    = inputs;
  this->selected_source = 1;

  pthread_cond_init(&this->display_condition_changed, NULL);
  pthread_mutex_init(&this->mutex, NULL);

  port = _x_post_intercept_video_port(&this->post, video_target[0], &input, &output);
  port->new_frame->draw = switch_draw;
  port->port_lock       = &this->mutex;
  port->frame_lock      = &this->mutex;
  this->post.xine_post.video_input[0] = &port->new_port;

  for (i = 1; i < inputs; i++) {
    port = _x_post_intercept_video_port(&this->post, video_target[0], &input, NULL);
    port->new_frame->draw = switch_draw;
    port->port_lock       = &this->mutex;
    port->frame_lock      = &this->mutex;
    this->post.xine_post.video_input[i] = &port->new_port;
  }

  xine_list_push_back(this->post.input, (void *)&params_input);

  this->post.dispose = switch_dispose;

  return &this->post;
}

static void *switch_init_plugin(xine_t *xine, const void *data)
{
  static const post_class_t post_switch_class = {
    .open_plugin     = switch_open_plugin,
    .identifier      = "switch",
    .description     = N_("Switch is a post plugin able to switch at any time between different streams"),
    .dispose         = NULL,
  };

  (void)xine;
  (void)data;

  return (void *)&post_switch_class;
}

/* plugin catalog information */
static const post_info_t switch_special_info = {
  .type = XINE_POST_TYPE_VIDEO_COMPOSE,
};

const plugin_info_t xine_plugin_info[] EXPORTED = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_POST, 10, "switch", XINE_VERSION_CODE, &switch_special_info, &switch_init_plugin },
  { PLUGIN_NONE, 0, NULL, 0, NULL, NULL }
};
