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

/** @file query.c
 *  @brief Querying servers to find their current status.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#include "../impl/system.h"
#include "../impl/timer.h"
#include "../misc/misc.h"

#include "common.h"
#include "defs.h"
#include "io.h"
#include "packet.h"
#include "query.h"
#include "sdl.h"
#include "structrw.h"

/** @brief DNS address of the Internet master server. */
#define MASTER_SERVER_ADDRESS "master.chocolate-doom.org:2342"

/** @brief Time to wait for a response before declaring a timeout. */
#define QUERY_TIMEOUT_SECS 2

/** @brief Time to wait for secure demo signatures before declaring a timeout. */
#define SIGNATURE_TIMEOUT_SECS 5

/** @brief Number of query attempts to make before giving up on a server. */
#define QUERY_MAX_ATTEMPTS 3

/** @brief Describes what type of target the query is directed at.
 * \ingroup query
 */
typedef enum {
    /** @brief The target is a normal server. */
    QUERY_TARGET_SERVER,
    /** @brief The target is the master server. */
    QUERY_TARGET_MASTER,
    /** @brief Send a broadcast query. */
    QUERY_TARGET_BROADCAST
} query_target_type_t;

/** @brief Describes the current state of the target being queried. */
typedef enum {
    QUERY_TARGET_QUEUED,    /** @brief Query not yet sent. */
    QUERY_TARGET_QUERIED,   /** @brief Query sent, waiting response. */
    QUERY_TARGET_RESPONDED, /** @brief Response received. */
    QUERY_TARGET_NO_RESPONSE
} query_target_state_t;

typedef struct {
    query_target_type_t type;
    query_target_state_t state;
    net_addr_t *addr;
    net_querydata_t data;
    unsigned int ping_time;
    unsigned int query_time;
    unsigned int query_attempts;
    boolean printed;
} query_target_t;

static boolean registered_with_master = false;
static boolean got_master_response = false;

static net_context_t *query_context;
static query_target_t *targets;
static int num_targets;

static boolean query_loop_running = false;
static boolean printed_header = false;
static int last_query_time = 0;

net_addr_t *net_query_resolve_master(net_context_t *context) {
    net_addr_t *addr;

    addr = net_resolve_address(context, MASTER_SERVER_ADDRESS);

    if (addr == NULL) {
        fprintf(stderr,
                "Warning: Failed to resolve address "
                "for master server: %s\n",
                MASTER_SERVER_ADDRESS);
    }

    return addr;
}

void net_query_add_to_master(net_addr_t *master_addr) {
    net_packet_t *packet;

    packet = net_new_packet(10);
    NET_WriteInt16(packet, NET_MASTER_PACKET_TYPE_ADD);
    NET_SendPacket(master_addr, packet);
    NET_FreePacket(packet);
}

// Process a packet received from the master server.

void net_query_AddResponse(net_packet_t *packet) {
    unsigned int result;

    if (!net_read_int16(packet, &result)) {
        return;
    }

    if (result != 0) {
        // Only show the message once.

        if (!registered_with_master) {
            printf("Registered with master server at %s\n", MASTER_SERVER_ADDRESS);
            registered_with_master = true;
        }
    } else {
        // Always show rejections.

        printf("Failed to register with master server at %s\n", MASTER_SERVER_ADDRESS);
    }

    got_master_response = true;
}

boolean net_query_CheckAddedToMaster(boolean *result) {
    // Got response from master yet?

    if (!got_master_response) {
        return false;
    }

    *result = registered_with_master;
    return true;
}

/** @brief Send a query to the master server.
 * \ingroup server
 */

static void net_query_SendMasterQuery(net_addr_t *addr) {
    net_packet_t *packet;

    packet = net_new_packet(4);
    NET_WriteInt16(packet, NET_MASTER_PACKET_TYPE_QUERY);
    NET_SendPacket(addr, packet);
    NET_FreePacket(packet);

    // We also send a NAT_HOLE_PUNCH_ALL packet so that servers behind
    // NAT gateways will open themselves up to us.
    packet = net_new_packet(4);
    NET_WriteInt16(packet, NET_MASTER_PACKET_TYPE_NAT_HOLE_PUNCH_ALL);
    NET_SendPacket(addr, packet);
    NET_FreePacket(packet);
}

void NET_RequestHolePunch(net_context_t *context, net_addr_t *addr) {
    net_addr_t *master_addr;
    net_packet_t *packet;

    master_addr = net_query_resolve_master(context);
    if (master_addr == NULL) {
        return;
    }

    packet = net_new_packet(32);
    NET_WriteInt16(packet, NET_MASTER_PACKET_TYPE_NAT_HOLE_PUNCH);
    NET_WriteString(packet, NET_AddrToString(addr));
    NET_SendPacket(master_addr, packet);

    NET_FreePacket(packet);
    NET_ReleaseAddress(master_addr);
}

// Given the specified address, find the target associated.  If no
// target is found, and 'create' is true, a new target is created.

static query_target_t *net_get_target_for_addr(net_addr_t *addr, boolean create) {
    query_target_t *target;
    int i;

    for (i = 0; i < num_targets; ++i) {
        if (targets[i].addr == addr) {
            return &targets[i];
        }
    }

    if (!create) {
        return NULL;
    }

    targets = I_Realloc(targets, sizeof(query_target_t) * (num_targets + 1));

    target = &targets[num_targets];
    target->type = QUERY_TARGET_SERVER;
    target->state = QUERY_TARGET_QUEUED;
    target->printed = false;
    target->query_attempts = 0;
    target->addr = addr;
    NET_ReferenceAddress(addr);
    ++num_targets;

    return target;
}

static void net_free_targets(void) {
    int i;

    for (i = 0; i < num_targets; ++i) {
        NET_ReleaseAddress(targets[i].addr);
    }
    free(targets);
    targets = NULL;
    num_targets = 0;
}

/** @brief Transmit a query packet
 *  @arg \c addr The address of the recipient
 */

static void net_query_SendQuery(net_addr_t *addr) {
    net_packet_t *request;

    request = net_new_packet(10);
    NET_WriteInt16(request, NET_PACKET_TYPE_QUERY);

    if (addr == NULL) {
        NET_SendBroadcast(query_context, request);
    } else {
        NET_SendPacket(addr, request);
    }

    NET_FreePacket(request);
}

static void net_query_ParseResponse(net_addr_t *addr, net_packet_t *packet, net_query_callback_t callback,
                                    void *usedata) {
    unsigned int packet_type;
    net_querydata_t querydata;
    query_target_t *target;

    // Read the header

    if (!net_read_int16(packet, &packet_type) || packet_type != NET_PACKET_TYPE_QUERY_RESPONSE) {
        return;
    }

    // Read query data

    if (!NET_ReadQueryData(packet, &querydata)) {
        return;
    }

    // Find the target that responded.

    target = net_get_target_for_addr(addr, false);

    // If the target is not found, it may be because we are doing
    // a LAN broadcast search, in which case we need to create a
    // target for the new responder.

    if (target == NULL) {
        query_target_t *broadcast_target;

        broadcast_target = net_get_target_for_addr(NULL, false);

        // Not in broadcast mode, unexpected response that came out
        // of nowhere. Ignore.

        if (broadcast_target == NULL || broadcast_target->state != QUERY_TARGET_QUERIED) {
            return;
        }

        // Create new target.

        target = net_get_target_for_addr(addr, true);
        target->state = QUERY_TARGET_QUERIED;
        target->query_time = broadcast_target->query_time;
    }

    if (target->state != QUERY_TARGET_RESPONDED) {
        target->state = QUERY_TARGET_RESPONDED;
        memcpy(&target->data, &querydata, sizeof(net_querydata_t));

        // Calculate RTT.

        target->ping_time = I_GetTimeMS() - target->query_time;

        // Invoke callback to signal that we have a new address.

        callback(addr, &target->data, target->ping_time, usedata);
    }
}

// Parse a response packet from the master server.

static void net_query_parse_master_response(net_addr_t *master_addr, net_packet_t *packet) {
    unsigned int packet_type;
    query_target_t *target;

    // Read the header.  We are only interested in query responses.

    if (!net_read_int16(packet, &packet_type) || packet_type != NET_MASTER_PACKET_TYPE_QUERY_RESPONSE) {
        return;
    }

    // Read a list of strings containing the addresses of servers
    // that the master knows about.

    for (;;) {
        char *addr_str;
        net_addr_t *addr;

        addr_str = NET_ReadString(packet);

        if (addr_str == NULL) {
            break;
        }

        // Resolve address and add to targets list if it is not already
        // there.

        addr = net_resolve_address(query_context, addr_str);
        if (addr != NULL) {
            net_get_target_for_addr(addr, true);
            NET_ReleaseAddress(addr);
        }
    }

    // Mark the master as having responded.

    target = net_get_target_for_addr(master_addr, true);
    target->state = QUERY_TARGET_RESPONDED;
}

static void net_query_parse_packet(net_addr_t *addr, net_packet_t *packet, net_query_callback_t callback,
                                  void *usedata) {
    query_target_t *target;

    // This might be the master server responding.

    target = net_get_target_for_addr(addr, false);

    if (target != NULL && target->type == QUERY_TARGET_MASTER) {
        net_query_parse_master_response(addr, packet);
    } else {
        net_query_ParseResponse(addr, packet, callback, usedata);
    }
}

static void net_query_get_response(net_query_callback_t callback, void *usedata) {
    net_addr_t *addr;
    net_packet_t *packet;

    if (NET_RecvPacket(query_context, &addr, &packet)) {
        net_query_parse_packet(addr, packet, callback, usedata);
        NET_ReleaseAddress(addr);
        NET_FreePacket(packet);
    }
}

// Find a target we have not yet queried and send a query.

static void net_send_one_query(void) {
    unsigned int now;
    unsigned int i;

    now = I_GetTimeMS();

    // Rate limit - only send one query every 50ms.

    if (now - last_query_time < 50) {
        return;
    }

    for (i = 0; i < num_targets; ++i) {
        // Not queried yet?
        // Or last query timed out without a response?

        if (targets[i].state == QUERY_TARGET_QUEUED ||
            (targets[i].state == QUERY_TARGET_QUERIED &&
             now - targets[i].query_time > QUERY_TIMEOUT_SECS * 1000)) {
            break;
        }
    }

    if (i >= num_targets) {
        return;
    }

    // Found a target to query.  Send a query; how to do this depends on
    // the target type.

    switch (targets[i].type) {
    case QUERY_TARGET_SERVER:
        net_query_SendQuery(targets[i].addr);
        break;

    case QUERY_TARGET_BROADCAST:
        net_query_SendQuery(NULL);
        break;

    case QUERY_TARGET_MASTER:
        net_query_SendMasterQuery(targets[i].addr);
        break;
    }

    // printf("Queried %s\n", NET_AddrToString(targets[i].addr));
    targets[i].state = QUERY_TARGET_QUERIED;
    targets[i].query_time = now;
    ++targets[i].query_attempts;

    last_query_time = now;
}

// Time out servers that have been queried and not responded.

static void net_check_target_timeouts(void) {
    unsigned int i;
    unsigned int now;

    now = I_GetTimeMS();

    for (i = 0; i < num_targets; ++i) {
        /*
        printf("target %i: state %i, queries %i, query time %i\n",
               i, targets[i].state, targets[i].query_attempts,
               now - targets[i].query_time);
        */

        // We declare a target to be "no response" when we've sent
        // multiple query packets to it (QUERY_MAX_ATTEMPTS) and
        // received no response to any of them.

        if (targets[i].state == QUERY_TARGET_QUERIED && targets[i].query_attempts >= QUERY_MAX_ATTEMPTS &&
            now - targets[i].query_time > QUERY_TIMEOUT_SECS * 1000) {
            targets[i].state = QUERY_TARGET_NO_RESPONSE;

            if (targets[i].type == QUERY_TARGET_MASTER) {
                fprintf(stderr, "NET_MasterQuery: no response "
                                "from master server.\n");
            }
        }
    }
}

// If all targets have responded or timed out, returns true.

static boolean net_all_targets_done(void) {
    unsigned int i;

    for (i = 0; i < num_targets; ++i) {
        if (targets[i].state != QUERY_TARGET_RESPONDED && targets[i].state != QUERY_TARGET_NO_RESPONSE) {
            return false;
        }
    }

    return true;
}

// Polling function, invoked periodically to send queries and
// interpret new responses received from remote servers.
// Returns zero when the query sequence has completed and all targets
// have returned responses or timed out.

int net_query_Poll(net_query_callback_t callback, void *usedata) {
    net_check_target_timeouts();

    // Send a query.  This will only send a single query at once.

    net_send_one_query();

    // Check for a response

    net_query_get_response(callback, usedata);

    return !net_all_targets_done();
}

// Stop the query loop

static void net_query_ExitLoop(void) { query_loop_running = false; }

// Loop waiting for responses.
// The specified callback is invoked when a new server responds.

static void net_query_QueryLoop(net_query_callback_t callback, void *usedata) {
    query_loop_running = true;

    while (query_loop_running && net_query_Poll(callback, usedata)) {
        // Don't thrash the CPU

        I_Sleep(1);
    }
}

void net_query_Init(void) {
    if (query_context == NULL) {
        query_context = NET_NewContext();
        NET_AddModule(query_context, &net_sdl_module);
        net_sdl_module.InitClient();
    }

    free(targets);
    targets = NULL;
    num_targets = 0;

    printed_header = false;
}

// Callback that exits the query loop when the first server is found.

static void net_query_ExitCallback() { net_query_ExitLoop(); }

/** @brief Search the targets list and find a target that has responded.
 *  @retval NULL If return value is NULL, no targets responded to the query.
 */

static query_target_t *net_find_first_responder(void) {
    unsigned int i;

    for (i = 0; i < num_targets; ++i) {
        if (targets[i].type == QUERY_TARGET_SERVER && targets[i].state == QUERY_TARGET_RESPONDED) {
            return &targets[i];
        }
    }

    return NULL;
}

// Return a count of the number of responses.

static int GetNumResponses(void) {
    unsigned int i;
    int result;

    result = 0;

    for (i = 0; i < num_targets; ++i) {
        if (targets[i].type == QUERY_TARGET_SERVER && targets[i].state == QUERY_TARGET_RESPONDED) {
            ++result;
        }
    }

    return result;
}

int net_start_lan_query(void) {
    query_target_t *target;

    net_query_Init();

    // Add a broadcast target to the list.

    target = net_get_target_for_addr(NULL, true);
    target->type = QUERY_TARGET_BROADCAST;

    return 1;
}

int net_start_master_query(void) {
    net_addr_t *master;
    query_target_t *target;

    net_query_Init();

    // Resolve master address and add to targets list.

    master = net_query_resolve_master(query_context);

    if (master == NULL) {
        return 0;
    }

    target = net_get_target_for_addr(master, true);
    target->type = QUERY_TARGET_MASTER;
    NET_ReleaseAddress(master);

    return 1;
}

// -----------------------------------------------------------------------

static void formatted_printf(int wide, const char *s, ...) PRINTF_ATTR(2, 3);
static void formatted_printf(int wide, const char *s, ...) {
    va_list args;
    int i;

    va_start(args, s);
    i = vprintf(s, args);
    va_end(args);

    while (i < wide) {
        putchar(' ');
        ++i;
    }
}

static const char *GameDescription(GameMode_t mode, GameMission_t mission) {
    switch (mission) {
    case doom:
        if (mode == shareware)
            return "swdoom";
        else if (mode == registered)
            return "regdoom";
        else
            return "doom";
    default:
        return "?";
    }
}

static void PrintHeader(void) {
    int i;

    putchar('\n');
    formatted_printf(5, "Ping");
    formatted_printf(18, "Address");
    formatted_printf(8, "Players");
    puts("Description");

    for (i = 0; i < 70; ++i)
        putchar('=');
    putchar('\n');
}

// Callback function that just prints information in a table.

static void net_queryPrintCallback(net_addr_t *addr, net_querydata_t *data, unsigned int ping_time) {
    // If this is the first server, print the header.

    if (!printed_header) {
        PrintHeader();
        printed_header = true;
    }

    formatted_printf(5, "%4i", ping_time);
    formatted_printf(22, "%s", NET_AddrToString(addr));
    formatted_printf(4, "%i/%i ", data->num_players, data->max_players);

    if (data->gamemode != indetermined) {
        printf("(%s) ", GameDescription(data->gamemode, data->gamemission));
    }

    if (data->servestate) {
        printf("(game running) ");
    }

    printf("%s\n", data->description);
}

/** @brief Find servers on a local LAN **/
void NET_LANQuery(void) {
    if (net_start_lan_query()) {
        printf("\nSearching for servers on local LAN ...\n");

        net_query_QueryLoop(net_queryPrintCallback, NULL);

        printf("\n%i server(s) found.\n", GetNumResponses());
        net_free_targets();
    }
}

void NET_MasterQuery(void) {
    if (net_start_master_query()) {
        printf("\nSearching for servers on Internet ...\n");

        net_query_QueryLoop(net_queryPrintCallback, NULL);

        printf("\n%i server(s) found.\n", GetNumResponses());
        net_free_targets();
    }
}

void net_queryAddress(const char *addr_str) {
    net_addr_t *addr;
    query_target_t *target;

    net_query_Init();

    addr = net_resolve_address(query_context, addr_str);

    if (addr == NULL) {
        error("net_queryAddress: Host '%s' not found!", addr_str);
    }

    // Add the address to the list of targets.

    target = net_get_target_for_addr(addr, true);

    printf("\nQuerying '%s'...\n", addr_str);

    // Run query loop.

    net_query_QueryLoop(net_query_ExitCallback, NULL);

    // Check if the target responded.

    if (target->state == QUERY_TARGET_RESPONDED) {
        net_queryPrintCallback(addr, &target->data, target->ping_time);
        NET_ReleaseAddress(addr);
        net_free_targets();
    } else {
        error("No response from '%s'", addr_str);
    }
}

net_addr_t *NET_FindLANServer(void) {
    query_target_t *target;
    query_target_t *responder;
    net_addr_t *result;

    net_query_Init();

    // Add a broadcast target to the list.

    target = net_get_target_for_addr(NULL, true);
    target->type = QUERY_TARGET_BROADCAST;

    // Run the query loop, and stop at the first target found.

    net_query_QueryLoop(net_query_ExitCallback, NULL);

    responder = net_find_first_responder();

    if (responder != NULL) {
        result = responder->addr;
        NET_ReferenceAddress(result);
    } else {
        result = NULL;
    }

    net_free_targets();
    return result;
}
