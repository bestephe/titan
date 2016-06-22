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
#include "kcompat.h"

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
		    !(first->tx_flags & IXGBE_TX_FLAGS_CC)) {

                        //pr_info ("ixgbe_tx_csum_batch_safe: returning 0\n");

                        //pr_info (" skb->ip_summed == CHECKSUM_PARTIAL: %d\n",
                        //         skb->ip_summed == CHECKSUM_PARTIAL);
                        //pr_info (" tx_flags & IXGBE_TX_FLAGS_HW_VLAN: %d\n",
                        //         first->tx_flags & IXGBE_TX_FLAGS_HW_VLAN);
                        //pr_info (" tx_flags & IXGBE_TX_FLAGS_CC: %d\n",
                        //         first->tx_flags & IXGBE_TX_FLAGS_CC);

			return 0;
                }
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

                //pr_info ("ixgbe_tx_csum_batch_safe: adding TX_FLAGS_CSUM\n");
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

        /* update first so that the context descriptor can be recreated. */
        first->vlan_macip_lens = vlan_macip_lens;
        first->type_tucmd = type_tucmd;
        first->mss_l4len_idx = mss_l4len_idx;

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

    /* The batch size should equal the number of segments for normal batched
     * mapping. */
    BUG_ON (tx_ring->skb_batch_size != tx_ring->skb_batch_seg_count);

    for (temp_index = 0; temp_index < tx_ring->skb_batch_size; temp_index++) {
        batch_rips[temp_index] = &&map_fpp_start;
    }
map_fpp_start:

    /* Prefetch */
    FPP_PSS(&tx_ring->skb_batch[I], map_fpp_skb_batch1,
        tx_ring->skb_batch_size);
map_fpp_skb_batch1:

    FPP_PSS(tx_ring->skb_batch[I].skb, map_fpp_skb1,
        tx_ring->skb_batch_size);
map_fpp_skb1:

    /* Init local variables */
    _cur_skb_data[I] = &tx_ring->skb_batch[I];
    _tx_flags[I] = 0;
    _protocol[I] = _cur_skb_data[I]->skb->protocol;
    _desc_i[I] = _cur_skb_data[I]->desc_ftu;
    _hdr_len[I] = 0;

    /* Prefetch */
    FPP_PSS(&tx_ring->tx_buffer_info[_desc_i[I]], map_fpp_first1, tx_ring->skb_batch_size);
map_fpp_first1:

    _first[I] = &tx_ring->tx_buffer_info[_desc_i[I]];
    _first[I]->skb = _cur_skb_data[I]->skb;
    _first[I]->bytecount = _cur_skb_data[I]->skb->len;
    _first[I]->gso_segs = 1;
    _first[I]->hr_i_valid = false;
    _first[I]->pktr_i_valid = false;

    //XXX: DEBUG
    //pr_info("ixgbe_xmit_batch_map: (txq: %d)\n", tx_ring->queue_index);
    //pr_info (" _cur_skb_data[I]->desc_ftu: %d\n", _cur_skb_data[I]->desc_ftu);
    //pr_info (" _cur_skb_data[I]->desc_count: %d\n", _cur_skb_data[I]->desc_count);

    /* Prefetch: skb should already be prefetched? benchmark? */

    if (skb_vlan_tag_present(_cur_skb_data[I]->skb)) {
        _tx_flags[I] |= skb_vlan_tag_get(_cur_skb_data[I]->skb) << IXGBE_TX_FLAGS_VLAN_SHIFT;
        _tx_flags[I] |= IXGBE_TX_FLAGS_HW_VLAN;
    } else if (_protocol[I] == htons(ETH_P_8021Q)) {
        /* TODO: support more features later once I've actually proved this helps */
        BUG ();
    }

    /* Prefetch: skb should already be prefetched? benchmark? */

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

/* While I have plans on using DCB in the future, I am not currently using
 * DCB.  I'll deal with this code later. */
#if 0
    /* Prefetch? */
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
#endif

    /* record initial flags and protocol */
    _first[I]->tx_flags = _tx_flags[I];
    _first[I]->protocol = _protocol[I];

    //XXX: DEBUG
    //pr_info (" _first[I]->tx_flags: %d\n", _first[I]->tx_flags);

#ifdef IXGBE_FCOE
    /* TODO: support more features later once I've actually proved this helps */
    //BUG ();
#endif /* IXGBE_FCOE */

    /* Prefetch */
    prefetch(tx_ring);
    prefetch(_first[I]);
    prefetchw(IXGBE_TX_CTXTDESC(tx_ring, _desc_i[I]));
    FPP_PSS(_first[I]->skb, map_fpp_tso_batch_safe, tx_ring->skb_batch_size);
map_fpp_tso_batch_safe:

    _tso[I] = ixgbe_tso_batch_safe(tx_ring, _first[I], _desc_i[I], &_hdr_len[I]);
    _csum[I] = 0;
    if (_tso[I] < 0) {
	dev_kfree_skb_any(_first[I]->skb);
	_first[I]->skb = NULL;
        //TODO: In this case, we need to make sure that all of the allocated
        // descriptors are null descriptors.
        BUG ();
        goto map_fpp_end;
    } else if (!_tso[I]) {
        _csum[I] = ixgbe_tx_csum_batch_safe(tx_ring, _first[I], _desc_i[I]);
    }

    //XXX: Debugging the tso_or_csum function.
#if 0
    pr_info ("ixgbe_xmit_batch_map:\n");
    pr_info (" tso: %d. csum: %d\n", _tso[I], _csum[I]);
    u8 tmp_hdr_len = 0;
    int tso_or_csum = ixgbe_is_tso_or_csum(adapter, _cur_skb_data[I]->skb, &tmp_hdr_len);
    pr_info (" tso_or_csum: %d\n", tso_or_csum);
    BUG_ON (tso_or_csum != (_tso[I] || _csum[I]));
#endif

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

    /* Prefetch */
    prefetch(_first[I]);
    prefetch(_cur_skb_data[I]->skb);
    FPP_PSS(IXGBE_TX_DESC(tx_ring, _desc_i[I]), map_fpp_init_data_desc,
            tx_ring->skb_batch_size);
map_fpp_init_data_desc:

    /* Init mapping variables */
    /* Note: Above functions add in more data to _first[I]->tx_flags */
    _tx_flags[I] = _first[I]->tx_flags;
    _cmd_type[I] = ixgbe_tx_cmd_type(_tx_flags[I]);
    _tx_desc[I] = IXGBE_TX_DESC(tx_ring, _desc_i[I]);

    ixgbe_tx_olinfo_status(_tx_desc[I], _tx_flags[I],
        _cur_skb_data[I]->skb->len - _hdr_len[I]);

    _size[I] = skb_headlen(_cur_skb_data[I]->skb);
    _data_len[I] = _cur_skb_data[I]->skb->data_len;

#ifdef IXGBE_FCOE
    /* TODO: support more features later once I've actually proved this helps */
    //BUG ();
#endif

    /* Prefetch */
    FPP_PSS(_cur_skb_data[I]->skb->data, map_fpp_skb_data1, tx_ring->skb_batch_size);
map_fpp_skb_data1:

    _dma[I] = dma_map_single(tx_ring->dev, _cur_skb_data[I]->skb->data,
        _size[I], DMA_TO_DEVICE);

    _tx_buffer[I] = _first[I];

    for (_frag[I] = &skb_shinfo(_cur_skb_data[I]->skb)->frags[0];; _frag[I]++) {

            if (dma_mapping_error(tx_ring->dev, _dma[I])) {
                //TODO: null descriptors solve this problem
                pr_info ("Mapping errors haven't been handled yet. Panicing\n");
                BUG ();
            }

            /* Prefetch */
            prefetch(_tx_buffer[I]);
            FPP_PSS(_tx_desc[I], map_fpp_dma1, tx_ring->skb_batch_size);
map_fpp_dma1:


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

            prefetch(_frag[I]);
            FPP_PSS(_tx_desc[I], map_fpp_dma2, tx_ring->skb_batch_size);
map_fpp_dma2:

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

    /* XXX: DEBUG: Print out the format of the first data descriptor */
    //pr_info (" cmd_type_len: %x\n", le32_to_cpu(_tx_desc[I]->read.cmd_type_len));
    //pr_info (" olinfo_status: %x\n", le32_to_cpu(_tx_desc[I]->read.olinfo_status));

    /*
     * Force memory writes to complete before letting h/w know there
     * are new descriptors to fetch.  (Only applicable for weak-ordered
     * memory model archs, such as IA-64).
     *
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
        /* Prefetch */
        FPP_PSS(IXGBE_TX_DESC(tx_ring, _desc_i[I]), map_fpp_nulldesc,
                tx_ring->skb_batch_size);
map_fpp_nulldesc:

        //pr_info(" nulldesc i: %d\n", _desc_i[I]);
        ixgbe_tx_nulldesc(tx_ring, _desc_i[I]);
        _first[I]->null_desc_count++;
        _desc_i[I]++;
        if (_desc_i[I] == tx_ring->count) {
                _tx_desc[I] = IXGBE_TX_DESC(tx_ring, 0);
                _desc_i[I] = 0;
        }
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

struct ixgbe_seg_batch_data {
    struct ixgbe_skb_batch_data *skb_batch_data;
    struct ixgbe_tx_buffer *first;
    u32 data_len;
    u32 data_offset;
    u16 desc_count;
    u16 desc_ftu;
    u16 hr_count;
    u16 hr_ftu;
    u8 last_seg;
};

static void ixgbe_tx_prepare_segs(struct ixgbe_ring *tx_ring,
                                  struct ixgbe_seg_batch_data *seg_data_array,
                                  struct ixgbe_skb_batch_data *skb_data,
                                  struct ixgbe_tx_buffer *first,
                                  u32 drv_gso_size)
{
    struct ixgbe_seg_batch_data *cur_seg_data;
    u32 data_offset = 0;
    u32 pkt_data_len;
    u32 data_len = skb_data->skb->len - skb_data->hdr_len;
    u32 gso_seg;
    u16 desc_per_seg;
    u16 hr_per_seg;
    u16 last_seg = 0;

    BUG_ON (skb_data->desc_ftu >= tx_ring->count);
    BUG_ON ((skb_data->desc_count % skb_data->drv_segs) != 0);
    BUG_ON ((skb_data->hr_count % skb_data->drv_segs) != 0);
    desc_per_seg = skb_data->desc_count / skb_data->drv_segs;
    hr_per_seg = skb_data->hr_count / skb_data->drv_segs;

    //pr_info ("ixgbe_tx_prepare_segs:\n");
    //pr_info (" desc_per_seg: %d\n", desc_per_seg);
    //pr_info (" skb_data->drv_segs: %d\n", skb_data->drv_segs);
    //pr_info (" skb_data->hr_count: %d\n", skb_data->hr_count);
    //pr_info (" hr_per_seg: %d\n", hr_per_seg);
    //pr_info (" hdr_len: %d. data_len: %d\n", skb_data->hdr_len, data_len);

    BUG_ON (hr_per_seg > 1);

    if (!skb_data->tso_or_csum || skb_data->drv_segs == 1) {
        BUG_ON (skb_data->drv_segs != 1);
        BUG_ON (skb_data->hdr_len != 0);

        seg_data_array[0].skb_batch_data = skb_data;
        seg_data_array[0].first = first;
        seg_data_array[0].data_len = data_len;
        seg_data_array[0].data_offset = 0;
        seg_data_array[0].desc_count = skb_data->desc_count;
        seg_data_array[0].desc_ftu = skb_data->desc_ftu;
        seg_data_array[0].hr_count = skb_data->hr_count;
        seg_data_array[0].hr_ftu = skb_data->hr_ftu;
        seg_data_array[0].last_seg = 1;

        /* We've asserted there is only one seg, so the other segs do not
         * need to be set to null. If the assertion goes away, this will need
         * to change. */
        
        return;
    }

    for (gso_seg = 0; gso_seg < skb_data->drv_segs; gso_seg++) {
        cur_seg_data = &seg_data_array[gso_seg];
        pkt_data_len = min_t(u32, drv_gso_size, data_len);

        if (pkt_data_len == 0) {
            pr_info ("Enqueing empty seg in a batch\n");

            // This was a bad idea. I should always compute the right amount of
            // data.
            BUG();
            cur_seg_data->skb_batch_data = NULL;
            cur_seg_data->first = NULL;
            BUG_ON (last_seg != 1);
        } else {
            cur_seg_data->skb_batch_data = skb_data;
            cur_seg_data->first = first;
            cur_seg_data->data_len = pkt_data_len;
            cur_seg_data->data_offset = data_offset;
        }
        cur_seg_data->desc_count = desc_per_seg;
        cur_seg_data->desc_ftu = (skb_data->desc_ftu + (gso_seg * desc_per_seg)) % tx_ring->count;
        cur_seg_data->hr_count = hr_per_seg;
        cur_seg_data->hr_ftu = (skb_data->hr_ftu + (gso_seg * hr_per_seg)) % tx_ring->hr_count;

        data_offset += pkt_data_len;
        BUG_ON (pkt_data_len > data_len);
        data_len -= pkt_data_len;

        if (gso_seg == (skb_data->drv_segs - 1)) {
            cur_seg_data->last_seg = 1;
        } else {
            cur_seg_data->last_seg = 0;
        }

#if 0
        if (data_len == 0 && !last_seg) {
            //pr_info (" last_seg: %d\n", gso_seg);
            cur_seg_data->last_seg = 1;
            last_seg = 1;
        } else {
            cur_seg_data->last_seg = 0;
        }
#endif

    }

    BUG_ON (data_len != 0);
}

static void ixgbe_tx_enqueue_sgsegs_batch(struct ixgbe_ring *tx_ring,
                                          struct ixgbe_seg_batch_data *seg_data_array,
                                          u16 skb_batch_seg_count)
{
    int loop_var = 0;

    //TODO: If this batch is too big, use smaller batch sizes to iterate over it

    foreach (loop_var, skb_batch_seg_count) {
        struct ixgbe_seg_batch_data *cur_seg_batch = &seg_data_array[loop_var];
        struct ixgbe_tx_buffer *first = cur_seg_batch->first;
        struct sk_buff *skb;
        u8 hdr_len = 0;
        u8 tmp_hdr_len = 0;
        u32 data_len = cur_seg_batch->data_len;
        u32 data_offset = cur_seg_batch->data_offset;
	struct ixgbe_tx_buffer *tx_buffer;
	union ixgbe_adv_tx_desc *tx_desc;
        struct ixgbe_pkt_hdr *tx_hdr;
	struct skb_frag_struct *frag;
	dma_addr_t dma;
        int tso, csum;
        u32 size;
        u32 frag_offset, frag_size, frag_i;
        u32 seq_offset, seqno;
        u32 tx_flags;
        u32 cmd_type = 0;
        u16 desc_i = cur_seg_batch->desc_ftu;
        u16 desc_count = cur_seg_batch->desc_count;
        u16 quit_desc_i;
        u16 hr_i = cur_seg_batch->hr_ftu;
        u16 hr_count = cur_seg_batch->hr_count;

        BUG_ON (desc_count == 0);
        BUG_ON (desc_i >= tx_ring->count);
        BUG_ON (hr_i >= tx_ring->hr_count);
        BUG_ON (cur_seg_batch->skb_batch_data == NULL); //I've made this case no longer possible
        
        /* These must be set after checking if we have an empty seg. */
        hdr_len = cur_seg_batch->skb_batch_data->hdr_len;
        skb = first->skb;
        
        //XXX: DEBUG
        //pr_info ("ixgbe_tx_enqueue_sgsegs_batch (tso seg): %d\n", loop_var);
        //pr_info (" data_len: %d\n", cur_seg_batch->data_len);
        ////pr_info (" hdr_len: %d\n", cur_seg_batch->skb_batch_data->hdr_len);
        //pr_info (" data_offset: %d\n", cur_seg_batch->data_offset);
        //pr_info (" desc_count: %d\n", cur_seg_batch->desc_count);
        //pr_info (" desc_ftu: %d\n", cur_seg_batch->desc_ftu);
        //    pr_info (" hr_count: %d\n", cur_seg_batch->hr_count);
        //    pr_info (" hr_ftu: %d\n", cur_seg_batch->hr_ftu);
        //    pr_info (" last_seg: %d\n", cur_seg_batch->last_seg);

        /* This function should not be used to try to transmit more data than
         * is in a single skb. */
        BUG_ON ((data_offset + data_len) > (skb->len - hdr_len));

	tx_desc = IXGBE_TX_DESC(tx_ring, desc_i);
        tx_hdr = IXGBE_TX_HDR(tx_ring, hr_i);

        //XXX: DEBUG
        //pr_info ("ixgbe_tx_enqueue_sgsegs_batch:\n");
        //pr_info (" init desc_i: %d\n", desc_i);
        //pr_info (" init tx_desc: %p\n", tx_desc);

        /* The first context descriptor has not yet been created.  Also, the
         * packet header does not need to be copied for the first packet. */
        if (data_offset == 0) {

                tso = ixgbe_tso_batch_safe(tx_ring, first, desc_i, &tmp_hdr_len);
                csum = 0;
                BUG_ON (tmp_hdr_len != hdr_len);
                if (tso < 0) {
                    BUG (); // Be more robust later
                } else if (!tso) {
                    csum = ixgbe_tx_csum_batch_safe(tx_ring, first, desc_i);
                }

                /* Update the descriptor if a context descriptor was used. */
                if (tso || csum) {
                    BUG_ON (!cur_seg_batch->skb_batch_data->tso_or_csum);
                    BUG_ON (((cur_seg_batch->desc_ftu + desc_count) % tx_ring->count) ==
                            desc_i);

                    desc_i++;
                    tx_desc++;
                    if (desc_i == tx_ring->count) {
                        desc_i = 0;
                        tx_desc = IXGBE_TX_DESC(tx_ring, 0);
                    }
                    tx_desc->read.olinfo_status = 0;
                    BUG_ON (desc_i >= tx_ring->count);
                }

                /* The above functions can update tx_flags */
                tx_flags = first->tx_flags;
                cmd_type = ixgbe_tx_cmd_type(tx_flags);

                //XXX: DEBUG
                //pr_info ("ixgbe_tx_enqueue_sgsegs_batch:\n");
                //pr_info (" tso: %d. csum: %d\n", tso, csum);
                //pr_info (" tx_flags: %d. first->tx_flags: %d\n", tx_flags, first->tx_flags);

                /* The first data descriptor must contain the entire length of
                 * the TSO segment. */
                ixgbe_tx_olinfo_status(tx_desc, tx_flags, data_len);
            
                /* In the first segment, we can enqueue a packet header and
                 * data in a single segment if they are located in the skb
                 * head, although I don't expect this to often be the case. */
                size = hdr_len + data_len; 
                if (likely(size > skb_headlen(skb))) {
                        size = skb_headlen(skb);
                }

                //DEBUG: Print out some variables of interest
                //pr_info ("ixgbe_tx_enqueue_sgsegs: data_offset == 0\n");
                //pr_info (" size: %d, skb_headlen(skb): %d\n", size, skb_headlen (skb));

                BUG_ON (size > IXGBE_MAX_DATA_PER_TXD);
                BUG_ON (size > first->len);
                BUG_ON (first->len != skb_headlen (skb));
                if (size != first->len) {
                    pr_info (" hdr_len: %d. data_len: %d\n", hdr_len, data_len);
                    pr_info (" size: %d. first->len: %d\n", size, first->len);
                }
                BUG_ON (size != first->len); // Sending a first segment
                                             // smaller than skb_headlen
                                             // (first->len) shouldn't be
                                             // allowed.

                /* Update the first tx data descriptor */
                dma = first->dma;
                tx_desc->read.buffer_addr = cpu_to_le64(dma);
		tx_desc->read.cmd_type_len = cpu_to_le32(cmd_type ^ size);

                //pr_info (" used desc: %d\n", i);

                /* Update the amount of data left to be sent. */
                //XXX: Don't screw up! size includes hdr_len right now! Is
                // this right?
                BUG_ON (data_len < (size - hdr_len));
                data_len -= (size - hdr_len);
                data_offset += (size - hdr_len);

                //DEBUG: how much data is left after enqueuing the skb head?
                //pr_info (" remaining data_len: %d\n", data_len);
                //pr_info (" new data_offset: %d\n", data_offset);

                /* TODO: If a header ring slot has been allocated, then this
                 *  needs to be noted in a tx_buffer. */
                if (hr_count > 0) {
                    //pr_info ("ixgbe_tx_enqueue_sgsegs_batch:\n");
                    //pr_info (" wasting a slot in the header ring!\n");
                    //pr_info (" hr_i: %d\n", hr_i);
                    BUG_ON (hr_count != 1);
                    tx_buffer = &tx_ring->tx_buffer_info[desc_i];
                    tx_buffer->hr_i = hr_i;
                    tx_buffer->hr_i_valid = true;

                    /* TODO: Skip the Update the pointer to the next header
                     * ring because there must only be a single use of the
                     * header ring */
                    hr_i++;
                    if (hr_i == tx_ring->hr_count)
                            hr_i = 0;
                    tx_hdr = IXGBE_TX_HDR(tx_ring, hr_i);
                    BUG_ON (hr_i != (cur_seg_batch->hr_ftu + cur_seg_batch->hr_count) % tx_ring->hr_count);
                }

                /* If it is possible that we are done enqueueing data.  Before
                 * updating i and tx_desc, we should goto the last descriptor
                 * code */
                if (data_len == 0) {
                        //TODO:
                        goto last_desc;
                }

                /* Move to the next descriptor */
                BUG_ON (((cur_seg_batch->desc_ftu + desc_count) % tx_ring->count) ==
                        desc_i);
		desc_i++;
		tx_desc++;
		if (desc_i == tx_ring->count) {
			tx_desc = IXGBE_TX_DESC(tx_ring, 0);
			desc_i = 0;
		}
		tx_desc->read.olinfo_status = 0;
                BUG_ON (desc_i >= tx_ring->count);

        } else {
                //XXX: TODO: this has a race condition because
                //first->mss_l4len, etc aren't set until ixgbe_tso_batch_safe
                //or ixgbe_tx_csum_batch_safe are called. */
#if 0
                /* Create the context descriptor. */
                ixgbe_tx_ctxtdesc_ntu(tx_ring, first->vlan_macip_lens, 0,
                                      first->type_tucmd, first->mss_l4len_idx,
                                      desc_i);
#endif

                /* It is tempting to use the saved values, but that causes a
                 * race condition.  For now, just rebuild the data. */
                tso = ixgbe_tso_batch_safe(tx_ring, first, desc_i, &tmp_hdr_len);
                csum = 0;
                BUG_ON (tmp_hdr_len != hdr_len);
                if (tso < 0) {
                    BUG (); // Be more robust later
                } else if (!tso) {
                    csum = ixgbe_tx_csum_batch_safe(tx_ring, first, desc_i);
                }

                //pr_info (" used desc: %d\n", i);

                /* Update the current descriptor. */
                BUG_ON (((cur_seg_batch->desc_ftu + desc_count) % tx_ring->count) ==
                        desc_i);
                desc_i++;
                tx_desc++;
                if (desc_i == tx_ring->count) {
                        tx_desc = IXGBE_TX_DESC(tx_ring, 0);
                        desc_i = 0;
                }
                tx_desc->read.olinfo_status = 0;
                tx_buffer = &tx_ring->tx_buffer_info[desc_i];
                BUG_ON (desc_i >= tx_ring->count);
                //ixgbe_tx_buffer_clean(tx_buffer);
                tx_buffer->hr_i = -1;
                tx_buffer->hr_i_valid = false;

                // DEBUG: Some sanity checking before using header rings
                BUG_ON (hdr_len > IXGBE_MAX_HDR_BYTES);
                BUG_ON (hdr_len == 0); // TSO requires header lens. This
                                       // part of this function requires TSO

                /* The above functions can update tx_flags */
                tx_flags = first->tx_flags;
                cmd_type = ixgbe_tx_cmd_type(tx_flags);

                /* Create the descriptor for the header.  The first data
                 * descriptor must contain the entire length of the TSO
                 * segment (data_len). */
                ixgbe_tx_olinfo_status(tx_desc, tx_flags, data_len);

                /* Copy the header to a header ring and update the TCP
                 * sequence number given the current data offset. */
                memcpy(tx_hdr->raw, skb->data, hdr_len);
                seq_offset = skb_transport_offset (skb) + 4;
                BUG_ON (seq_offset + 4 >= hdr_len);
                seqno = be32_to_cpu(*((__be32 *) &tx_hdr->raw[seq_offset]));

                //DEBUG: Are the sequence numbers reasonable?
                //pr_info ("ixgbe_tx_enqueue_sgsegs:\n");
                //pr_info (" data_offset: %u\n", data_offset);
                //pr_info (" before seqno: %u\n", seqno);

                /* Finish updating the sequence number in the header ring. */
                seqno += data_offset; // Do I need to do anything else?

                //DEBUG: 
                //pr_info (" after seqno: %u\n", seqno);

                //DEBUG: Do we actually have a correct pointer to the TCP header?
                //u16 port;
                //port = be16_to_cpu(*((__be16 *) &tx_hdr->raw[skb_transport_offset (skb)]));
                //pr_info (" src port: %u\n", port);
                //port = be16_to_cpu(*((__be16 *) &tx_hdr->raw[skb_transport_offset (skb) + 2]));
                //pr_info (" dst port: %u\n", port);
                //pr_info (" ackno: %u\n", be32_to_cpu(*((__be32 *) &tx_hdr->raw[seq_offset + 4])));

                /* Update set the new seqno. */
                *((__be32 *) &tx_hdr->raw[seq_offset]) = cpu_to_be32(seqno);

                /* Get the DMA address of the header in the ring. */
                dma = tx_ring->header_ring_dma + IXGBE_TX_HDR_OFFSET(hr_i);

                /* Create the descriptor for the header data. */
                BUG_ON (hr_count != 1);
                tx_buffer->hr_i = hr_i;
                tx_buffer->hr_i_valid = true;
                tx_desc->read.buffer_addr = cpu_to_le64(dma);
                tx_desc->read.cmd_type_len = cpu_to_le32(cmd_type ^ hdr_len);

                //pr_info (" hr_i: %d\n", hr_i);

                /* Update the pointer to the next header ring */
                hr_i++;
                if (hr_i == tx_ring->hr_count)
                        hr_i = 0;
                tx_hdr = IXGBE_TX_HDR(tx_ring, hr_i);

                //pr_info (" used desc: %d\n", i);

                /* Update the current descriptor. */
                BUG_ON (((cur_seg_batch->desc_ftu + desc_count) % tx_ring->count) ==
                        desc_i);
                desc_i++;
                tx_desc++;
                if (desc_i == tx_ring->count) {
                        tx_desc = IXGBE_TX_DESC(tx_ring, 0);
                        desc_i = 0;
                }
                tx_desc->read.olinfo_status = 0;
                tx_buffer = &tx_ring->tx_buffer_info[desc_i];
                //ixgbe_tx_buffer_clean(tx_buffer);
                tx_buffer->hr_i = -1;
                tx_buffer->hr_i_valid = false;
                BUG_ON (desc_i >= tx_ring->count);
        }

        /*
         * Send data_len of data starting at data_offset
         */
        //pr_info ("ixgbe_tx_enqueue_sgsegs_batch: (loop_var: %d)\n", loop_var);
        //pr_info (" sending data_len (%d) starting at data_offset (%d)\n",
        //         data_len, data_offset);

        /* This function currently assumes that there is at least one more
         * data descriptor to be created at this point. */
        BUG_ON (data_len == 0); // This function should only be called
                                // for things that should be TSO
                                // segments

        /* This assumes that there was some skb_head mapped. */
        BUG_ON (first->len == 0);

        size = (skb_headlen(skb) - hdr_len);
        dma = first->dma + hdr_len;
        frag_offset = 0;
        frag_i = 0;
        frag_size = size;
	for (frag = &skb_shinfo(skb)->frags[0];; frag++) {
                //pr_err ("ixgbe_tx_enqueue_sgsegs: nr_frags: %d\n",
                //        skb_shinfo(skb)->nr_frags);
                //pr_err (" frag_i: %d\n", frag_i);

                /* We should always break before the reaching past the last
                 * fragment because we've run out of data. */
                // This can loop around so that frag_i == nr_frags, but it
                // should never loop once more than that.
                BUG_ON (frag_i > skb_shinfo(skb)->nr_frags);

                //DEBUG
                //pr_info ("ixgbe_tx_enqueue_sgsegs: frag loop\n");
                //pr_info (" frag_offset: %d\n", frag_offset);
                //pr_info (" data_offset: %d\n", data_offset);
                //pr_info (" data_len: %d\n", data_len);
                //pr_info (" size: %d\n", size);
        
                if (data_offset < (frag_offset + size)) {
                    /* Pick the right dma address and size. */
                    BUG_ON (frag_offset > data_offset);
                    dma += (data_offset - frag_offset);
                    size -= (data_offset - frag_offset);
                    BUG_ON (size == 0);
                    if (size > data_len)
                        size = data_len;

                    /* Update how much data we have sent and what offset we
                     * are currently at. Because we will enqueue at least
                     * size data right now. */
                    data_len -= size;
                    data_offset += size;

                    /* Since all mapps are performed in advance, nothing
                     * needs to be noted in the tx_buffer. */

                    /* Add the address to the descriptor */
                    tx_desc->read.buffer_addr = cpu_to_le64(dma);

                    while (unlikely(size > IXGBE_MAX_DATA_PER_TXD)) {
                            tx_desc->read.cmd_type_len =
                                    cpu_to_le32(cmd_type ^ IXGBE_MAX_DATA_PER_TXD);
                            //pr_info (" used desc: %d\n", i);

                            BUG_ON (((cur_seg_batch->desc_ftu + desc_count) % tx_ring->count) ==
                                    desc_i);
                            desc_i++;
                            tx_desc++;
                            if (desc_i == tx_ring->count) {
                                    tx_desc = IXGBE_TX_DESC(tx_ring, 0);
                                    desc_i = 0;
                            }
                            tx_desc->read.olinfo_status = 0;

                            dma += IXGBE_MAX_DATA_PER_TXD;
                            size -= IXGBE_MAX_DATA_PER_TXD;

                            tx_desc->read.buffer_addr = cpu_to_le64(dma);
                    }
                    
                    //pr_info (" used desc: %d\n", i);

                    //pr_info ("ixgbe_tx_enqueue_sgsegs: transmitted data.\n");
                    //pr_info (" new data_len: %d, data_offset: %d\n",
                    //         data_len, data_offset);

                    if (!data_len)
                            break;

                    tx_desc->read.cmd_type_len = cpu_to_le32(cmd_type ^ size);

                    BUG_ON (((cur_seg_batch->desc_ftu + desc_count) % tx_ring->count) ==
                            desc_i);
                    desc_i++;
                    tx_desc++;
                    if (desc_i == tx_ring->count) {
                            tx_desc = IXGBE_TX_DESC(tx_ring, 0);
                            desc_i = 0;
                    }
                    tx_desc->read.olinfo_status = 0;

                }

                /* Update our frag_offset even if data has not been sent. */
                frag_offset += frag_size;

                /* Update loop variables for the next fragment */
                frag_size = skb_frag_size(frag);
		size = frag_size;
                //XXX: Trying to copy the FCOE code from the existing code
                // leads to a bug.  The above code already limits the size to
                // data_size anyways, so there shouldn't be any problems.

		dma = first->frag_dma[frag_i].fdma;
                BUG_ON (first->frag_dma[frag_i].flen != frag_size);

		tx_buffer = &tx_ring->tx_buffer_info[desc_i];
                //ixgbe_tx_buffer_clean(tx_buffer);
                tx_buffer->hr_i = -1;
                tx_buffer->hr_i_valid = false;

                /* iterating without a loop variable and then updating it at
                 * the end is dumb. */
                frag_i++;
	}

last_desc:
        /* DEBUG: Error checking that we sent all of the data */
        //TODO

	/* write last descriptor with RS and EOP bits */
	cmd_type |= size | IXGBE_TXD_CMD;
	tx_desc->read.cmd_type_len = cpu_to_le32(cmd_type);

        /* only update data on the last segment */
        if (cur_seg_batch->last_seg) {
            //pr_info ("ixgbe_tx_enqueue_sgsegs_batch:\n");
            //pr_info (" tx-%d: Updating BQL: %d bytes (first: %p, first_i: %d)\n",
            //    tx_ring->queue_index, first->bytecount, first,
            //        cur_seg_batch->skb_batch_data->desc_ftu);

            /* Note how much data has been sent */
            netdev_tx_sent_queue(txring_txq(tx_ring), first->bytecount);

            /* set the timestamp */
            first->time_stamp = jiffies;

            //TODO: should this happen on every seg or only the last seg?
            /*
             * Force memory writes to complete before letting h/w know there
             * are new descriptors to fetch.  (Only applicable for weak-ordered
             * memory model archs, such as IA-64).
             *
             */
            wmb();
        }

        /* set next_to_watch value indicating a packet is present */
        /* This code is less subtle than I've previous thought because the NIC
         * actually does set the DD bit, but still be compatible with subtlety. */
        if (cur_seg_batch->last_seg) {
            BUG_ON (first->next_to_watch != NULL);
            //pr_info (" first (%p) next_to_watch = %p (%d)\n",
            //    first, tx_desc, desc_i);
            first->next_to_watch = tx_desc;

            //XXX: DEBUG
            //pr_info (" setting next_to_watch: %p\n", tx_desc);
            //pr_info (" first: %p\n", first);
            //pr_info (" tx_desc[0]: %p\n", IXGBE_TX_DESC(tx_ring, 0));
        }

        //TODO: we should be asserting that we are never equal to
        // cur_seg_batch->desc_ftu + desc_count.
        //TODO: assert in more places
        BUG_ON (((cur_seg_batch->desc_ftu + desc_count) % tx_ring->count) == desc_i);

        /* Move on to the next descriptor after finishing setting the flags
         * for the last data descriptor. */
	desc_i++;
        tx_desc++;
	if (desc_i == tx_ring->count) {
            tx_desc = IXGBE_TX_DESC(tx_ring, 0);
            desc_i = 0;
        }

        /* Assert that we only try to set the null descriptors once. */
        if (cur_seg_batch->last_seg)
            BUG_ON (first->null_desc_count != 0);

        /* Set null descriptors as necessary.
         *  As a subtlty, the null descriptor count only needs to be set for
         *  the last descriptor because it only applies to descriptors after
         *  next_to_watch. */
        if (cur_seg_batch->last_seg) {
            quit_desc_i = (cur_seg_batch->skb_batch_data->desc_ftu + cur_seg_batch->skb_batch_data->desc_count) % tx_ring->count;
            BUG_ON (quit_desc_i != ((cur_seg_batch->desc_ftu + desc_count) % tx_ring->count));
        } else {
            quit_desc_i = (cur_seg_batch->desc_ftu + desc_count) % tx_ring->count;
        }

        //pr_info (" quit_desc_i: %d\n", quit_desc_i);
        while (desc_i != quit_desc_i) {
            ixgbe_tx_nulldesc(tx_ring, desc_i);
            //pr_info (" making a nulldesc. i: %d\n", desc_i);

            if (cur_seg_batch->last_seg) {
                first->null_desc_count++;
                //pr_info (" counting a nulldesc. i: %d\n", desc_i);
            }

            desc_i++;
            tx_desc++;
            if (desc_i == tx_ring->count) {
                //TODO: delete as tx_desc is no longer necessary
                tx_desc = IXGBE_TX_DESC(tx_ring, 0);
                desc_i = 0;
            }
        }

    }

}

static netdev_tx_t ixgbe_xmit_batch_map_sgseg(struct ixgbe_adapter *adapter,
                                              struct ixgbe_ring *tx_ring)
{
    struct ixgbe_seg_batch_data seg_data[tx_ring->skb_batch_seg_count]; 

    /* Local batch variables (ixgbe_xmit_frame_ring). */
    struct ixgbe_skb_batch_data *_cur_skb_data[IXGBE_MAX_XMIT_BATCH_SIZE];
    struct ixgbe_seg_batch_data *_cur_seg_data[IXGBE_MAX_XMIT_BATCH_SIZE];
    struct ixgbe_tx_buffer *_first[IXGBE_MAX_XMIT_BATCH_SIZE];
    u32 _tx_flags[IXGBE_MAX_XMIT_BATCH_SIZE];
    __be16 _protocol[IXGBE_MAX_XMIT_BATCH_SIZE];
    u16 _desc_i[IXGBE_MAX_XMIT_BATCH_SIZE];
    u8 _hdr_len[IXGBE_MAX_XMIT_BATCH_SIZE];

    /* Local batch variables (ixgbe_tx_map_sgseg) */
    struct skb_frag_struct *_frag[IXGBE_MAX_XMIT_BATCH_SIZE];
    unsigned int _frag_i[IXGBE_MAX_XMIT_BATCH_SIZE];
    dma_addr_t _dma[IXGBE_MAX_XMIT_BATCH_SIZE];
    unsigned int _size[IXGBE_MAX_XMIT_BATCH_SIZE];


    //XXX: DEBUG
    //XXX: Nuclear debugging of all written descriptors!
#if 0
    u32 start_ntu = tx_ring->skb_batch[0].desc_ftu;
    u32 end_ntu = (tx_ring->skb_batch[0].desc_ftu + tx_ring->skb_batch[0].desc_count) % tx_ring->count;
    u32 debug_ntu_i;
#endif

    /* G-opt style (FPP) prefetching variables */
    int I = 0;  // batch index
    void *batch_rips[IXGBE_MAX_XMIT_BATCH_SIZE];
    u64 iMask = 0;
    int temp_index;

    /* iMask needs to be larger than a u64 for larger batch sizes */
    BUG_ON (IXGBE_MAX_XMIT_BATCH_SIZE > 64);

    for (temp_index = 0; temp_index < tx_ring->skb_batch_size; temp_index++) {
        batch_rips[temp_index] = &&sgseg_fpp_start;
    }
sgseg_fpp_start:

    /* Prefetch */
    FPP_PSS(&tx_ring->skb_batch[I], sgseg_fpp_skb_batch1,
        tx_ring->skb_batch_size);
sgseg_fpp_skb_batch1:

    FPP_PSS(tx_ring->skb_batch[I].skb, sgseg_fpp_skb1,
        tx_ring->skb_batch_size);
sgseg_fpp_skb1:

    /* Init local variables */
    _cur_skb_data[I] = &tx_ring->skb_batch[I];
    _tx_flags[I] = 0;
    _protocol[I] = _cur_skb_data[I]->skb->protocol;
    _desc_i[I] = _cur_skb_data[I]->desc_ftu;
    _hdr_len[I] = 0;

    /* Prefetch */
    FPP_PSS(&tx_ring->tx_buffer_info[_desc_i[I]], sgseg_fpp_first1, tx_ring->skb_batch_size);
sgseg_fpp_first1:

    _first[I] = &tx_ring->tx_buffer_info[_desc_i[I]];
    _first[I]->skb = _cur_skb_data[I]->skb;
    _first[I]->bytecount = _cur_skb_data[I]->skb->len;
    _first[I]->gso_segs = 1;
    _first[I]->hr_i_valid = false;
    _first[I]->pktr_i_valid = false;

    //XXX: DEBUG
    //pr_info ("ixgbe_xmit_batch_map_sgseg: (txq: %d)\n", tx_ring->queue_index);
    //pr_info (" _cur_skb_data[I]->desc_ftu: %d\n", _cur_skb_data[I]->desc_ftu);
    //pr_info (" _cur_skb_data[I]->desc_count: %d\n", _cur_skb_data[I]->desc_count);
    ////pr_info (" _desc_i[I]: %d\n", _desc_i[I]);
    ////pr_info (" _first[I]: %p\n", _first[I]);

    /* Prefetch: skb should already be prefetched? benchmark? */

    if (skb_vlan_tag_present(_cur_skb_data[I]->skb)) {
        _tx_flags[I] |= skb_vlan_tag_get(_cur_skb_data[I]->skb) << IXGBE_TX_FLAGS_VLAN_SHIFT;
        _tx_flags[I] |= IXGBE_TX_FLAGS_HW_VLAN;
    } else if (_protocol[I] == htons(ETH_P_8021Q)) {
        /* TODO: support more features later once I've actually proved this helps */
        BUG ();
    }

    /* Prefetch: skb should already be prefetched? benchmark? */

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

/* While I have plans on using DCB in the future, I am not currently using
 * DCB.  I'll deal with this code later. */
#if 0
    /* Prefetch? */
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
#endif

    /* record initial flags and protocol */
    _first[I]->tx_flags = _tx_flags[I];
    _first[I]->protocol = _protocol[I];

#ifdef IXGBE_FCOE
    /* TODO: support more features later once I've actually proved this helps */
    //BUG ();
#endif /* IXGBE_FCOE */

    /* 
     * ixgbe_tx_prepare_skb_sgsegs
     */

    /* assert that this tx_buffer was cleaned properly */
    BUG_ON (_first[I]->len != 0);

    /* First map the skb head. */
    _size[I] = skb_headlen(_first[I]->skb);
    _dma[I] = dma_map_single(tx_ring->dev, _first[I]->skb->data, _size[I], DMA_TO_DEVICE);
    if (dma_mapping_error(tx_ring->dev, _dma[I]))
            BUG (); // Be more robust if this is actually better
    dma_unmap_len_set(_first[I], len, _size[I]);
    dma_unmap_addr_set(_first[I], dma, _dma[I]);

    /* Then map all of the fragments. */
    _frag_i[I] = 0;
    for (_frag[I] = &skb_shinfo(_first[I]->skb)->frags[0];; _frag[I]++) {
        /* Sanity check. */
        //XXX: This is dumb.  I should write this for loop diferently.
        // How about "for (frag_i = 0; frag_i < skb_shinfo(skb)->nr_frags; frag_i++)"
        BUG_ON (_frag_i[I] >= MAX_SKB_FRAGS);
        if (_frag_i[I] == skb_shinfo(_first[I]->skb)->nr_frags)
            break;

        /* map the fragment. */
        _size[I] = skb_frag_size(_frag[I]);
        _dma[I] = skb_frag_dma_map(tx_ring->dev, _frag[I], 0, _size[I],
                       DMA_TO_DEVICE);

                /* check for errors. */
                if (dma_mapping_error(tx_ring->dev, _dma[I]))
                        BUG (); // Be more robust if this is actually better

                /* assert that this tx_buffer was cleaned properly */
                BUG_ON (_first[I]->frag_dma[_frag_i[I]].flen != 0);

        /* record length, and DMA address */
        dma_unmap_len_set(_first[I], frag_dma[_frag_i[I]].flen, _size[I]);
        dma_unmap_addr_set(_first[I], frag_dma[_frag_i[I]].fdma, _dma[I]);

        /* iterating without a loop variable and then updating it at
         * the end is dumb. */
        _frag_i[I]++;
    }


    //XXX: DEBUG
    BUG_ON ((_cur_skb_data[I]->drv_seg_ftu + _cur_skb_data[I]->drv_segs) > 
        tx_ring->skb_batch_seg_count);

    //XXX: DEBUG
    //pr_info ("ixgbe_xmit_batch_map_sgseg:\n");
    //pr_info (" preparing seg data: %d (+%d)\n", 
    //         _cur_skb_data[I]->drv_seg_ftu,
    //         _cur_skb_data[I]->drv_segs);

    /* Build the data structures for each individual segment that will be
     * sent */
    _cur_seg_data[I] = &seg_data[_cur_skb_data[I]->drv_seg_ftu];
    ixgbe_tx_prepare_segs(tx_ring, _cur_seg_data[I], _cur_skb_data[I],
                          _first[I], adapter->drv_gso_size);

sgseg_fpp_end:
    batch_rips[I] = &&sgseg_fpp_end;
    iMask = FPP_SET(iMask, I); 
    if(iMask == (1 << tx_ring->skb_batch_size) - 1) {
        goto sgseg_fpp_nobatch;
    }
    I = (I + 1) % tx_ring->skb_batch_size;
    goto *batch_rips[I];

/*
 * END BATCHED CODE
 */

sgseg_fpp_nobatch:
    //XXX: DEBUG
    //pr_info ("ixgbe_xmit_batch_map_sgseg: after batching\n");
    //pr_info (" tx_ring->skb_batch_seg_count: %d\n",
    //         tx_ring->skb_batch_seg_count);
    //pr_info (" seg_data[0].hr_count: %d\n", seg_data[0].hr_count);

    //XXX: DEBUG: the last segment must be marked as the last segment
    if (seg_data[tx_ring->skb_batch_seg_count - 1].last_seg != 1) {
        pr_info ("txq_%d: ERR No last_seg on batch! seg_count: %d\n", 
                 tx_ring->queue_index, tx_ring->skb_batch_seg_count);
    }
    BUG_ON (seg_data[tx_ring->skb_batch_seg_count - 1].last_seg != 1);

    /* Actually enqueue and send the batch. */
    ixgbe_tx_enqueue_sgsegs_batch (tx_ring, seg_data, tx_ring->skb_batch_seg_count);

    //XXX: DEBUG
    //XXX: This doesn't actually hold even though we haven't written the tail
    // pointer yet?
#if 0
    for (I = 0; I < tx_ring->skb_batch_size; I++) {
        pr_err ("txq_%d ERR: ntw == NULL. I: %d. first: %p\n",
                tx_ring->queue_index, I, _first[I]);
        //BUG_ON (_first[I]->next_to_watch == NULL);
    }
#endif

    //XXX: DEBUG: how many descriptors were actually consumed?
    //XXX: Nuclear debugging of all written descriptors!
#if 0
    BUG_ON (end_ntu != tx_ring->next_to_use);
    pr_info ("ixgbe_xmit_batch_map_sgseg:\n");
    pr_info (" start_ntu: %d\n", start_ntu);
    pr_info (" end_ntu: %d\n", end_ntu);

    struct ixgbe_adv_tx_context_desc *context_desc;
    union ixgbe_adv_tx_desc *tx_desc;
    for (debug_ntu_i = start_ntu;; debug_ntu_i++) {
        if (debug_ntu_i == tx_ring->count)
            debug_ntu_i = 0;
        if (debug_ntu_i == end_ntu)
            break;

        //XXX: not all of these are data descriptors
        tx_desc = IXGBE_TX_DESC(tx_ring, debug_ntu_i);
        if ((le32_to_cpu(tx_desc->read.cmd_type_len) &
             IXGBE_ADVTXD_DTYP_DATA) == IXGBE_ADVTXD_DTYP_DATA) {
            pr_info (" desc: %d. cmd_type_len: %X. olinfo_status: %X\n",
                     debug_ntu_i, tx_desc->read.cmd_type_len,
                     tx_desc->read.olinfo_status);
        } else {
            BUG_ON ((le32_to_cpu(tx_desc->read.cmd_type_len) &
             IXGBE_ADVTXD_DTYP_CTXT) != IXGBE_ADVTXD_DTYP_CTXT);
            context_desc = (struct ixgbe_adv_tx_context_desc *)
                tx_desc;
            pr_info (" desc: %d. vlan_macip_lens: %d. seqnum_seed: %d\n",
                     debug_ntu_i, context_desc->vlan_macip_lens,
                     context_desc->seqnum_seed);
            pr_info ("  type_tucmd_mlhl: %d. mss_l4len_idx: %d\n",
                     context_desc->type_tucmd_mlhl,
                     context_desc->mss_l4len_idx);
        }
    }
#endif


    return NETDEV_TX_OK;

}

static netdev_tx_t ixgbe_xmit_batch_purge(struct ixgbe_adapter *adapter,
                                          struct ixgbe_ring *tx_ring)
{
    netdev_tx_t ret = NETDEV_TX_OK;

    /* Quit early if there are no skb's */
    if (tx_ring->skb_batch_size == 0)
        return NETDEV_TX_OK;

    //XXX: DEBUG
    //pr_info ("ixgbe_xmit_batch_purge:\n");
    //pr_info (" tx_ring->skb_batch_size: %d\n", tx_ring->skb_batch_size);

    //XXX: DEBUG: Track the batch sizes
    if (tx_ring->skb_batch_size_stats_count < IXGBE_MAX_BATCH_SIZE_STATS) {
        if (tx_ring->skb_batch_size > 1)
            tx_ring->skb_batch_size_stats[tx_ring->skb_batch_size_stats_count++] = tx_ring->skb_batch_size;
        else
            tx_ring->skb_batch_size_of_one_stats++;
    }

    /* Use the appropriate batched code for this driver configuration */
    if (adapter->use_sgseg) {
        ret = ixgbe_xmit_batch_map_sgseg(adapter, tx_ring);
        //ret = ixgbe_xmit_batch_map(adapter, tx_ring);
    } else if (adapter->use_pkt_ring) {
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
    tx_ring->skb_batch_seg_count = 0;
    tx_ring->skb_batch_desc_count = 0;
    tx_ring->skb_batch_hr_count = 0;
    tx_ring->skb_batch_pktr_count = 0;

    //XXX: DEBUG
    //pr_info ("\n");

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
    u16 drv_segs = 1;
    u8 tso_or_csum = 0;
    u8 hdr_len = 0;

    /* Sanity check the configuration. */
    BUG_ON (!adapter->xmit_batch);

    /* We should never run out of space in skb_batch */
    BUG_ON (tx_ring->skb_batch_size >= IXGBE_MAX_XMIT_BATCH_SIZE);

    /* Check if this is a TSO or csum packet. */
    tso_or_csum = ixgbe_is_tso_or_csum(adapter, skb, &hdr_len);

    /* Get the number of descriptors required to send the whole batch. This
     * count is dependent on how we are enqueueing skbs. */
    //XXX: There should also be an option to dynamically switch between
    // descriptor approaches based on whether multiple sockets are sharing
    // the batch or not. 
    if (adapter->use_sgseg) {
        skb_desc_count = ixgbe_txd_count_sgsegs(skb, hdr_len, adapter->drv_gso_size);
    } else if (adapter->use_pkt_ring) {
        skb_desc_count = ixgbe_txd_count_pktring(skb);
    } else {
        skb_desc_count = ixgbe_txd_count(skb);
    }

    //XXX: DEBUG
    //u16 port;
    //port = be16_to_cpu(*((__be16 *) &skb->data[skb_transport_offset (skb)]));
    //pr_info ("ixgbe_xmit_frame_ring_batch: (txq: %d)\n", tx_ring->queue_index);
    //pr_info (" skb->len: %d. src port: %u\n", skb->len, port);
    //pr_info (" tso_or_csum: %d\n", tso_or_csum);

    /* Count the number of other ring entries that will be used.  Only TSO or
     * CSUM packets will use the header and packet rings. */
    hr_count = 0;
    pktr_count = 0;
    //XXX: BUG: This should just be ixgbe_is_tso(...) because csum packets do
    // not need to use the header ring.
    if (ixgbe_is_tso(skb)) {
        if (adapter->use_sgseg) {
            drv_segs = DIV_ROUND_UP((skb->len - hdr_len),
                                    adapter->drv_gso_size);
            hr_count = drv_segs; // -1 if the first packet
                                 // uses the existing header
        } else if (adapter->use_pkt_ring) {
            drv_segs = DIV_ROUND_UP((skb->len - hdr_len),
                                    adapter->drv_gso_size);
            pktr_count = drv_segs;
        }
    }

    //XXX: DEBUG
    //pr_info ("ixgbe_xmit_frame_ring_batch:\n");
    //pr_info (" hr_count: %d\n", hr_count);

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
        //pr_info ("ixgbe_xmit_frame_ring_batch: purging and returning NETDEV_TX_BUSY.\n");
        //pr_info (" batch_desc_count: %d\n, ixgbe_desc_unused(tx_ring): %d\n",
        //         batch_desc_count, ixgbe_desc_unused(tx_ring));
        //TODO: check tx_busy somewhere in the measurement scripts 

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
    skb_batch_data->tso_or_csum = tso_or_csum;
    skb_batch_data->hdr_len = hdr_len;
    skb_batch_data->drv_segs = drv_segs;
    /* drv_seg_ftu will need to change with better packet scheduling. */
    skb_batch_data->drv_seg_ftu = tx_ring->skb_batch_seg_count;
    skb_batch_data->desc_count = skb_desc_count;
    skb_batch_data->desc_ftu = tx_ring->next_to_use;

    //pr_info ("txq_%d Adding to batch at index %d: desc_ftu: %d. desc_count: %d. first: %p\n",
    //         tx_ring->queue_index, tx_ring->skb_batch_size - 1,
    //         skb_batch_data->desc_ftu, skb_batch_data->desc_count,
    //         &tx_ring->tx_buffer_info[skb_batch_data->desc_ftu]);

    tx_ring->skb_batch_seg_count += drv_segs;
    tx_ring->skb_batch_desc_count += skb_desc_count;
    tx_ring->next_to_use = (tx_ring->next_to_use + skb_desc_count) % 
        tx_ring->count; //XXX: Is this math right?
    if (adapter->use_sgseg) {
        skb_batch_data->hr_count = hr_count;
        skb_batch_data->hr_ftu = tx_ring->hr_next_to_use;
        tx_ring->skb_batch_hr_count += hr_count;
        tx_ring->hr_next_to_use = (tx_ring->hr_next_to_use + hr_count) % 
            tx_ring->hr_count; //XXX: check math
    } else if (adapter->use_pkt_ring) {
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


