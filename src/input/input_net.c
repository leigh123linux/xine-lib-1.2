/* 
 * Copyright (C) 2000-2001 the xine project
 * 
 * This file is part of xine, a unix video player.
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
 * Read from a tcp network stream over a lan (put a tweaked mp1e encoder the
 * other end and you can watch tv anywhere in the house ..)
 *
 * how to set up mp1e for use with this plugin:
 * 
 * use mp1 to capture the live stream, e.g.
 * mp1e -b 1200 -R 4,32 -a 0 -B 160 -v >live.mpg 
 *
 * add an extra service "xine" to /etc/services and /etc/inetd.conf, e.g.:
 * /etc/services:
 * xine       1025/tcp
 * /etc/inetd.conf:
 * xine            stream  tcp     nowait  bartscgr        /usr/sbin/tcpd /usr/bin/tail -f /home/bartscgr/Projects/inf.misc/live.mpg
 *
 * now restart inetd and you can use xine to watch the live stream, e.g.:
 * xine tcp://192.168.0.43:1025.mpg
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/time.h>

#include "xine_internal.h"
#include "xineutils.h"
#include "input_plugin.h"
#include "strict_scr.h"

extern int errno;

#if !defined(NDELAY) && defined(O_NDELAY)
#define	FNDELAY	O_NDELAY
#endif

#ifdef __GNUC__
#define LOG_MSG_STDERR(xine, message, args...) {                     \
    xine_log(xine, XINE_LOG_INPUT, message, ##args);                 \
    fprintf(stderr, message, ##args);                                \
  }
#define LOG_MSG(xine, message, args...) {                            \
    xine_log(xine, XINE_LOG_INPUT, message, ##args);                 \
    printf(message, ##args);                                         \
  }
#else
#define LOG_MSG_STDERR(xine, ...) {                                  \
    xine_log(xine, XINE_LOG_INPUT, __VA_ARGS__);                     \
    fprintf(stderr, __VA_ARGS__);                                    \
  }
#define LOG_MSG(xine, ...) {                                         \
    xine_log(xine, XINE_LOG_INPUT, __VA_ARGS__);                     \
    printf(__VA_ARGS__);                                             \
  }
#endif

#define NET_BS_LEN 2324
#define PREBUF_SIZE 100000

typedef struct {
  input_plugin_t   input_plugin;

  xine_t          *xine;
  
  int              fh;
  char            *mrl;
  config_values_t *config;

  off_t            curpos;

  int              buffering;

  strictscr_t     *scr;

} net_input_plugin_t;

/* **************************************************************** */
/*                       Private functions                          */
/* **************************************************************** */


static int host_connect_attempt(struct in_addr ia, int port, xine_t *xine) {
	int s=socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	
	struct sockaddr_in sin;
	
	fd_set wfd;
	struct timeval tv;
	
	if(s==-1)
	{
		LOG_MSG_STDERR(xine, _("socket(): %s\n"), strerror(errno));
		return -1;
	}
	
	if(fcntl(s, F_SETFL, FNDELAY)==-1)
	{
		LOG_MSG_STDERR(xine, _("fcntl(nonblocking): %s\n"), strerror(errno));
		close(s);
		return -1;
	}

	sin.sin_family = AF_INET;	
	sin.sin_addr   = ia;
	sin.sin_port   = htons(port);
	
	if(connect(s, (struct sockaddr *)&sin, sizeof(sin))==-1 && errno != EINPROGRESS)
	{
		LOG_MSG_STDERR(xine, _("connect(): %s\n"), strerror(errno));
		close(s);
		return -1;
	}	
	
	tv.tv_sec = 60;		/* We use 60 second timeouts for now */
	tv.tv_usec = 0;
	
	FD_ZERO(&wfd);
	FD_SET(s, &wfd);
	
	switch(select(s+1, NULL, &wfd, NULL, &tv))
	{
		case 0:
			/* Time out */
			close(s);
			return -1;
		case -1:
			/* Ermm.. ?? */
			LOG_MSG(xine, _("select(): %s\n"), strerror(errno));
			close(s);
			return -1;
	}
	
	return s;
}

static int host_connect(const char *host, int port, xine_t *xine) {
	struct hostent *h;
	int i;
	int s;
	
	h=gethostbyname(host);
	if(h==NULL)
	{
		LOG_MSG_STDERR(xine, _("unable to resolve '%s'.\n"), host);
		return -1;
	}
	
	
	for(i=0; h->h_addr_list[i]; i++)
	{
		struct in_addr ia;
		memcpy(&ia, h->h_addr_list[i],4);
		s = host_connect_attempt(ia, port, xine);
		if(s != -1)
			return s;
	}
	LOG_MSG_STDERR(xine, _("unable to connect to '%s'.\n"), host);
	return -1;
}
/* **************************************************************** */
/*                          END OF PRIVATES                         */
/* **************************************************************** */

/*
 *
 */
static int net_plugin_open (input_plugin_t *this_gen, char *mrl) {
  net_input_plugin_t *this = (net_input_plugin_t *) this_gen;
  char *filename;
  char *pptr;
  int port = 7658;

  this->mrl = strdup(mrl); /* FIXME: small memory leak */

  if (!strncasecmp (mrl, "tcp:",4))
    filename = (char *) &this->mrl[4];
  else
    return 0;
    
  if(strncmp(filename, "//", 2)==0)
  	filename+=2;

  pptr=strrchr(filename, ':');
  if(pptr) {
  	*pptr++=0;
  	sscanf(pptr,"%d", &port);
  }

  this->fh = host_connect(filename, port, this->xine);
  this->curpos = 0;
  this->buffering = 0;

  if (this->fh == -1) {
    return 0;
  }

  this->mrl = strdup(mrl); /* FIXME: small memory leak */

  /* register our scr plugin */

  this->xine->metronom->register_scr (this->xine->metronom, &this->scr->scr);

  return 1;
}

#define LOW_WATER_MARK  50
#define HIGH_WATER_MARK 100

static off_t net_plugin_read (input_plugin_t *this_gen, 
			      char *buf, off_t len) {
  net_input_plugin_t *this = (net_input_plugin_t *) this_gen;
  off_t n, total;
  int fifo_fill;

  fifo_fill = this->xine->video_fifo->size(this->xine->video_fifo);

  if (fifo_fill<LOW_WATER_MARK) {
    
    this->xine->metronom->set_speed (this->xine->metronom, SPEED_PAUSE);
    this->buffering = 1;
    this->scr->adjustable = 0;
    printf ("input_net: buffering...\n");

  } else if ( (fifo_fill>HIGH_WATER_MARK) && (this->buffering)) {
    this->xine->metronom->set_speed (this->xine->metronom, SPEED_NORMAL);
    this->buffering = 0;
    this->scr->adjustable = 1;
    printf ("input_net: buffering...done\n");
  }

  /*
  printf ("input_net: read at pts %d\n",
	  this->xine->metronom->get_current_time (this->xine->metronom));
  */
  /*

  if (this->curpos==0) {
    this->xine->metronom->set_speed (this->xine->metronom, SPEED_PAUSE);
    this->buffering = 1;
    printf ("input_net: buffering...\n");
  } else if ((this->curpos>PREBUF_SIZE) && this->buffering) {
    this->xine->metronom->set_speed (this->xine->metronom, SPEED_NORMAL);
    this->buffering = 0;
    printf ("input_net: buffering...finished\n");
  }
*/

  total=0;
  while (total<len){ 
    n = read (this->fh, &buf[total], len-total);
    /*
    printf ("input_net: got %lld bytes (%lld/%lld bytes read)\n",
	    n,total,len);
    */
    if (n > 0){
      this->curpos += n;
      total += n;
    }
    else if (n<0 && errno!=EAGAIN) 
      return total;
  }
  return total;
}

static buf_element_t *net_plugin_read_block (input_plugin_t *this_gen, 
					     fifo_buffer_t *fifo, off_t todo) {
  /*net_input_plugin_t   *this = (net_input_plugin_t *) this_gen; */
  buf_element_t        *buf = fifo->buffer_pool_alloc (fifo);
  int total_bytes;

  buf->content = buf->mem;
  buf->type = BUF_DEMUX_BLOCK;
  
  total_bytes = net_plugin_read (this_gen, buf->content, todo);

  if (total_bytes != todo) {
    buf->free_buffer (buf);
    return NULL;
  }

  buf->size = total_bytes;

  return buf;
}

/*
 *
 */
static off_t net_plugin_get_length (input_plugin_t *this_gen) {

  return 0;
}

/*
 *
 */
static uint32_t net_plugin_get_capabilities (input_plugin_t *this_gen) {

  return INPUT_CAP_NOCAP;
}

/*
 *
 */
static uint32_t net_plugin_get_blocksize (input_plugin_t *this_gen) {

  return NET_BS_LEN;
;
}

/*
 *
 */
static off_t net_plugin_get_current_pos (input_plugin_t *this_gen){
  net_input_plugin_t *this = (net_input_plugin_t *) this_gen;

  return this->curpos;
}

/*
 *
 */
static int net_plugin_eject_media (input_plugin_t *this_gen) {
  return 1;
}

/*
 *
 */
static void net_plugin_close (input_plugin_t *this_gen) {
  net_input_plugin_t *this = (net_input_plugin_t *) this_gen;

  close(this->fh);
  this->fh = -1;

  this->xine->metronom->unregister_scr (this->xine->metronom, &this->scr->scr);
}

/*
 *
 */
static void net_plugin_stop (input_plugin_t *this_gen) {

  net_plugin_close(this_gen);
}

/*
 *
 */
static char *net_plugin_get_description (input_plugin_t *this_gen) {
	return _("net input plugin as shipped with xine");
}

/*
 *
 */
static char *net_plugin_get_identifier (input_plugin_t *this_gen) {
  return "TCP";
}

/*
 *
 */
static char* net_plugin_get_mrl (input_plugin_t *this_gen) {
  net_input_plugin_t *this = (net_input_plugin_t *) this_gen;

  return this->mrl;
}

/*
 *
 */
static int net_plugin_get_optional_data (input_plugin_t *this_gen, 
					 void *data, int data_type) {

  return INPUT_OPTIONAL_UNSUPPORTED;
}

/*
 *
 */
input_plugin_t *init_input_plugin (int iface, xine_t *xine) {

  net_input_plugin_t *this;
  config_values_t    *config;

  if (iface != 5) {
    LOG_MSG(xine,
	    _("net input plugin doesn't support plugin API version %d.\n"
	      "PLUGIN DISABLED.\n"
	      "This means there's a version mismatch between xine and this input"
	      "plugin.\nInstalling current input plugins should help.\n"),
	    iface);
    return NULL;
  }

  this       = (net_input_plugin_t *) xine_xmalloc(sizeof(net_input_plugin_t));
  config     = xine->config;
  this->xine = xine;

  this->input_plugin.interface_version = INPUT_PLUGIN_IFACE_VERSION;
  this->input_plugin.get_capabilities  = net_plugin_get_capabilities;
  this->input_plugin.open              = net_plugin_open;
  this->input_plugin.read              = net_plugin_read;
  this->input_plugin.read_block        = net_plugin_read_block;
  this->input_plugin.seek              = NULL;
  this->input_plugin.get_current_pos   = net_plugin_get_current_pos;
  this->input_plugin.get_length        = net_plugin_get_length;
  this->input_plugin.get_blocksize     = net_plugin_get_blocksize;
  this->input_plugin.get_dir           = NULL;
  this->input_plugin.eject_media       = net_plugin_eject_media;
  this->input_plugin.get_mrl           = net_plugin_get_mrl;
  this->input_plugin.close             = net_plugin_close;
  this->input_plugin.stop              = net_plugin_stop;
  this->input_plugin.get_description   = net_plugin_get_description;
  this->input_plugin.get_identifier    = net_plugin_get_identifier;
  this->input_plugin.get_autoplay_list = NULL;
  this->input_plugin.get_optional_data = net_plugin_get_optional_data;
  this->input_plugin.is_branch_possible= NULL;

  this->fh        = -1;
  this->mrl       = NULL;
  this->config    = config;
  this->curpos    = 0;
  this->buffering = 0;

  this->scr       = strictscr_init ();
  
  return (input_plugin_t *) this;
}
