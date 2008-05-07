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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * config file management
 */

#ifndef HAVE_CONFIGFILE_H
#define HAVE_CONFIGFILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>

#ifdef XINE_COMPILE
#  include "xine.h"
#else
#  include <xine.h>
#endif

#define CONFIG_FILE_VERSION 2

/*
 * config entries above this experience 
 * level must never be changed from MRL
 */
#define XINE_CONFIG_SECURITY 30
	

typedef struct cfg_entry_s cfg_entry_t;
typedef struct config_values_s config_values_t;

struct cfg_entry_s {
  cfg_entry_t     *next;
  config_values_t *config;

  char            *key;
  int              type;

  /* type unknown */
  char            *unknown_value;

  /* type string */
  char            *str_value;
  char            *str_default;

  /* common to range, enum, num, bool: */

  int              num_value;
  int              num_default;

  /* type range specific: */
  int              range_min;
  int              range_max;

  /* type enum specific: */
  char           **enum_values;

  /* help info for the user */
  char            *description;
  char            *help;

  /* user experience level */
  int              exp_level;

  /* callback function and data for live changeable values */
  xine_config_cb_t callback;
  void            *callback_data;
};

struct config_values_s {

  /*
   * register config values
   *
   * these functions return the current value of the
   * registered item, i.e. the default value if it was
   * not found in the config file or the current value
   * from the config file otherwise
   */

  char* (*register_string) (config_values_t *self,
			    const char *key,
			    const char *def_value,
			    const char *description,
			    const char *help,
			    int exp_level,
			    xine_config_cb_t changed_cb,
			    void *cb_data);

  char* (*register_filename) (config_values_t *self,
			      const char *key,
			      const char *def_value,
			      int req_type,
			      const char *description,
			      const char *help,
			      int exp_level,
			      xine_config_cb_t changed_cb,
			      void *cb_data);

  int (*register_range) (config_values_t *self,
			 const char *key,
			 int def_value,
			 int min, int max,
			 const char *description,
			 const char *help,
			 int exp_level,
			 xine_config_cb_t changed_cb,
			 void *cb_data);

  int (*register_enum) (config_values_t *self,
			const char *key,
			int def_value,
			char **values,
			const char *description,
			const char *help,
			int exp_level,
			xine_config_cb_t changed_cb,
			void *cb_data);

  int (*register_num) (config_values_t *self,
		       const char *key,
		       int def_value,
		       const char *description,
		       const char *help,
		       int exp_level,
		       xine_config_cb_t changed_cb,
		       void *cb_data);

  int (*register_bool) (config_values_t *self,
			const char *key,
			int def_value,
			const char *description,
			const char *help,
			int exp_level,
			xine_config_cb_t changed_cb,
			void *cb_data);

  /* convenience function to update range, enum, num and bool values */
  void (*update_num) (config_values_t *self, const char *key, int value);

  /* convenience function to update string values */
  void (*update_string) (config_values_t *self, const char *key, const char *value);

  /* small utility function for enum handling */
  int (*parse_enum) (const char *str, const char **values);

  /*
   * lookup config entries
   *
   * remember to call the changed_cb if it exists
   * and you changed the value of this item
   */

  cfg_entry_t* (*lookup_entry) (config_values_t *self, const char *key);

  /*
   * unregister callback function
   */
  void (*unregister_callback) (config_values_t *self, const char *key);

  /*
   * dispose of all config entries in memory
   */
  void (*dispose) (config_values_t *self);

  /*
   * config values are stored here:
   */
  cfg_entry_t         *first, *last, *cur;

  /*
   * mutex for modification to the config
   */
  pthread_mutex_t      config_lock;
  
  /*
   * current config file's version number
   */
  int current_version;
};

/*
 * allocate and init a new xine config object
 */
config_values_t *_x_config_init (void) XINE_PROTECTED;

/*
 * interpret stream_setup part of mrls for config value changes
 */

int _x_config_change_opt(config_values_t *config, const char *opt) XINE_PROTECTED;


#ifdef __cplusplus
}
#endif

#endif

