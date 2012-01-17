#!/bin/sh

LC_ALL=POSIX
export LC_ALL

TESTDIR=tests
OUTPUT=`mktemp`
LOGFILE=tests.log
CMD=../src/main/xmp
TEST_CMD="$CMD -o/dev/null --dump"
FONTDIR="$1"

run_test() {
	test_num=$1
	test_dsc=$2
	test_cmd="$TEST_CMD $3"

	echo >> $LOGFILE
	echo -n "Run test $test_num: ${test_dsc}... " | tee -a $LOGFILE
	echo >> $LOGFILE
	echo "Command: $test_cmd" >> $LOGFILE
	eval "$test_cmd" > "$OUTPUT" 2>> $LOGFILE
	zcmp "$OUTPUT" "res_${test_num}.txt.gz" >> $LOGFILE 2>&1
	if [ $? -eq 0 ]; then
		echo "pass" | tee -a $LOGFILE
	else
		echo "**fail**" | tee -a $LOGFILE
		result=1
		fail=`expr $fail + 1`
	fi
}

result=0
fail=0
$CMD -V > $LOGFILE
echo Date: `date` >> $LOGFILE
echo Host: `uname -a` >> $LOGFILE
echo -e "\nTest results\n============" >> $LOGFILE

run_test 0001 "Ode to Protracker replay" "mod/ode2ptk.zip"
run_test 0002 "Pattern delay + break" "-s 0x19 -t 20 mod/corruption.mod.zip"
run_test 0003 "MOD pattern loops" "mod/running_lamer.mod.zip"
run_test 0004 "Storlek 01: Arpeggio and pitch slide" "mod/storlek/01.it"
run_test 0005 "Storlek 02: Arpeggio with no value" "mod/storlek/02.it"
run_test 0006 "Storlek 03: Compatible Gxx off" "mod/storlek/03.it"
run_test 0007 "Storlek 04: Compatible Gxx on" "mod/storlek/04.it"
run_test 0008 "Storlek 05: Gxx, fine slides, effect memory" "mod/storlek/05.it"
run_test 0009 "Storlek 06: Volume column and fine slides" "mod/storlek/06.it"
run_test 0010 "Storlek 07: Note cut with sample" "mod/storlek/07.it"
run_test 0011 "Storlek 08: Out-of-range note delays" "mod/storlek/08.it"
run_test 0012 "Storlek 09: Sample change with no note" "mod/storlek/09.it"
run_test 0013 "Storlek 10: IT pattern loop" "mod/storlek/10.it"
run_test 0014 "Storlek 11: Infinite loop exploit" "mod/storlek/11.it"

rm -f "$OUTPUT"

echo
if [ $result -ne 0 ]; then
	echo " $fail tests failed. See $LOGFILE for result details"
else
	echo " All tests passed."
fi

exit $result
