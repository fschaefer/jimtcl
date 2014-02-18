# Implements script-based implementation of [lintersect],
# [lunion] and [ldifference]
#
# (c) 2014 Florian Schäfer <florian.schaefer@gmail.com>
#
# based on: http://wiki.tcl.tk/13098
#

proc lintersect {list1 list2} {
    set result {}
    foreach e $list1 {
        if {$e in $list2} {lappend result $e} 
    }
    return $result
}

proc lunion {list1 list2} {
    set result $list1
    foreach e $list2 {
        if {$e ni $list1} {lappend result $e} 
    }
    return $result
}

proc ldifference {list1 list2} {
    set result {}
    foreach e $list1 {
        if {$e ni $list2} {lappend result $e} 
    }
    foreach e $list2 {
        if {$e ni $list1} {lappend result $e} 
    }
    return $result
}
