
# open replying socket
set socket [zeromq.new REP]
$socket bind tcp://*:5000

set i 0

while {1} {

    # receive some data
    set helo [$socket receive]

    puts "received $helo"

    incr i

    # and answer with new HELO#
    $socket send "HELO$i"

    puts "sending HELO$i"

    #exit if interrupted
    if { [$socket interrupted] } {
        break;
    }

    sleep 1;
}
