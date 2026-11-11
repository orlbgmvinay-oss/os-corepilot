#!/usr/bin/env bash
# Runs every CampusCompute command (valid and invalid) and checks exit status
# plus a key phrase of the output. Usage: bash tests/run_tests.sh
cd "$(dirname "$0")/.." || exit 1
BIN=./campuscompute
[ -x "$BIN" ] || { echo "build first: make"; exit 1; }
pass=0; fail=0

# check <name> <expected exit code> <regex expected in output> <command...>
check() {
    local name=$1 rc_want=$2 pat=$3; shift 3
    local out rc
    out=$("$@" 2>&1); rc=$?
    if [ "$rc" -eq "$rc_want" ] && grep -qE -- "$pat" <<<"$out"; then
        printf '  PASS  %s\n' "$name"; pass=$((pass+1))
    else
        printf '  FAIL  %s (exit %s, wanted %s, pattern: %s)\n' "$name" "$rc" "$rc_want" "$pat"
        fail=$((fail+1))
    fi
}

echo "== CLI =="
check "help"                 0 "Usage: campuscompute"        $BIN help
check "no command"           2 "no command given"            $BIN
check "unknown command"      2 "unknown command 'bogus'"     $BIN bogus

echo "== FR1/FR2 scheduling =="
check "fcfs"                 0 "Average waiting time"        $BIN sched fcfs tests/jobs_basic.txt
check "rr quantum shown"     0 "TIME QUANTUM = 2"            $BIN sched rr tests/jobs_basic.txt --quantum 2
check "priority tie-break"   0 "earlier arrival, then lower" $BIN sched priority tests/jobs_basic.txt
check "priority aging"       0 "Aging: ON"                   $BIN sched priority tests/jobs_basic.txt --aging 3
check "idle gap"             0 "idle"                        $BIN sched fcfs tests/jobs_idle.txt
check "compare"              0 "Best average waiting time"   $BIN sched compare tests/jobs_basic.txt
check "bad policy"           2 "unknown scheduling policy"   $BIN sched lifo tests/jobs_basic.txt
check "bad quantum"          2 "--quantum expects"           $BIN sched rr tests/jobs_basic.txt --quantum 0
check "missing file"         2 "cannot open"                 $BIN sched fcfs tests/nope.txt
check "malformed job line"   2 "expected 5 fields"           $BIN sched fcfs tests/jobs_bad_format.txt
check "invalid burst"        2 "burst '0' must be"           $BIN sched fcfs tests/jobs_bad_value.txt

echo "== FR3 logger =="
check "logger default"       0 "Integrity check +: PASS"     $BIN logger
check "logger tiny queue"    0 "Integrity check +: PASS"     $BIN logger --producers 4 --consumers 1 --capacity 1 --items 6
check "logger bad option"    2 "unknown option"              $BIN logger --bogus 1

echo "== FR4 IPC =="
check "ipc exchange"         0 "IPC result: SUCCESS"         $BIN ipc
check "ipc bad policy"       2 "'--policy' needs"            $BIN ipc --policy xyz

echo "== FR5 Banker =="
check "banker safe"          0 "SAFE STATE.  Safe sequence"  $BIN banker tests/banker_safe.txt
check "banker unsafe"        0 "UNSAFE STATE"                $BIN banker tests/banker_unsafe.txt
check "banker requests"      0 "GRANTED"                     $BIN banker tests/banker_request.txt
check "banker missing file"  2 "cannot open"                 $BIN banker tests/nope.txt

echo "== FR6 workers =="
check "worker normal"        0 "exited normally, code 0"     $BIN worker normal
check "worker fail"          0 "SIMULATED"                   $BIN worker fail
check "worker mix"           0 "Detection check: 5 of 5"     $BIN worker mix
check "worker bad mode"      2 "unknown worker mode"         $BIN worker explode

echo "== FR7 rlimit & cleanup =="
check "rlimit nofile"        0 "EMFILE|Too many open files"  $BIN rlimit nofile
check "rlimit mem"           0 "Cannot allocate memory"      $BIN rlimit mem
check "rlimit cpu"           0 "CPU time limit exceeded"     $BIN rlimit cpu
check "rlimit bad mode"      2 "usage: campuscompute rlimit" $BIN rlimit disk
check "cleanup"              0 "remaining: 0"                $BIN cleanup

echo "== integrated demo =="
check "demo"                 0 "ALL CHECKS PASSED"           $BIN demo

echo "== no leftovers =="
left=$(ls /dev/shm 2>/dev/null | grep -c campuscompute)
if [ "$left" -eq 0 ]; then echo "  PASS  no campuscompute objects in /dev/shm"; pass=$((pass+1))
else echo "  FAIL  $left campuscompute object(s) left in /dev/shm"; fail=$((fail+1)); fi

echo; echo "Result: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
