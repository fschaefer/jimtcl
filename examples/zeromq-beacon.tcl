set beacon1 [zeromq.beacon.new 4000]
set socket1 [$beacon1 socket]

set beacon2 [zeromq.beacon.new 4000]
set socket2 [$beacon2 socket]

$beacon1 interval 2000
$beacon1 publish foo:blah
$beacon1 noecho

$beacon2 subscribe foo
while {![$socket2 interrupted]} {
    puts [$socket2 receive];
    sleep 0.1
}
