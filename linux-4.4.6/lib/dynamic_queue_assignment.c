/*
 * Dynamic byte queue limits.  See include/linux/dynamic_queue_assignment.h
 *
 * Copyright (c) 2016, Brent Stephens <bstephens2@wisc.edu>
 *
 * Notes: The ifdef's in the individual functions are likely not needed anymore.
 */

/* TODO: some of this likely aren't needed. Ye goode olde copy+pasta */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/dynamic_queue_assignment.h>
#include <linux/compiler.h>
#include <linux/export.h>

void dqa_queue_init(struct dqa_queue *dqaq)
{
	atomic_set(&dqaq->tx_sk_enqcnt, 0);
	/* Must increment before using first */
	atomic_set(&dqaq->tx_sk_trace_maxi, -1); 
}
EXPORT_SYMBOL(dqa_queue_init);

/* This function probably has locking problems. */
static void dqa_update_xps(struct net_device *dev)
{
	struct xps_dev_maps *dev_maps;
	struct xps_map *map;
	struct netdev_queue *txq;
	int queue_index;
	int cpu_i;

	rcu_read_lock();
	dev_maps = rcu_dereference(dev->xps_maps);
	if (dev_maps) {
		for (cpu_i = 0; cpu_i < num_online_cpus(); cpu_i++) {
			map = rcu_dereference(
			    dev_maps->cpu_map[cpu_i]);
			if (map) {
				queue_index = map->queues[map->len - 1];
				txq = netdev_get_tx_queue(dev,
					queue_index);
				txq->dqa_queue.tx_overflowq = 1;

				/* XXX: DEBUG */
				trace_printk("dqa_update: Marked txq-%d as "
					     "an XPS overflowq\n",
					     queue_index);


			}
		}
	}
	rcu_read_unlock();
}

/* Note: This function is used to mark the overflowq so that it may always be
 * segmented.  To avoid errors, this will always mark the overflowq even if it
 * is not the current DQA algorithm that is being used. */
void dqa_update(struct net_device *dev)
{
	struct netdev_queue *txq;
	int i;

	/* Start by resetting the overflowq flags. */
	for (i = 0; i < dev->num_tx_queues; i++) {
		txq = &dev->_tx[i];
		txq->dqa_queue.tx_overflowq = 0;
		txq->dqa_queue.tx_qi = i;
	}

	/* Mark the XPS overflow queues. */
	dqa_update_xps(dev);
	
	/* If the non-XPS overflowq is not an XPS
	 * overflowq, then the system is misconfigured. */
	txq = &dev->_tx[(dev->real_num_tx_queues - 1)];
	if (!txq->dqa_queue.tx_overflowq) {
		trace_printk("dqa_update: WARNING! txq-%d is the non-XPS "
			     "overflowq but not an XPS overflowq!\n",
			     (dev->real_num_tx_queues - 1));
		netdev_warn(dev, "dqa_update: WARNING! txq-%d is the "
			    "non-XPS overflowq but not an XPS overflowq!\n",
			    (dev->real_num_tx_queues - 1));
	}

	/* Mark the last queue as an overflowq. */
	txq = &dev->_tx[(dev->real_num_tx_queues - 1)];
	txq->dqa_queue.tx_overflowq = 1;

	/* XXX: DEBUG */
	trace_printk("dqa_update: Marked txq-%d as overflowq\n",
		     (dev->real_num_tx_queues - 1));

}
EXPORT_SYMBOL(dqa_update);

void dqa_init(struct net_device *dev)
{
	struct dqa *dqa = &dev->dqa;

	dqa->dqa_alg = DQA_ALG_HASH;
	dqa->segment_sharedq = 0;

	spin_lock_init(&dqa->oq_lock);

	dqa_update(dev);
}
EXPORT_SYMBOL(dqa_init);

static int dqa_get_xps_queue_even(struct net_device *dev, struct sk_buff *skb)
{
#if defined(CONFIG_XPS) && defined(CONFIG_DQA)
	struct xps_dev_maps *dev_maps;
	struct xps_map *map;
	struct netdev_queue *txq;
	int min_enqcnt = INT_MAX;
	int min_queue = -1;
	int queue_index;
	int enqcnt;
	int map_i;

	rcu_read_lock();
	dev_maps = rcu_dereference(dev->xps_maps);
	/* XXX: DEBUG */	
	//netdev_err(dev, "get_xps_dqa_queue_even: dev_maps: %p\n", dev_maps);
	if (dev_maps) {
		map = rcu_dereference(
		    dev_maps->cpu_map[skb->sender_cpu - 1]);
		if (map) {
			if (map->len == 1)
				min_queue = map->queues[0];
			else {
				/* XXX: DEBUG */	
				//netdev_err(dev, "get_xps_dqa_queue_even:\n");

				for (map_i = 0; map_i < map->len; map_i++) {
					/* TODO: add in prefetching of the txq's */

					queue_index = map->queues[map_i];
					txq = netdev_get_tx_queue(dev,
						queue_index);
					enqcnt = atomic_read(&txq->dqa_queue.tx_sk_enqcnt);

					/* XXX: DEBUG */
					//netdev_err(dev, " mqi: %d, menq: %d, qi: %d, enqcnt: %d\n",
					//	   min_queue, min_enqcnt, queue_index, enqcnt);

					if (enqcnt < min_enqcnt) {
						min_queue = queue_index;
						min_enqcnt = enqcnt;
					}

					if (enqcnt == 0)
						break;
				}
			}

			if (unlikely(min_queue >= dev->real_num_tx_queues))
				min_queue = -1;
		}
	}
	rcu_read_unlock();

	return min_queue;
#else
	return -1;
#endif
}

static int dqa_get_xps_queue_overflowq(struct net_device *dev, struct sk_buff *skb)
{
#if defined(CONFIG_XPS) && defined(CONFIG_DQA)
	struct xps_dev_maps *dev_maps;
	struct xps_map *map;
	struct netdev_queue *txq;
	int queue_index = -1;
	int enqcnt;
	int map_i;

	rcu_read_lock();
	dev_maps = rcu_dereference(dev->xps_maps);
	/* XXX: DEBUG */	
	//netdev_err(dev, "get_xps_dqa_queue_overflowq: dev_maps: %p\n", dev_maps);
	if (dev_maps) {
		map = rcu_dereference(
		    dev_maps->cpu_map[skb->sender_cpu - 1]);
		if (map) {
			if (map->len == 1)
				queue_index = map->queues[0];
			else {
				/* XXX: DEBUG */	
				//netdev_err(dev, "get_xps_dqa_queue_overflowq:\n");

				for (map_i = 0; map_i < map->len; map_i++) {
					/* TODO: add in prefetching of the txq's */

					queue_index = map->queues[map_i];
					txq = netdev_get_tx_queue(dev,
						queue_index);
					enqcnt = atomic_read(&txq->dqa_queue.tx_sk_enqcnt);

					/* XXX: DEBUG */
					//netdev_err(dev, " qi: %d, enqcnt: %d\n",
					//	   queue_index, enqcnt);

					if (enqcnt == 0)
						break;
				}

				/* This queue should be already selected
				 * because of the for loop above.  This code is
				 * really only necessary for sanity checking.
				 * */
				if (map_i == map->len) {
					queue_index = map->queues[map->len - 1];

					/* XXX: DEBUG: Using the overflowq.
					 * Assert that it is marked as such. */
					txq = netdev_get_tx_queue(dev,
						queue_index);
					BUG_ON(!txq->dqa_queue.tx_overflowq);
				}

				
			}

			if (unlikely(queue_index >= dev->real_num_tx_queues))
				queue_index = -1;
		}
	}
	rcu_read_unlock();

	return queue_index;
#else
	return -1;
#endif
}

int dqa_get_xps_queue(struct net_device *dev, struct sk_buff *skb)
{
	int queue_index = -1;

	/* BS: this code feels ugly. Is there a cleaner way to change between
	 * all of the queue assignment algorithms? */
	if (dev->dqa.dqa_alg == DQA_ALG_EVEN) {
		queue_index = dqa_get_xps_queue_even(dev, skb);
	} else if (dev->dqa.dqa_alg == DQA_ALG_OVERFLOWQ) {
		queue_index = dqa_get_xps_queue_overflowq(dev, skb);
	}

	return queue_index;
}
EXPORT_SYMBOL(dqa_get_xps_queue);

static int dqa_get_queue_even(struct net_device *dev, struct sk_buff *skb)
{
#ifdef CONFIG_DQA
	struct netdev_queue *txq;
	int min_enqcnt = INT_MAX;
	int min_queue = -1;
	int queue_index;
	int enqcnt;

	/* XXX: DEBUG */	
	//trace_printk("dqa_get_queue_even:\n");
	//netdev_err(dev, "dqa_get_queue_even:\n");

	for (queue_index = 0; queue_index < dev->real_num_tx_queues;
	     queue_index++) {
		/* TODO: add in prefetching of the txq's */

		txq = netdev_get_tx_queue(dev, queue_index);
		enqcnt = atomic_read(&txq->dqa_queue.tx_sk_enqcnt);

		/* XXX: DEBUG */
		//netdev_err(dev, " mqi: %d, menq: %d, qi: %d, enqcnt: %d\n",
		//	   min_queue, min_enqcnt, queue_index, enqcnt);

		if (enqcnt == 0) {
			return queue_index;
		} else if (enqcnt < min_enqcnt) {
			min_queue = queue_index;
			min_enqcnt = enqcnt;
		}
	}

	return min_queue;
#else
	return -1;
#endif
}

static int dqa_get_queue_overflowq(struct net_device *dev, struct sk_buff *skb)
{
#ifdef CONFIG_DQA
	struct netdev_queue *txq;
	int queue_index;
	int enqcnt;

	/* XXX: TODO: This algorithm would be much faster if we tracked the
	 * empty queues (or number of empty queues) somewhere*/

	/* XXX: DEBUG */	
	//trace_printk("dqa_get_queue_overflowq:\n");
	//netdev_err(dev, "dqa_get_queue_overflowq:\n");

	for (queue_index = 0; queue_index < dev->real_num_tx_queues;
	     queue_index++) {
		/* TODO: add in prefetching of the txq's */

		txq = netdev_get_tx_queue(dev, queue_index);
		enqcnt = atomic_read(&txq->dqa_queue.tx_sk_enqcnt);

		/* XXX: DEBUG */
		//netdev_err(dev, " qi: %d, enqcnt: %d\n", queue_index, enqcnt);

		if (enqcnt == 0)
			return queue_index;
	}


	/* XXX: DEBUG: Using the overflowq.  Assert that it is marked as such. */
	if (queue_index == (dev->real_num_tx_queues - 1)) {
		txq = netdev_get_tx_queue(dev,
			queue_index);
		BUG_ON(!txq->dqa_queue.tx_overflowq);
	}

	/* The last queue is the overflow queue if there are no empty queues. */
	return dev->real_num_tx_queues - 1;
#else
	return -1;
#endif
}

int dqa_get_queue(struct net_device *dev, struct sk_buff *skb)
{
	int queue_index = -1;

	if (dev->dqa.dqa_alg == DQA_ALG_EVEN)
		queue_index = dqa_get_queue_even(dev, skb);
	else if (dev->dqa.dqa_alg == DQA_ALG_OVERFLOWQ)
		queue_index = dqa_get_queue_overflowq(dev, skb);

	/* Do nothing to get the behavior of DQA_ALG_HASH */

	return queue_index;
}
EXPORT_SYMBOL(dqa_get_queue);

static int dqa_txq_needs_seg_overflowq(struct net_device *dev,
				       struct netdev_queue *txq)
{
	/* Sanity checking probably not required. */
	if (!txq->dqa_queue.tx_overflowq) {
		//BUG_ON(atomic_read(&txq->dqa_queue.tx_sk_enqcnt) > 1);
		if (atomic_read(&txq->dqa_queue.tx_sk_enqcnt) > 1) {
			//printk(KERN_ERR "non-overflowq has more than one sk. "
			//       "enqcnt: %d\n",
			//       atomic_read(&txq->dqa_queue.tx_sk_enqcnt));
			trace_printk("non-overflowq has more than one sk. "
				     "enqcnt: %d\n",
				     atomic_read(&txq->dqa_queue.tx_sk_enqcnt));
		}
	}

	return txq->dqa_queue.tx_overflowq;
}

/* Note: it would be desirable to mark a queue as being an overflowq in the
 * init phase of dqa, but this is probably trickier than it seems given that
 * XPS mappings can be updated (See netif_set_real_num_tx_queues(...),
 * remove_xps_queue(...), netif_set_xps_queue(...), etc.). */
int dqa_txq_needs_seg(struct net_device *dev, struct netdev_queue *txq)
{
	struct dqa *dqa = &dev->dqa;
	int needs_seg = 0;

	if (dqa->dqa_alg == DQA_ALG_OVERFLOWQ) {
		needs_seg = dqa_txq_needs_seg_overflowq(dev, txq);
	}

	if (dqa->segment_sharedq &&
	    atomic_read(&txq->dqa_queue.tx_sk_enqcnt) > 1) {
		needs_seg = 1;
	}

	return needs_seg;
}
EXPORT_SYMBOL(dqa_txq_needs_seg);
