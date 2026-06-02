namespace eval zlink_dev {
    variable SOF5 0x15
    variable TYPE_POLL 0
    variable TYPE_EMPTY 1
    variable TYPE_DATA 2
    variable TYPE_ACK 3
    variable KERNEL_TTY 15
    variable CMD_GET_STATS 0x01
    variable CMD_GET_TASK_INFO 0x02
    variable CMD_GET_TASK_LIST 0x03
    variable CMD_GET_STACK_WM 0x07
    variable RSP_STATS 0x81
    variable RSP_TASK_INFO 0x82
    variable RSP_TASK_LIST 0x83
    variable RSP_STACK_WM 0x87

    variable port 0x3A
    variable tx_stream {}
    variable rx_fifo {}
    variable rx_head 0
    variable pending_data {}
    variable next_data_seq 0

    variable stat_rx_frames 0
    variable stat_rx_data_frames 0
    variable stat_rx_crc_err 0
    variable stat_rx_sof_err 0
    variable stat_tx_frames 0
    variable stat_tx_data_frames 0
    variable stat_in_calls 0
    variable stat_in_data 0
    variable stat_in_ff 0
    variable last_msx_data_tty -1
    variable last_msx_data_hex ""
    variable last_msx_data_ascii ""
    variable kernel_stats_json_pending 0
    variable kernel_task_info_json_pending 0
    variable kernel_task_list_json_pending 0
    variable kernel_task_list_items {}
    variable verbose_poll 0
    variable verbose_data 1
    variable shell_decode_tty -1
    variable shell_decode_enabled 0
    variable shell_decode_line ""
    variable shell_decode_last_cr 0
    variable shell_decode_prompt "msx> "
    variable pd_prefix ""
}

proc zlink_dev::u8 {v} {
    return [expr {$v & 0xFF}]
}

proc zlink_dev::type_name {t} {
    switch -- $t {
        0 { return "POLL" }
        1 { return "EMPTY" }
        2 { return "DATA" }
        3 { return "ACK" }
        4 { return "NACK" }
        default { return [format "TYPE%u" $t] }
    }
}

proc zlink_dev::fmt_bytes {bytes} {
    set out {}
    foreach b $bytes {
        lappend out [format "%02X" [u8 $b]]
    }
    return [join $out " "]
}

proc zlink_dev::u16le {lo hi} {
    return [expr {([u8 $lo]) | (([u8 $hi]) << 8)}]
}

proc zlink_dev::decode_text_payload {payload} {
    set out ""
    foreach b $payload {
        set v [u8 $b]
        if {$v == 13} {
            append out "\\r"
        } elseif {$v == 10} {
            append out "\\n"
        } elseif {$v == 9} {
            append out "\\t"
        } elseif {$v >= 32 && $v <= 126} {
            append out [format %c $v]
        } else {
            append out [format "\\x%02X" $v]
        }
    }
    return $out
}

proc zlink_dev::payload_looks_text {payload} {
    set seen_printable 0
    foreach b $payload {
        set v [u8 $b]
        if {$v == 13 || $v == 10 || $v == 9} {
            continue
        }
        if {$v >= 32 && $v <= 126} {
            set seen_printable 1
            continue
        }
        return 0
    }
    return $seen_printable
}

proc zlink_dev::emit_friendly_tty_text {tty payload} {
    if {[payload_looks_text $payload] == 0} {
        return
    }
    puts [format "zlink_dev: tty=%u text=\"%s\"" $tty [decode_text_payload $payload]]
}

proc zlink_dev::parse_bool_u8 {value} {
    set v [string tolower [string trim $value]]
    if {$v in {"1" "true" "on" "yes"}} {
        return 1
    }
    if {$v in {"0" "false" "off" "no"}} {
        return 0
    }
    error "invalid boolean value: $value"
}

proc zlink_dev::set_shell_decode {tty enabled} {
    variable shell_decode_tty
    variable shell_decode_enabled
    variable shell_decode_line
    variable shell_decode_last_cr

    if {$tty < 0 || $tty > 15} {
        error "tty must be 0..15"
    }

    set shell_decode_tty [u8 $tty]
    set shell_decode_enabled [expr {$enabled ? 1 : 0}]
    set shell_decode_line ""
    set shell_decode_last_cr 0
    puts [format "zlink_dev: shell decode tty=%u enabled=%u" $shell_decode_tty $shell_decode_enabled]
}

proc zlink_dev::shell_decode_emit_line {tty line} {
    puts [format "zlink_dev: shell rx tty=%u text=\"%s\"" $tty $line]
}

proc zlink_dev::shell_decode_feed_payload {tty payload} {
    variable shell_decode_line
    variable shell_decode_last_cr
    variable shell_decode_prompt

    foreach _b $payload {
        set v [u8 $_b]

        if {$v == 13} {
            if {$shell_decode_line ne ""} {
                shell_decode_emit_line $tty $shell_decode_line
                set shell_decode_line ""
            }
            set shell_decode_last_cr 1
            continue
        }

        if {$v == 10} {
            if {$shell_decode_last_cr == 0 && $shell_decode_line ne ""} {
                shell_decode_emit_line $tty $shell_decode_line
                set shell_decode_line ""
            }
            set shell_decode_last_cr 0
            continue
        }

        set shell_decode_last_cr 0

        if {$v == 9} {
            append shell_decode_line "\\t"
        } elseif {$v >= 32 && $v <= 126} {
            append shell_decode_line [format %c $v]
        } else {
            append shell_decode_line [format "\\x%02X" $v]
        }

        if {$shell_decode_line eq $shell_decode_prompt} {
            shell_decode_emit_line $tty $shell_decode_line
            set shell_decode_line ""
        }
    }
}

proc zlink_dev::emit_kernel_stats_json {status len payload} {
    if {$status == 0 && $len >= 18} {
        set tx_drop [u16le [lindex $payload 2] [lindex $payload 3]]
        set rx_overflow [u16le [lindex $payload 4] [lindex $payload 5]]
        set attach_fail [u16le [lindex $payload 6] [lindex $payload 7]]
        set zlink_ok [u16le [lindex $payload 8] [lindex $payload 9]]
        set zlink_crc [u16le [lindex $payload 10] [lindex $payload 11]]
        set zlink_dup [u16le [lindex $payload 12] [lindex $payload 13]]
        set zlink_type [u16le [lindex $payload 14] [lindex $payload 15]]
        set zlink_len [u16le [lindex $payload 16] [lindex $payload 17]]

        puts [format "{\"type\":\"kernel_stats\",\"status\":0,\"tx_drop\":%u,\"rx_overflow\":%u,\"attach_fail\":%u,\"zlink_rx_frames_ok\":%u,\"zlink_rx_crc_err\":%u,\"zlink_rx_dup\":%u,\"zlink_rx_type_err\":%u,\"zlink_rx_len_err\":%u}" \
            $tx_drop $rx_overflow $attach_fail $zlink_ok $zlink_crc $zlink_dup $zlink_type $zlink_len]
    } else {
        puts [format "{\"type\":\"kernel_stats\",\"status\":%u,\"len\":%u,\"payload_hex\":\"%s\"}" \
            $status $len [fmt_bytes $payload]]
    }
}

proc zlink_dev::extract_task_name {payload base name_len} {
    set task_name ""
    set limit $name_len
    if {$limit > 8} {
        set limit 8
    }

    for {set i 0} {$i < 8} {incr i} {
        set b [u8 [lindex $payload [expr {$base + $i}]]]
        if {$i < $limit && $b >= 32 && $b <= 126} {
            append task_name [format %c $b]
        } elseif {$i < $limit} {
            append task_name "."
        }
    }

    return $task_name
}

proc zlink_dev::json_escape_text {text} {
    return [string map {\\ \\\\ \" \\\"} $text]
}

proc zlink_dev::emit_kernel_task_info_json {status len payload} {
    if {$status == 0 && $len >= 16} {
        set task_id [u8 [lindex $payload 2]]
        set task_state [u8 [lindex $payload 3]]
        set task_tty [u8 [lindex $payload 4]]
        set task_sp [u16le [lindex $payload 5] [lindex $payload 6]]
        set name_len [u8 [lindex $payload 7]]
        set display_name_len $name_len
        if {$display_name_len > 8} {
            set display_name_len 8
        }
        set name [extract_task_name $payload 8 $name_len]
        set name_json [json_escape_text $name]

        puts [format "{\"type\":\"kernel_task_info\",\"status\":0,\"id\":%u,\"tty\":%u,\"state\":%u,\"sp\":\"0x%04X\",\"name_len\":%u,\"name\":\"%s\"}" \
            $task_id $task_tty $task_state $task_sp $display_name_len $name_json]
    } else {
        puts [format "{\"type\":\"kernel_task_info\",\"status\":%u,\"len\":%u,\"payload_hex\":\"%s\"}" \
            $status $len [fmt_bytes $payload]]
    }
}

proc zlink_dev::emit_kernel_task_list_json {status len payload} {
    variable kernel_task_list_items

    if {$status == 0 && $len >= 3} {
        if {[llength $kernel_task_list_items] != 0} {
            puts [format {{"type":"kernel_task_list","status":%u,"count":%u,"tasks":[%s]}} \
                $status [llength $kernel_task_list_items] [join $kernel_task_list_items ","]]
        } else {
            puts [format {{"type":"kernel_task_list","status":0,"count":0,"tasks":[]}}]
        }
    } else {
        puts [format "{\"type\":\"kernel_task_list\",\"status\":%u,\"len\":%u,\"payload_hex\":\"%s\"}" \
            $status $len [fmt_bytes $payload]]
    }
}

proc zlink_dev::reset_kernel_task_list_accumulator {} {
    variable kernel_task_list_items
    set kernel_task_list_items {}
}

proc zlink_dev::append_kernel_task_list_entry {payload idx} {
    set task_id [u8 [lindex $payload $idx]]
    set task_tty [u8 [lindex $payload [expr {$idx + 1}]]]
    set name_len [u8 [lindex $payload [expr {$idx + 2}]]]
    set display_name_len $name_len
    if {$display_name_len > 8} {
        set display_name_len 8
    }
    set task_name [extract_task_name $payload [expr {$idx + 3}] $name_len]
    set task_name_json [json_escape_text $task_name]
    return [format "{\"id\":%u,\"tty\":%u,\"name_len\":%u,\"name\":\"%s\"}" \
        $task_id $task_tty $display_name_len $task_name_json]
}

proc zlink_dev::crc8 {bytes} {
    set crc 0
    foreach b $bytes {
        set crc [expr {$crc ^ ([u8 $b])}]
        for {set i 0} {$i < 8} {incr i} {
            if {$crc & 0x80} {
                set crc [expr {(($crc << 1) ^ 0x07) & 0xFF}]
            } else {
                set crc [expr {(($crc << 1)) & 0xFF}]
            }
        }
    }
    return [u8 $crc]
}

proc zlink_dev::build_frame {type tty seq payload} {
    variable SOF5

    set tty [u8 $tty]
    set seq [u8 $seq]
    set len [llength $payload]
    if {$len < 0 || $len > 64} {
        error "payload length must be 0..64"
    }

    set b0 [u8 [expr {($SOF5 << 3) | ($type & 0x07)}]]
    set b1 [u8 [expr {$tty & 0x0F}]]
    set b2 [u8 $len]

    set raw [list $b0 $b1 $b2 $seq]
    foreach b $payload {
        lappend raw [u8 $b]
    }
    lappend raw [crc8 $raw]
    return $raw
}

proc zlink_dev::push_rx_bytes {bytes} {
    variable rx_fifo

    foreach b $bytes {
        lappend rx_fifo [u8 $b]
    }
}

proc zlink_dev::pop_rx_byte {} {
    variable rx_fifo
    variable rx_head

    if {$rx_head >= [llength $rx_fifo]} {
        return 0xFF
    }

    set b [lindex $rx_fifo $rx_head]
    incr rx_head

    if {$rx_head >= 256 && $rx_head >= ([llength $rx_fifo] / 2)} {
        set rx_fifo [lrange $rx_fifo $rx_head end]
        set rx_head 0
    }

    return [u8 $b]
}

proc zlink_dev::reset_state {} {
    variable tx_stream
    variable rx_fifo
    variable rx_head
    variable pending_data
    variable next_data_seq
    variable stat_rx_frames
    variable stat_rx_data_frames
    variable stat_rx_crc_err
    variable stat_rx_sof_err
    variable stat_tx_frames
    variable stat_tx_data_frames
    variable stat_in_calls
    variable stat_in_data
    variable stat_in_ff
    variable last_msx_data_tty
    variable last_msx_data_hex
    variable last_msx_data_ascii
    variable kernel_stats_json_pending
    variable verbose_poll
    variable verbose_data
    variable kernel_task_info_json_pending
    variable kernel_task_list_json_pending
    variable kernel_task_list_items
    variable shell_decode_tty
    variable shell_decode_enabled
    variable shell_decode_line
    variable shell_decode_last_cr

    set tx_stream {}
    set rx_fifo {}
    set rx_head 0
    set pending_data {}
    set next_data_seq 0
    set stat_rx_frames 0
    set stat_rx_data_frames 0
    set stat_rx_crc_err 0
    set stat_rx_sof_err 0
    set stat_tx_frames 0
    set stat_tx_data_frames 0
    set stat_in_calls 0
    set stat_in_data 0
    set stat_in_ff 0
    set last_msx_data_tty -1
    set last_msx_data_hex ""
    set last_msx_data_ascii ""
    set kernel_stats_json_pending 0
    set kernel_task_info_json_pending 0
    set kernel_task_list_json_pending 0
    set kernel_task_list_items {}
    set verbose_poll 0
    set verbose_data 1
    set shell_decode_tty 0
    set shell_decode_enabled 1
    set shell_decode_line ""
    set shell_decode_last_cr 0
}

proc zlink_dev::set_verbose {{poll ""} {data ""}} {
    variable verbose_poll
    variable verbose_data

    if {$poll ne ""} {
        set verbose_poll [expr {$poll ? 1 : 0}]
    }
    if {$data ne ""} {
        set verbose_data [expr {$data ? 1 : 0}]
    }

    puts [format "zlink_dev: verbose poll=%u data=%u" $verbose_poll $verbose_data]
}

proc zlink_dev::omx_setting_set {name value} {
    uplevel #0 [list set $name $value]
}

proc zlink_dev::omx_setting_get {name {fallback "<n/a>"}} {
    if {[catch {
        uplevel #0 [list set $name]
    } _v]} {
        return $fallback
    }
    return $_v
}

proc zlink_dev::on_reset {} {
    reset_state
}

proc zlink_dev::queue_data {tty payload {seq auto}} {
    variable pending_data
    variable next_data_seq
    variable shell_decode_tty
    variable shell_decode_enabled

    if {$tty < 0 || $tty > 15} {
        error "tty must be 0..15"
    }

    set payload_len [llength $payload]
    if {$payload_len < 1 || $payload_len > 64} {
        error "payload length must be 1..64"
    }

    set clean_payload {}
    foreach b $payload {
        lappend clean_payload [u8 $b]
    }

    if {$seq eq "auto"} {
        set seq $next_data_seq
        set next_data_seq [u8 [expr {$next_data_seq + 1}]]
    } else {
        set seq [u8 $seq]
    }

    lappend pending_data [list [u8 $tty] [u8 $seq] $clean_payload]
    if {!($shell_decode_enabled != 0 && [u8 $tty] == $shell_decode_tty)} {
        puts [format "zlink_dev: queued DATA tty=%u seq=0x%02X len=%u payload=%s" \
            [u8 $tty] [u8 $seq] $payload_len [fmt_bytes $clean_payload]]
    }
}

proc zlink_dev::queue_hex {tty hex_string {seq auto}} {
    set payload {}
    foreach tok [regexp -all -inline {\S+} $hex_string] {
        if {[scan $tok %x byte] != 1} {
            error "invalid hex token: $tok"
        }
        lappend payload [u8 $byte]
    }
    queue_data $tty $payload $seq
}

proc zlink_dev::queue_text {tty text {seq auto}} {
    set payload {}
    set n [string length $text]
    for {set i 0} {$i < $n} {incr i} {
        scan [string index $text $i] %c c
        lappend payload [u8 $c]
    }
    queue_data $tty $payload $seq
}

proc zlink_dev::shell_cmd {text args} {
    set tty 0
    set seq auto
    set decode_mode ""
    set idx 0
    set argc [llength $args]

    if {$idx < $argc && ![string match "-*" [lindex $args $idx]]} {
        set tty [u8 [lindex $args $idx]]
        incr idx
    }
    if {$idx < $argc && ![string match "-*" [lindex $args $idx]]} {
        set seq [lindex $args $idx]
        incr idx
    }

    while {$idx < $argc} {
        set opt [lindex $args $idx]
        incr idx
        if {$idx >= $argc} {
            error "missing value for option: $opt"
        }
        set val [lindex $args $idx]
        incr idx

        switch -- $opt {
            -decode {
                set decode_mode [parse_bool_u8 $val]
            }
            default {
                error "unknown option: $opt (allowed: -decode)"
            }
        }
    }

    if {$decode_mode ne ""} {
        set_shell_decode $tty $decode_mode
    }

    queue_text $tty "${text}\r" $seq
}

proc zlink_dev::get_stats {{seq auto}} {
    variable KERNEL_TTY
    variable CMD_GET_STATS
    queue_data $KERNEL_TTY [list $CMD_GET_STATS] $seq
}

proc zlink_dev::get_stats_json {{seq auto}} {
    variable kernel_stats_json_pending
    set kernel_stats_json_pending 1
    get_stats $seq
}

proc zlink_dev::get_task_info {{task_id ""} {seq auto}} {
    variable KERNEL_TTY
    variable CMD_GET_TASK_INFO

    if {$task_id eq ""} {
        queue_data $KERNEL_TTY [list $CMD_GET_TASK_INFO] $seq
    } else {
        queue_data $KERNEL_TTY [list $CMD_GET_TASK_INFO [u8 $task_id]] $seq
    }
}

proc zlink_dev::get_task_info_json {{task_id ""} {seq auto}} {
    variable kernel_task_info_json_pending
    set kernel_task_info_json_pending 1
    get_task_info $task_id $seq
}

proc zlink_dev::get_task_list {{seq auto}} {
    variable KERNEL_TTY
    variable CMD_GET_TASK_LIST
    reset_kernel_task_list_accumulator
    queue_data $KERNEL_TTY [list $CMD_GET_TASK_LIST] $seq
}

proc zlink_dev::get_task_list_json {{seq auto}} {
    variable kernel_task_list_json_pending
    set kernel_task_list_json_pending 1
    get_task_list $seq
}

proc zlink_dev::get_stack_wm {{task_id ""} {seq auto}} {
    variable KERNEL_TTY
    variable CMD_GET_STACK_WM

    if {$task_id eq ""} {
        queue_data $KERNEL_TTY [list $CMD_GET_STACK_WM] $seq
    } else {
        queue_data $KERNEL_TTY [list $CMD_GET_STACK_WM [u8 $task_id]] $seq
    }
}

proc zlink_dev::status {} {
    variable port
    variable pending_data
    variable rx_fifo
    variable rx_head
    variable stat_rx_frames
    variable stat_rx_data_frames
    variable stat_rx_crc_err
    variable stat_rx_sof_err
    variable stat_tx_frames
    variable stat_tx_data_frames
    variable stat_in_calls
    variable stat_in_data
    variable stat_in_ff
    variable last_msx_data_tty
    variable last_msx_data_hex
    variable last_msx_data_ascii
    variable verbose_poll
    variable verbose_data
    variable shell_decode_tty
    variable shell_decode_enabled
    variable pd_prefix

    set rx_pending [expr {[llength $rx_fifo] - $rx_head}]
    puts [format "zlink_dev: port=0x%02X pending_data=%u rx_pending_bytes=%u" \
        [u8 $port] [llength $pending_data] $rx_pending]
    puts [format "zlink_dev: stats rx_control_frames=%u rx_data_frames=%u rx_crc_err=%u rx_sof_err=%u tx_control_frames=%u tx_data_frames=%u" \
        $stat_rx_frames $stat_rx_data_frames $stat_rx_crc_err $stat_rx_sof_err $stat_tx_frames $stat_tx_data_frames]
    puts [format "zlink_dev: in_cb calls=%u data=%u ff=%u" \
        $stat_in_calls $stat_in_data $stat_in_ff]
    puts [format "zlink_dev: verbose poll=%u data=%u" $verbose_poll $verbose_data]
    puts [format "zlink_dev: shell_decode tty=%d enabled=%u" $shell_decode_tty $shell_decode_enabled]
    if {$pd_prefix ne ""} {
        set _pd_ports [omx_setting_get [format "%s ports" $pd_prefix]]
        set _pd_in [omx_setting_get [format "%s input callback" $pd_prefix]]
        set _pd_out [omx_setting_get [format "%s output callback" $pd_prefix]]
        puts [format "zlink_dev: pd_prefix=%s ports=%s in_cb=%s out_cb=%s" \
            $pd_prefix $_pd_ports $_pd_in $_pd_out]
    } else {
        puts "zlink_dev: pd_prefix=<unset>"
    }
    puts [format "zlink_dev: last_msx_data tty=%d hex=%s ascii=%s" \
        $last_msx_data_tty $last_msx_data_hex $last_msx_data_ascii]
}

proc zlink_dev::send_empty {seq tty} {
    variable TYPE_EMPTY
    variable stat_tx_frames
    variable verbose_poll
    variable shell_decode_tty
    variable shell_decode_enabled

    set frame [build_frame $TYPE_EMPTY $tty $seq {}]
    push_rx_bytes $frame
    incr stat_tx_frames
    if {$verbose_poll != 0 && !($shell_decode_enabled != 0 && [u8 $tty] == $shell_decode_tty)} {
        puts [format "HOST->MSX EMPTY tty=%u seq=0x%02X frame=%s" \
            [u8 $tty] [u8 $seq] [fmt_bytes $frame]]
    }
}

proc zlink_dev::send_data_frame {tty seq payload} {
    variable TYPE_DATA
    variable stat_tx_data_frames
    variable verbose_data
    variable shell_decode_tty
    variable shell_decode_enabled

    set frame [build_frame $TYPE_DATA $tty $seq $payload]
    set b0 [lindex $frame 0]
    set b1 [lindex $frame 1]
    set b2 [lindex $frame 2]
    set crc8 [lindex $frame end]
    push_rx_bytes $frame
    incr stat_tx_data_frames
    if {$verbose_data != 0 && !($shell_decode_enabled != 0 && [u8 $tty] == $shell_decode_tty)} {
        puts [format "HOST->MSX DATA SOF5=0x%02X TYPE=%u TTY=%u LEN=%u SEQ=0x%02X CRC8=0x%02X payload=%s" \
            [expr {([u8 $b0] >> 3) & 0x1F}] [expr {[u8 $b0] & 0x07}] \
            [u8 $b1] [u8 $b2] [u8 $seq] [u8 $crc8] \
            [fmt_bytes $payload]]
    }
}

proc zlink_dev::send_ack {seq tty} {
    variable TYPE_ACK
    variable stat_tx_frames
    variable verbose_data
    variable shell_decode_tty
    variable shell_decode_enabled

    set frame [build_frame $TYPE_ACK $tty $seq {}]
    push_rx_bytes $frame
    incr stat_tx_frames
    if {$verbose_data != 0 && !($shell_decode_enabled != 0 && [u8 $tty] == $shell_decode_tty)} {
        puts [format "HOST->MSX ACK tty=%u seq=0x%02X frame=%s" \
            [u8 $tty] [u8 $seq] [fmt_bytes $frame]]
    }
}

proc zlink_dev::handle_msx_frame {frame} {
    variable TYPE_POLL
    variable TYPE_DATA
    variable pending_data
    variable KERNEL_TTY
    variable RSP_STATS
    variable RSP_TASK_INFO
    variable RSP_TASK_LIST
    variable RSP_STACK_WM
    variable stat_rx_frames
    variable stat_rx_data_frames
    variable last_msx_data_tty
    variable last_msx_data_hex
    variable last_msx_data_ascii
    variable kernel_stats_json_pending
    variable kernel_task_info_json_pending
    variable kernel_task_list_json_pending
    variable kernel_task_list_items
    variable verbose_poll
    variable verbose_data
    variable shell_decode_tty
    variable shell_decode_enabled

    set b0 [lindex $frame 0]
    set b1 [lindex $frame 1]
    set type [expr {$b0 & 0x07}]
    set tty [expr {$b1 & 0x0F}]
    set b2 [lindex $frame 2]
    set len [u8 $b2]
    set seq [lindex $frame 3]
    set payload [lrange $frame 4 [expr {3 + $len}]]

    if {$type != $TYPE_DATA} {
        incr stat_rx_frames
    }

    set shell_tty_quiet [expr {$shell_decode_enabled != 0 && $tty == $shell_decode_tty}]

    if {!$shell_tty_quiet && ((($type == $TYPE_POLL) && ($verbose_poll != 0))
        || (($type != $TYPE_POLL) && ($verbose_data != 0)))} {
        if {$type == $TYPE_DATA} {
            set crc8 [lindex $frame end]
            puts [format "MSX->HOST DATA SOF5=0x%02X TYPE=%u TTY=%u LEN=%u SEQ=0x%02X CRC8=0x%02X payload=%s" \
                [expr {([u8 $b0] >> 3) & 0x1F}] $type $tty $len [u8 $seq] [u8 $crc8] [fmt_bytes $payload]]
        } else {
            puts [format "MSX->HOST %s tty=%u seq=0x%02X len=%u payload=%s frame=%s" \
                [type_name $type] $tty [u8 $seq] $len [fmt_bytes $payload] [fmt_bytes $frame]]
        }
    }

    if {$type == $TYPE_POLL} {
        if {[llength $pending_data] > 0} {
            set item [lindex $pending_data 0]
            set pending_data [lrange $pending_data 1 end]
            set data_tty [lindex $item 0]
            set data_seq [lindex $item 1]
            set data_payload [lindex $item 2]
            send_data_frame $data_tty $data_seq $data_payload
        } else {
            send_empty $seq $tty
        }
    } elseif {$type == $TYPE_DATA} {
        set last_msx_data_tty $tty
        set last_msx_data_hex [fmt_bytes $payload]
        set last_msx_data_ascii ""
        foreach _b $payload {
            if {$_b >= 32 && $_b <= 126} {
                append last_msx_data_ascii [format %c $_b]
            } else {
                append last_msx_data_ascii "."
            }
        }
        incr stat_rx_data_frames

        if {$shell_decode_enabled != 0 && $tty == $shell_decode_tty} {
            shell_decode_feed_payload $tty $payload
        }

        if {$tty != $KERNEL_TTY && !($shell_decode_enabled != 0 && $tty == $shell_decode_tty)} {
            emit_friendly_tty_text $tty $payload
        }

        if {$tty == $KERNEL_TTY && $len >= 2 && [lindex $payload 0] == $RSP_STATS} {
            set status [u8 [lindex $payload 1]]
            if {$status == 0 && $len >= 18} {
                set tx_drop [u16le [lindex $payload 2] [lindex $payload 3]]
                set rx_overflow [u16le [lindex $payload 4] [lindex $payload 5]]
                set attach_fail [u16le [lindex $payload 6] [lindex $payload 7]]
                set zlink_ok [u16le [lindex $payload 8] [lindex $payload 9]]
                set zlink_crc [u16le [lindex $payload 10] [lindex $payload 11]]
                set zlink_dup [u16le [lindex $payload 12] [lindex $payload 13]]
                set zlink_type [u16le [lindex $payload 14] [lindex $payload 15]]
                set zlink_len [u16le [lindex $payload 16] [lindex $payload 17]]

                puts [format "zlink_dev: kernel stats tx_drop=%u rx_overflow=%u attach_fail=%u zlink_ok=%u zlink_crc_err=%u zlink_dup=%u zlink_type_err=%u zlink_len_err=%u" \
                    $tx_drop $rx_overflow $attach_fail $zlink_ok $zlink_crc $zlink_dup $zlink_type $zlink_len]
            } else {
                puts [format "zlink_dev: kernel stats error status=%u len=%u payload=%s" \
                    $status $len [fmt_bytes $payload]]
            }

            if {$kernel_stats_json_pending != 0} {
                emit_kernel_stats_json $status $len $payload
                set kernel_stats_json_pending 0
            }
        } elseif {$tty == $KERNEL_TTY && $len >= 2 && [lindex $payload 0] == $RSP_TASK_INFO} {
            set status [u8 [lindex $payload 1]]
            if {$status == 0 && $len >= 16} {
                set task_id [u8 [lindex $payload 2]]
                set task_state [u8 [lindex $payload 3]]
                set task_tty [u8 [lindex $payload 4]]
                set task_sp [u16le [lindex $payload 5] [lindex $payload 6]]
                set name_len [u8 [lindex $payload 7]]
                set display_name_len $name_len
                if {$display_name_len > 8} {
                    set display_name_len 8
                }
                set task_name [extract_task_name $payload 8 $name_len]
                puts [format "zlink_dev: kernel task_info task_id=%u tty=%u state=%u sp=0x%04X name_len=%u name=%s" \
                    $task_id $task_tty $task_state $task_sp $display_name_len $task_name]
            } else {
                puts [format "zlink_dev: kernel task_info error status=%u len=%u payload=%s" \
                    $status $len [fmt_bytes $payload]]
            }

            if {$kernel_task_info_json_pending != 0} {
                emit_kernel_task_info_json $status $len $payload
                set kernel_task_info_json_pending 0
            }
        } elseif {$tty == $KERNEL_TTY && $len >= 3 && [lindex $payload 0] == $RSP_TASK_LIST} {
            set status_raw [u8 [lindex $payload 1]]
            set status [expr {$status_raw & 0x7F}]
            set more [expr {$status_raw & 0x80}]
            if {$status == 0} {
                set count [u8 [lindex $payload 2]]
                set idx 3
                set parsed 0
                set frame_items {}
                while {$parsed < $count && ($idx + 10) < $len} {
                    lappend frame_items [append_kernel_task_list_entry $payload $idx]
                    set idx [expr {$idx + 11}]
                    incr parsed
                }

                if {$parsed != $count || $idx != $len} {
                    puts [format "zlink_dev: kernel task_list error status=%u len=%u payload=%s" \
                        $status_raw $len [fmt_bytes $payload]]
                    reset_kernel_task_list_accumulator
                    if {$kernel_task_list_json_pending != 0} {
                        emit_kernel_task_list_json $status_raw $len $payload
                        set kernel_task_list_json_pending 0
                    }
                } else {
                    if {[llength $frame_items] > 0} {
                        set kernel_task_list_items [concat $kernel_task_list_items $frame_items]
                    }

                    if {$more != 0} {
                        puts [format "zlink_dev: kernel task_list chunk count=%u more=1" [llength $frame_items]]
                    } else {
                        puts [format "zlink_dev: kernel task_list count=%u items=%s" \
                            [llength $kernel_task_list_items] [join $kernel_task_list_items { | }]]
                        if {$kernel_task_list_json_pending != 0} {
                            emit_kernel_task_list_json $status_raw $len $payload
                            set kernel_task_list_json_pending 0
                        }
                        reset_kernel_task_list_accumulator
                    }
                }
            } else {
                puts [format "zlink_dev: kernel task_list error status=%u len=%u payload=%s" \
                    $status_raw $len [fmt_bytes $payload]]
                reset_kernel_task_list_accumulator
                if {$kernel_task_list_json_pending != 0} {
                    emit_kernel_task_list_json $status_raw $len $payload
                    set kernel_task_list_json_pending 0
                }
            }
        } elseif {$tty == $KERNEL_TTY && $len >= 4 && [lindex $payload 0] == $RSP_STACK_WM} {
            set status [u8 [lindex $payload 1]]
            set task_id [u8 [lindex $payload 2]]
            set task_state [u8 [lindex $payload 3]]
            if {$status == 0 && $len >= 10} {
                set stack_size [u16le [lindex $payload 4] [lindex $payload 5]]
                set peak_used [u16le [lindex $payload 6] [lindex $payload 7]]
                set current_used [u16le [lindex $payload 8] [lindex $payload 9]]
                set free_peak [expr {$stack_size - $peak_used}]
                set free_now [expr {$stack_size - $current_used}]
                puts [format "zlink_dev: stack_wm task_id=%u state=%u size=%u peak_used=%u free_peak=%u current_used=%u free_now=%u" \
                    $task_id $task_state $stack_size $peak_used $free_peak $current_used $free_now]
            } else {
                puts [format "zlink_dev: stack_wm error status=%u task=%u len=%u payload=%s" \
                    $status $task_id $len [fmt_bytes $payload]]
            }
        }

        send_ack $seq $tty
    }
}

proc zlink_dev::parse_tx_stream {} {
    variable tx_stream
    variable SOF5
    variable stat_rx_crc_err
    variable stat_rx_sof_err

    while {1} {
        set avail [llength $tx_stream]
        if {$avail < 1} {
            break
        }

        set b0 [lindex $tx_stream 0]
        if {[expr {([u8 $b0] >> 3)}] != $SOF5} {
            incr stat_rx_sof_err
            set tx_stream [lrange $tx_stream 1 end]
            continue
        }

        if {$avail < 3} {
            break
        }

        set b2 [lindex $tx_stream 2]
        set len [u8 $b2]
        set needed [expr {5 + $len}]
        if {$avail < $needed} {
            break
        }

        set frame [lrange $tx_stream 0 [expr {$needed - 1}]]
        set tx_stream [lrange $tx_stream $needed end]

        set payload_plus [lrange $frame 0 end-1]
        set crc_expect [crc8 $payload_plus]
        set crc_rx [lindex $frame end]
        if {$crc_expect != [u8 $crc_rx]} {
            incr stat_rx_crc_err
            puts [format "MSX->HOST CRC_ERR expect=0x%02X got=0x%02X frame=%s" \
                [u8 $crc_expect] [u8 $crc_rx] [fmt_bytes $frame]]
            continue
        }

        handle_msx_frame $frame
    }
}

proc zlink_dev::on_output {_io_port value} {
    variable tx_stream

    lappend tx_stream [u8 $value]
    parse_tx_stream
}

proc zlink_dev::on_input {_io_port} {
    variable stat_in_calls
    variable stat_in_data
    variable stat_in_ff
    variable rx_fifo
    variable rx_head

    incr stat_in_calls
    if {$rx_head >= [llength $rx_fifo]} {
        incr stat_in_ff
        return 0xFF
    }

    incr stat_in_data
    return [pop_rx_byte]
}

proc zlink_dev::port_in_list {ports io_port} {
    set wanted [u8 $io_port]
    foreach p $ports {
        if {[catch {set pv [expr {$p + 0}]}]} {
            continue
        }
        if {[u8 $pv] == $wanted} {
            return 1
        }
    }
    return 0
}

proc zlink_dev::install {{io_port 0x3A}} {
    variable port
    variable pd_prefix
    set configured 0

    set port [u8 $io_port]

    if {[catch {list_extensions} _ext_list]} {
        set _ext_list {}
    }
    if {[lsearch -glob $_ext_list "programmabledevice*"] < 0} {
        if {[catch {ext programmabledevice} _ext_err]} {
            puts stderr "zlink_dev: ERROR cannot load extension 'programmabledevice': $_ext_err"
            return
        }
    }

    if {[catch {lsort [info globals "*Programmable Device* ports"]} _pd_ports_vars]} {
        set _pd_ports_vars {}
    }
    set cb_out "::zlink_dev::on_output"
    set cb_in "::zlink_dev::on_input"
    set cb_reset "::zlink_dev::on_reset"
    set active_prefixes {}
    set preferred_prefix ""

    foreach _ports_var $_pd_ports_vars {
        set _prefix [string range $_ports_var 0 [expr {[string length $_ports_var] - [string length " ports"] - 1}]]
        if {$preferred_prefix eq ""} {
            set preferred_prefix $_prefix
        }
        if {[string match "*::Programmable Device" $_prefix] || $_prefix eq "Programmable Device"} {
            set preferred_prefix $_prefix
        }
        set _ports [omx_setting_get [format "%s ports" $_prefix] {}]
        if {[port_in_list $_ports $port]} {
            lappend active_prefixes $_prefix
        }
    }

    if {[llength $active_prefixes] == 0} {
        if {$preferred_prefix eq ""} {
            set preferred_prefix "Programmable Device"
        }
        set active_prefixes [list $preferred_prefix]
        catch {omx_setting_set [format "%s ports" $preferred_prefix] [list $port]}
    }

    foreach _prefix $active_prefixes {
        if {![catch {
            omx_setting_set [format "%s output callback" $_prefix] $cb_out
            omx_setting_set [format "%s input callback" $_prefix] $cb_in
            omx_setting_set [format "%s reset callback" $_prefix] $cb_reset
        }]} {
            set configured 1
        }
    }

    if {$configured == 0} {
        puts stderr "zlink_dev: ERROR programmabledevice settings not found or not writable."
        return
    }

    reset_state
    set pd_prefix [lindex $active_prefixes 0]

    puts [format "zlink_dev: installed on port 0x%02X" $port]
    puts "zlink_dev: use queue_text <tty> <text>, queue_hex <tty> <hex bytes>, shell_cmd <text> ?tty? ?seq? ?-decode 0|1?, status"
    puts "zlink_dev: kernel helpers get_stats/get_task_info/get_task_list/get_stack_wm"
}

# Backward-compatible command aliases.
foreach _compat_ns {zlink} {
    namespace eval ${_compat_ns} {}
    foreach _cmd {install set_verbose status reset_state queue_data queue_hex queue_text shell_cmd set_shell_decode decode_text_payload get_stats get_stats_json get_task_info get_task_info_json get_task_list get_task_list_json get_stack_wm} {
        proc ${_compat_ns}::${_cmd} {args} "return \[uplevel 1 \[linsert \$args 0 zlink_dev::${_cmd}\]\]"
    }
}
