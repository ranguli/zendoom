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
// Network server code
//

/** @file server.h
 *
 */

#ifndef NET_SERVER_H
#define NET_SERVER_H

/** @brief Initialize server and wait for connections.
 * \ingroup server
 */

void net_server_init(void);

/** Actually run the server (check for new packets received etc.)
 * \ingroup server
 */

void net_server_run(void);

/** @brief Shut down the server.
 *  Blocks until all clients disconnect, or until a 5 second timeout.
 *  \ingroup server
 */

void net_server_shutdown(void);

/** @brief Add a network module to the context used by the server
 * \ingroup server
 */

void net_server_AddModule(net_module_t *module);

/** @brief Register server with master server.
 * \ingroup server
 * \deprecated This is used to register with the Chocolate Doom master server.
 */

void net_server_register_with_master(void);

#endif /* #ifndef NET_SERVER_H */
