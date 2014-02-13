set forks {#1 #2 #3 #4 #5 #6 #7 #8 #9}

foreach i $forks {
    puts "starting worker $i"

    if {[os.fork] == 0} {
        set ipc [zeromq.open PUSH ipc:///tmp/workers]

        # do some work
        sleep [expr {5+round(rand()*10)}]

        $ipc send "worker $i complete"
        $ipc close
        exit 0
    }
}

set ipc [zeromq.open -bind PULL ipc:///tmp/workers]

set complete [llength $forks]

while { $complete > 0 } {
    set message [$ipc receive]
    incr complete -1
    puts $message
}

$ipc close
puts done
