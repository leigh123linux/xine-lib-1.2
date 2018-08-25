/*
 * Copyright (C) 2018 the xine project
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

#ifndef XINE_MODULE_H
#define XINE_MODULE_H

#define XINE_MODULE_IFACE_VERSION  1

typedef struct xine_module_class_s xine_module_class_t;
typedef struct xine_module_s xine_module_t;

struct xine_module_class_s {

  /*
   * create a new instance of this plugin class
   *  - may return NULL
   */
  xine_module_t *(*get_instance)(xine_module_class_t *, const void *params);

  /**
   * @brief short human readable identifier for this plugin class
   */
  const char *identifier;

  /**
   * @brief human readable (verbose = 1 line) description for this plugin class
   *
   * The description is passed to gettext() to internationalise.
   */
  const char *description;

  /**
   * @brief Optional non-standard catalog to use with dgettext() for description.
   */
  const char *text_domain;

  /*
   * free all class-related resources
   *  - optional, may be NULL
   */

  void (*dispose)(xine_module_class_t *);
};

struct xine_module_s {

  /**
   * @brief Pointer to the loaded plugin node.
   *
   * Used by the plugins loader. It's an opaque type when using the
   * structure outside of xine's build.
   */
  struct plugin_node_s *node;

  /*
   * close down, free all resources
   */
  void (*dispose)(xine_module_t *);
};

#endif /* XINE_MODULE_H_ */
