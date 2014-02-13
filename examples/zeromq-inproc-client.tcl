
set pipe1_output [zeromq.new PAIR]
$pipe1_output bind {inproc://#1}
set pipe1_input [zeromq.new PAIR]
$pipe1_input connect {inproc://#1}

set pipe2_output [zeromq.new PAIR]
$pipe2_output bind {inproc://#2}
set pipe2_input [zeromq.new PAIR]
$pipe2_input connect {inproc://#2}

sleep 2

$pipe2_output send PING

while {1} {

    set message [$pipe1_input receive -nowait]

    if { $message eq "PONG" } {
        $pipe2_output send PING
        puts "sending to pipe2: PING"
        set message ""
    }

    set message [$pipe2_input receive -nowait]

    if { $message eq "PING" } {
        $pipe1_output send PONG
        puts "sending to pipe1: PONG"
        set message ""
    }

    if { [$pipe1_output interrupted] || [$pipe2_output interrupted] } {
        break;
    }

    sleep 1;
}
