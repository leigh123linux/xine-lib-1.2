/*
 * Copyright (C) 2000-2006 the xine project
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
 * $Id: load_plugins.c,v 1.230 2007/02/19 23:33:33 dgp85 Exp $
 *
 *
 * Load input/demux/audio_out/video_out/codec plugins
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#include <dlfcn.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>

#define LOG_MODULE "load_plugins"
#define LOG_VERBOSE

/*
#define LOG
*/

#define XINE_ENABLE_EXPERIMENTAL_FEATURES 1
#define XINE_ENGINE_INTERNAL
#include "xine_internal.h"
#include "xine_plugin.h"
#include "plugin_catalog.h"
#include "demuxers/demux.h"
#include "input/input_plugin.h"
#include "video_out.h"
#include "post.h"
#include "metronom.h"
#include "configfile.h"
#include "xineutils.h"
#include "compat.h"

#if 0

static char *plugin_name;

#if DONT_CATCH_SIGSEGV

#define install_segv_handler()
#define remove_segv_handler()

#else

void (*old_segv_handler)(int);

static void segv_handler (int hubba) {
  printf ("\nload_plugins: Initialization of plugin '%s' failed (segmentation fault).\n",plugin_name);
  printf ("load_plugins: You probably need to remove the offending file.\n");
  printf ("load_plugins: (This error is usually due an incorrect plugin version)\n");
  _x_abort();
}

static void install_segv_handler(void){
  old_segv_handler = signal (SIGSEGV, segv_handler);
}

static void remove_segv_handler(void){
  signal (SIGSEGV, old_segv_handler );
}

#endif
#endif /* 0 */


static const int plugin_iface_versions[] = {
  INPUT_PLUGIN_IFACE_VERSION,
  DEMUXER_PLUGIN_IFACE_VERSION,
  AUDIO_DECODER_IFACE_VERSION,
  VIDEO_DECODER_IFACE_VERSION,
  SPU_DECODER_IFACE_VERSION,
  AUDIO_OUT_IFACE_VERSION,
  VIDEO_OUT_DRIVER_IFACE_VERSION,
  POST_PLUGIN_IFACE_VERSION 
};

static void _build_list_typed_plugins(plugin_catalog_t **catalog, xine_sarray_t *type) {
  plugin_node_t    *node;
  int               list_id, list_size;
  int               i, j;

  list_size = xine_sarray_size (type);
  for (list_id = 0, i = 0; list_id < list_size; list_id++) {
    node = xine_sarray_get(type, list_id);
    
    /* add only unique ids to the list */
    for ( j = 0; j < i; j++ ) {
      if( !strcmp((*catalog)->ids[j], node->info->id) )
        break;
    }
    if ( j == i )
      (*catalog)->ids[i++] = node->info->id;
  }
  (*catalog)->ids[i] = NULL;
}

static void inc_file_ref(plugin_file_t *file) {
  _x_assert(file);
  file->ref++;
}

static void dec_file_ref(plugin_file_t *file) {
  _x_assert(file);

  file->ref--;
  lprintf("file %s referenced %d time(s)\n", file->filename, file->ref);
}

static void inc_node_ref(plugin_node_t *node) {
  _x_assert(node);
  node->ref++;
}

static void dec_node_ref(plugin_node_t *node) {
  _x_assert(node);

  node->ref--;
  lprintf("node %s referenced %d time(s)\n", node->info->id, node->ref);
}

/*
 * plugin list/catalog management functions
 *
 */

static int get_decoder_priority(xine_t *this, plugin_node_t *node) {
  cfg_entry_t *entry;
  char key[80];

  snprintf(key, sizeof(key), "engine.decoder_priorities.%s", node->info->id);
  
  entry = this->config->lookup_entry(this->config, key);
  
  if (entry && entry->num_value)
    /* user given priorities should definitely override defaults, so multiply them */
    return entry->num_value * 100;
  else
    return ((decoder_info_t *)node->info->special_info)->priority;
}

static void map_decoder_list (xine_t *this,
			      xine_sarray_t *decoder_list,
			      plugin_node_t *decoder_map[DECODER_MAX][PLUGINS_PER_TYPE]) {
  int i;
  int list_id, list_size;

  /* init */
  for (i = 0; i < DECODER_MAX; i++) {
    decoder_map[i][0] = NULL;
  }

  /*
   * map decoders
   */
  list_size = xine_sarray_size(decoder_list);
  for (list_id = 0; list_id < list_size; list_id++) {

    plugin_node_t *node = xine_sarray_get(decoder_list, list_id);
    const uint32_t *type = ((decoder_info_t *)node->info->special_info)->supported_types;
    int priority = get_decoder_priority(this, node);

    lprintf ("mapping decoder %s\n", node->info->id);

    while (type && (*type)) {

      int pos;
      int streamtype = ((*type)>>16) & 0xFF;

      lprintf ("load_plugins: decoder handles stream type %02x, priority %d\n", streamtype, priority);

      /* find the right place based on the priority */
      for (pos = 0; pos < PLUGINS_PER_TYPE; pos++)
	if (!decoder_map[streamtype][pos] ||
	    priority > get_decoder_priority(this, decoder_map[streamtype][pos]))
	  break;

      if ( pos == PLUGINS_PER_TYPE ) {
	xine_log (this, XINE_LOG_PLUGIN,
		  _("map_decoder_list: no space for decoder, skipped.\n"));
	type++;
	continue;
      }

      /* shift the decoder list for this type by one to make room for new decoder */
      for (i = PLUGINS_PER_TYPE - 1; i > pos; i--)
        decoder_map[streamtype][i] = decoder_map[streamtype][i - 1];

      /* insert new decoder */
      decoder_map[streamtype][pos] = node;

      lprintf("decoder inserted in decoder map at %d\n", pos);

      type++;
    }
  }
}

static void map_decoders (xine_t *this) {

  plugin_catalog_t *catalog = this->plugin_catalog;

  lprintf ("map_decoders\n");

  /*
   * map audio decoders
   */
  map_decoder_list(this, catalog->plugin_lists[PLUGIN_AUDIO_DECODER - 1], catalog->audio_decoder_map);
  map_decoder_list(this, catalog->plugin_lists[PLUGIN_VIDEO_DECODER - 1], catalog->video_decoder_map);
  map_decoder_list(this, catalog->plugin_lists[PLUGIN_SPU_DECODER - 1], catalog->spu_decoder_map);

}

/* Decoder priority callback */
static void _decoder_priority_cb(void *data, xine_cfg_entry_t *cfg) {
  /* sort decoders by priority */
  map_decoders((xine_t *)data);
}

static plugin_info_t *_get_cached_info (xine_t *this,
					char *filename, off_t filesize, time_t filemtime,
					plugin_info_t *previous_info) {
  xine_sarray_t *list = this->plugin_catalog->cache_list;
  int            list_id, list_size;

  list_size = xine_sarray_size (list);
  for (list_id = 0; list_id < list_size; list_id++) {
    plugin_node_t *node = xine_sarray_get (list, list_id);
    if( !previous_info &&
	node->file->filesize == filesize &&
	node->file->filemtime == filemtime &&
	!strcmp( node->file->filename, filename )) {
      
      return node->info;
    }
    
    /* skip previously returned items */
    if( node->info == previous_info )
      previous_info = NULL;
    
  }
  return NULL;
}


static plugin_file_t *_insert_file (xine_t *this,
				    xine_list_t *list,
				    char *filename, struct stat *statbuffer,
				    void *lib) {
  plugin_file_t *entry;

  /* create the file entry */
  entry = malloc(sizeof(plugin_file_t));
  entry->filename  = strdup(filename);
  entry->filesize  = statbuffer->st_size;
  entry->filemtime = statbuffer->st_mtime;
  entry->lib_handle = lib;
  entry->ref = 0;
  entry->no_unload = 0;

  xine_list_push_back (list, entry);
  return entry;
}


static void _insert_node (xine_t *this,
			  xine_sarray_t *list,
			  plugin_file_t *file,
			  plugin_info_t *info,
			  int api_version){

  plugin_catalog_t     *catalog = this->plugin_catalog;
  plugin_node_t        *entry;
  vo_info_t            *vo_new;
  const vo_info_t      *vo_old;
  ao_info_t            *ao_new;
  const ao_info_t      *ao_old;
  decoder_info_t       *decoder_new;
  const decoder_info_t *decoder_old;
  post_info_t          *post_new;
  const post_info_t    *post_old;
  demuxer_info_t       *demux_new;
  const demuxer_info_t *demux_old;
  input_info_t         *input_new;
  const input_info_t   *input_old;
  uint32_t             *types;
  char                  key[80];
  char                  desc[100];
  int                   i;

  _x_assert(list);
  _x_assert(info);
  if (info->API != api_version) {
    xprintf(this, XINE_VERBOSITY_LOG, 
	    _("load_plugins: ignoring plugin %s, wrong iface version %d (should be %d)\n"),
	    info->id, info->API, api_version);
    return;
  }

  entry = xine_xmalloc(sizeof(plugin_node_t));
  entry->info         = xine_xmalloc(sizeof(plugin_info_t));
  *(entry->info)      = *info;
  entry->info->id     = strdup(info->id);
  entry->info->init   = info->init;
  entry->plugin_class = NULL;
  entry->file         = file;
  entry->ref          = 0;
  entry->priority     = 0; /* default priority */

  switch (info->type & PLUGIN_TYPE_MASK){

  case PLUGIN_VIDEO_OUT:
    vo_old = info->special_info;
    vo_new = xine_xmalloc(sizeof(vo_info_t));
    entry->priority = vo_new->priority = vo_old->priority;
    vo_new->visual_type = vo_old->visual_type;
    entry->info->special_info = vo_new;
    break;

  case PLUGIN_AUDIO_OUT:
    ao_old = info->special_info;
    ao_new = xine_xmalloc(sizeof(ao_info_t));
    entry->priority = ao_new->priority = ao_old->priority;
    entry->info->special_info = ao_new;
    break;

  case PLUGIN_AUDIO_DECODER:
  case PLUGIN_VIDEO_DECODER:
  case PLUGIN_SPU_DECODER:
    decoder_old = info->special_info;
    decoder_new = xine_xmalloc(sizeof(decoder_info_t));
    if (decoder_old == NULL) {
      if (file)
	xprintf (this, XINE_VERBOSITY_DEBUG,
		 "load_plugins: plugin %s from %s is broken: special_info = NULL\n",
		 info->id, entry->file->filename);
      else
	xprintf (this, XINE_VERBOSITY_DEBUG,
		 "load_plugins: statically linked plugin %s is broken: special_info = NULL\n",
		 info->id);
      _x_abort();
    }
    for (i=0; decoder_old->supported_types[i] != 0; ++i);
    types = xine_xmalloc((i+1)*sizeof(uint32_t));
    for (i=0; decoder_old->supported_types[i] != 0; ++i){
      types[i] = decoder_old->supported_types[i];
    }
    decoder_new->supported_types = types;
    entry->priority = decoder_new->priority = decoder_old->priority;
    
    snprintf(key, sizeof(key), "engine.decoder_priorities.%s", info->id);
    snprintf(desc, sizeof(desc), _("priority for %s decoder"), info->id);
    /* write the description on the heap because the config system
     * does not strdup() it, so we have to provide a different pointer
     * for each decoder */
    for (i = 0; catalog->prio_desc[i]; i++);
    catalog->prio_desc[i] = malloc(strlen(desc) + 1);
    strcpy(catalog->prio_desc[i], desc);
    this->config->register_num (this->config,
				key,
				0,
				catalog->prio_desc[i],
				_("The priority provides a ranking in case some media "
				  "can be handled by more than one decoder.\n"
				  "A priority of 0 enables the decoder's default priority."), 20,
				_decoder_priority_cb, (void *)this);

    /* reset priority on old config files */
    if (this->config->current_version < 1)
      this->config->update_num(this->config, key, 0);

    entry->info->special_info = decoder_new;
    break;
  
  case PLUGIN_POST:
    post_old = info->special_info;
    post_new = xine_xmalloc(sizeof(post_info_t));
    post_new->type = post_old->type;
    entry->info->special_info = post_new;
    break;
    
  case PLUGIN_DEMUX:
    demux_old = info->special_info;
    demux_new = xine_xmalloc(sizeof(demuxer_info_t));
    
    if (demux_old) {
      entry->priority = demux_new->priority = demux_old->priority;
      lprintf("demux: %s, priority: %d\n", info->id, entry->priority);
    } else {
      xprintf(this, XINE_VERBOSITY_LOG, 
              _("load_plugins: demuxer plugin %s does not provide a priority,"
                " xine-lib will use the default priority.\n"),
              info->id);
      entry->priority = demux_new->priority = 0;
    }
    entry->info->special_info = demux_new;
    break;

  case PLUGIN_INPUT:
    input_old = info->special_info;
    input_new = xine_xmalloc(sizeof(input_info_t));
    
    if (input_old) {
      entry->priority = input_new->priority = input_old->priority;
      lprintf("input: %s, priority: %d\n", info->id, entry->priority);
    } else {
      xprintf(this, XINE_VERBOSITY_LOG, 
              _("load_plugins: input plugin %s does not provide a priority,"
                " xine-lib will use the default priority.\n"),
              info->id);
      entry->priority = input_new->priority = 0;
    }
    entry->info->special_info = input_new;
    break;
  }

  if (file && (info->type & PLUGIN_NO_UNLOAD)) {
    file->no_unload = 1;
  }

  xine_sarray_add(list, entry);
}


static int _plugin_node_comparator(void *a, void *b) {
  plugin_node_t *node_a = (plugin_node_t *)a;
  plugin_node_t *node_b = (plugin_node_t *)b;

  if (node_a->priority > node_b->priority) {
    return -1;
  } else if (node_a->priority == node_b->priority) {
    return 0;
  } else {
    return 1;
  }
}

static plugin_catalog_t *_new_catalog(void){

  plugin_catalog_t *catalog;
  int i;

  catalog = xine_xmalloc(sizeof(plugin_catalog_t));

  for (i = 0; i < PLUGIN_TYPE_MAX; i++) {
    catalog->plugin_lists[i] = xine_sarray_new(0, _plugin_node_comparator);
  }

  catalog->cache_list = xine_sarray_new(0, _plugin_node_comparator);
  catalog->file_list  = xine_list_new();
  pthread_mutex_init (&catalog->lock, NULL);

  return catalog;
}

static void _register_plugins_internal(xine_t *this, plugin_file_t *file, plugin_info_t *info) {
  _x_assert(this);
  _x_assert(info);

  while ( info && info->type != PLUGIN_NONE ) {

    if (file)
      xine_log (this, XINE_LOG_PLUGIN,
		_("load_plugins: plugin %s found\n"), file->filename);
    else
      xine_log (this, XINE_LOG_PLUGIN,
		_("load_plugins: static plugin found\n"));
      
    if (this->plugin_catalog->plugin_count >= PLUGIN_MAX ||
	(this->plugin_catalog->decoder_count >= DECODER_MAX &&
	 info->type >= PLUGIN_AUDIO_DECODER && info->type <= PLUGIN_SPU_DECODER)) {
      if (file)
	xine_log (this, XINE_LOG_PLUGIN,
		  _("load_plugins: plugin limit reached, %s could not be loaded\n"), file->filename);
      else
	xine_log (this, XINE_LOG_PLUGIN,
		  _("load_plugins: plugin limit reached, static plugin could not be loaded\n"));
    } else {
      int plugin_type = info->type & PLUGIN_TYPE_MASK;

      if ((plugin_type > 0) && (plugin_type <= PLUGIN_TYPE_MAX)) {
	_insert_node (this, this->plugin_catalog->plugin_lists[plugin_type - 1], file, info,
		      plugin_iface_versions[plugin_type - 1]);

	if ((plugin_type == PLUGIN_AUDIO_DECODER) ||
	    (plugin_type == PLUGIN_VIDEO_DECODER) ||
	    (plugin_type == PLUGIN_SPU_DECODER)) {
	  this->plugin_catalog->decoder_count++;
	}
      } else {

	if (file)
	  xine_log (this, XINE_LOG_PLUGIN,
		    _("load_plugins: unknown plugin type %d in %s\n"),
		    info->type, file->filename);
	else
	  xine_log (this, XINE_LOG_PLUGIN,
		    _("load_plugins: unknown statically linked plugin type %d\n"), info->type);
      }
      this->plugin_catalog->plugin_count++;
    }
    
    /* get next info */
    if( file && !file->lib_handle ) {
      lprintf("get cached info\n");
      info = _get_cached_info (this, file->filename, file->filesize, file->filemtime, info);
    } else {
      info++;
    }
  }
}

void xine_register_plugins(xine_t *self, plugin_info_t *info) {
  _register_plugins_internal(self, NULL, info);
}

/*
 * First stage plugin loader (catalog builder)
 *
 ***************************************************************************/

static void collect_plugins(xine_t *this, char *path){

  DIR *dir;

  lprintf ("collect_plugins in %s\n", path);

  dir = opendir(path);
  if (dir) {
    struct dirent *pEntry;
    size_t path_len, str_size;
    char *str = NULL;

    path_len = strlen(path);
    str_size = path_len * 2 + 2; /* +2 for '/' and '\0' */
    str = malloc(str_size);
    xine_fast_memcpy(str, path, path_len);
    str[path_len] = '/';
    str[path_len + 1] = '\0';

    while ((pEntry = readdir (dir)) != NULL) {
      size_t new_str_size, d_len;
      void *lib = NULL;
      plugin_info_t *info = NULL;
      
      struct stat statbuffer;

      d_len = strlen(pEntry->d_name);
      new_str_size = path_len + d_len + 2;
      if (str_size < new_str_size) {
	str_size = new_str_size + new_str_size / 2;
	str = realloc(str, str_size);
      }

      xine_fast_memcpy(&str[path_len + 1], pEntry->d_name, d_len + 1);

      if (stat(str, &statbuffer)) {
	xine_log (this, XINE_LOG_PLUGIN, _("load_plugins: unable to stat %s\n"), str);
      } else {

	switch (statbuffer.st_mode & S_IFMT){

	case S_IFREG:
	  /* regular file, ie. plugin library, found => load it */

	  /* this will fail whereever shared libs are called *.dll or such
	   * better solutions:
           * a) don't install .la files on user's system
           * b) also cache negative hits, ie. files that failed to dlopen()
	   */
#if defined(__hpux)
	  if(!strstr(str, ".sl")
#elif defined(__CYGWIN__) || defined(WIN32)
	  if(!strstr(str, ".dll")
#else
	  if(!strstr(str, ".so")
#endif
#ifdef HOST_OS_DARWIN
             && !strcasestr(str, ".xineplugin")
#endif
            )
	    break;

	  lib = NULL;

	  /* get the first plugin_info_t */
	     info = _get_cached_info (this, str, statbuffer.st_size, statbuffer.st_mtime, NULL);
#ifdef LOG
	  if( info )
	    printf("load_plugins: using cached %s\n", str);
	  else
	    printf("load_plugins: %s not cached\n", str);
#endif

	  if(!info && (lib = dlopen (str, RTLD_LAZY | RTLD_GLOBAL)) == NULL) {
	    const char *error = dlerror();
	    /* too noisy -- but good to catch unresolved references */
	    xprintf(this, XINE_VERBOSITY_LOG, 
		    _("load_plugins: cannot open plugin lib %s:\n%s\n"), str, error);

	  } else {

	    if (info || (info = dlsym(lib, "xine_plugin_info"))) {
	      plugin_file_t *file;

	      file = _insert_file(this, this->plugin_catalog->file_list, str, &statbuffer, lib);

	      _register_plugins_internal(this, file, info);
	    }
	    else {
	      const char *error = dlerror();

	      xine_log (this, XINE_LOG_PLUGIN,
			_("load_plugins: can't get plugin info from %s:\n%s\n"), str, error);
	    }
	  }
	  break;
	case S_IFDIR:

	  /* unless ".", "..", ".hidden" or vidix driver dirs */
	  if (*pEntry->d_name != '.' && strcmp(pEntry->d_name, "vidix")) {
	    collect_plugins(this, str);
	  }
	} /* switch */
      } /* if (stat(...)) */
    } /* while */
    free(str);
    closedir (dir);
  } /* if (dir) */
  else {
    xine_log (this, XINE_LOG_PLUGIN,
	      _("load_plugins: skipping unreadable plugin directory %s.\n"),
	      path);
  }
} /* collect_plugins */

/*
 * generic 2nd stage plugin loader
 */

static inline int _plugin_info_equal(const plugin_info_t *a, 
                                     const plugin_info_t *b) {
  if (a->type != b->type || 
      a->API != b->API ||
      strcasecmp(a->id, b->id) ||
      a->version != b->version)
    return 0;
   
  switch (a->type & PLUGIN_TYPE_MASK) {
    case PLUGIN_VIDEO_OUT:
      /* FIXME: Could special_info be NULL? */
      if (a->special_info && b->special_info) {
        return (((vo_info_t*)a->special_info)->visual_type ==
                ((vo_info_t*)b->special_info)->visual_type);
      }
      break;
      
    default:
      break;
  }
  
  return 1;
}

static int _load_plugin_class(xine_t *this,
			      plugin_node_t *node,
			      void *data) {
  if (node->file) {
    char *filename = node->file->filename;
    plugin_info_t *target = node->info;
    void *lib;
    plugin_info_t *info;

    /* load the dynamic library if needed */
    if (!node->file->lib_handle) {
      lprintf("dlopen %s\n", filename);
      if ((lib = dlopen (filename, RTLD_LAZY | RTLD_GLOBAL)) == NULL) {
	const char *error = dlerror();

	xine_log (this, XINE_LOG_PLUGIN,
		  _("load_plugins: cannot (stage 2) open plugin lib %s:\n%s\n"), filename, error);
	return 0;
      } else {
	node->file->lib_handle = lib;
      }
    } else {
      lprintf("%s already loaded\n", filename);
    }

    if ((info = dlsym(node->file->lib_handle, "xine_plugin_info"))) {
      /* TODO: use sigsegv handler */
      while (info->type != PLUGIN_NONE){
	if (_plugin_info_equal(info, target)){
	  if ((node->plugin_class = info->init(this, data))) {
	    inc_file_ref(node->file);
	    return 1;
	  } else {
	    return 0;
	  }
	}
	info++;
      }
      lprintf("plugin not found\n");
    
    } else {
      xine_log (this, XINE_LOG_PLUGIN,
		_("load_plugins: Yikes! %s doesn't contain plugin info.\n"), filename);
    }
  } else {
    /* statically linked plugin */
    lprintf("statically linked plugin\n");
    if (node->info->init) {
      node->plugin_class = node->info->init(this, data);
      return 1;
    }
  }
  return 0; /* something failed if we came here... */
}

static void _dispose_plugin_class(plugin_node_t *node) {

  _x_assert(node);
  if (node->plugin_class) {
    void *cls = node->plugin_class;

    _x_assert(node->info);
    /* dispose of plugin class */
    switch (node->info->type & PLUGIN_TYPE_MASK) {
    case PLUGIN_INPUT:
      ((input_class_t *)cls)->dispose ((input_class_t *)cls);
      break;
    case PLUGIN_DEMUX:
      ((demux_class_t *)cls)->dispose ((demux_class_t *)cls);
      break;
    case PLUGIN_SPU_DECODER:
      ((spu_decoder_class_t *)cls)->dispose ((spu_decoder_class_t *)cls);
      break;
    case PLUGIN_AUDIO_DECODER:
      ((audio_decoder_class_t *)cls)->dispose ((audio_decoder_class_t *)cls);
      break;
    case PLUGIN_VIDEO_DECODER:
      ((video_decoder_class_t *)cls)->dispose ((video_decoder_class_t *)cls);
      break;
    case PLUGIN_AUDIO_OUT:
      ((audio_driver_class_t *)cls)->dispose ((audio_driver_class_t *)cls);
      break;
    case PLUGIN_VIDEO_OUT:
      ((video_driver_class_t *)cls)->dispose ((video_driver_class_t *)cls);
      break;
    case PLUGIN_POST:
      ((post_class_t *)cls)->dispose ((post_class_t *)cls);
      break;
    }
    node->plugin_class = NULL;
    if (node->file)
      dec_file_ref(node->file);
  }
}

/*
 *  load input+demuxer plugins
 *  load plugins that asked to be initialized
 */
static void _load_required_plugins(xine_t *this, xine_sarray_t *list) {

  int list_id = 0;
  int list_size;

  list_size = xine_sarray_size(list);
  while (list_id < list_size) {
    plugin_node_t *node = xine_sarray_get(list, list_id);
    
    if( (node->info->type & PLUGIN_MUST_PRELOAD) && !node->plugin_class ) {
      
      lprintf("preload plugin %s from %s\n", node->info->id, node->file->filename);

      if (! _load_plugin_class (this, node, NULL)) {
	/* in case of failure remove from list */

	xine_sarray_remove(list, list_id);
	list_size = xine_sarray_size(list);

      } else
	list_id++;
    } else
      list_id++;
  }
}

static void load_required_plugins(xine_t *this) {
  int i;

  for (i = 0; i < PLUGIN_TYPE_MAX; i++) {
    _load_required_plugins (this, this->plugin_catalog->plugin_lists[i]);
  }
}



/*
 *  save plugin list information to file (cached catalog)
 */
static void save_plugin_list(FILE *fp, xine_sarray_t *list) {

  const plugin_node_t *node;
  const plugin_file_t *file;
  const decoder_info_t *decoder_info;
  const demuxer_info_t *demuxer_info;
  const input_info_t *input_info;
  const vo_info_t *vo_info;
  const ao_info_t *ao_info;
  const post_info_t *post_info;
  
  int i;
  int list_id = 0;
  int list_size;

  list_size = xine_sarray_size (list);
  while (list_id < list_size) {
    node = xine_sarray_get(list, list_id);

    file = node->file;
    fprintf(fp, "[%s]\n", file->filename );
    fprintf(fp, "size=%" PRId64 "\n", (uint64_t) file->filesize );
    fprintf(fp, "mtime=%" PRId64 "\n", (uint64_t) file->filemtime );
    fprintf(fp, "type=%d\n", node->info->type );
    fprintf(fp, "api=%d\n", node->info->API );
    fprintf(fp, "id=%s\n", node->info->id );
    fprintf(fp, "version=%lu\n", (unsigned long) node->info->version );
  
    switch (node->info->type & PLUGIN_TYPE_MASK){
    
      case PLUGIN_VIDEO_OUT:
        vo_info = node->info->special_info;
        fprintf(fp, "visual_type=%d\n", vo_info->visual_type );
        fprintf(fp, "vo_priority=%d\n", vo_info->priority );
        break;
    
      case PLUGIN_AUDIO_OUT:
        ao_info = node->info->special_info;
        fprintf(fp, "ao_priority=%d\n", ao_info->priority );
        break;
    
      case PLUGIN_AUDIO_DECODER:
      case PLUGIN_VIDEO_DECODER:
      case PLUGIN_SPU_DECODER:
        decoder_info = node->info->special_info;
        fprintf(fp, "supported_types=");
        for (i=0; decoder_info->supported_types[i] != 0; ++i){
          fprintf(fp, "%lu ", (unsigned long) decoder_info->supported_types[i]);
        }
        fprintf(fp, "\n");
        fprintf(fp, "decoder_priority=%d\n", decoder_info->priority );
        break;

      case PLUGIN_DEMUX:
        demuxer_info = node->info->special_info;
        fprintf(fp, "demuxer_priority=%d\n", demuxer_info->priority);
        break;
      
      case PLUGIN_INPUT:
        input_info = node->info->special_info;
        fprintf(fp, "input_priority=%d\n", input_info->priority);
        break;

      case PLUGIN_POST:
        post_info = node->info->special_info;
	fprintf(fp, "post_type=%lu\n", (unsigned long)post_info->type);
	break;
    }        
    
    fprintf(fp, "\n");
    list_id++;
  }
}

/*
 *  load plugin list information from file (cached catalog)
 */
static void load_plugin_list(FILE *fp, xine_sarray_t *plugins) {

  plugin_node_t *node;
  plugin_file_t *file;
  decoder_info_t *decoder_info = NULL;
  vo_info_t *vo_info = NULL;
  ao_info_t *ao_info = NULL;
  demuxer_info_t *demuxer_info = NULL;
  input_info_t *input_info = NULL;
  post_info_t *post_info = NULL;
  int i;
  uint64_t llu;
  unsigned long lu;
  char line[1024];
  char *value;
  int version_ok = 0;
  
  node = NULL;
  file = NULL;
  while (fgets (line, 1023, fp)) {
    if (line[0] == '#')
      continue;
      
    if( (value = strchr(line, '\r')) || (value = strchr(line, '\n')) )
      *value = (char) 0; /* eliminate any cr/lf */

    if( (value = strchr(line, '\r')) || (value = strchr(line, '\n')) )
      *value = (char) 0; /* eliminate any cr/lf */

    if (line[0] == '[' && version_ok) {
      if((value = strchr (line, ']')))
        *value = (char) 0;

      if( node ) {
        xine_sarray_add (plugins, node);
      }
      node                = xine_xmalloc(sizeof(plugin_node_t));
      file                = xine_xmalloc(sizeof(plugin_file_t));
      node->file          = file;
      file->filename      = strdup(line+1);
      node->info          = xine_xmalloc(2*sizeof(plugin_info_t));
      node->info[1].type  = PLUGIN_NONE;
      decoder_info        = NULL;
      vo_info             = NULL;
      ao_info             = NULL;
      post_info           = NULL;
    }

    if ((value = strchr (line, '='))) {

      *value = (char) 0;
      value++;

      if( !version_ok ) {
        if( !strcmp("cache_catalog_version",line) ) {
          sscanf(value," %d",&i);
          if( i == CACHE_CATALOG_VERSION )
            version_ok = 1;
          else
            return;  
        }
      } else if (node) {
        if( !strcmp("size",line) ) {
          sscanf(value," %" SCNu64,&llu);
          file->filesize = (off_t) llu;
        } else if( !strcmp("mtime",line) ) {
          sscanf(value," %" SCNu64,&llu);
          file->filemtime = (time_t) llu;
        } else if( !strcmp("type",line) ) {
          sscanf(value," %d",&i);
          node->info->type = i;
          
          switch (node->info->type & PLUGIN_TYPE_MASK){
          
            case PLUGIN_VIDEO_OUT:
              node->info->special_info = vo_info =
                        xine_xmalloc(sizeof(vo_info_t));
              break;
          
            case PLUGIN_AUDIO_OUT:
              node->info->special_info = ao_info =
		             xine_xmalloc(sizeof(ao_info_t));
              break;
          
            case PLUGIN_DEMUX:
              node->info->special_info = demuxer_info =
		             xine_xmalloc(sizeof(demuxer_info_t));
              break;

            case PLUGIN_INPUT:
              node->info->special_info = input_info =
		           xine_xmalloc(sizeof(input_info_t));
              break;

            case PLUGIN_AUDIO_DECODER:
            case PLUGIN_VIDEO_DECODER:
            case PLUGIN_SPU_DECODER:
              node->info->special_info = decoder_info =
                             xine_xmalloc(sizeof(decoder_info_t));
              break;
	    
	    case PLUGIN_POST:
	      node->info->special_info = post_info =
			  xine_xmalloc(sizeof(post_info_t));
	      break;
          }        
          
        } else if( !strcmp("api",line) ) {
          sscanf(value," %d",&i);
          node->info->API = i;
        } else if( !strcmp("id",line) ) {
          node->info->id = strdup(value);
        } else if( !strcmp("version",line) ) {
          sscanf(value," %lu",&lu);
          node->info->version = lu;
        } else if( !strcmp("visual_type",line) && vo_info ) {
          sscanf(value," %d",&i);
          vo_info->visual_type = i;
        } else if( !strcmp("supported_types",line) && decoder_info ) {
          char *s;
          
          for( s = value, i = 0; s && sscanf(s," %lu",&lu) > 0; i++ ) {
            s = strchr(s+1, ' ');
          }
          decoder_info->supported_types = xine_xmalloc((i+1)*sizeof(uint32_t));
          for( s = value, i = 0; s && sscanf(s," %lu",&lu) > 0; i++ ) {
            decoder_info->supported_types[i] = lu;
            s = strchr(s+1, ' ');
          }
        } else if( !strcmp("vo_priority",line) && vo_info ) {
          sscanf(value," %d",&i);
          vo_info->priority = i;
        } else if( !strcmp("ao_priority",line) && ao_info ) {
          sscanf(value," %d",&i);
          ao_info->priority = i;
        } else if( !strcmp("decoder_priority",line) && decoder_info ) {
          sscanf(value," %d",&i);
          decoder_info->priority = i;
        } else if( !strcmp("demuxer_priority",line) && demuxer_info ) {
          sscanf(value," %d",&i);
          demuxer_info->priority = i;
        } else if( !strcmp("input_priority",line) && input_info ) {
          sscanf(value," %d",&i);
          input_info->priority = i;
        } else if( !strcmp("post_type",line) && post_info ) {
	  sscanf(value," %lu",&lu);
	  post_info->type = lu;
        }
      }
    }
  }
      
  if( node ) {
    xine_sarray_add (plugins, node);
  }
}


/*
 * save catalog to cache file
 */
static void save_catalog (xine_t *this) {

  FILE       *fp;
  char       *cachefile, *dirfile; 
  const char *relname = CACHE_CATALOG_FILE;
  const char *dirname = CACHE_CATALOG_DIR;
    
  cachefile = (char *) xine_xmalloc(strlen(xine_get_homedir()) + 
                                    strlen(relname) + 2);
  sprintf(cachefile, "%s/%s", xine_get_homedir(), relname);
  
  /* make sure homedir (~/.xine) exists */
  dirfile = (char *) xine_xmalloc(strlen(xine_get_homedir()) + 
				  strlen(dirname) + 2);
  sprintf(dirfile, "%s/%s", xine_get_homedir(), dirname);
  mkdir (dirfile, 0755);
  free (dirfile);

  if( (fp = fopen(cachefile,"w")) != NULL ) {
    int i;
  
    fprintf(fp, "# this file is automatically created by xine, do not edit.\n\n");
    fprintf(fp, "cache_catalog_version=%d\n\n", CACHE_CATALOG_VERSION);

    for (i = 0; i < PLUGIN_TYPE_MAX; i++) {
      save_plugin_list (fp, this->plugin_catalog->plugin_lists[i]);
    }
    fclose(fp);
  }
  free(cachefile);
}

/*
 * load cached catalog from file
 */
static void load_cached_catalog (xine_t *this) {

  FILE *fp;
  char *cachefile;                                               
  const char *relname = CACHE_CATALOG_FILE;
    
  cachefile = (char *) xine_xmalloc(strlen(xine_get_homedir()) + 
                                    strlen(relname) + 2);
  sprintf(cachefile, "%s/%s", xine_get_homedir(), relname);
  
  if( (fp = fopen(cachefile,"r")) != NULL ) {
    load_plugin_list (fp, this->plugin_catalog->cache_list);
    fclose(fp);
  }
  free(cachefile);
}


/*
 *  initialize catalog, load all plugins into new catalog
 */
void _x_scan_plugins (xine_t *this) {
  
  char *homedir, *plugindir, *pluginpath;
  int i,j;
  int lenpluginpath;
  
  lprintf("_x_scan_plugins()\n");

  if (this == NULL || this->config == NULL) {
    fprintf(stderr, "%s(%s@%d): parameter should be non null, exiting\n",
	    __FILE__, __XINE_FUNCTION__, __LINE__);
    _x_abort();
  }

  homedir = strdup(xine_get_homedir());
  this->plugin_catalog = _new_catalog();
  load_cached_catalog (this);

  if ((pluginpath = getenv("XINE_PLUGIN_PATH")) != NULL) {
    pluginpath = strdup(pluginpath);
  } else {
    const char *str1, *str2;
    int len;

    str1 = "~/.xine/plugins";
    str2 = XINE_PLUGINDIR;
    len = strlen(str1) + strlen(str2) + 2;
    pluginpath = xine_xmalloc(len);
    snprintf(pluginpath, len, "%s" XINE_PATH_SEPARATOR_STRING "%s", str1, str2);
  }
  plugindir = xine_xmalloc(strlen(pluginpath)+strlen(homedir)+2);
  j=0;
  lenpluginpath = strlen(pluginpath);
  for (i=0; i <= lenpluginpath; ++i){
    switch (pluginpath[i]){
    case XINE_PATH_SEPARATOR_CHAR:
    case '\0':
      plugindir[j] = '\0';
      collect_plugins(this, plugindir);
      j = 0;
      break;
    case '~':
      if (j == 0){
	strcpy(plugindir, homedir);
	j = strlen(plugindir);
	break;
      }
    default:
      plugindir[j++] = pluginpath[i];
    }
  }
  free(plugindir);
  free(pluginpath);
  free(homedir);

  save_catalog (this);
    
  load_required_plugins (this);

  map_decoders (this);
}

/*
 * input / demuxer plugin loading
 */

input_plugin_t *_x_find_input_plugin (xine_stream_t *stream, const char *mrl) {

  xine_t           *xine = stream->xine;
  plugin_catalog_t *catalog = xine->plugin_catalog;
  plugin_node_t    *node;
  input_plugin_t   *plugin = NULL;
  int               list_id, list_size;

  pthread_mutex_lock (&catalog->lock);

  list_size = xine_sarray_size(catalog->plugin_lists[PLUGIN_INPUT - 1]);
  for (list_id = 0; list_id < list_size; list_id++) {
    node = xine_sarray_get (catalog->plugin_lists[PLUGIN_INPUT - 1], list_id);

    if (node->plugin_class || _load_plugin_class(xine, node, NULL)) {
      if ((plugin = ((input_class_t *)node->plugin_class)->get_instance(node->plugin_class, stream, mrl))) {
        inc_node_ref(node);
        plugin->node = node;
        break;
      }
    }  
  }

  pthread_mutex_unlock (&catalog->lock);

  return plugin;
}


void _x_free_input_plugin (xine_stream_t *stream, input_plugin_t *input) {
  plugin_catalog_t *catalog = stream->xine->plugin_catalog;
  plugin_node_t    *node = input->node;

  input->dispose(input);
  if (node) {
    pthread_mutex_lock(&catalog->lock);
    dec_node_ref(node);
    pthread_mutex_unlock(&catalog->lock);
  }
}


static demux_plugin_t *probe_demux (xine_stream_t *stream, int method1, int method2,
				    input_plugin_t *input) {

  int               i;
  int               methods[3];
  plugin_catalog_t *catalog = stream->xine->plugin_catalog;
  demux_plugin_t   *plugin = NULL;

  methods[0] = method1;
  methods[1] = method2;
  methods[2] = -1;

  if (methods[0] == -1) {
    xprintf (stream->xine, XINE_VERBOSITY_DEBUG, "load_plugins: probe_demux method1 = %d is not allowed \n", method1);
    _x_abort();
  }

  i = 0;
  while (methods[i] != -1 && !plugin) {
    int list_id, list_size;

    stream->content_detection_method = methods[i];

    pthread_mutex_lock (&catalog->lock);

    list_size = xine_sarray_size(catalog->plugin_lists[PLUGIN_DEMUX - 1]);
    for(list_id = 0; list_id < list_size; list_id++) {
      plugin_node_t *node;

      node = xine_sarray_get (catalog->plugin_lists[PLUGIN_DEMUX - 1], list_id);

      xprintf(stream->xine, XINE_VERBOSITY_DEBUG, "load_plugins: probing demux '%s'\n", node->info->id);

      if (node->plugin_class || _load_plugin_class(stream->xine, node, NULL)) {
        if ((plugin = ((demux_class_t *)node->plugin_class)->open_plugin(node->plugin_class, stream, input))) {
	  inc_node_ref(node);
	  plugin->node = node;
	  break;
        }
      }
    }

    pthread_mutex_unlock (&catalog->lock);

    i++;
  }

  return plugin;
}

demux_plugin_t *_x_find_demux_plugin (xine_stream_t *stream, input_plugin_t *input) {

  switch (stream->xine->demux_strategy) {

  case XINE_DEMUX_DEFAULT_STRATEGY:
    return probe_demux (stream, METHOD_BY_CONTENT, METHOD_BY_EXTENSION, input);

  case XINE_DEMUX_REVERT_STRATEGY:
    return probe_demux (stream, METHOD_BY_EXTENSION, METHOD_BY_CONTENT, input);

  case XINE_DEMUX_CONTENT_STRATEGY:
    return probe_demux (stream, METHOD_BY_CONTENT, -1, input);

  case XINE_DEMUX_EXTENSION_STRATEGY:
    return probe_demux (stream, METHOD_BY_EXTENSION, -1, input);

  default:
    xprintf (stream->xine, XINE_VERBOSITY_LOG,
	     _("load_plugins: unknown content detection strategy %d\n"), stream->xine->demux_strategy);
    _x_abort();
  }

  return NULL;
}

demux_plugin_t *_x_find_demux_plugin_by_name(xine_stream_t *stream, const char *name, input_plugin_t *input) {

  plugin_catalog_t  *catalog = stream->xine->plugin_catalog;
  plugin_node_t     *node;
  demux_plugin_t    *plugin = NULL;
  int                list_id, list_size;

  pthread_mutex_lock(&catalog->lock);

  stream->content_detection_method = METHOD_EXPLICIT;

  list_size = xine_sarray_size(catalog->plugin_lists[PLUGIN_DEMUX - 1]);
  for (list_id = 0; list_id < list_size; list_id++) {

    node = xine_sarray_get(catalog->plugin_lists[PLUGIN_DEMUX - 1], list_id);

    if (strcasecmp(node->info->id, name) == 0) {
      if (node->plugin_class || _load_plugin_class(stream->xine, node, NULL)) {
        if ((plugin = ((demux_class_t *)node->plugin_class)->open_plugin(node->plugin_class, stream, input))) {
	  inc_node_ref(node);
	  plugin->node = node;
	  break;
        }
      }
    }
  }

  pthread_mutex_unlock(&catalog->lock);
  return plugin;
}

/*
 * this is a special test mode for content detection: all demuxers are probed
 * by content and extension except last_demux_name which is tested after
 * every other demuxer.
 *
 * this way we can make sure no demuxer will interfere on probing of a
 * known stream.
 */

demux_plugin_t *_x_find_demux_plugin_last_probe(xine_stream_t *stream, const char *last_demux_name, input_plugin_t *input) {

  int               i;
  int               methods[3];
  xine_t           *xine = stream->xine;
  plugin_catalog_t *catalog = xine->plugin_catalog;
  plugin_node_t    *last_demux = NULL;
  demux_plugin_t   *plugin = NULL;

  methods[0] = METHOD_BY_CONTENT;
  methods[1] = METHOD_BY_EXTENSION;
  methods[2] = -1;

  i = 0;
  while (methods[i] != -1 && !plugin) {
    int list_id, list_size;

    stream->content_detection_method = methods[i];

    pthread_mutex_lock (&catalog->lock);

    list_size = xine_sarray_size(catalog->plugin_lists[PLUGIN_DEMUX - 1]);
    for (list_id = 0; list_id < list_size; list_id++) {
      plugin_node_t *node;

      node = xine_sarray_get (catalog->plugin_lists[PLUGIN_DEMUX - 1], list_id);

      lprintf ("probing demux '%s'\n", node->info->id);

      if (strcasecmp(node->info->id, last_demux_name) == 0) {
        last_demux = node;
      } else {
	xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
		"load_plugin: probing '%s' (method %d)...\n", node->info->id, stream->content_detection_method );
	if (node->plugin_class || _load_plugin_class(xine, node, NULL)) {
          if ((plugin = ((demux_class_t *)node->plugin_class)->open_plugin(node->plugin_class, stream, input))) {
	    xprintf (stream->xine, XINE_VERBOSITY_DEBUG,
		     "load_plugins: using demuxer '%s' (instead of '%s')\n", node->info->id, last_demux_name);
	    inc_node_ref(node);
	    plugin->node = node;
	    break;
          }
        }
      }
    }

    pthread_mutex_unlock (&catalog->lock);

    i++;
  }

  if( plugin )
    return plugin;

  if( !last_demux )
    return NULL;

  stream->content_detection_method = METHOD_BY_CONTENT;

  if (!last_demux->plugin_class && !_load_plugin_class(xine, last_demux, NULL))
    return NULL;

  if ((plugin = ((demux_class_t *)last_demux->plugin_class)->open_plugin(last_demux->plugin_class, stream, input))) {
    xprintf (stream->xine, XINE_VERBOSITY_LOG, _("load_plugins: using demuxer '%s'\n"), last_demux_name);
    inc_node_ref(last_demux);
    plugin->node = last_demux;
    return plugin;
  }

  return NULL;
}


void _x_free_demux_plugin (xine_stream_t *stream, demux_plugin_t *demux) {
  plugin_catalog_t *catalog = stream->xine->plugin_catalog;
  plugin_node_t    *node = demux->node;

  demux->dispose(demux);
  if (node) {
    pthread_mutex_lock(&catalog->lock);
    dec_node_ref(node);
    pthread_mutex_unlock(&catalog->lock);
  }
}


const char *const *xine_get_autoplay_input_plugin_ids(xine_t *this) {

  plugin_catalog_t   *catalog;
  plugin_node_t      *node;
  int                 list_id, list_size;

  catalog = this->plugin_catalog;

  pthread_mutex_lock (&catalog->lock);

  catalog->ids[0] = NULL;

  list_size = xine_sarray_size (catalog->plugin_lists[PLUGIN_INPUT - 1]);
  for (list_id = 0; list_id < list_size; list_id++) {
    input_class_t *ic;

    node = xine_sarray_get(catalog->plugin_lists[PLUGIN_INPUT - 1], list_id);
    if (node->plugin_class || _load_plugin_class(this, node, NULL)) {

      ic = (input_class_t *) node->plugin_class;
      if (ic->get_autoplay_list) {
	int i = 0, j;

	while (catalog->ids[i] && strcmp(catalog->ids[i], node->info->id) < 0)
	  i++;
	for (j = PLUGIN_MAX - 1; j > i; j--)
	  catalog->ids[j] = catalog->ids[j - 1];

	catalog->ids[i] = node->info->id;
      }
    }
  }

  pthread_mutex_unlock (&catalog->lock);

  return catalog->ids;
}

const char *const *xine_get_browsable_input_plugin_ids(xine_t *this) {


  plugin_catalog_t   *catalog;
  plugin_node_t      *node;
  int                 list_id, list_size;

  catalog = this->plugin_catalog;

  pthread_mutex_lock (&catalog->lock);

  catalog->ids[0] = NULL;
  list_size = xine_sarray_size (catalog->plugin_lists[PLUGIN_INPUT - 1]);

  for (list_id = 0; list_id < list_size; list_id++) {
    input_class_t *ic;
    
    node = xine_sarray_get(catalog->plugin_lists[PLUGIN_INPUT - 1], list_id);
    if (node->plugin_class || _load_plugin_class(this, node, NULL)) {

      ic = (input_class_t *) node->plugin_class;
      if (ic->get_dir) {
	int i = 0, j;

	while (catalog->ids[i] && strcmp(catalog->ids[i], node->info->id) < 0)
	  i++;
	for (j = PLUGIN_MAX - 1; j > i; j--)
	  catalog->ids[j] = catalog->ids[j - 1];

	catalog->ids[i] = node->info->id;
      }
    }
  }

  pthread_mutex_unlock (&catalog->lock);

  return catalog->ids;
}

/*
 *  video out plugins section
 */

static vo_driver_t *_load_video_driver (xine_t *this, plugin_node_t *node,
					void *data) {

  vo_driver_t *driver;

  if (!node->plugin_class && !_load_plugin_class (this, node, data))
    return NULL;

  driver = ((video_driver_class_t *)node->plugin_class)->open_plugin(node->plugin_class, data);

  if (driver) {
    inc_node_ref(node);
    driver->node = node;
  }

  return driver;
}

vo_driver_t *_x_load_video_output_plugin(xine_t *this,
					   char *id,
					   int visual_type, void *visual) {

  plugin_node_t      *node;
  vo_driver_t        *driver;
  vo_info_t          *vo_info;
  plugin_catalog_t   *catalog = this->plugin_catalog;
  int                 list_id, list_size;

  driver = NULL;

  if (id && !strcasecmp(id, "auto"))
    id = NULL;
  
  pthread_mutex_lock (&catalog->lock);

  list_size = xine_sarray_size (catalog->plugin_lists[PLUGIN_VIDEO_OUT - 1]);
  for (list_id = 0; list_id < list_size; list_id++) {

    node = xine_sarray_get (catalog->plugin_lists[PLUGIN_VIDEO_OUT - 1], list_id);

    vo_info = (vo_info_t *)node->info->special_info;
    if (vo_info->visual_type == visual_type) {
      if (id) {
	if (!strcasecmp (node->info->id, id)) {
	  driver = _load_video_driver (this, node, visual);
	  break;
	}

      } else {

	driver = _load_video_driver (this, node, visual);

	if (driver) {

	  break;
	}
      }
    }
  }

  pthread_mutex_unlock (&catalog->lock);

  return driver;
}

xine_video_port_t *xine_open_video_driver (xine_t *this,
					   const char *id,
					   int visual_type, void *visual) {

  vo_driver_t        *driver;
  xine_video_port_t  *port;

  driver = _x_load_video_output_plugin(this, (char *)id, visual_type, visual);

  if (!driver) {
    lprintf ("failed to load video output plugin <%s>\n", id);
    return NULL;
  }

  port = _x_vo_new_port(this, driver, 0);
  
  return port;
}

xine_video_port_t *xine_new_framegrab_video_port (xine_t *this) {

  plugin_node_t      *node;
  vo_driver_t        *driver;
  xine_video_port_t  *port;
  vo_info_t          *vo_info;
  plugin_catalog_t   *catalog = this->plugin_catalog;
  char               *id;
  int                 list_id, list_size;

  driver = NULL;
  id     = "none";

  pthread_mutex_lock (&catalog->lock);

  list_size = xine_sarray_size (catalog->plugin_lists[PLUGIN_VIDEO_OUT - 1]);
  for (list_id = 0; list_id < list_size; list_id++) {

    node = xine_sarray_get (catalog->plugin_lists[PLUGIN_VIDEO_OUT - 1], list_id);

    vo_info = (vo_info_t *)node->info->special_info;
    if (!strcasecmp (node->info->id, id)) {
      driver = _load_video_driver (this, node, NULL);
      break;
    }
  }

  pthread_mutex_unlock (&catalog->lock);

  if (!driver) {
    lprintf ("failed to load video output plugin <%s>\n", id);
    return NULL;
  }

  port = _x_vo_new_port(this, driver, 1);
  
  return port;
}

/*
 *  audio output plugins section
 */

const char *const *xine_list_audio_output_plugins (xine_t *xine) {
  plugin_catalog_t *catalog = xine->plugin_catalog;
  
  pthread_mutex_lock (&catalog->lock);
  _build_list_typed_plugins(&catalog, catalog->plugin_lists[PLUGIN_AUDIO_OUT - 1]);
  pthread_mutex_unlock (&catalog->lock);

  return catalog->ids;
}

const char *const *xine_list_video_output_plugins (xine_t *xine) {
  plugin_catalog_t *catalog = xine->plugin_catalog;
  
  pthread_mutex_lock (&catalog->lock);
  _build_list_typed_plugins(&catalog, catalog->plugin_lists[PLUGIN_VIDEO_OUT - 1]);
  pthread_mutex_unlock (&catalog->lock);

  return catalog->ids;
}

static ao_driver_t *_load_audio_driver (xine_t *this, plugin_node_t *node,
					void *data) {

  ao_driver_t *driver;

  if (!node->plugin_class && !_load_plugin_class (this, node, data))
    return NULL;

  driver = ((audio_driver_class_t *)node->plugin_class)->open_plugin(node->plugin_class, data);

  if (driver) {
    inc_node_ref(node);
    driver->node = node;
  }

  return driver;
}

ao_driver_t *_x_load_audio_output_plugin (xine_t *this, const char *id)
{
  plugin_node_t      *node;
  ao_driver_t        *driver = NULL;
  ao_info_t          *ao_info;
  plugin_catalog_t   *catalog = this->plugin_catalog;
  int                 list_id, list_size;

  pthread_mutex_lock (&catalog->lock);

  list_size = xine_sarray_size (this->plugin_catalog->plugin_lists[PLUGIN_AUDIO_OUT - 1]);
  for (list_id = 0; list_id < list_size; list_id++) {

    node = xine_sarray_get (this->plugin_catalog->plugin_lists[PLUGIN_AUDIO_OUT - 1], list_id);

    ao_info = (ao_info_t *)node->info->special_info;

    if (!strcasecmp(node->info->id, id)) {
      driver = _load_audio_driver (this, node, NULL);
      break;
    }
  }

  pthread_mutex_unlock (&catalog->lock);

  if (!driver) {
    xprintf (this, XINE_VERBOSITY_LOG,
        _("load_plugins: failed to load audio output plugin <%s>\n"), id);
  }
  return driver;
}

xine_audio_port_t *xine_open_audio_driver (xine_t *this, const char *id,
					   void *data) {

  plugin_node_t      *node;
  ao_driver_t        *driver;
  xine_audio_port_t  *port;
  ao_info_t          *ao_info;
  plugin_catalog_t   *catalog = this->plugin_catalog;
  int                 list_id, list_size;

  if (id && !strcasecmp(id, "auto") )
    id = NULL;
  
  pthread_mutex_lock (&catalog->lock);

  driver = NULL;

  list_size = xine_sarray_size (this->plugin_catalog->plugin_lists[PLUGIN_AUDIO_OUT - 1]);
  for (list_id = 0; list_id < list_size; list_id++) {

    node = xine_sarray_get (this->plugin_catalog->plugin_lists[PLUGIN_AUDIO_OUT - 1], list_id);

    ao_info = (ao_info_t *)node->info->special_info;

    if (id) {
      if (!strcasecmp(node->info->id, id)) {
	driver = _load_audio_driver (this, node, data);
	break;
      }
    } else if( ao_info->priority >= 0 ) {
      driver = _load_audio_driver (this, node, data);
      if (driver) {
	break;
      }
    }
  }

  pthread_mutex_unlock (&catalog->lock);
  
  if (!driver) {
    if (id)
      xprintf (this, XINE_VERBOSITY_LOG,
	       _("load_plugins: failed to load audio output plugin <%s>\n"), id);
    else
      xprintf (this, XINE_VERBOSITY_LOG, 
	       _("load_plugins: audio output auto-probing didn't find any usable audio driver.\n"));
    return NULL;
  }

  port = _x_ao_new_port(this, driver, 0);

  return port;
}

xine_audio_port_t *xine_new_framegrab_audio_port (xine_t *this) {

  xine_audio_port_t  *port;

  port = _x_ao_new_port (this, NULL, 1);

  return port;
}

void xine_close_audio_driver (xine_t *this, xine_audio_port_t  *ao_port) {

  if( ao_port )
    ao_port->exit(ao_port);

}

void xine_close_video_driver (xine_t *this, xine_video_port_t  *vo_port) {

  if( vo_port )
    vo_port->exit(vo_port);

}


/*
 * get autoplay mrl list from input plugin
 */

char **xine_get_autoplay_mrls (xine_t *this, const char *plugin_id,
			       int *num_mrls) {

  plugin_catalog_t     *catalog;
  plugin_node_t        *node;
  int                   list_id, list_size;

  catalog = this->plugin_catalog;

  list_size = xine_sarray_size (catalog->plugin_lists[PLUGIN_INPUT - 1]);
  for (list_id = 0; list_id < list_size; list_id++) {

    node = xine_sarray_get (catalog->plugin_lists[PLUGIN_INPUT - 1], list_id);

    if (!strcasecmp (node->info->id, plugin_id)) {
      input_class_t *ic;

      if (node->plugin_class || _load_plugin_class (this, node, NULL)) {

	ic = (input_class_t *) node->plugin_class;
	
	if (!ic->get_autoplay_list)
	  return NULL;

	return ic->get_autoplay_list (ic, num_mrls);
      }
    }
  }
  return NULL;
}

/*
 * input plugin mrl browser support
 */
xine_mrl_t **xine_get_browse_mrls (xine_t *this, const char *plugin_id,
				   const char *start_mrl, int *num_mrls) {

  plugin_catalog_t   *catalog;
  plugin_node_t      *node;
  int                 list_id, list_size;

  catalog = this->plugin_catalog;

  list_size = xine_sarray_size (catalog->plugin_lists[PLUGIN_INPUT - 1]);
  for (list_id = 0; list_id < list_size; list_id++) {

    node = xine_sarray_get (catalog->plugin_lists[PLUGIN_INPUT - 1], list_id);

    if (!strcasecmp (node->info->id, plugin_id)) {
      input_class_t *ic;

      if (node->plugin_class || _load_plugin_class (this, node, NULL)) {

	ic = (input_class_t *) node->plugin_class;
	
	if (!ic->get_dir)
	  return NULL;

	return ic->get_dir (ic, start_mrl, num_mrls);
      }
    }
  }
  return NULL;
}

video_decoder_t *_x_get_video_decoder (xine_stream_t *stream, uint8_t stream_type) {

  plugin_node_t    *node;
  int               i, j;
  plugin_catalog_t *catalog = stream->xine->plugin_catalog;
  video_decoder_t  *vd = NULL;

  lprintf ("looking for video decoder for streamtype %02x\n", stream_type);

  pthread_mutex_lock (&catalog->lock);

  for (i = 0; i < PLUGINS_PER_TYPE; i++) {

    node = catalog->video_decoder_map[stream_type][i];

    if (!node) {
      break;
    }

    if (!node->plugin_class && !_load_plugin_class (stream->xine, node, NULL)) {
      /* remove non working plugin from catalog */
      xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
	      "load_plugins: plugin %s failed to init its class.\n", node->info->id);
      for (j = i + 1; j < PLUGINS_PER_TYPE; j++)
        catalog->video_decoder_map[stream_type][j - 1] =
          catalog->video_decoder_map[stream_type][j];
      catalog->video_decoder_map[stream_type][PLUGINS_PER_TYPE-1] = NULL;
      i--;
      continue;
    }

    vd = ((video_decoder_class_t *)node->plugin_class)->open_plugin(node->plugin_class, stream);

    if (vd) {
      inc_node_ref(node);
      vd->node = node;
      xprintf(stream->xine, XINE_VERBOSITY_DEBUG,
          "load_plugins: plugin %s will be used for video streamtype %02x.\n", 
          node->info->id, stream_type);
      
      break;
    } else {
      /* remove non working plugin from catalog */
      xprintf(stream->xine, XINE_VERBOSITY_DEBUG,
	      "load_plugins: plugin %s failed to instantiate itself.\n", node->info->id);
      for (j = i + 1; j < PLUGINS_PER_TYPE; j++)
        catalog->video_decoder_map[stream_type][j - 1] =
          catalog->video_decoder_map[stream_type][j];
      catalog->video_decoder_map[stream_type][PLUGINS_PER_TYPE-1] = NULL;
      i--;
    }
  }

  pthread_mutex_unlock (&catalog->lock);
  return vd;
}

void _x_free_video_decoder (xine_stream_t *stream, video_decoder_t *vd) {
  plugin_catalog_t *catalog = stream->xine->plugin_catalog;
  plugin_node_t    *node = vd->node;

  vd->dispose (vd);

  if (node) {
    pthread_mutex_lock (&catalog->lock);
    dec_node_ref(node);
    pthread_mutex_unlock (&catalog->lock);
  }
}


audio_decoder_t *_x_get_audio_decoder (xine_stream_t *stream, uint8_t stream_type) {

  plugin_node_t    *node;
  int               i, j;
  plugin_catalog_t *catalog = stream->xine->plugin_catalog;
  audio_decoder_t  *ad = NULL;

  lprintf ("looking for audio decoder for streamtype %02x\n", stream_type);

  pthread_mutex_lock (&catalog->lock);

  for (i = 0; i < PLUGINS_PER_TYPE; i++) {

    node = catalog->audio_decoder_map[stream_type][i];

    if (!node) {
      break;
    }

    if (!node->plugin_class && !_load_plugin_class (stream->xine, node, NULL)) {
      /* remove non working plugin from catalog */
      xprintf(stream->xine, XINE_VERBOSITY_DEBUG, 
	      "load_plugins: plugin %s failed to init its class.\n", node->info->id);
      for (j = i + 1; j < PLUGINS_PER_TYPE; j++)
        catalog->audio_decoder_map[stream_type][j - 1] =
          catalog->audio_decoder_map[stream_type][j];
      catalog->audio_decoder_map[stream_type][PLUGINS_PER_TYPE-1] = NULL;
      i--;
      continue;
    }

    ad = ((audio_decoder_class_t *)node->plugin_class)->open_plugin(node->plugin_class, stream);

    if (ad) {
      inc_node_ref(node);
      ad->node = node;
      xprintf(stream->xine, XINE_VERBOSITY_DEBUG,
          "load_plugins: plugin %s will be used for audio streamtype %02x.\n", 
          node->info->id, stream_type);
      break;
    } else {
      /* remove non working plugin from catalog */
      xprintf(stream->xine, XINE_VERBOSITY_DEBUG,
	      "load_plugins: plugin %s failed to instantiate itself.\n", node->info->id);
      for (j = i + 1; j < PLUGINS_PER_TYPE; j++)
        catalog->audio_decoder_map[stream_type][j - 1] =
          catalog->audio_decoder_map[stream_type][j];
      catalog->audio_decoder_map[stream_type][PLUGINS_PER_TYPE-1] = NULL;
      i--;
    }
  }

  pthread_mutex_unlock (&catalog->lock);
  return ad;
}

void _x_free_audio_decoder (xine_stream_t *stream, audio_decoder_t *ad) {
  plugin_catalog_t *catalog = stream->xine->plugin_catalog;
  plugin_node_t    *node = ad->node;

  ad->dispose (ad);

  if (node) {
    pthread_mutex_lock (&catalog->lock);
    dec_node_ref(node);
    pthread_mutex_unlock (&catalog->lock);
  }
}

int _x_decoder_available (xine_t *xine, uint32_t buftype)
{
  plugin_catalog_t *catalog = xine->plugin_catalog;
  int stream_type = (buftype>>16) & 0xFF;

  if ( (buftype & 0xFF000000) == BUF_VIDEO_BASE ) {
    if( catalog->video_decoder_map[stream_type][0] )
      return 1;
  } else 
  if ( (buftype & 0xFF000000) == BUF_AUDIO_BASE ) {
    if( catalog->audio_decoder_map[stream_type][0] )
      return 1;
  } else 
  if ( (buftype & 0xFF000000) == BUF_SPU_BASE ) {
    if( catalog->spu_decoder_map[stream_type][0] )
      return 1;
  }

  return 0;
}

#ifdef LOG
static void _display_file_plugin_list (xine_list_t *list, plugin_file_t *file) {
  xine_list_iterator_t ite = xine_list_front(list);

  while (ite) {
    plugin_node_t *node = xine_list_get_value(list, ite);

    if ((node->file == file) && (node->ref)) {
      printf("    plugin: %s, class: %p , %d instance(s)\n",
	     node->info->id, node->plugin_class, node->ref);
    }

    ite = xine_list_next(list, ite);
  }
}
#endif

static void _unload_unref_plugins(xine_t *this, xine_sarray_t *list) {
  
  plugin_node_t *node;
  int            list_id, list_size;

  list_size = xine_sarray_size (list);
  for (list_id = 0; list_id < list_size; list_id++) {

    node = xine_sarray_get (list, list_id);

    if (node->ref == 0) {
      plugin_file_t *file = node->file;

      /* no plugin of this class is instancied */
      _dispose_plugin_class(node);

      /* check file references */
      if (file && !file->ref && file->lib_handle && !file->no_unload) {
	/* unload this file */
	lprintf("unloading plugin %s\n", file->filename);
	if (dlclose(file->lib_handle)) {
	   const char *error = dlerror();

	   xine_log (this, XINE_LOG_PLUGIN,
		_("load_plugins: cannot unload plugin lib %s:\n%s\n"), file->filename, error);
	}
	file->lib_handle = NULL;
      }
    }
  }
}

void xine_plugins_garbage_collector(xine_t *self) {
  plugin_catalog_t *catalog = self->plugin_catalog;
  int i;

  pthread_mutex_lock (&catalog->lock);
  for(i = 0; i < PLUGIN_TYPE_MAX; i++) {
    _unload_unref_plugins(self, self->plugin_catalog->plugin_lists[i]);
  } 

#if 0
  {
    plugin_file_t *file;

    printf("\nPlugin summary after garbage collection : \n");
    file = xine_list_first_content(self->plugin_catalog->file);
    while (file) {
      if (file->ref) {
	printf("\n  file %s referenced %d time(s)\n", file->filename, file->ref);

	for(i = 0; i < PLUGIN_TYPE_MAX; i++) {
	  _display_file_plugin_list (self->plugin_catalog->plugin_lists[i], file)
	}
      }
      file = xine_list_next_content(self->plugin_catalog->file);
    }
    printf("End of plugin summary\n\n");
  }
#endif

  pthread_mutex_unlock (&catalog->lock);
}

spu_decoder_t *_x_get_spu_decoder (xine_stream_t *stream, uint8_t stream_type) {

  plugin_node_t    *node;
  int               i, j;
  plugin_catalog_t *catalog = stream->xine->plugin_catalog;
  spu_decoder_t    *sd = NULL;

  lprintf ("looking for spu decoder for streamtype %02x\n", stream_type);

  pthread_mutex_lock (&catalog->lock);

  for (i = 0; i < PLUGINS_PER_TYPE; i++) {

    node = catalog->spu_decoder_map[stream_type][i];

    if (!node) {
      break;
    }

    if (!node->plugin_class && !_load_plugin_class (stream->xine, node, NULL)) {
      /* remove non working plugin from catalog */
      xprintf(stream->xine, XINE_VERBOSITY_DEBUG,
	      "load_plugins: plugin %s failed to init its class.\n", node->info->id);
      for (j = i + 1; j < PLUGINS_PER_TYPE; j++)
        catalog->spu_decoder_map[stream_type][j - 1] =
          catalog->spu_decoder_map[stream_type][j];
      catalog->spu_decoder_map[stream_type][PLUGINS_PER_TYPE-1] = NULL;
      i--;
      continue;
    }

    sd = ((spu_decoder_class_t *)node->plugin_class)->open_plugin(node->plugin_class, stream);

    if (sd) {
      inc_node_ref(node);
      sd->node = node;
      xprintf(stream->xine, XINE_VERBOSITY_DEBUG,
          "load_plugins: plugin %s will be used for spu streamtype %02x.\n", 
          node->info->id, stream_type);
      break;
    } else {
      /* remove non working plugin from catalog */
      xprintf(stream->xine, XINE_VERBOSITY_DEBUG,
	      "load_plugins: plugin %s failed to instantiate itself.\n", node->info->id);
      for (j = i + 1; j < PLUGINS_PER_TYPE; j++)
        catalog->spu_decoder_map[stream_type][j - 1] =
          catalog->spu_decoder_map[stream_type][j];
      catalog->spu_decoder_map[stream_type][PLUGINS_PER_TYPE-1] = NULL;
      i--;
    }
  }

  pthread_mutex_unlock (&catalog->lock);
  return sd;
}

void _x_free_spu_decoder (xine_stream_t *stream, spu_decoder_t *sd) {
  plugin_catalog_t *catalog = stream->xine->plugin_catalog;
  plugin_node_t    *node = sd->node;

  sd->dispose (sd);

  if (node) {
    pthread_mutex_lock (&catalog->lock);
    dec_node_ref(node);
    pthread_mutex_unlock (&catalog->lock);
  }
}

const char *const *xine_list_demuxer_plugins(xine_t *xine) {
  plugin_catalog_t *catalog = xine->plugin_catalog;
  
  pthread_mutex_lock (&catalog->lock);
  _build_list_typed_plugins(&catalog, catalog->plugin_lists[PLUGIN_DEMUX - 1]);
  pthread_mutex_unlock (&catalog->lock);

  return catalog->ids;
}

const char *const *xine_list_input_plugins(xine_t *xine) {
  plugin_catalog_t *catalog = xine->plugin_catalog;
  
  pthread_mutex_lock (&catalog->lock);
  _build_list_typed_plugins(&catalog, catalog->plugin_lists[PLUGIN_INPUT - 1]);
  pthread_mutex_unlock (&catalog->lock);

  return catalog->ids;
}

const char *const *xine_list_spu_plugins(xine_t *xine) {
  plugin_catalog_t *catalog = xine->plugin_catalog;
  
  pthread_mutex_lock (&catalog->lock);
  _build_list_typed_plugins(&catalog, catalog->plugin_lists[PLUGIN_SPU_DECODER - 1]);
  pthread_mutex_unlock (&catalog->lock);

  return catalog->ids;
}

const char *const *xine_list_audio_decoder_plugins(xine_t *xine) {
  plugin_catalog_t *catalog = xine->plugin_catalog;
  
  pthread_mutex_lock (&catalog->lock);
  _build_list_typed_plugins(&catalog, catalog->plugin_lists[PLUGIN_AUDIO_DECODER - 1]);
  pthread_mutex_unlock (&catalog->lock);

  return catalog->ids;
}

const char *const *xine_list_video_decoder_plugins(xine_t *xine) {
  plugin_catalog_t *catalog = xine->plugin_catalog;
  
  pthread_mutex_lock (&catalog->lock);
  _build_list_typed_plugins(&catalog, catalog->plugin_lists[PLUGIN_VIDEO_DECODER - 1]);
  pthread_mutex_unlock (&catalog->lock);

  return catalog->ids;
}

const char *const *xine_list_post_plugins(xine_t *xine) {
  plugin_catalog_t *catalog = xine->plugin_catalog;
  
  pthread_mutex_lock (&catalog->lock);
  _build_list_typed_plugins(&catalog, catalog->plugin_lists[PLUGIN_POST - 1]);
  pthread_mutex_unlock (&catalog->lock);

  return catalog->ids;
}

const char *const *xine_list_post_plugins_typed(xine_t *xine, int type) {
  plugin_catalog_t *catalog = xine->plugin_catalog;
  plugin_node_t    *node;
  int               i;
  int               list_id, list_size;
  
  pthread_mutex_lock (&catalog->lock);

  i = 0;
  list_size = xine_sarray_size (catalog->plugin_lists[PLUGIN_POST - 1]);

  for (list_id = 0; list_id < list_size; list_id++) {
    node = xine_sarray_get (catalog->plugin_lists[PLUGIN_POST - 1], list_id);
    if (((post_info_t *)node->info->special_info)->type == type)
      catalog->ids[i++] = node->info->id;
  }
  catalog->ids[i] = NULL;

  pthread_mutex_unlock (&catalog->lock);
  return catalog->ids;
}

#define GET_PLUGIN_DESC(NAME,TYPE,CATITEM) \
  const char *xine_get_##NAME##_plugin_description (xine_t *this, const char *plugin_id) { \
    plugin_catalog_t *catalog = this->plugin_catalog;					   \
    plugin_node_t    *node;                                                                \
    int               list_id, list_size;                                                  \
    list_size = xine_sarray_size (catalog->plugin_lists[CATITEM - 1]);                     \
    for (list_id = 0; list_id < list_size; list_id++) {                                    \
      node = xine_sarray_get (catalog->plugin_lists[CATITEM - 1], list_id);                \
      if (!strcasecmp (node->info->id, plugin_id)) {					   \
	TYPE##_class_t *ic = (TYPE##_class_t *) node->plugin_class;			   \
	if (!ic) {									   \
	  if (_load_plugin_class (this, node, NULL))					   \
	    ic = node->plugin_class;							   \
	  else										   \
	    return NULL;								   \
	}										   \
	return ic->get_description(ic);							   \
      }											   \
    }											   \
    return NULL;									   \
  }

GET_PLUGIN_DESC (input,		input,		PLUGIN_INPUT)
GET_PLUGIN_DESC (demux,		demux,		PLUGIN_DEMUX)
GET_PLUGIN_DESC (spu,		spu_decoder,	PLUGIN_SPU_DECODER)
GET_PLUGIN_DESC (audio,		audio_decoder,	PLUGIN_AUDIO_DECODER)
GET_PLUGIN_DESC (video,		video_decoder,	PLUGIN_VIDEO_DECODER)
GET_PLUGIN_DESC (audio_driver,	audio_driver,	PLUGIN_AUDIO_OUT)
GET_PLUGIN_DESC (video_driver,	video_driver,	PLUGIN_VIDEO_OUT)
GET_PLUGIN_DESC (post,		post,		PLUGIN_POST)

xine_post_t *xine_post_init(xine_t *xine, const char *name, int inputs,
			    xine_audio_port_t **audio_target,
			    xine_video_port_t **video_target) {
  plugin_catalog_t *catalog = xine->plugin_catalog;
  plugin_node_t    *node;
  post_plugin_t    *post = NULL;
  int               list_id, list_size;
  
  if( !name )
    return NULL;
  
  pthread_mutex_lock(&catalog->lock);
  
  list_size = xine_sarray_size (catalog->plugin_lists[PLUGIN_POST - 1]);

  for (list_id = 0; list_id < list_size; list_id++) {
    node = xine_sarray_get (catalog->plugin_lists[PLUGIN_POST - 1], list_id);
    
    if (strcmp(node->info->id, name) == 0) {
      
      if (!node->plugin_class && !_load_plugin_class(xine, node, NULL)) {
        xprintf(xine, XINE_VERBOSITY_DEBUG,
		"load_plugins: requested post plugin %s failed to load\n", name);
	break;
      }
      
      post = ((post_class_t *)node->plugin_class)->open_plugin(node->plugin_class,
        inputs, audio_target, video_target);

      if (post) {
        xine_post_in_t  *input;
	xine_post_out_t *output;
	xine_list_iterator_t ite;
	int i;
	
	post->running_ticket = xine->port_ticket;
	post->xine = xine;
	post->node = node;
	inc_node_ref(node);
	
	/* init the lists of announced connections */
	i = 0;
	ite = xine_list_front(post->input);
	while (ite) {
	  input = xine_list_get_value (post->input, ite);
	  i++;
	  ite = xine_list_next (post->input, ite);
	}
	post->input_ids = malloc(sizeof(char *) * (i + 1));
	i = 0;
	ite = xine_list_front (post->input);
	while (ite)  {
	  input = xine_list_get_value (post->input, ite);
	  post->input_ids[i++] = input->name;
	  ite = xine_list_next (post->input, ite);
	}
	post->input_ids[i] = NULL;
	
	i = 0;
	ite = xine_list_front (post->output);
	while (ite) {
	  output = xine_list_get_value (post->output, ite);
	  i++;
	  ite = xine_list_next (post->output, ite);
	}
	post->output_ids = malloc(sizeof(char *) * (i + 1));
	i = 0;
	ite = xine_list_front (post->output);
	while (ite)  {
	  output = xine_list_get_value (post->output, ite);
	  post->output_ids[i++] = output->name;
	  ite = xine_list_next (post->output, ite);
	}
	post->output_ids[i] = NULL;
	
	/* copy the post plugin type to the public part */
	post->xine_post.type = ((post_info_t *)node->info->special_info)->type;
	
	break;
      } else {
        xprintf(xine, XINE_VERBOSITY_DEBUG,
		"load_plugins: post plugin %s failed to instantiate itself\n", name);
	break;
      }
    }
  }
  
  pthread_mutex_unlock(&catalog->lock);
  
  if(post)
    return &post->xine_post;
  else {
    xprintf(xine, XINE_VERBOSITY_DEBUG, "load_plugins: no post plugin named %s found\n", name);
    return NULL;
  }
}

void xine_post_dispose(xine_t *xine, xine_post_t *post_gen) {
  post_plugin_t *post = (post_plugin_t *)post_gen;
  post->dispose(post);
  /* we cannot decrement the reference counter, since post plugins can delay
   * their disposal if they are still in use => post.c handles the counting for us */
}

/* get a list of file extensions for file types supported by xine
 * the list is separated by spaces 
 *
 * the pointer returned can be free()ed when no longer used */
char *xine_get_file_extensions (xine_t *self) {

  plugin_catalog_t *catalog = self->plugin_catalog;
  int               len, pos;
  plugin_node_t    *node;
  char             *str;
  int               list_id, list_size;

  pthread_mutex_lock (&catalog->lock);

  /* calc length of output */

  len = 0; 
  list_size = xine_sarray_size (catalog->plugin_lists[PLUGIN_DEMUX - 1]);
  for (list_id = 0; list_id < list_size; list_id++) {
    demux_class_t *cls;
    const char    *exts;

    node = xine_sarray_get (catalog->plugin_lists[PLUGIN_DEMUX - 1], list_id);
    if (node->plugin_class || _load_plugin_class(self, node, NULL)) {

      cls = (demux_class_t *)node->plugin_class;

      if((exts = cls->get_extensions(cls)) && *exts)
	len += strlen(exts) + 1;
    }
  }

  /* create output */
  str = malloc (len); /* '\0' space is already counted in the previous loop */
  pos = 0;

  list_size = xine_sarray_size (catalog->plugin_lists[PLUGIN_DEMUX - 1]);
  for (list_id = 0; list_id < list_size; list_id++) {
    demux_class_t *cls;
    const char    *e;
    int            l;
    
    node = xine_sarray_get (catalog->plugin_lists[PLUGIN_DEMUX - 1], list_id);
    if (node->plugin_class || _load_plugin_class(self, node, NULL)) {

      cls = (demux_class_t *)node->plugin_class;

      if((e = cls->get_extensions (cls)) && *e) {
	l = strlen(e);
	memcpy (&str[pos], e, l);
      
	pos += l;

	/* Don't add ' ' char at the end of str */
	if((pos + 1) < len) {
	  str[pos] = ' ';
	  pos++;
	}
      }
    }
  }

  str[pos] = 0;
  
  pthread_mutex_unlock (&catalog->lock);

  return str;
}

/* get a list of mime types supported by xine
 *
 * the pointer returned can be free()ed when no longer used */
char *xine_get_mime_types (xine_t *self) {

  plugin_catalog_t *catalog = self->plugin_catalog;
  int               len, pos;
  plugin_node_t    *node;
  char             *str;
  int               list_id, list_size;

  pthread_mutex_lock (&catalog->lock);

  /* calc length of output */

  len = 0;
  list_size = xine_sarray_size (catalog->plugin_lists[PLUGIN_DEMUX - 1]);

  for (list_id = 0; list_id < list_size; list_id++) {
    demux_class_t *cls;
    const char *s;

    node = xine_sarray_get (catalog->plugin_lists[PLUGIN_DEMUX - 1], list_id);
    if (node->plugin_class || _load_plugin_class(self, node, NULL)) {

      cls = (demux_class_t *)node->plugin_class;

      s = cls->get_mimetypes (cls);
      if (s)
	len += strlen(s);
    }
  }

  /* create output */

  str = malloc (len+1);
  pos = 0;

  list_size = xine_sarray_size (catalog->plugin_lists[PLUGIN_DEMUX - 1]);

  for (list_id = 0; list_id < list_size; list_id++) {
    demux_class_t *cls;
    const char *s;
    int l;

    node = xine_sarray_get (catalog->plugin_lists[PLUGIN_DEMUX - 1], list_id);
    if (node->plugin_class || _load_plugin_class(self, node, NULL)) {

      cls = (demux_class_t *)node->plugin_class;

      s = cls->get_mimetypes (cls);
      if (s) {
	l = strlen(s);
	memcpy (&str[pos], s, l);

	pos += l;
      }
    }
  }

  str[pos] = 0;
  
  pthread_mutex_unlock (&catalog->lock);

  return str;
}


/* get the demuxer identifier that handles a given mime type
 *
 * the pointer returned can be free()ed when no longer used
 * returns NULL if no demuxer is available to handle this. */
char *xine_get_demux_for_mime_type (xine_t *self, const char *mime_type) {

  plugin_catalog_t *catalog = self->plugin_catalog;
  plugin_node_t    *node;
  char             *id = NULL;
  char             *mime_arg, *mime_demux;
  char             *s;
  const char       *mt;
  int               list_id, list_size;

  /* create a copy and convert to lower case */  
  mime_arg = strdup(mime_type);
  for(s=mime_arg; *s; s++)
    *s = tolower(*s);
  
  pthread_mutex_lock (&catalog->lock);

  list_size = xine_sarray_size (catalog->plugin_lists[PLUGIN_DEMUX - 1]);

  for (list_id = 0; (list_id < list_size) && !id; list_id++) {
    demux_class_t *cls;

    node = xine_sarray_get (catalog->plugin_lists[PLUGIN_DEMUX - 1], list_id);
    if (node->plugin_class || _load_plugin_class(self, node, NULL)) {

      cls = (demux_class_t *)node->plugin_class;

      mt = cls->get_mimetypes (cls);
      if (mt) {
	mime_demux = strdup(mt);
      
	for(s=mime_demux; *s; s++)
	  *s = tolower(*s);
      
	if( strstr(mime_demux, mime_arg) )
	  id = strdup(node->info->id);
      
	free(mime_demux);
      }
    }
  }

  pthread_mutex_unlock (&catalog->lock);

  free(mime_arg);
  
  return id;
}


static void dispose_plugin_list (xine_sarray_t *list) {

  plugin_node_t  *node;
  decoder_info_t *decoder_info;
  int             list_id, list_size;

  if (list) {
    
    list_size = xine_sarray_size (list);

    for (list_id = 0; list_id < list_size; list_id++) {
      node = xine_sarray_get (list, list_id);

      if (node->ref == 0)
	_dispose_plugin_class(node);
      else {
	lprintf("node \"%s\" still referenced %d time(s)\n", node->info->id, node->ref);
	continue;
      }

      /* free special info */
      switch (node->info->type & PLUGIN_TYPE_MASK) {
      case PLUGIN_SPU_DECODER:
      case PLUGIN_AUDIO_DECODER:
      case PLUGIN_VIDEO_DECODER:
	decoder_info = (decoder_info_t *)node->info->special_info;

	free (decoder_info->supported_types);
	
      default:
	free ((void *)node->info->special_info);
	break;
      }
      
      /* free info structure and string copies */
      free (node->info->id);
      free (node->info);
      free (node);
    }
    xine_sarray_delete(list);
  }
}


static void dispose_plugin_file_list (xine_list_t *list) {
  plugin_file_t        *file;
  xine_list_iterator_t  ite;

  ite = xine_list_front (list);
  while (ite) {
    file = xine_list_get_value (list, ite);
    free (file->filename);
    free (file);
    ite = xine_list_next (list, ite);
  }
  xine_list_delete (list);
}


/*
 * dispose all currently loaded plugins (shutdown)
 */

void _x_dispose_plugins (xine_t *this) {

  if(this->plugin_catalog) {
    int i;

    for (i = 0; i < PLUGIN_TYPE_MAX; i++) {
      dispose_plugin_list (this->plugin_catalog->plugin_lists[i]);
    }

    dispose_plugin_list (this->plugin_catalog->cache_list);
    dispose_plugin_file_list (this->plugin_catalog->file_list);

    for (i = 0; this->plugin_catalog->prio_desc[i]; i++)
      free(this->plugin_catalog->prio_desc[i]);

    pthread_mutex_destroy(&this->plugin_catalog->lock);

    free (this->plugin_catalog);
  }
}
