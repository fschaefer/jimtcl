
#open publisher socket
set socket [zeromq.open -bind PUB tcp://*:5000]

# encryption key
set key testkey

# alternatively generate OTP to encrypt message
#set key [totp testkey]

while {1} {

    # data to send encrypted
    set data testdata

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
