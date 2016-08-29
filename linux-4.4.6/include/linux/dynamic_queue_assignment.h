/*
 * Dynamic queue assignment (dqa) - Definitions
 *
 * Copyright (c) 2016, Brent Stephens <bstephens2@wisc.edu>
 *
 * This header file contains the definitions for dynamic queue assignment (dqa).
 * 
 * Similar to dynamic queue limits (dql), this file defines functions for
 * dynamically choosing which queue to use in a multi-queue system.  However,
 * unlike dql, these functions are intimately tied to the implementation of
 * multiqueue network devices. 
 *
 * This functionality is a WIP.
 */

#ifndef _LINUX_DQA_H
#define _LINUX_DQA_H

#ifdef __KERNEL__

#include <linux/netdevice.h>
#include <linux/skbuff.h>

/**
 * enum net_device_dqa_algs - &struct net_device dqa_alg
 *
 * @DQA_ALG_HASH: Default hashing algorithm
 * @DQA_ALG_EVEN: Try to evenly distribute flows across queues
 * @DQA_ALG_*: Description
 */
enum netdev_dqa_algs {
	DQA_ALG_HASH,
	DQA_ALG_EVEN,
	DQA_ALG_OVERFLOWQ,
};

#define DQA_ALG_HASH			DQA_ALG_HASH
#define DQA_ALG_EVEN			DQA_ALG_EVEN
#define DQA_ALG_OVERFLOWQ		DQA_ALG_OVERFLOWQ


void dqa_queue_init(struct dqa_queue *dqaq);

/* Should be called after dqa_queue_init */
void dqa_init(struct net_device *dev);

/* Should be called whenever the number of tx queues or XPS is updated. */
void dqa_update(struct net_device *dev);

int dqa_get_xps_queue(struct net_device *dev, struct sk_buff *skb);

int dqa_get_queue(struct net_device *dev, struct sk_buff *skb);

int dqa_txq_needs_seg(struct net_device *dev, struct netdev_queue *txq);

#endif /* _KERNEL_ */

#endif /* _LINUX_DQA_H */
