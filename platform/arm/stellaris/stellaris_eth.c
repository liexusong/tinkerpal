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
#include "platform/arm/stellaris/stellaris_eth.h"
#include "util/tp_types.h"
#include "util/tp_misc.h"
#include "util/debug.h"
#include "util/event.h"
#include "mem/tmalloc.h"
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "inc/hw_ethernet.h"
#include "driverlib/rom_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/ethernet.h"
#include "driverlib/interrupt.h"

typedef struct {
    etherif_t ethif;
} stellaris_eth_t;

static stellaris_eth_t g_eth; /* Singleton */

static int stellaris_eth_link_status(etherif_t *ethif)
{
    return MAP_EthernetPHYRead(ETH_BASE, PHY_MR1) & PHY_MR1_LINK ? 1 : 0;
}

static int stellaris_eth_packet_size(etherif_t *ethif)
{
    return 0;
}

static int stellaris_eth_packet_recv(etherif_t *ethif, u8 *buf, int size)
{
    return 0;
}

static void stellaris_eth_packet_xmit(etherif_t *ethif, u8 *buf, int size)
{
}

void stellaris_eth_free(etherif_t *ethif)
{
    etherif_uninit(ethif);
}

static const etherif_ops_t stellaris_eth_etherif_ops = {
    .link_status = stellaris_eth_link_status,
    .packet_size = stellaris_eth_packet_size,
    .packet_recv = stellaris_eth_packet_recv,
    .packet_xmit = stellaris_eth_packet_xmit,
};

static void phy_info(void)
{
    u32 mr2, mr3;
    
    mr2 = MAP_EthernetPHYRead(ETH_BASE, PHY_MR2);
    mr3 = MAP_EthernetPHYRead(ETH_BASE, PHY_MR3);
    tp_out(("PHY OUI %x:%x\n", (mr2 & PHY_MR2_OUI_M) >> PHY_MR2_OUI_S,
	(mr3 & PHY_MR3_OUI_M) >> PHY_MR3_OUI_S));
    tp_out(("Model Number %x\n", (mr3 & PHY_MR3_MN_M) >> PHY_MR3_MN_S));
    tp_out(("Revision Number %x\n", (mr3 & PHY_MR3_RN_M) >> PHY_MR3_RN_S));
}

static void hw_init(void)
{
    MAP_SysCtlPeripheralEnable(SYSCTL_PERIPH_ETH);
    MAP_SysCtlPeripheralReset(SYSCTL_PERIPH_ETH);

    /* Disable all Ethernet Interrupts. */
    MAP_EthernetIntDisable(ETH_BASE, ETH_INT_PHY | ETH_INT_MDIO |
	ETH_INT_RXER | ETH_INT_RXOF | ETH_INT_TX | ETH_INT_TXER | ETH_INT_RX);
    /* Disable any possible pending interrupts */
    MAP_EthernetIntClear(ETH_BASE, MAP_EthernetIntStatus(ETH_BASE, false));

    /* Initialize the Ethernet Controller. */
    MAP_EthernetInitExpClk(ETH_BASE, MAP_SysCtlClockGet());

    /* Configure the Ethernet Controller */
    MAP_EthernetConfigSet(ETH_BASE, ETH_CFG_TX_DPLXEN | ETH_CFG_TX_CRCEN |
	ETH_CFG_TX_PADEN | ETH_CFG_RX_AMULEN);

    /* Enable the Ethernet Controller transmitter and receiver */
    MAP_EthernetEnable(ETH_BASE);

    /* Enable the Ethernet Interrupt handler */
    MAP_IntEnable(INT_ETH);

    /* Enable Ethernet TX and RX Packet Interrupts */
    MAP_EthernetIntEnable(ETH_BASE, ETH_INT_RX | ETH_INT_TX);
}

etherif_t *stellaris_eth_new(void)
{
    stellaris_eth_t *se = &g_eth;

    etherif_init(&se->ethif, &stellaris_eth_etherif_ops);

    hw_init();
    phy_info();

    return &se->ethif;
}
