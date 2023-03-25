#!/bin/sh
# Copyright (C) 2009 OpenWrt.org

. /lib/network/vlan_func.sh

#update vlan info to config -- /etc/vlan.d/vlan
lock_vlan="/var/run/vlan.lock"

DEBUG_OUTOUT=1

switch_echo() {
    if [ "$DEBUG_OUTOUT" -gt 0 ]; then
            echo "${1}: ""$2"> /dev/console
        fi
}


__set_wan_vlan() {

    config_load iptv_v2
    config_get iptv_enable info enable 0
    config_get iptv_vid info iptv_vid
    config_get iptv_prio info iptv_prio
    config_get iptv_type info iptv_type "normal"
    config_clear

    local used_vid
    local vid_p4
    local vid_p5
    local used_vid=$(get_used_vlan)

    local is_eth0_at_wan=`brctl show br-wan | grep eth0`
    local is_eth1_at_wan=`brctl show br-wan | grep eth1`

    switch_name=$(uci -c /etc/vlan.d get vlan.@switch[0].name)
	vlan_id=$(uci get network.vlan.id)
	vlan_tag=$(uci get network.vlan.tag_802_1q)
    iptv_vlan_config=$(uci -c /etc/vlan.d get vlan.iptv)

    if [ "$iptv_enable" = "0" ]; then
        if [ -n "$iptv_vlan_config" ]; then
            switch_echo switch "iptv disable, delete vlan config about iptv"
            uci -c /etc/vlan.d delete vlan.iptv
            uci commit -c /etc/vlan.d
            swconfig dev $switch_name load /etc/vlan.d/vlan
        fi
    fi

    if [ -z "$is_eth0_at_wan" -a -z "$is_eth1_at_wan" ]; then
        switch_echo switch "no interface at br-wan"

    elif [ -z "$is_eth0_at_wan" ]; then
        switch_echo switch "eth1 at br-wan"
        if [ "$vlan_tag" == "0" ]; then
            __set_port4_wvid "$vlan_id" "u"
        else
            __set_port4_wvid "$vlan_id" "t"
        fi

        vid_p5=$(__set_port5_lvid "${used_vid}" "u")
        append used_vid "$vid_p5"
        switch_echo switch "set_wan_vlan: select p5: $vid_p5"

        if [ "$iptv_enable" = "1" ]; then
            switch_echo switch "set eth0 to iptv"
            uci -c /etc/vlan.d set vlan.iptv="switch_vlan"
            uci -c /etc/vlan.d set vlan.iptv.ports="0t 1 2 3 4t"
            uci -c /etc/vlan.d set vlan.iptv.device="$switch_name"
            uci -c /etc/vlan.d set vlan.iptv.vlan="10"
            uci -c /etc/vlan.d set vlan.iptv.vid=$iptv_vid
        fi

        uci commit -c /etc/vlan.d
        swconfig dev $switch_name load /etc/vlan.d/vlan

    elif [ -z "$is_eth1_at_wan" ]; then
        switch_echo switch "eth0 at br-wan"

        if [ "$vlan_tag" == "0" ]; then
            __set_port5_wvid "$vlan_id" "u"
        else
            __set_port5_wvid "$vlan_id" "t"
        fi

        vid_p4=$(__set_port4_lvid "${used_vid}" "u")
        append used_vid "$vid_p4"
        switch_echo switch "set_wan_vlan: select p4: $vid_p4"

        if [ "$iptv_enable" = "1" ]; then
            switch_echo switch "set eth1 to iptv"
            uci -c /etc/vlan.d set vlan.iptv="switch_vlan"
            uci -c /etc/vlan.d set vlan.iptv.ports="0t 5t"
            uci -c /etc/vlan.d set vlan.iptv.device="$switch_name"
            uci -c /etc/vlan.d set vlan.iptv.vlan="10"
            uci -c /etc/vlan.d set vlan.iptv.vid=$iptv_vid
        fi

        uci commit -c /etc/vlan.d
        swconfig dev $switch_name load /etc/vlan.d/vlan
    else
        switch_echo switch "both eth0 and eth1 at br-wan"

        local wanstatus=$(cat /tmp/wanDetection) 
        if [ "$wanstatus" = "0" ]; then
            local eth0_flag=$(isWan eth0)
            local eth1_flag=$(isWan eth1)
            if [ "$eth0_flag" = "true" ] && [ "$eth1_flag" = "false" ]; then
                #eth0 is wan
                if [ "$vlan_tag" == "0" ]; then
                    __set_port5_wvid "$vlan_id" "u"
                else
                    __set_port5_wvid "$vlan_id" "t"
                fi

                vid_p4=$(__set_port4_lvid "${used_vid}" "u")
                append used_vid "$vid_p4"
                switch_echo switch "set_wan_vlan: select p4: $vid_p4"

            elif [ "$eth0_flag" = "false" ] && [ "$eth1_flag" = "true" ]; then
                #eth1 is wan
                if [ "$vlan_tag" == "0" ]; then
                    __set_port4_wvid "$vlan_id" "u"
                else
                    __set_port4_wvid "$vlan_id" "t"
                fi

                vid_p5=$(__set_port5_lvid "${used_vid}" "u")
                append used_vid "$vid_p5"
                switch_echo switch "set_wan_vlan: select p5: $vid_p5"
            else
                if [ "$vlan_tag" == "0" ]; then
                    __set_both_wvid_tag "$vlan_id" "u"
                else
                    __set_both_wvid_tag "$vlan_id" "t"
                fi
            fi
        else
            if [ "$vlan_tag" == "0" ]; then
                __set_both_wvid_tag "$vlan_id" "u"
            else
                __set_both_wvid_tag "$vlan_id" "t"
            fi
        fi

        uci commit -c /etc/vlan.d
        swconfig dev $switch_name load /etc/vlan.d/vlan

    fi

}

set_wan_vlan() {
    lock $lock_vlan
    trap "" INT TERM ABRT QUIT ALRM 
    __set_wan_vlan
    trap - INT TERM ABRT QUIT ALRM 
    lock -u $lock_vlan
}

__reset_wan_vlan() {
    switch_name=$(uci -c /etc/vlan.d get vlan.@switch[0].name)
    iptv_vlan_config=$(uci -c /etc/vlan.d get vlan.iptv)

    config_load iptv_v2
    config_get iptv_enable info enable 0
    config_get iptv_vid info iptv_vid
    config_get iptv_prio info iptv_prio
    config_get iptv_type info iptv_type "normal"
    config_clear

    local used_vid
    local vid_p4
    local vid_p5
    local used_vid=$(get_used_vlan)

    local is_eth0_at_wan=`brctl show br-wan | grep eth0`
    local is_eth1_at_wan=`brctl show br-wan | grep eth1`

    if [ "$iptv_enable" = "0" ]; then
        if [ -n "$iptv_vlan_config" ]; then
            switch_echo switch "iptv disable, delete vlan config about iptv"
            uci -c /etc/vlan.d delete vlan.iptv
        fi
    else

        if [ -z "$is_eth0_at_wan" -a -z "$is_eth1_at_wan" ]; then
            switch_echo switch "vlan not enable, no interface at br-wan"
        elif [ -z "$is_eth0_at_wan" ]; then
            switch_echo switch "vlan not enable, eth1 at br-wan"

            if [ "$iptv_enable" = "1" ]; then
                switch_echo switch "set eth1 to iptv"
                uci -c /etc/vlan.d set vlan.iptv="switch_vlan"
                uci -c /etc/vlan.d set vlan.iptv.ports="0t 1 2 3 4t"
                uci -c /etc/vlan.d set vlan.iptv.device="$switch_name"
                uci -c /etc/vlan.d set vlan.iptv.vlan="10"
                uci -c /etc/vlan.d set vlan.iptv.vid=$iptv_vid
            fi

        elif [ -z "$is_eth1_at_wan" ]; then
            switch_echo switch "vlan not enable, eth0 at br-wan"

            if [ "$iptv_enable" = "1" ]; then
                switch_echo switch "set eth0 to iptv"
                uci -c /etc/vlan.d set vlan.iptv="switch_vlan"
                uci -c /etc/vlan.d set vlan.iptv.ports="0t 5t"
                uci -c /etc/vlan.d set vlan.iptv.device="$switch_name"
                uci -c /etc/vlan.d set vlan.iptv.vlan="10"
                uci -c /etc/vlan.d set vlan.iptv.vid=$iptv_vid
            fi
        fi
    fi

    vid_p4=$(__set_port4_lvid "${used_vid}" "u")
    append used_vid "$vid_p4"
    switch_echo switch "reset_wan_vlan: select p4: $vid_p4"

    vid_p5=$(__set_port5_lvid "${used_vid}" "u")
    append used_vid "$vid_p5"
    switch_echo switch "reset_wan_vlan: select p5: $vid_p5"

    uci commit -c /etc/vlan.d
    swconfig dev $switch_name load /etc/vlan.d/vlan

}

reset_wan_vlan()
{

    lock $lock_vlan
    trap "" INT TERM ABRT QUIT ALRM 
    __reset_wan_vlan
    trap - INT TERM ABRT QUIT ALRM 
    lock -u $lock_vlan
}

setup_switch_dev() {
	config_get name "$1" name
	name="${name:-$1}"

	device_id=$(getfirm DEV_ID)
	role=$(uci get bind_device_list."$device_id".role)
	vlan_enable=$(uci get network.vlan.enable)
	if [ "$vlan_enable" == "1" -o "$vlan_enable" == "0" ];then
        [ "$role" != "RE" ] && {
            if [ "$vlan_enable" == "1" ]; then 
                __set_wan_vlan
            elif [ "$vlan_enable" == "0" ]; then 
                __reset_wan_vlan
			fi
		}
	else
		uci set network.vlan=vlan
		uci set network.vlan.enable=0
		uci set network.vlan.isp_name=0
		uci set network.vlan.id=0
		uci set network_sync.vlan=vlan
		uci set network_sync.vlan.enable=0
		uci set network_sync.vlan.isp_name=0
		uci set network_sync.vlan.id=0
		uci commit network
		uci commit network_sync
		saveconfig 
	fi
	
	#set switch name
	[ -d "/sys/class/net/$name" ] && ifconfig "$name" up
	device_name=$(uci -c /etc/vlan.d get vlan.@switch[0].name)
	[ "$device_name" != "$name" ] && {
		uci -c /etc/vlan.d set vlan.@switch[0].name="$name"
		uci -c /etc/vlan.d set vlan.vlan0.device="$name"
		uci -c /etc/vlan.d set vlan.vlan1.device="$name"
	}

	#todo uci get network vlan info -> /etc/vlan.d/vlan
	swconfig dev "$name" load /etc/vlan.d/vlan
}

set_switch_default_fdb() {
	mac=$(getfirm MAC)
	# bind lan mac to cpu port to avoid ARP attack #	
	local lan_vsi_list=$(uci get profile.switch.lan_vsi_list -c /etc/profile.d) 	
	if [ -n "$lan_vsi_list" ]; then
		local vsi
		for vsi in $lan_vsi_list; do
			ssdk_sh fdb entry add $mac $vsi forward forward 0 yes no no no no no no no
			ssdk_sh vsi stationmove set $vsi disable forward
			ssdk_sh vsi learnctrl set $vsi disable forward			
		done
	else
		ssdk_sh fdb entry add $mac 1 forward forward 0 yes no no no no no no no		
	fi
}

setup_switch() {
    lock $lock_vlan
    trap "" INT TERM ABRT QUIT ALRM 

	config_load network
	config_foreach setup_switch_dev switch
    config_clear
	set_switch_default_fdb

    trap - INT TERM ABRT QUIT ALRM 
    lock -u $lock_vlan
}


