//
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     Querying servers to find their current status.
//

#ifndef NET_QUERY_H
#define NET_QUERY_H

#include "defs.h"

typedef void (*net_query_callback_t)(net_addr_t *addr, net_querydata_t *querydata, unsigned int ping_time,
                                     void *usedata);

extern int net_start_lan_query(void);
extern int net_start_master_query(void);

extern void NET_LANQuery(void);
extern void NET_MasterQuery(void);
extern void net_queryAddress(const char *addr);
extern net_addr_t *NET_FindLANServer(void);

extern int net_query_Poll(net_query_callback_t callback, void *usedata);

/** @brief Resolve the IP address of the master server.
 *  @return IP address of the master server.
 * \ingroup net
 */

extern net_addr_t *net_query_resolve_master(net_context_t *context);

/** @brief Send a registration packet to the master server to register ourselves with the global list.
 *  @arg \c The IP address of the master server
 * \ingroup query
 */

extern void net_query_add_to_master(net_addr_t *master_addr);

extern boolean net_query_CheckAddedToMaster(boolean *result);
extern void net_query_AddResponse(net_packet_t *packet);

/** @brief Send a hole punch (direct connection) request to the master server for the server at the
 * given address
 * \ingroup query
 */
extern void NET_RequestHolePunch(net_context_t *context, net_addr_t *addr);

#endif /* #ifndef NET_QUERY_H */
