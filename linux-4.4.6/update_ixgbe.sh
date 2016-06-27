#!/bin/bash

rsync -avzh ~/git/titan/code/ixgbe_tree_sgsegs/ drivers/net/ethernet/intel/ixgbe

sudo make drivers/net/ethernet/intel/ixgbe/ixgbe.ko

# sudo make-kpkg -j 8 --initrd --append-to-version=-sgsegs kernel-image kernel-headers
