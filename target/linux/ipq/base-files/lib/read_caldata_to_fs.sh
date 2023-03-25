#!/bin/sh
#
# Copyright (c) 2015 The Linux Foundation. All rights reserved.
# Copyright (C) 2011 OpenWrt.org

. /lib/ipq806x.sh

do_load_ipq4019_board_bin()
{
    local board=$(ipq806x_board_name)
    local mtdblock=$(find_mtd_part 0:ART)

    local apdk="/tmp"
    local dft_country='US'
    local suffix='.b292'
    local country=$(getfirm COUNTRY)
    
    local bd_filename dft_bd_filename target_board_bd_filename target_bd_filename

    if [ -z "$mtdblock" ]; then
        # read from mmc
        mtdblock=$(find_mmc_part 0:ART)
    fi

    [ -n "$mtdblock" ] || return

    # load board.bin
    case "$board" in
            ap-dk0*)
                    mkdir -p ${apdk}
                    dd if=${mtdblock} of=${apdk}/wifi0.caldata bs=32 count=377 skip=128
                    dd if=${mtdblock} of=${apdk}/wifi1.caldata bs=32 count=377 skip=640
            ;;
            ap16* | ap148*)
                    mkdir -p ${apdk}
                    dd if=${mtdblock} of=${apdk}/wifi0.caldata bs=32 count=377 skip=128
                    dd if=${mtdblock} of=${apdk}/wifi1.caldata bs=32 count=377 skip=640
                    dd if=${mtdblock} of=${apdk}/wifi2.caldata bs=32 count=377 skip=1152
            ;;
            ap-hk01-*)
                    HK_BD_FILENAME=/lib/firmware/IPQ8074/bdwlan.bin
                    mkdir -p ${apdk}/IPQ8074
                    if [ -f "$HK_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$HK_BD_FILENAME")
                    else
                        FILESIZE=131072
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ8074/caldata.bin bs=1 count=$FILESIZE skip=4096
                    [ -L /lib/firmware/IPQ8074/caldata.bin ] || \
                    ln -s ${apdk}/IPQ8074/caldata.bin /lib/firmware/IPQ8074/caldata.bin
            ;;
            ap-hk* | ap-ac* | ap-oa*)
                    HK_BD_FILENAME=/lib/firmware/IPQ8074/bdwlan.bin
                    mkdir -p ${apdk}/IPQ8074

                    [ "$board" == "ap-ac01" ] && suffix='.b91'
                    [ "$board" == "ap-ac02" ] && suffix='.b92'			
                    [ "$board" == "ap-ac03" ] && suffix='.b291'			
                    [ "$board" == "ap-ac04" ] && suffix='.b292'			

                    bd_filename="/lib/firmware/IPQ8074/bdwlan_""$country""$suffix"
                    dft_bd_filename="/lib/firmware/IPQ8074/bdwlan_""$dft_country""$suffix"
                    target_board_bd_filename=${apdk}"/IPQ8074/bdwlan""$suffix"
                    target_bd_filename=${apdk}"/IPQ8074/bdwlan.bin"

                    if [ -f $bd_filename ]; then
                    	cp $bd_filename $target_board_bd_filename
                    	cp $bd_filename $target_bd_filename
                    elif [ -f $dft_bd_filename ]; then 
                    	cp $dft_bd_filename $target_board_bd_filename
                    	cp $dft_bd_filename $target_bd_filename
                    fi
	
                    dd if=${mtdblock} of=${apdk}/wifi1.caldata bs=1 count=12064 skip=208896
                    if [ -f "$HK_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$HK_BD_FILENAME")
                    else
                        FILESIZE=131072
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ8074/caldata.bin bs=1 count=$FILESIZE skip=4096
#                    [ -L /lib/firmware/IPQ8074/caldata.bin ] || \
#                    ln -s ${apdk}/IPQ8074/caldata.bin /lib/firmware/IPQ8074/caldata.bin
            ;;
            ap-cp*)
                    CP_BD_FILENAME=/lib/firmware/IPQ6018/bdwlan.bin
                    mkdir -p ${apdk}/IPQ6018
                    if [ -f "$CP_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$CP_BD_FILENAME")
                    else
                        FILESIZE=65536
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ6018/caldata.bin bs=1 count=$FILESIZE skip=4096
                    [ -L /lib/firmware/IPQ6018/caldata.bin ] || \
                    ln -s ${apdk}/IPQ6018/caldata.bin /lib/firmware/IPQ6018/caldata.bin
            ;;
   esac
}

