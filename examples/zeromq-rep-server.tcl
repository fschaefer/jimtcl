
set socket [zeromq.open -bind REP tcp://*:5000]

set i 0

while {1} {

    set helo [$socket receive]

    puts "received $helo"

    incr i

    $socket send "HELO$i"
    
    puts "sending HELO$i"

    if { [$socket interrupted] } {
        break;
    }

    sleep 1;
}
