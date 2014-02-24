# Implements script-based implementation of zeromq.socket.open
#
# (c) 2014 Florian Schäfer <florian.schaefer@gmail.com>
#

proc {zeromq.socket.open} {args} {

    set argc [llength $args]
    if {$argc < 2 || $argc > 4} {
        return -code error {wrong # args: should be "?-bind? type=PUSH|PULL|REP|REQ|PUB|SUB|PAIR|STREAM ?subscribe? endpoint"}
    }

    set bind 0
    foreach name $args {
        if {$name eq "-bind"} {
            set bind 1
        }
    }

    set type [lindex $args [expr 0 + $bind]]

    set subscribe ""
    if {$argc == (3 + $bind) } {
        set subscribe [lindex $args [expr $argc - 2]]
    }

    set endpoint [lindex $args [expr $argc - 1]]

    set socket [zeromq.socket.new $type]
    if {$bind} {
        $socket bind $endpoint
    } else {
        $socket connect $endpoint
    }

    if {$type eq "SUB"} {
        $socket sockopt SUBSCRIBE $subscribe
    }

    return $socket
}
