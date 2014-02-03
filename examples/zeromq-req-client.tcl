
#open publisher socket
set socket [zeromq.open REQ tcp://localhost:5000]

set message HELO 

while {1} {

    # send HELO
    $socket send $message
    
    puts "sending message $message"

    # receive HELO
    set message "[$socket receive]"

    #exit if interrupted
    if { [$socket interrupted] } {
        break;
    }

    sleep 1;
}
