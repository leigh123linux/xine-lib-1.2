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

#ifndef HAVE_XINE_TICKETS_H
#define HAVE_XINE_TICKETS_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * xine thread tickets.
 * This is similar to a rw lock in "prefer writers" mode.
 * - Shared recursive read lock: acquire () / release ()
 * - Recursive write lock:       revoke ()  / issue ()
 * plus there are a few special hacks.
 * - Low overhead temporary read unlock: renew ()
 *   (better than release () + acquire ()).
 *   Mutex less check ticket_revoked whether it is time to do so.
 *   Yes this may delay writers a bit due to multicore data cache
 *   sync issues (and your check interval, of course) but since
 *   writing is rare we accept that.
 *   Alternatively, register a revoke notify callback that stops
 *   waiting for something else.
 * - Read lock with priority over writers: "irrevocable"
 *   (use for known quick actions only).
 * - Top level uninterruptible write lock: "atomic".
 * Note that the write side is for engine internal use.
 * It now also supports turning an already held read into write
 * temporarily.
 */

typedef void xine_ticket_revoke_cb_t (void *user_data, int flags);
typedef struct xine_ticket_s xine_ticket_t;

struct xine_ticket_s {

  /* the ticket owner must assure to check for ticket revocation in
   * intervals of finite length; this means that you must release
   * the ticket before any operation that might block
   *
   * you must never write to this member directly
   */
  int    ticket_revoked;

  /* apply for a ticket; between acquire and relese of an irrevocable
   * ticket (be sure to pair them properly!), it is guaranteed that you
   * will never be blocked by ticket revocation */
  void (*acquire)(xine_ticket_t *self, int irrevocable);

  /* give a ticket back */
  void (*release)(xine_ticket_t *self, int irrevocable);

  /* renew a ticket, when it has been revoked, see ticket_revoked above;
   * irrevocable must be set to one, if your thread might have acquired
   * irrevocable tickets you don't know of; set it to zero only when
   * you know that this is impossible. */
  void (*renew)(xine_ticket_t *self, int irrevocable);

#ifdef XINE_ENGINE_INTERNAL
  /* XXX: port rewiring in the middle of a decoder loop iteration is a bad idea.
   * We could make that halfway safe by (un)referencing post ports consistently.
   * Unfortunately, post plugins intercept our calls at will.
   * Even worse, we ourselves told them to inline _x_post_rewire () then
   * still use the previous port :-/
   * KLUDGE:
   * - We now have 2 levels of revocation, plain and rewire.
   * - If there is a pending rewire, ticket_revoked has the rewire flag set.
   * - A pending or incoming rewire revoke repels plain renewers, so they
   *   can advance to a code location more suitable for a rewire, then
   *   renew again with rewire flag set.
   * - Output layer get funcs return an emergency buffer/frame if engine is paused.
   *   Why the f**k didnt we allow NULL there??
   * - Decoder loops let the full thing go right before next iteration. */
#define XINE_TICKET_FLAG_ATOMIC 1
#define XINE_TICKET_FLAG_REWIRE 2

  /* allow handing out new tickets. atomic needs to be same as below. */
  void (*issue)(xine_ticket_t *self, int flags);

  /* revoke all tickets and deny new ones;
   * a pair of atomic revoke and issue cannot be interrupted by another
   * revocation or by other threads acquiring tickets.
   * set rewire flag to also do that lock. */
  void (*revoke)(xine_ticket_t *self, int flags);

  /* behaves like acquire() but doesn't block the calling thread; when
   * the thread would have been blocked, 0 is returned otherwise 1
   * this function acquires a ticket even if ticket revocation is active */
  int (*acquire_nonblocking)(xine_ticket_t *self, int irrevocable);

  /* behaves like release() but doesn't block the calling thread; should
   * be used in combination with acquire_nonblocking() */
  void (*release_nonblocking)(xine_ticket_t *self, int irrevocable);

  /* forbid port rewiring for critical code sections. */
  int (*lock_port_rewiring)(xine_ticket_t *self, int ms_timeout);
  void (*unlock_port_rewiring)(xine_ticket_t *self);

  void (*dispose)(xine_ticket_t *self);

  /* (un)register a revoke notify callback, telling the current revoke flags.
   * Return from it does not * imply anything about the ticket itself,
   * it just shall shorten wait.
   * Note that unregister needs the same data pointer as well to
   * handle multiple instances of the same object. */
  void (*revoke_cb_register)  (xine_ticket_t *self, xine_ticket_revoke_cb_t *cb, void *user_data);
  void (*revoke_cb_unregister)(xine_ticket_t *self, xine_ticket_revoke_cb_t *cb, void *user_data);
#endif
};

#ifdef __cplusplus
}
#endif

#endif
