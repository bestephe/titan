/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2014 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  Linux NICS <linux.nics@intel.com>
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

//TODO: It is likely that not all of these are necessary
#include <linux/types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/sctp.h>
#include <linux/pkt_sched.h>
#include <linux/ipv6.h>
#include <linux/slab.h>
#include <net/checksum.h>
#include <net/ip6_checksum.h>
#include <net/tcp.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if.h>
#include <linux/if_vlan.h>
#include <linux/if_macvlan.h>
#include <linux/if_bridge.h>
#include <linux/prefetch.h>
#include <scsi/fc/fc_fcoe.h>
#include <net/vxlan.h>

#include "ixgbe_xmit_batch.h"
#include "fpp.h"

// batch_index must be declared outside of the xmit functions
static int batch_index = 0;

static netdev_tx_t ixgbe_xmit_batch_map(struct ixgbe_adapter *adapter,
                                        struct ixgbe_ring *tx_ring)
{
    /* G-opt style (FPP) prefetching variables */
    int i[IXGBE_MAX_XMIT_BATCH_SIZE];
    struct ixgbe_skb_batch_data *cur_skb_data[IXGBE_MAX_XMIT_BATCH_SIZE];
    struct ixgbe_tx_buffer *first[IXGBE_MAX_XMIT_BATCH_SIZE];
    int I = 0;  // batch index
    void *batch_rips[IXGBE_MAX_XMIT_BATCH_SIZE];
    u64 iMask = 0;
    int temp_index;

    /* iMask needs to be larger than a u64 for larger batch sizes */
    BUG_ON (IXGBE_MAX_XMIT_BATCH_SIZE > 64);

    for (temp_index = 0; temp_index < tx_ring->skb_batch_size; temp_index++) {
        batch_rips[temp_index] = &&map_fpp_start;
    }

map_fpp_start:

    cur_skb_data[I] = &tx_ring->skb_batch[I];

    for (i[I] = 0; i[I] < tx_ring->skb_batch_size; i[I]++) {
        FPP_PSS(cur_skb_data[I], map_fpp_label_1, tx_ring->skb_batch_size);
map_fpp_label_1:

        first[I] = &tx_ring->tx_buffer_info[cur_skb_data[I]->desc_ftu];
    }

map_fpp_end:
    batch_rips[I] = &&map_fpp_end;
    iMask = FPP_SET(iMask, I); 
    if(iMask == (1 << tx_ring->skb_batch_size) - 1) {
        return NETDEV_TX_OK;
    }
    I = (I + 1) % tx_ring->skb_batch_size;
    goto *batch_rips[I];

}

static netdev_tx_t ixgbe_xmit_batch_purge(struct ixgbe_adapter *adapter,
                                          struct ixgbe_ring *tx_ring)
{
    netdev_tx_t ret = NETDEV_TX_OK;

    /* Quit early if there are no skb's */
    if (tx_ring->skb_batch_size == 0)
        return NETDEV_TX_OK;

    /* Use the appropriate batched code for this driver configuration */
    if (use_sgseg) {
        //TODO
        BUG ();
    } else if (use_pkt_ring) {
        //TODO
        BUG ();
    } else {
        ret = ixgbe_xmit_batch_map(adapter, tx_ring);
    }

    /* Reset the batch variables. */
    tx_ring->skb_batch_size = 0;
    tx_ring->skb_batch_desc_count = 0;
    tx_ring->skb_batch_hr_count = 0;
    tx_ring->skb_batch_pktr_count = 0;

    return ret;
}

netdev_tx_t ixgbe_xmit_frame_ring_batch(struct sk_buff *skb,
			                struct ixgbe_adapter *adapter,
			                struct ixgbe_ring *tx_ring)
{
    struct ixgbe_skb_batch_data *skb_batch_data = NULL;
    netdev_tx_t purge_ret;
    u16 skb_desc_count = 0;
    u16 batch_desc_count = 0;
    u16 hr_count = 1;
    u16 pktr_count = 1;

    /* Sanity check the configuration. */
    BUG_ON (!xmit_batch);

    /* We should never run out of space in skb_batch */
    BUG_ON (tx_ring->skb_batch_size >= IXGBE_MAX_XMIT_BATCH_SIZE);

    /* Get the number of descriptors required to send the whole batch. This
     * count is dependent on how we are enqueueing skbs. */
    //XXX: There should also be an option to dynamically switch between
    // descriptor approaches based on whether multiple sockets are sharing
    // the batch or not. 
    if (use_sgseg) {
        skb_desc_count = ixgbe_txd_count_sgsegs(skb);
    } else if (use_pkt_ring) {
        skb_desc_count = ixgbe_txd_count_pktring(skb);
    } else {
        skb_desc_count = ixgbe_txd_count(skb);
    }

    /* Count the number of other ring entries that will be used */
    hr_count = (skb_shinfo(skb)->gso_segs); // -1 if the first packet
                                            // uses the existing header
    pktr_count = skb_shinfo(skb)->gso_segs;

    // This should always hold now, but may not in the future. */
    BUG_ON((tx_ring->skb_batch_hr_count + hr_count) > 
        ixgbe_hr_unused(tx_ring));
    BUG_ON((tx_ring->skb_batch_pktr_count + pktr_count) >
        ixgbe_pktr_unused(tx_ring));

    batch_desc_count = tx_ring->skb_batch_desc_count + skb_desc_count + 2;

    /* Quit early if we really can't enqueue this segment. This is bad
     * because we should've sent the batch earlier if we thought there
     * wouldn't be enough room for another skb. */
    if (ixgbe_maybe_stop_tx(tx_ring, batch_desc_count)) {
        pr_info ("ixgbe_xmit_frame_ring_batch: purging and returning NETDEV_TX_BUSY.\n");
        pr_info (" batch_desc_count: %d\n, ixgbe_desc_unused(tx_ring): %d\n",
                 batch_desc_count, ixgbe_desc_unused(tx_ring));

        /* Purge the current batch */
        purge_ret = ixgbe_xmit_batch_purge(adapter, tx_ring);
        BUG_ON (purge_ret != NETDEV_TX_OK);

        /* Return busy (making sure to ignore the current skb) */
        tx_ring->tx_stats.tx_busy++;
        return NETDEV_TX_BUSY;
    }

    /* Get the skb data to use */
    skb_batch_data = &tx_ring->skb_batch[tx_ring->skb_batch_size];
    tx_ring->skb_batch_size++;

    /* Save per-skb metadata and update the descriptor rings. */
    skb_batch_data->desc_count = skb_desc_count;
    skb_batch_data->desc_ftu = tx_ring->next_to_use;
    tx_ring->skb_batch_desc_count += skb_desc_count;
    tx_ring->next_to_use = (tx_ring->next_to_use + skb_desc_count) % 
        tx_ring->count; //XXX: Is this math right?
    if (use_sgseg) {
        skb_batch_data->hr_count = hr_count;
        skb_batch_data->hr_ftu = tx_ring->hr_next_to_use;
        tx_ring->skb_batch_hr_count += hr_count;
        tx_ring->hr_next_to_use = (tx_ring->hr_next_to_use + hr_count) % 
            tx_ring->hr_count; //XXX: check math
    } else if (use_pkt_ring) {
        skb_batch_data->pktr_count = pktr_count;
        skb_batch_data->pktr_ftu = tx_ring->pktr_next_to_use;
        tx_ring->skb_batch_pktr_count += pktr_count;
        tx_ring->pktr_next_to_use = (tx_ring->pktr_next_to_use + pktr_count) % 
            tx_ring->pktr_count; //XXX: check math
    }

    /* Check if the batch should be sent now or not. */
    //TODO: if we think that the next skb will cause us to be over the limit,
    // we should send the batch now.
    if (!skb->xmit_more || ixgbe_maybe_stop_tx(tx_ring, DESC_NEEDED)) {
        /* Purge the current batch */
        purge_ret = ixgbe_xmit_batch_purge(adapter, tx_ring);
        BUG_ON (purge_ret != NETDEV_TX_OK);
    }

    return NETDEV_TX_OK;
}


