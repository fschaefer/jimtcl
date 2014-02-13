
#open pushing socket
set socket [zeromq.new PUSH]
$socket bind tcp://*:5000

# encryption key
set key testkey

# alternatively generate OTP to encrypt message
#set key [totp testkey]

#counter
set i 0

while {1} {

    incr i

    # data to send encrypted
    set data "workload #$i"

    # show what we have
    puts "sending $data"

    # encrypt data
    set encrypted [aes encrypt $key $data]

    # publish it
    $socket send $encrypted\n

    #exit if interrupted
    if { [$socket interrupted] } {
        break;
    }

    sleep 1;
}
