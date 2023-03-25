. /lib/functions.sh

eth0_dft_vid=2
eth1_dft_vid=1


select_vid()
{
	local used="$1"
	local vid=1

	
	for id in `seq 4094 -1 1`; do
		! list_contains used $id && {
			vid=$id
			break		
		}
	done

    eval "$2='$vid'"
}

# $1: vid
# $2: used vid
is_used_vid()
{
	local used="$1"
	local vid="$2"
	if list_contains used "$vid"; then
        return 0;
	else 
        return 1;
	fi
}

#eth0 lan vid
select_port5_lvid()
{
    local used_vid="$1"
    local slt

    if is_used_vid "$used_vid" "$eth0_dft_vid"; then
        select_vid "$used_vid" slt
    else
        slt="$eth0_dft_vid"
    fi

    eval "$2='$slt'"
}

select_port4_lvid()
{
    local used_vid="$1"
    local slt

    if is_used_vid "$used_vid" "$eth1_dft_vid"; then
        select_vid "$used_vid" slt
    else
        slt="$eth1_dft_vid"
    fi

    eval "$2='$slt'"
}

#eth1 lan vid

set_port5_dft_vlan()
{
    local vlan_id=$1
    echo $vlan_id > /proc/sys/net/edma/default_wan_tag
}

set_port4_dft_vlan()
{
    local vlan_id=$1
    echo $vlan_id > /proc/sys/net/edma/default_lan_tag
}


get_used_vlan()
{
    local used_vid
    local internet_vid
    local iptv_vid
    local gst_vid

    #internet vid
    config_load network
    config_get internet_vid "vlan" "id" "0"
    config_clear
    if [ "$internet_vid" != "0" ]; then
        append used_vid "$internet_vid"
    fi

    #iptv vid
    config_load iptv_v2
    config_get iptv_vid "info" "iptv_vid" "0"
    config_clear
    if [ "$iptv_vid" != "0" ]; then
        append used_vid "$iptv_vid"
    fi

    #guest eth vid
    config_load wifi 
    config_get gst_vid "guest" "vlan_id" "0"
    config_clear
    if [ "$gst_vid" != "0" ]; then
        append used_vid "$gst_vid"
    fi

    echo "$used_vid"
}

__set_port4_lvid()
{
    local used_vid="$1"
    local tag="$2"
    local vid_p4

    if [ "$tag" == "t" ]; then
        uci -c /etc/vlan.d set vlan.vlan0.ports="0t 1 2 3 4t"
    elif [ "$tag" == "u" ]; then
        uci -c /etc/vlan.d set vlan.vlan0.ports="0t 1 2 3 4"
    fi

    select_port4_lvid "$used_vid" vid_p4
    uci -c /etc/vlan.d set vlan.vlan0.vid="$vid_p4"
    set_port4_dft_vlan "$vid_p4"
    echo "$vid_p4"
}

__set_port5_lvid()
{
    local used_vid="$1"
    local tag="$2"
    local vid_p5

    if [ "$tag" == "t" ]; then
        uci -c /etc/vlan.d set vlan.vlan1.ports="0t 5t"
    elif [ "$tag" == "u" ]; then
        uci -c /etc/vlan.d set vlan.vlan1.ports="0t 5"
    fi
    select_port5_lvid "$used_vid" vid_p5
    uci -c /etc/vlan.d set vlan.vlan1.vid="$vid_p5"
    set_port5_dft_vlan "$vid_p5"
    echo "$vid_p5"
}


__set_port4_wvid()
{
    local vid="$1"
    local tag="$2"
    uci -c /etc/vlan.d set vlan.vlan0.vid=$vid
    if [ "$tag" == "t" ]; then
        uci -c /etc/vlan.d set vlan.vlan0.ports="0t 1 2 3 4t"
    elif [ "$tag" == "u" ]; then
        uci -c /etc/vlan.d set vlan.vlan0.ports="0t 1 2 3 4"
    fi
    set_port4_dft_vlan "4095"
}

__set_port5_wvid()
{
    local vid="$1"
    local tag="$2"
    uci -c /etc/vlan.d set vlan.vlan1.vid=$vid
    if [ "$tag" == "t" ]; then
        uci -c /etc/vlan.d set vlan.vlan1.ports="0t 5t"
    elif [ "$tag" == "u" ]; then
        uci -c /etc/vlan.d set vlan.vlan1.ports="0t 5"
    fi
    set_port5_dft_vlan "4095"
}

__set_both_wvid_tag()
{
    local vlan_id="$1"
    local tag="$2"

    uci -c /etc/vlan.d set vlan.vlan0.vid="1"
    uci -c /etc/vlan.d set vlan.vlan0.ports="0t 1 2 3 4"
    uci -c /etc/vlan.d set vlan.vlan1.vid="$vlan_id"
    if [ "$tag" == "t" ];then
        uci -c /etc/vlan.d set vlan.vlan1.ports="0t 4t 5t"
    elif [ "$tag" == "u" ]; then
        uci -c /etc/vlan.d set vlan.vlan1.ports="0t 4 5"
    fi
    set_port4_dft_vlan "4095"
    set_port5_dft_vlan "4095"
}


__iptv_set_vlan()
{
    local port="$1"
    local iptv_vid="$2"
    local switch_device=$(uci -c /etc/vlan.d get vlan.@switch[0].name)
    uci -c /etc/vlan.d set vlan.iptv="switch_vlan"
    uci -c /etc/vlan.d set vlan.iptv.ports="0t ${port}t"
    uci -c /etc/vlan.d set vlan.iptv.device="$switch_device"
    uci -c /etc/vlan.d set vlan.iptv.vlan="10"
    uci -c /etc/vlan.d set vlan.iptv.vid=$iptv_vid
}

__iptv_clear_vlan()
{
    local iptv_vlan_config=$(uci -c /etc/vlan.d get vlan.iptv 2>/dev/null)

    if [ -n "$iptv_vlan_config" ]; then
        uci -c /etc/vlan.d delete vlan.iptv
        return 0
    fi

    return 1
}

__guest_set_vlan()
{
    local port="$1"
    local guest_vlan_id="$2"

    local switch_device=$(uci -c /etc/vlan.d get vlan.@switch[0].name)

    uci -c /etc/vlan.d set vlan.guest_eth="switch_vlan"                              
    uci -c /etc/vlan.d set vlan.guest_eth.device="$switch_device"
    uci -c /etc/vlan.d set vlan.guest_eth.vlan="3"
    uci -c /etc/vlan.d set vlan.guest_eth.vid="$guest_vlan_id"
    uci -c /etc/vlan.d set vlan.guest_eth.ports="0t ${port}t"
    
}

__guest_set_both_vlan()
{
    local switch_device=$(uci -c /etc/vlan.d get vlan.@switch[0].name)
    uci -c /etc/vlan.d set vlan.guest_eth="switch_vlan"                              
    uci -c /etc/vlan.d set vlan.guest_eth.device="$switch_device"
    uci -c /etc/vlan.d set vlan.guest_eth.vlan="3"
    uci -c /etc/vlan.d set vlan.guest_eth.vid="$guest_vlan_id"
    uci -c /etc/vlan.d set vlan.guest_eth.ports="0t 4t 5t"
}

__guest_clear_vlan()
{
    local section_name
    section_name=$(uci -c /etc/vlan.d get vlan.guest_eth 2>/dev/null)

    [ -n "$section_name" ] && {
        uci -c /etc/vlan.d delete vlan.guest_eth
        return 0
    }
    return 1

}
