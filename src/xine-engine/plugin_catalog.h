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
 * $Id: plugin_catalog.h,v 1.21 2006/09/26 05:19:49 dgp85 Exp $
 *
 * xine-internal header: Definitions for plugin lists
 *
 */

#ifndef _PLUGIN_CATALOG_H
#define _PLUGIN_CATALOG_H

#ifdef XINE_COMPILE
#  include "xine_plugin.h"
#  include "xineutils.h"
#else
#  include <xine/xine_plugin.h>
#  include <xine/xineutils.h>
#endif

#define DECODER_MAX 128
#define PLUGIN_MAX  256

/* the engine takes this many plugins for one stream type */
#define PLUGINS_PER_TYPE 10

#define CACHE_CATALOG_VERSION 2
#define CACHE_CATALOG_FILE    ".xine/catalog.cache"
#define CACHE_CATALOG_DIR     ".xine"

typedef struct {
  char            *filename;
  off_t            filesize;
  time_t           filemtime;
  int              ref;          /* count number of classes */
  void            *lib_handle;
  int              no_unload;    /* set if the file can't be unloaded */
} plugin_file_t ;

typedef struct {
  plugin_file_t   *file;
  plugin_info_t   *info;
  void            *plugin_class;
  xine_list_t     *config_entry_list;
  int              ref;          /* count intances of plugins */
  int              priority;
} plugin_node_t ;

struct plugin_catalog_s {
  xine_sarray_t   *plugin_lists[PLUGIN_TYPE_MAX];
  
  xine_sarray_t   *cache_list;
  xine_list_t     *file_list;

  plugin_node_t   *audio_decoder_map[DECODER_MAX][PLUGINS_PER_TYPE];
  plugin_node_t   *video_decoder_map[DECODER_MAX][PLUGINS_PER_TYPE];
  plugin_node_t   *spu_decoder_map[DECODER_MAX][PLUGINS_PER_TYPE];
  
  const char      *ids[PLUGIN_MAX];
  
  /* memory block for the decoder priority config entry descriptions */
  char            *prio_desc[DECODER_MAX];

  pthread_mutex_t  lock;

  int              plugin_count;
  int              decoder_count;
};
typedef struct plugin_catalog_s plugin_catalog_t;

/*
 * load plugins into catalog
 *
 * all input+demux plugins will be fully loaded+initialized
 * decoder plugins are loaded on demand
 * video/audio output plugins have special load/probe functions
 */
void _x_scan_plugins (xine_t *this) XINE_PROTECTED;


/*
 * dispose all currently loaded plugins (shutdown)
 */

void _x_dispose_plugins (xine_t *this) XINE_PROTECTED;

#endif
