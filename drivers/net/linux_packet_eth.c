/* Copyright (c) 2013, Eyal Birger
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * The name of the author may not be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <string.h>
#include "util/debug.h"
#include "util/tp_misc.h"
#include "util/tp_types.h"
#include "mem/tmalloc.h"
#include "drivers/resources.h"
#include "drivers/serial/serial.h"
#include "drivers/net/linux_packet_eth.h"
#include "platform/unix/sim.h"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <arpa/inet.h>

typedef struct {
    etherif_t ethif;
    event_t packet_event;
    int packet_event_id;
    int packet_socket;
    u8 packet[1518];
    int packet_size;
} linux_packet_eth_t;

static linux_packet_eth_t g_lpe; /* Singleton for now */

#define ETHIF_TO_PACKET_ETH(x) container_of(x, linux_packet_eth_t, ethif)
#define NET_RES (RES(UART_RESOURCE_ID_BASE, NET_ID))
#define PACKET_ETH_RES(lpe) RES(ETHERIF_RESOURCE_ID_BASE, (lpe)->ethif.id)

static void cur_packet_dump(linux_packet_eth_t *lpe) __attribute__((unused));
static void cur_packet_dump(linux_packet_eth_t *lpe)
{
    int i;

    printf("\n-------------------------------------\n");
    for (i = 0; i < lpe->packet_size; i++)
	printf("%02x%s", lpe->packet[i], (i + 1) % 16 ? " " : "\n");
    printf("-------------------------------------\n");
}

int packet_eth_link_status(etherif_t *ethif)
{
    return 1;
}

int packet_eth_packet_size(etherif_t *ethif)
{
    return ETHIF_TO_PACKET_ETH(ethif)->packet_size;
}

int packet_eth_packet_recv(etherif_t *ethif, u8 *buf, int size)
{
    linux_packet_eth_t *lpe = ETHIF_TO_PACKET_ETH(ethif);

    if (size > lpe->packet_size)
	size = lpe->packet_size;

    memcpy(buf, lpe->packet, size);
    return lpe->packet_size;
}

void packet_eth_packet_xmit(etherif_t *ethif, u8 *buf, int size)
{
    linux_packet_eth_t *lpe = ETHIF_TO_PACKET_ETH(ethif);

    serial_write(NET_RES, (char *)buf, size);
    if (lpe->ethif.on_packet_xmit)
    {
	/* XXX: resource ID will not be provided */
	event_timer_set(0, lpe->ethif.on_packet_xmit);
    }
}

static void packet_eth_packet_event(event_t *ev, int resource_id)
{
    linux_packet_eth_t *lpe = container_of(ev, linux_packet_eth_t,
	packet_event);

    tp_debug(("Packet received\n"));
    lpe->packet_size = serial_read(NET_RES, (char *)lpe->packet,
	sizeof(lpe->packet));
    if (lpe->packet_size <= 0)
    {
	perror("read");
	return;
    }
    if (lpe->ethif.on_packet_received)
    {
	lpe->ethif.on_packet_received->trigger(lpe->ethif.on_packet_received,
	    PACKET_ETH_RES(lpe));
    }
}

static const etherif_ops_t linux_packet_eth_ops = {
    .link_status = packet_eth_link_status,
    .packet_size = packet_eth_packet_size,
    .packet_recv = packet_eth_packet_recv,
    .packet_xmit = packet_eth_packet_xmit,
};

static int packet_socket_create(const char *ifname)
{
    int sock;
    struct ifreq ifr;
    struct packet_mreq mreq;
    struct sockaddr_ll sll;

    sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sock < 0) {
	perror("Failed to create packet socket\n");
	return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, ifname);
    ioctl(sock, SIOCGIFHWADDR, &ifr);

    if (ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER) 
    {
	tp_err(("%s is not an Ethernet device (%d)\n", ifname,
	    (int)ifr.ifr_hwaddr.sa_family));
	return -1;
    }

    ioctl(sock, SIOCGIFINDEX, &ifr);

    memset(&sll, 0, sizeof(sll));
    sll.sll_family = AF_PACKET;
    sll.sll_protocol = htons(ETH_P_ALL);
    sll.sll_ifindex = ifr.ifr_ifindex;
    bind(sock, (struct sockaddr *)&sll, sizeof(sll));

    memset(&mreq, 0, sizeof(mreq));
    mreq.mr_ifindex = ifr.ifr_ifindex;
    mreq.mr_type = PACKET_MR_PROMISC;
    setsockopt(sock, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    return sock;
}

void linux_packet_eth_free(etherif_t *ethif)
{
    linux_packet_eth_t *lpe = ETHIF_TO_PACKET_ETH(ethif);

    event_watch_del(lpe->packet_event_id);
    close(lpe->packet_socket);
}

etherif_t *linux_packet_eth_new(char *dev_name)
{
    linux_packet_eth_t *lpe = &g_lpe;

    if ((lpe->packet_socket = packet_socket_create(dev_name)) < 0)
	return NULL;

    lpe->packet_event.trigger = packet_eth_packet_event;
    etherif_init(&lpe->ethif, &linux_packet_eth_ops);
    unix_sim_add_fd_event_to_map(NET_ID, lpe->packet_socket,
        lpe->packet_socket);
    lpe->packet_event_id = event_watch_set(NET_RES, &lpe->packet_event);

    printf("Created Linux Packet Ethernet Interface. fd %d\n",
	lpe->packet_socket);
    return &lpe->ethif;
}