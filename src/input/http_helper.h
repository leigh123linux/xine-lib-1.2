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
 * URL helper functions
 *
 * $Id: http_helper.h,v 1.3 2004/12/24 01:59:12 dsalt Exp $ 
 */

#ifndef HTTP_HELPER_H
#define HTTP_HELPER_H

/*
 * url parser
 * {proto}://{user}:{password}@{host}:{port}{uri}
 * {proto}://{user}:{password}@{[host]}:{port}{uri}
 *
 * return:
 *   0  invalid url
 *   1  valid url
 */
int _x_parse_url (char *url, char **proto, char** host, int *port,
                  char **user, char **password, char **uri);

/*
 * canonicalise url, given base
 * base must be valid according to _x_parse_url
 * url may only contain "://" if it's absolute
 *
 * return:
 *   the canonicalised URL (caller must free() it)
 */
char *_x_canonicalise_url (const char *base, const char *url);

#endif /* HTTP_HELPER_H */
