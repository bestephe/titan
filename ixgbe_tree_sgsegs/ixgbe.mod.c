#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

MODULE_INFO(intree, "Y");

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x41b56551, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x2d3385d3, __VMLINUX_SYMBOL_STR(system_wq) },
	{ 0xa1a29548, __VMLINUX_SYMBOL_STR(mdio45_probe) },
	{ 0x2cdc0ef9, __VMLINUX_SYMBOL_STR(kmem_cache_destroy) },
	{ 0x69ff8370, __VMLINUX_SYMBOL_STR(netdev_info) },
	{ 0xfde50075, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x289ab546, __VMLINUX_SYMBOL_STR(pci_bus_read_config_byte) },
	{ 0xd2b09ce5, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0x766bb9dd, __VMLINUX_SYMBOL_STR(ethtool_op_get_ts_info) },
	{ 0xe4689576, __VMLINUX_SYMBOL_STR(ktime_get_with_offset) },
	{ 0xf9a482f9, __VMLINUX_SYMBOL_STR(msleep) },
	{ 0x99840d00, __VMLINUX_SYMBOL_STR(timecounter_init) },
	{ 0xe4fbd6dd, __VMLINUX_SYMBOL_STR(dcb_ieee_setapp) },
	{ 0x85a332b6, __VMLINUX_SYMBOL_STR(pci_enable_sriov) },
	{ 0x619cb7dd, __VMLINUX_SYMBOL_STR(simple_read_from_buffer) },
	{ 0xe2fe7928, __VMLINUX_SYMBOL_STR(debugfs_create_dir) },
	{ 0xd6ee688f, __VMLINUX_SYMBOL_STR(vmalloc) },
	{ 0x6bf1c17f, __VMLINUX_SYMBOL_STR(pv_lock_ops) },
	{ 0x42c6b2f, __VMLINUX_SYMBOL_STR(param_ops_int) },
	{ 0x5d12f949, __VMLINUX_SYMBOL_STR(dcb_ieee_delapp) },
	{ 0x43e4d455, __VMLINUX_SYMBOL_STR(napi_disable) },
	{ 0x754d539c, __VMLINUX_SYMBOL_STR(strlen) },
	{ 0x75b78d57, __VMLINUX_SYMBOL_STR(pci_sriov_set_totalvfs) },
	{ 0x3f368e00, __VMLINUX_SYMBOL_STR(skb_pad) },
	{ 0x19f462ab, __VMLINUX_SYMBOL_STR(kfree_call_rcu) },
	{ 0x1b07640e, __VMLINUX_SYMBOL_STR(napi_gro_flush) },
	{ 0xbd100793, __VMLINUX_SYMBOL_STR(cpu_online_mask) },
	{ 0x3be4a104, __VMLINUX_SYMBOL_STR(node_data) },
	{ 0xf6667985, __VMLINUX_SYMBOL_STR(napi_hash_del) },
	{ 0x49d7a825, __VMLINUX_SYMBOL_STR(pci_disable_device) },
	{ 0xc7a4fbed, __VMLINUX_SYMBOL_STR(rtnl_lock) },
	{ 0xad0112b, __VMLINUX_SYMBOL_STR(pci_disable_msix) },
	{ 0xc9d2c408, __VMLINUX_SYMBOL_STR(netif_carrier_on) },
	{ 0xd9d3bcd3, __VMLINUX_SYMBOL_STR(_raw_spin_lock_bh) },
	{ 0x2065bae, __VMLINUX_SYMBOL_STR(pci_disable_sriov) },
	{ 0xc0a3d105, __VMLINUX_SYMBOL_STR(find_next_bit) },
	{ 0xf33ee054, __VMLINUX_SYMBOL_STR(netif_carrier_off) },
	{ 0x88bfa7e, __VMLINUX_SYMBOL_STR(cancel_work_sync) },
	{ 0x22bce513, __VMLINUX_SYMBOL_STR(mdio_mii_ioctl) },
	{ 0x3fec048f, __VMLINUX_SYMBOL_STR(sg_next) },
	{ 0x949f7342, __VMLINUX_SYMBOL_STR(__alloc_percpu) },
	{ 0xeb0ea004, __VMLINUX_SYMBOL_STR(driver_for_each_device) },
	{ 0x57a670a, __VMLINUX_SYMBOL_STR(__dev_kfree_skb_any) },
	{ 0xeae3dfd6, __VMLINUX_SYMBOL_STR(__const_udelay) },
	{ 0x9580deb, __VMLINUX_SYMBOL_STR(init_timer_key) },
	{ 0xd7507e86, __VMLINUX_SYMBOL_STR(pcie_capability_clear_and_set_word) },
	{ 0x999e8297, __VMLINUX_SYMBOL_STR(vfree) },
	{ 0x5abfbd84, __VMLINUX_SYMBOL_STR(pci_bus_write_config_word) },
	{ 0x68e80556, __VMLINUX_SYMBOL_STR(debugfs_create_file) },
	{ 0x4629334c, __VMLINUX_SYMBOL_STR(__preempt_count) },
	{ 0xb5aa7165, __VMLINUX_SYMBOL_STR(dma_pool_destroy) },
	{ 0x7a2af7b4, __VMLINUX_SYMBOL_STR(cpu_number) },
	{ 0x91715312, __VMLINUX_SYMBOL_STR(sprintf) },
	{ 0x1e6803ff, __VMLINUX_SYMBOL_STR(debugfs_remove_recursive) },
	{ 0xf4c91ed, __VMLINUX_SYMBOL_STR(ns_to_timespec) },
	{ 0x6e336f98, __VMLINUX_SYMBOL_STR(__alloc_pages_nodemask) },
	{ 0x5ff789cc, __VMLINUX_SYMBOL_STR(netif_napi_del) },
	{ 0x7d11c268, __VMLINUX_SYMBOL_STR(jiffies) },
	{ 0xc9ec4e21, __VMLINUX_SYMBOL_STR(free_percpu) },
	{ 0x7b520be6, __VMLINUX_SYMBOL_STR(__dynamic_netdev_dbg) },
	{ 0x733c3b54, __VMLINUX_SYMBOL_STR(kasprintf) },
	{ 0x27c33efe, __VMLINUX_SYMBOL_STR(csum_ipv6_magic) },
	{ 0x531e3889, __VMLINUX_SYMBOL_STR(__pskb_pull_tail) },
	{ 0xccb03e62, __VMLINUX_SYMBOL_STR(ptp_clock_unregister) },
	{ 0x4f8b5ddb, __VMLINUX_SYMBOL_STR(_copy_to_user) },
	{ 0xfe7c4287, __VMLINUX_SYMBOL_STR(nr_cpu_ids) },
	{ 0xb5922dcd, __VMLINUX_SYMBOL_STR(pci_set_master) },
	{ 0xb3558bd0, __VMLINUX_SYMBOL_STR(dca3_get_tag) },
	{ 0xcaef83cf, __VMLINUX_SYMBOL_STR(ptp_clock_event) },
	{ 0x706d051c, __VMLINUX_SYMBOL_STR(del_timer_sync) },
	{ 0xfb578fc5, __VMLINUX_SYMBOL_STR(memset) },
	{ 0xaf62d4e5, __VMLINUX_SYMBOL_STR(devm_hwmon_device_register_with_groups) },
	{ 0xd151919a, __VMLINUX_SYMBOL_STR(dcb_getapp) },
	{ 0xe9fb9db, __VMLINUX_SYMBOL_STR(pci_enable_pcie_error_reporting) },
	{ 0xac34ecec, __VMLINUX_SYMBOL_STR(dca_register_notify) },
	{ 0xcb795fab, __VMLINUX_SYMBOL_STR(netif_tx_wake_queue) },
	{ 0xa75c3478, __VMLINUX_SYMBOL_STR(pci_restore_state) },
	{ 0x5a2cc3b5, __VMLINUX_SYMBOL_STR(netif_tx_stop_all_queues) },
	{ 0x1a33ab9, __VMLINUX_SYMBOL_STR(dca_unregister_notify) },
	{ 0x29338c2e, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0x1916e38c, __VMLINUX_SYMBOL_STR(_raw_spin_unlock_irqrestore) },
	{ 0x56ebd6c7, __VMLINUX_SYMBOL_STR(dev_addr_del) },
	{ 0xfa796766, __VMLINUX_SYMBOL_STR(netif_set_xps_queue) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x9dbaf0cc, __VMLINUX_SYMBOL_STR(ethtool_op_get_link) },
	{ 0x20c55ae0, __VMLINUX_SYMBOL_STR(sscanf) },
	{ 0x3c3fce39, __VMLINUX_SYMBOL_STR(__local_bh_enable_ip) },
	{ 0x449ad0a7, __VMLINUX_SYMBOL_STR(memcmp) },
	{ 0x4c9d28b0, __VMLINUX_SYMBOL_STR(phys_base) },
	{ 0xcd279169, __VMLINUX_SYMBOL_STR(nla_find) },
	{ 0x7b44a2fb, __VMLINUX_SYMBOL_STR(vxlan_get_rx_port) },
	{ 0x4ca1e3bc, __VMLINUX_SYMBOL_STR(free_netdev) },
	{ 0xa1c76e0a, __VMLINUX_SYMBOL_STR(_cond_resched) },
	{ 0xf50f902, __VMLINUX_SYMBOL_STR(register_netdev) },
	{ 0xde4db7, __VMLINUX_SYMBOL_STR(netif_receive_skb) },
	{ 0x5792f848, __VMLINUX_SYMBOL_STR(strlcpy) },
	{ 0xe6205542, __VMLINUX_SYMBOL_STR(kmem_cache_free) },
	{ 0x16305289, __VMLINUX_SYMBOL_STR(warn_slowpath_null) },
	{ 0x6c06c11f, __VMLINUX_SYMBOL_STR(dev_close) },
	{ 0x3b2a6199, __VMLINUX_SYMBOL_STR(pci_wait_for_pending_transaction) },
	{ 0x74d0e802, __VMLINUX_SYMBOL_STR(sk_free) },
	{ 0x61001fe9, __VMLINUX_SYMBOL_STR(netif_set_real_num_rx_queues) },
	{ 0x16e5c2a, __VMLINUX_SYMBOL_STR(mod_timer) },
	{ 0x4ac5ba40, __VMLINUX_SYMBOL_STR(netif_set_real_num_tx_queues) },
	{ 0xf91d6888, __VMLINUX_SYMBOL_STR(netif_napi_add) },
	{ 0x2a37d074, __VMLINUX_SYMBOL_STR(dma_pool_free) },
	{ 0xf0e2fda1, __VMLINUX_SYMBOL_STR(dcb_ieee_getapp_mask) },
	{ 0xb6a68816, __VMLINUX_SYMBOL_STR(find_last_bit) },
	{ 0x97f5a2a9, __VMLINUX_SYMBOL_STR(ptp_clock_register) },
	{ 0x2072ee9b, __VMLINUX_SYMBOL_STR(request_threaded_irq) },
	{ 0xc3878ca6, __VMLINUX_SYMBOL_STR(dca_add_requester) },
	{ 0xc3d6f4f9, __VMLINUX_SYMBOL_STR(simple_open) },
	{ 0xf11543ff, __VMLINUX_SYMBOL_STR(find_first_zero_bit) },
	{ 0xa86f2049, __VMLINUX_SYMBOL_STR(dev_open) },
	{ 0xe523ad75, __VMLINUX_SYMBOL_STR(synchronize_irq) },
	{ 0xc542933a, __VMLINUX_SYMBOL_STR(timecounter_read) },
	{ 0xacdab57, __VMLINUX_SYMBOL_STR(arch_dma_alloc_attrs) },
	{ 0xc911b9d5, __VMLINUX_SYMBOL_STR(eth_get_headlen) },
	{ 0x167c5967, __VMLINUX_SYMBOL_STR(print_hex_dump) },
	{ 0x82318272, __VMLINUX_SYMBOL_STR(pci_select_bars) },
	{ 0xa8b76a68, __VMLINUX_SYMBOL_STR(timecounter_cyc2time) },
	{ 0xc7fbbdc2, __VMLINUX_SYMBOL_STR(netif_device_attach) },
	{ 0x70e48e40, __VMLINUX_SYMBOL_STR(napi_gro_receive) },
	{ 0x2da6cc08, __VMLINUX_SYMBOL_STR(_dev_info) },
	{ 0x40a9b349, __VMLINUX_SYMBOL_STR(vzalloc) },
	{ 0xb8a0c81, __VMLINUX_SYMBOL_STR(kmem_cache_alloc) },
	{ 0x78764f4e, __VMLINUX_SYMBOL_STR(pv_irq_ops) },
	{ 0x890d1753, __VMLINUX_SYMBOL_STR(dev_addr_add) },
	{ 0x7d36e1d5, __VMLINUX_SYMBOL_STR(__free_pages) },
	{ 0x618911fc, __VMLINUX_SYMBOL_STR(numa_node) },
	{ 0x2810ec78, __VMLINUX_SYMBOL_STR(netif_device_detach) },
	{ 0xc1dbc3da, __VMLINUX_SYMBOL_STR(__alloc_skb) },
	{ 0x42c8de35, __VMLINUX_SYMBOL_STR(ioremap_nocache) },
	{ 0x12a38747, __VMLINUX_SYMBOL_STR(usleep_range) },
	{ 0xb79faf88, __VMLINUX_SYMBOL_STR(pci_enable_msix_range) },
	{ 0x5b5d4087, __VMLINUX_SYMBOL_STR(pci_bus_read_config_word) },
	{ 0x1b20f104, __VMLINUX_SYMBOL_STR(__napi_schedule) },
	{ 0x18c08168, __VMLINUX_SYMBOL_STR(pci_bus_read_config_dword) },
	{ 0xbba70a2d, __VMLINUX_SYMBOL_STR(_raw_spin_unlock_bh) },
	{ 0xa651689e, __VMLINUX_SYMBOL_STR(pci_cleanup_aer_uncorrect_error_status) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0xb9249d16, __VMLINUX_SYMBOL_STR(cpu_possible_mask) },
	{ 0x85f39634, __VMLINUX_SYMBOL_STR(kfree_skb) },
	{ 0x34489721, __VMLINUX_SYMBOL_STR(napi_hash_add) },
	{ 0x771e0294, __VMLINUX_SYMBOL_STR(ndo_dflt_fdb_add) },
	{ 0x8cac9ee0, __VMLINUX_SYMBOL_STR(napi_complete_done) },
	{ 0xe09678da, __VMLINUX_SYMBOL_STR(eth_type_trans) },
	{ 0x771cf835, __VMLINUX_SYMBOL_STR(dma_pool_alloc) },
	{ 0xc71904ab, __VMLINUX_SYMBOL_STR(pskb_expand_head) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0xb259b858, __VMLINUX_SYMBOL_STR(netdev_err) },
	{ 0xf85decd1, __VMLINUX_SYMBOL_STR(netdev_features_change) },
	{ 0x467df16d, __VMLINUX_SYMBOL_STR(netdev_rss_key_fill) },
	{ 0xd8a5fd48, __VMLINUX_SYMBOL_STR(pci_enable_msi_range) },
	{ 0x76dd4c17, __VMLINUX_SYMBOL_STR(pci_unregister_driver) },
	{ 0xcc5005fe, __VMLINUX_SYMBOL_STR(msleep_interruptible) },
	{ 0xa294ecf7, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xe259ae9e, __VMLINUX_SYMBOL_STR(_raw_spin_lock) },
	{ 0x3928efe9, __VMLINUX_SYMBOL_STR(__per_cpu_offset) },
	{ 0x680ec266, __VMLINUX_SYMBOL_STR(_raw_spin_lock_irqsave) },
	{ 0xa354d988, __VMLINUX_SYMBOL_STR(kmem_cache_create) },
	{ 0xf6ebc03b, __VMLINUX_SYMBOL_STR(net_ratelimit) },
	{ 0xb3d08165, __VMLINUX_SYMBOL_STR(pci_set_power_state) },
	{ 0x71828aca, __VMLINUX_SYMBOL_STR(netdev_warn) },
	{ 0xbb4f4766, __VMLINUX_SYMBOL_STR(simple_write_to_buffer) },
	{ 0xd92e9ba8, __VMLINUX_SYMBOL_STR(eth_validate_addr) },
	{ 0x1e047854, __VMLINUX_SYMBOL_STR(warn_slowpath_fmt) },
	{ 0xe5ccb157, __VMLINUX_SYMBOL_STR(pci_disable_pcie_error_reporting) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x69acdf38, __VMLINUX_SYMBOL_STR(memcpy) },
	{ 0x33b32ed4, __VMLINUX_SYMBOL_STR(ptp_clock_index) },
	{ 0x45315f2a, __VMLINUX_SYMBOL_STR(pci_disable_msi) },
	{ 0x4b2ef0be, __VMLINUX_SYMBOL_STR(dma_supported) },
	{ 0xc67ea84e, __VMLINUX_SYMBOL_STR(skb_add_rx_frag) },
	{ 0xadbd0986, __VMLINUX_SYMBOL_STR(pci_num_vf) },
	{ 0xedc03953, __VMLINUX_SYMBOL_STR(iounmap) },
	{ 0xab9adbcd, __VMLINUX_SYMBOL_STR(pci_prepare_to_sleep) },
	{ 0x621101e8, __VMLINUX_SYMBOL_STR(__pci_register_driver) },
	{ 0xa8721b97, __VMLINUX_SYMBOL_STR(system_state) },
	{ 0xb352177e, __VMLINUX_SYMBOL_STR(find_first_bit) },
	{ 0x118b737d, __VMLINUX_SYMBOL_STR(pci_get_device) },
	{ 0x63c4d61f, __VMLINUX_SYMBOL_STR(__bitmap_weight) },
	{ 0x984c342c, __VMLINUX_SYMBOL_STR(dev_warn) },
	{ 0xace7802b, __VMLINUX_SYMBOL_STR(unregister_netdev) },
	{ 0x9e0e9e39, __VMLINUX_SYMBOL_STR(ndo_dflt_bridge_getlink) },
	{ 0x55f5019b, __VMLINUX_SYMBOL_STR(__kmalloc_node) },
	{ 0xfdd3cb47, __VMLINUX_SYMBOL_STR(pci_dev_put) },
	{ 0x7abe1b5d, __VMLINUX_SYMBOL_STR(netif_wake_subqueue) },
	{ 0x2e0d2f7f, __VMLINUX_SYMBOL_STR(queue_work_on) },
	{ 0x2b827698, __VMLINUX_SYMBOL_STR(pci_vfs_assigned) },
	{ 0x9e0c711d, __VMLINUX_SYMBOL_STR(vzalloc_node) },
	{ 0x28318305, __VMLINUX_SYMBOL_STR(snprintf) },
	{ 0xd6248830, __VMLINUX_SYMBOL_STR(netdev_all_upper_get_next_dev_rcu) },
	{ 0xa9a23bb1, __VMLINUX_SYMBOL_STR(consume_skb) },
	{ 0xc21bf0bb, __VMLINUX_SYMBOL_STR(dca_remove_requester) },
	{ 0x1d74143e, __VMLINUX_SYMBOL_STR(pci_enable_device_mem) },
	{ 0x7891d22f, __VMLINUX_SYMBOL_STR(__napi_alloc_skb) },
	{ 0xf7389a6b, __VMLINUX_SYMBOL_STR(skb_tstamp_tx) },
	{ 0x24fa1697, __VMLINUX_SYMBOL_STR(skb_put) },
	{ 0xb5263e0d, __VMLINUX_SYMBOL_STR(pci_wake_from_d3) },
	{ 0x96a8e1f0, __VMLINUX_SYMBOL_STR(devm_kmalloc) },
	{ 0x248ec52a, __VMLINUX_SYMBOL_STR(pci_release_selected_regions) },
	{ 0x6d383e8a, __VMLINUX_SYMBOL_STR(pci_request_selected_regions) },
	{ 0xbb128381, __VMLINUX_SYMBOL_STR(irq_set_affinity_hint) },
	{ 0x4f6b400b, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0xb51822bb, __VMLINUX_SYMBOL_STR(param_ops_uint) },
	{ 0x716ed9de, __VMLINUX_SYMBOL_STR(dma_pool_create) },
	{ 0xdb59c5b3, __VMLINUX_SYMBOL_STR(skb_copy_bits) },
	{ 0x38ca2501, __VMLINUX_SYMBOL_STR(pci_find_ext_capability) },
	{ 0x6e720ff2, __VMLINUX_SYMBOL_STR(rtnl_unlock) },
	{ 0x9e7d6bd0, __VMLINUX_SYMBOL_STR(__udelay) },
	{ 0x4bc6fff0, __VMLINUX_SYMBOL_STR(dma_ops) },
	{ 0x6c3c6d2c, __VMLINUX_SYMBOL_STR(pcie_get_minimum_link) },
	{ 0x2f09a788, __VMLINUX_SYMBOL_STR(device_set_wakeup_enable) },
	{ 0x95e4ee5b, __VMLINUX_SYMBOL_STR(pcie_capability_read_word) },
	{ 0xd92f635a, __VMLINUX_SYMBOL_STR(dev_get_stats) },
	{ 0xf20dabd8, __VMLINUX_SYMBOL_STR(free_irq) },
	{ 0xecf7ede0, __VMLINUX_SYMBOL_STR(pci_save_state) },
	{ 0xc60172b0, __VMLINUX_SYMBOL_STR(alloc_etherdev_mqs) },
	{ 0xaf2c2e88, __VMLINUX_SYMBOL_STR(netdev_crit) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=mdio,ptp,dca,vxlan";

MODULE_ALIAS("pci:v00008086d000010B6sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000010C6sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000010C7sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000010C8sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d0000150Bsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000010DDsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000010ECsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000010F1sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000010E1sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000010F4sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000010DBsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00001508sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000010F7sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000010FCsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00001517sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000010FBsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00001507sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00001514sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000010F9sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d0000152Asv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00001529sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d0000151Csv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000010F8sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00001528sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d0000154Dsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d0000154Fsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00001558sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00001557sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d0000154Asv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00001560sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00001563sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000015AAsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000015ABsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000015ADsv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d000015ACsv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "3DFD1E897A814E48F905589");
