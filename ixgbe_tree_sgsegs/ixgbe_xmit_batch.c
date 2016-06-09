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

static int ixgbe_tso_batch_safe(struct ixgbe_ring *tx_ring,
		                struct ixgbe_tx_buffer *first,
                                u16 desc_i,
		                u8 *hdr_len)
{
	struct sk_buff *skb = first->skb;
	u32 vlan_macip_lens, type_tucmd;
	u32 mss_l4len_idx, l4len;
	int err;

	if (skb->ip_summed != CHECKSUM_PARTIAL)
		return 0;

	if (!skb_is_gso(skb))
		return 0;

	err = skb_cow_head(skb, 0);
	if (err < 0)
		return err;

	/* ADV DTYP TUCMD MKRLOC/ISCSIHEDLEN */
	type_tucmd = IXGBE_ADVTXD_TUCMD_L4T_TCP;

	if (first->protocol == htons(ETH_P_IP)) {
		struct iphdr *iph = ip_hdr(skb);
		iph->tot_len = 0;
		iph->check = 0;
		tcp_hdr(skb)->check = ~csum_tcpudp_magic(iph->saddr,
							 iph->daddr, 0,
							 IPPROTO_TCP,
							 0);
		type_tucmd |= IXGBE_ADVTXD_TUCMD_IPV4;
		first->tx_flags |= IXGBE_TX_FLAGS_TSO |
				   IXGBE_TX_FLAGS_CSUM |
				   IXGBE_TX_FLAGS_IPV4;
	} else if (skb_is_gso_v6(skb)) {
		ipv6_hdr(skb)->payload_len = 0;
		tcp_hdr(skb)->check =
		    ~csum_ipv6_magic(&ipv6_hdr(skb)->saddr,
				     &ipv6_hdr(skb)->daddr,
				     0, IPPROTO_TCP, 0);
		first->tx_flags |= IXGBE_TX_FLAGS_TSO |
				   IXGBE_TX_FLAGS_CSUM;
	}

	/* compute header lengths */
	l4len = tcp_hdrlen(skb);
	*hdr_len = skb_transport_offset(skb) + l4len;

	/* update gso size and bytecount with header size */
	first->gso_segs = skb_shinfo(skb)->gso_segs;
	first->bytecount += (first->gso_segs - 1) * *hdr_len;

        //DEBUG: Is gso_segs already set correctly?
        //pr_info ("gso_segs: %d\n", first->gso_segs);

	/* mss_l4len_id: use 0 as index for TSO */
	mss_l4len_idx = l4len << IXGBE_ADVTXD_L4LEN_SHIFT;
	mss_l4len_idx |= skb_shinfo(skb)->gso_size << IXGBE_ADVTXD_MSS_SHIFT;

	/* vlan_macip_lens: HEADLEN, MACLEN, VLAN tag */
	vlan_macip_lens = skb_network_header_len(skb);
	vlan_macip_lens |= skb_network_offset(skb) << IXGBE_ADVTXD_MACLEN_SHIFT;
	vlan_macip_lens |= first->tx_flags & IXGBE_TX_FLAGS_VLAN_MASK;

        //XXX: DEBUG
        //pr_info ("ixgbe_tso_batch_safe:\n");
        //pr_info (" vlan_macip_lens: %d\n", vlan_macip_lens);
        //pr_info (" type_tucmd: %d\n", type_tucmd);
        //pr_info (" mss_l4len_idx: %d\n", mss_l4len_idx);
        //pr_info (" desc_i: %d\n", desc_i);

        /* Use the batch safe version of setting a context descriptor */
	ixgbe_tx_ctxtdesc_ntu(tx_ring, vlan_macip_lens, 0, type_tucmd,
			      mss_l4len_idx, desc_i);

        /* update first so that the context descriptor can be recreated. */
        first->vlan_macip_lens = vlan_macip_lens;
        first->type_tucmd = type_tucmd;
        first->mss_l4len_idx = mss_l4len_idx;

	return 1;
}

static int ixgbe_tx_csum_batch_safe(struct ixgbe_ring *tx_ring,
			             struct ixgbe_tx_buffer *first,
                                     u16 desc_i)
{
	struct sk_buff *skb = first->skb;
	u32 vlan_macip_lens = 0;
	u32 mss_l4len_idx = 0;
	u32 type_tucmd = 0;

	if (skb->ip_summed != CHECKSUM_PARTIAL) {
		if (!(first->tx_flags & IXGBE_TX_FLAGS_HW_VLAN) &&
		    !(first->tx_flags & IXGBE_TX_FLAGS_CC))
			return 0;
		vlan_macip_lens = skb_network_offset(skb) <<
				  IXGBE_ADVTXD_MACLEN_SHIFT;
	} else {
		u8 l4_hdr = 0;
		union {
			struct iphdr *ipv4;
			struct ipv6hdr *ipv6;
			u8 *raw;
		} network_hdr;
		union {
			struct tcphdr *tcphdr;
			u8 *raw;
		} transport_hdr;

		if (skb->encapsulation) {
			network_hdr.raw = skb_inner_network_header(skb);
			transport_hdr.raw = skb_inner_transport_header(skb);
			vlan_macip_lens = skb_inner_network_offset(skb) <<
					  IXGBE_ADVTXD_MACLEN_SHIFT;
		} else {
			network_hdr.raw = skb_network_header(skb);
			transport_hdr.raw = skb_transport_header(skb);
			vlan_macip_lens = skb_network_offset(skb) <<
					  IXGBE_ADVTXD_MACLEN_SHIFT;
		}

		/* use first 4 bits to determine IP version */
		switch (network_hdr.ipv4->version) {
		case IPVERSION:
			vlan_macip_lens |= transport_hdr.raw - network_hdr.raw;
			type_tucmd |= IXGBE_ADVTXD_TUCMD_IPV4;
			l4_hdr = network_hdr.ipv4->protocol;
			break;
		case 6:
			vlan_macip_lens |= transport_hdr.raw - network_hdr.raw;
			l4_hdr = network_hdr.ipv6->nexthdr;
			break;
		default:
			if (unlikely(net_ratelimit())) {
				dev_warn(tx_ring->dev,
					 "partial checksum but version=%d\n",
					 network_hdr.ipv4->version);
			}
		}

		switch (l4_hdr) {
		case IPPROTO_TCP:
			type_tucmd |= IXGBE_ADVTXD_TUCMD_L4T_TCP;
			mss_l4len_idx = (transport_hdr.tcphdr->doff * 4) <<
					IXGBE_ADVTXD_L4LEN_SHIFT;
			break;
		case IPPROTO_SCTP:
			type_tucmd |= IXGBE_ADVTXD_TUCMD_L4T_SCTP;
			mss_l4len_idx = sizeof(struct sctphdr) <<
					IXGBE_ADVTXD_L4LEN_SHIFT;
			break;
		case IPPROTO_UDP:
			mss_l4len_idx = sizeof(struct udphdr) <<
					IXGBE_ADVTXD_L4LEN_SHIFT;
			break;
		default:
			if (unlikely(net_ratelimit())) {
				dev_warn(tx_ring->dev,
				 "partial checksum but l4 proto=%x!\n",
				 l4_hdr);
			}
			break;
		}

		/* update TX checksum flag */
		first->tx_flags |= IXGBE_TX_FLAGS_CSUM;
	}

	/* vlan_macip_lens: MACLEN, VLAN tag */
	vlan_macip_lens |= first->tx_flags & IXGBE_TX_FLAGS_VLAN_MASK;

        //XXX: DEBUG
        //pr_info ("ixgbe_tx_csum_batch_safe:\n");
        //pr_info (" vlan_macip_lens: %d\n", vlan_macip_lens);
        //pr_info (" type_tucmd: %d\n", type_tucmd);
        //pr_info (" mss_l4len_idx: %d\n", mss_l4len_idx);
        //pr_info (" desc_i: %d\n", desc_i);

	ixgbe_tx_ctxtdesc_ntu(tx_ring, vlan_macip_lens, 0,
			      type_tucmd, mss_l4len_idx, desc_i);

        return 1;
}

//TODO: the goto version should be easily debugged by removing all calls to
// FPP_PSS(...).  If this is done, then the batch processing will proceed
// iteratively, as normal.
static netdev_tx_t ixgbe_xmit_batch_map(struct ixgbe_adapter *adapter,
                                             struct ixgbe_ring *tx_ring)
{
    /* Local batch variables (ixgbe_xmit_frame_ring). */
    struct ixgbe_skb_batch_data *_cur_skb_data[IXGBE_MAX_XMIT_BATCH_SIZE];
    struct ixgbe_tx_buffer *_first[IXGBE_MAX_XMIT_BATCH_SIZE];
    int _tso[IXGBE_MAX_XMIT_BATCH_SIZE];
    int _csum[IXGBE_MAX_XMIT_BATCH_SIZE];
    u32 _tx_flags[IXGBE_MAX_XMIT_BATCH_SIZE];
    __be16 _protocol[IXGBE_MAX_XMIT_BATCH_SIZE];
    u16 _desc_i[IXGBE_MAX_XMIT_BATCH_SIZE];
    u16 _quit_desc[IXGBE_MAX_XMIT_BATCH_SIZE];
    u8 _hdr_len[IXGBE_MAX_XMIT_BATCH_SIZE];

    /* Local batch variables (ixgbe_tx_map) */
    struct ixgbe_tx_buffer *_tx_buffer[IXGBE_MAX_XMIT_BATCH_SIZE];
    union ixgbe_adv_tx_desc *_tx_desc[IXGBE_MAX_XMIT_BATCH_SIZE];
    struct skb_frag_struct *_frag[IXGBE_MAX_XMIT_BATCH_SIZE];
    dma_addr_t _dma[IXGBE_MAX_XMIT_BATCH_SIZE];
    unsigned int _data_len[IXGBE_MAX_XMIT_BATCH_SIZE];
    unsigned int _size[IXGBE_MAX_XMIT_BATCH_SIZE];
    u32 _cmd_type[IXGBE_MAX_XMIT_BATCH_SIZE];

    /* G-opt style (FPP) prefetching variables */
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

    //TODO: prefetch
    /* Init local variables */
    _cur_skb_data[I] = &tx_ring->skb_batch[I];
    _tx_flags[I] = 0;
    _protocol[I] = _cur_skb_data[I]->skb->protocol;
    _desc_i[I] = _cur_skb_data[I]->desc_ftu;
    _hdr_len[I] = 0;

    //TODO: prefetch
    _first[I] = &tx_ring->tx_buffer_info[_desc_i[I]];
    _first[I]->skb = _cur_skb_data[I]->skb;
    _first[I]->bytecount = _cur_skb_data[I]->skb->len;
    _first[I]->gso_segs = 1;
    _first[I]->hr_i_valid = false;
    _first[I]->pktr_i_valid = false;

    //TODO: prefetch
    if (skb_vlan_tag_present(_cur_skb_data[I]->skb)) {
        _tx_flags[I] |= skb_vlan_tag_get(_cur_skb_data[I]->skb) << IXGBE_TX_FLAGS_VLAN_SHIFT;
        _tx_flags[I] |= IXGBE_TX_FLAGS_HW_VLAN;
    } else if (_protocol[I] == htons(ETH_P_8021Q)) {
        /* TODO: support more features later once I've actually proved this helps */
        BUG ();
    }

    //TODO: prefetch
    _protocol[I] = vlan_get_protocol(_cur_skb_data[I]->skb);

    if (unlikely(skb_shinfo(_cur_skb_data[I]->skb)->tx_flags & SKBTX_HW_TSTAMP) &&
        adapter->ptp_clock &&
        !test_and_set_bit_lock(__IXGBE_PTP_TX_IN_PROGRESS,
                               &adapter->state)) {
        /* TODO: support more features later once I've actually proved this helps */
        BUG ();
    }

    skb_tx_timestamp(_cur_skb_data[I]->skb);

#ifdef CONFIG_PCI_IOV
    /*
     * Use the l2switch_enable flag - would be false if the DMA
     * Tx switch had been disabled.
     */
    if (adapter->flags & IXGBE_FLAG_SRIOV_ENABLED)
            _tx_flags[I] |= IXGBE_TX_FLAGS_CC;

#endif

    /* DCB maps skb priorities 0-7 onto 3 bit PCP of VLAN tag. */
    //XXX: This code has never been tested
    if ((adapter->flags & IXGBE_FLAG_DCB_ENABLED) &&
        ((_tx_flags[I] & (IXGBE_TX_FLAGS_HW_VLAN | IXGBE_TX_FLAGS_SW_VLAN)) ||
         (_cur_skb_data[I]->skb->priority != TC_PRIO_CONTROL))) {
            _tx_flags[I] &= ~IXGBE_TX_FLAGS_VLAN_PRIO_MASK;
            _tx_flags[I] |= (_cur_skb_data[I]->skb->priority & 0x7) <<
                                    IXGBE_TX_FLAGS_VLAN_PRIO_SHIFT;
            if (_tx_flags[I] & IXGBE_TX_FLAGS_SW_VLAN) {
                    struct vlan_ethhdr *vhdr;

                    if (skb_cow_head(_cur_skb_data[I]->skb, 0))
                            BUG (); // Figure out how to implement "goto out_drop;" later
                    vhdr = (struct vlan_ethhdr *)_cur_skb_data[I]->skb->data;
                    vhdr->h_vlan_TCI = htons(_tx_flags[I] >>
                                             IXGBE_TX_FLAGS_VLAN_SHIFT);
            } else {
                    _tx_flags[I] |= IXGBE_TX_FLAGS_HW_VLAN;
            }
    }

    /* record initial flags and protocol */
    _first[I]->tx_flags = _tx_flags[I];
    _first[I]->protocol = _protocol[I];

#ifdef IXGBE_FCOE
    /* TODO: support more features later once I've actually proved this helps */
    //BUG ();
#endif /* IXGBE_FCOE */

    //TODO: prefetch
    _tso[I] = ixgbe_tso_batch_safe(tx_ring, _first[I], _desc_i[I], &_hdr_len[I]);
    _csum[I] = 0;
    if (_tso[I] < 0) {
	dev_kfree_skb_any(_first[I]->skb);
	_first[I]->skb = NULL;
        //TODO: In this case, we need to make sure that all of the allocated
        // descriptors are set correctly for 0-byte segments.
        BUG ();
        goto map_fpp_end;
    } else if (!_tso[I]) {
        _csum[I] = ixgbe_tx_csum_batch_safe(tx_ring, _first[I], _desc_i[I]);
    }

    /* Track that we have use a descriptor as a context descriptor */
    //XXX: Only if we actually created a context descriptor, which is apparently optional. */
    if (_tso[I] || _csum[I]) {
        _desc_i[I]++;
        _desc_i[I] = (_desc_i[I] < tx_ring->count) ? _desc_i[I] : 0;
    }

    /* add the ATR filter if ATR is on */
    if (test_bit(__IXGBE_TX_FDIR_INIT_DONE, &tx_ring->state)) {
        ixgbe_atr(tx_ring, _first[I]);
    }

    /* 
     * Map the skb fragments and create data descriptors.
     *  This code is mostly copy/pasted from ixgbe_tx_map(...).
     */

    /* Init mapping variables */
    /* Note: Above functions add in more data to _first[I]->tx_flags */
    _tx_flags[I] = _first[I]->tx_flags;
    _cmd_type[I] = ixgbe_tx_cmd_type(_cur_skb_data[I]->skb, _tx_flags[I]);
    _tx_desc[I] = IXGBE_TX_DESC(tx_ring, _desc_i[I]);

    ixgbe_tx_olinfo_status(_tx_desc[I], _tx_flags[I],
        _cur_skb_data[I]->skb->len - _hdr_len[I]);

    _size[I] = skb_headlen(_cur_skb_data[I]->skb);
    _data_len[I] = _cur_skb_data[I]->skb->data_len;

#ifdef IXGBE_FCOE
    /* TODO: support more features later once I've actually proved this helps */
    //BUG ();
#endif

    _dma[I] = dma_map_single(tx_ring->dev, _cur_skb_data[I]->skb->data,
        _size[I], DMA_TO_DEVICE);

    _tx_buffer[I] = _first[I];

    for (_frag[I] = &skb_shinfo(_cur_skb_data[I]->skb)->frags[0];; _frag[I]++) {
            if (dma_mapping_error(tx_ring->dev, _dma[I])) {
                pr_info ("Mapping errors haven't been handled yet. Panicing\n");
                BUG ();
            }

            /* record length, and DMA address */
            dma_unmap_len_set(_tx_buffer[I], len, _size[I]);
            dma_unmap_addr_set(_tx_buffer[I], dma, _dma[I]);

            _tx_desc[I]->read.buffer_addr = cpu_to_le64(_dma[I]);

            while (unlikely(_size[I] > IXGBE_MAX_DATA_PER_TXD)) {
                    _tx_desc[I]->read.cmd_type_len =
                            cpu_to_le32(_cmd_type[I] ^ IXGBE_MAX_DATA_PER_TXD);

                    _desc_i[I]++;
                    _tx_desc[I]++;
                    if (_desc_i[I] == tx_ring->count) {
                            _tx_desc[I] = IXGBE_TX_DESC(tx_ring, 0);
                            _desc_i[I] = 0;
                    }
                    _tx_desc[I]->read.olinfo_status = 0;

                    _dma[I] += IXGBE_MAX_DATA_PER_TXD;
                    _size[I] -= IXGBE_MAX_DATA_PER_TXD;

                    _tx_desc[I]->read.buffer_addr = cpu_to_le64(_dma[I]);
            }

            if (likely(!_data_len[I]))
                    break;

            _tx_desc[I]->read.cmd_type_len = cpu_to_le32(_cmd_type[I] ^ _size[I]);

            _desc_i[I]++;
            _tx_desc[I]++;
            if (_desc_i[I] == tx_ring->count) {
                    _tx_desc[I] = IXGBE_TX_DESC(tx_ring, 0);
                    _desc_i[I] = 0;
            }
            _tx_desc[I]->read.olinfo_status = 0;

#ifdef IXGBE_FCOE
            _size[I] = min_t(unsigned int, _data_len[I], skb_frag_size(_frag[I]));
#else
            _size[I] = skb_frag_size(_frag[I]);
#endif
            _data_len[I] -= _size[I];

            _dma[I] = skb_frag_dma_map(tx_ring->dev, _frag[I], 0, _size[I],
                                   DMA_TO_DEVICE);

            _tx_buffer[I] = &tx_ring->tx_buffer_info[_desc_i[I]];
    }

    /* write last descriptor with RS and EOP bits */
    _cmd_type[I] |= _size[I] | IXGBE_TXD_CMD;
    _tx_desc[I]->read.cmd_type_len = cpu_to_le32(_cmd_type[I]);

    netdev_tx_sent_queue(txring_txq(tx_ring), _first[I]->bytecount);

    /* set the timestamp */
    _first[I]->time_stamp = jiffies;

    /*
     * Force memory writes to complete before letting h/w know there
     * are new descriptors to fetch.  (Only applicable for weak-ordered
     * memory model archs, such as IA-64).
     *
     * We also need this memory barrier to make certain all of the
     * status bits have been updated before next_to_watch is written.
     */
    wmb();

    /* set next_to_watch value indicating a packet is present */
    /* This code is a little subtle because DD will not be set on data
     * descriptors of length 0 (null descriptors).  Because of this,
     * next_to_watch needs to be the last non-null data descriptor.  However,
     * ixgbe_clean_tx_irq needs to be able to find the next non-null context or
     * data descriptor again because DD will not be set on null data
     * descriptors. */
    _first[I]->next_to_watch = _tx_desc[I];

    /* Move to the next descriptor */
    _desc_i[I]++;
    if (_desc_i[I] == tx_ring->count)
            _desc_i[I] = 0;

    /* If there are any unused descriptors, count them and make sure they are
     * null data descriptors. */
    BUG_ON (_first[I]->null_desc_count != 0);
    _quit_desc[I] = ((_cur_skb_data[I]->desc_ftu + 
                      _cur_skb_data[I]->desc_count) % 
                     tx_ring->count);
    while (_desc_i[I] != _quit_desc[I]) {
        //pr_info(" nulldesc i: %d\n", _desc_i[I]);
        ixgbe_tx_nulldesc(tx_ring, _desc_i[I]);
        _first[I]->null_desc_count++;
        _desc_i[I]++;
    }

    //XXX: DEBUG
    //pr_info ("ixgbe_xmit_batch_map:\n");
    //pr_info (" null_desc_count: %d\n", _first[I]->null_desc_count);
    //pr_info (" ntu _desc_i[I]: %d\n", _desc_i[I]);

#if 0
    /* Assert we used the correct number of descriptors */
    pr_info ("_desc_i[I]: %d\n", _desc_i[I]);
    pr_info ("desc_ftu: %d\n", _cur_skb_data[I]->desc_ftu);
    pr_info ("desc_count: %d\n", _cur_skb_data[I]->desc_count);
    pr_info ("expected count: %d\n", ((_cur_skb_data[I]->desc_ftu + _cur_skb_data[I]->desc_count) % tx_ring->count));
    BUG_ON (_desc_i[I] != ((_cur_skb_data[I]->desc_ftu + _cur_skb_data[I]->desc_count) % tx_ring->count));
#endif

map_fpp_end:
    batch_rips[I] = &&map_fpp_end;
    iMask = FPP_SET(iMask, I); 
    if(iMask == (1 << tx_ring->skb_batch_size) - 1) {
        return NETDEV_TX_OK;
    }
    I = (I + 1) % tx_ring->skb_batch_size;
    goto *batch_rips[I];

}

#if 0
// batch_index must be declared outside of the xmit functions
static int batch_index = 0;

static netdev_tx_t ixgbe_xmit_batch_map_nogoto(struct ixgbe_adapter *adapter,
                                             struct ixgbe_ring *tx_ring)
{
    foreach (batch_index, tx_ring->skb_batch_size) {
        
    }
}
#endif

static netdev_tx_t ixgbe_xmit_batch_purge(struct ixgbe_adapter *adapter,
                                          struct ixgbe_ring *tx_ring)
{
    netdev_tx_t ret = NETDEV_TX_OK;

    /* Quit early if there are no skb's */
    if (tx_ring->skb_batch_size == 0)
        return NETDEV_TX_OK;

    //pr_info ("ixgbe_xmit_batch_purge:\n");

    //XXX: DEBUG: Track the batch sizes
    if (tx_ring->skb_batch_size_stats_count < IXGBE_MAX_BATCH_SIZE_STATS) {
        //pr_info ("Updating batch size stats...\n");
        if (tx_ring->skb_batch_size > 1)
            tx_ring->skb_batch_size_stats[tx_ring->skb_batch_size_stats_count++] = tx_ring->skb_batch_size;
        else
            tx_ring->skb_batch_size_of_one_stats++;
    }

    /* Use the appropriate batched code for this driver configuration */
    if (use_sgseg) {
        //TODO
        BUG ();
    } else if (use_pkt_ring) {
        //TODO
        BUG ();
    } else {
        //ret = ixgbe_xmit_batch_map_nogoto(adapter, tx_ring);
        ret = ixgbe_xmit_batch_map(adapter, tx_ring);
    }

    /* Write the tail pointer to the NIC */
    writel(tx_ring->next_to_use, tx_ring->tail);

    /* we need this if more than one processor can write to our tail
     * at a time, it synchronizes IO on IA64/Altix systems
     */
    mmiowb();

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

    /* Save the skb and per-skb metadata and update the descriptor rings. */
    skb_batch_data->skb = skb;
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
    if (!skb->xmit_more || ixgbe_maybe_stop_tx(tx_ring, DESC_NEEDED) ||
            tx_ring->skb_batch_size == IXGBE_MAX_XMIT_BATCH_SIZE) {

        /* Purge the current batch */
        //pr_info ("Purging a batch of size %u\n", tx_ring->skb_batch_size);
        purge_ret = ixgbe_xmit_batch_purge(adapter, tx_ring);
        BUG_ON (purge_ret != NETDEV_TX_OK);
    }

    return NETDEV_TX_OK;
}


