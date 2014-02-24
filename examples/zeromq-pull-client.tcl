
# open pulling socket
set socket [zeromq.socket.new PULL]
$socket connect tcp://localhost:5000

# encryption key
set key testkey

# alternatively generate OTP to encrypt message
#set key [totp testkey]

while {1} {

    # receive encrypted data
    set data [$socket receive]

    # try to decrypt data
    set decrypted [aes decrypt $key $data]

    # show what we've got
    puts "received $decrypted"

    # exit if interrupted
    if { [$socket interrupted] } {
        break;
    }

    sleep 1;
}
