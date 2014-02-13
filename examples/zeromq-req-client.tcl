
#open requesting socket
set socket [zeromq.new REQ]
$socket connect tcp://localhost:5000

set message HELO

while {1} {

    # send HELO
    $socket send $message

    puts "sending message $message"

    # and receive a new HELO
    set message "[$socket receive]"

    #exit if interrupted
    if { [$socket interrupted] } {
        break;
    }

    sleep 1;
}
