#! /bin/sh
# $Id$

case `echo "testing\c"; echo 1,2,3`,`echo -n testing; echo 1,2,3` in
  *c*,-n*) ECHO_N= ECHO_C='
' ECHO_T='	' ;;
  *c*,*  ) ECHO_N=-n ECHO_C= ECHO_T= ;;
  *)       ECHO_N= ECHO_C='\c' ECHO_T= ;;
esac

mkdir results >/dev/null 2>&1

#
# Verify that all test cases match
#

passedct=0
failedct=0

echo $ECHO_N "Test $1: "
for asm in ${srcdir}/$2/*.asm
do
    a=`echo ${asm} | sed 's,^.*/,,;s,.asm$,,'`
    o=${a}$5
    oh=${a}.hx
    og=`echo ${asm} | sed 's,.asm$,.hex,'`
    e=${a}.ew
    eg=`echo ${asm} | sed 's,.asm$,.errwarn,'`

    # Run within a subshell to prevent signal messages from displaying.
    sh -c "cat ${asm} | ./yasm $4 -o results/${o} 2>results/${e}" >/dev/null 2>/dev/null
    status=$?
    if test $status -gt 128; then
	# We should never get a coredump!
	echo $ECHO_N "C"
        eval "failed$failedct='C: ${a} crashed!'"
	failedct=`expr $failedct + 1`
    elif test $status -gt 0; then
	echo ${asm} | grep err >/dev/null
       	if test $? -gt 0; then
	    # YASM detected errors but shouldn't have!
	    echo $ECHO_N "E"
            eval "failed$failedct='E: ${a} returned an error code!'"
	    failedct=`expr $failedct + 1`
	else
	    # We got errors, check to see if they match:
	    diff -w ${eg} results/${e} > /dev/null
	    if test $? -eq 0; then
		# Error/warnings match, it passes!
		echo $ECHO_N "."
		passedct=`expr $passedct + 1`
	    else
		# Error/warnings don't match.
		echo $ECHO_N "W"
                eval "failed$failedct='W: ${a} did not match errors and warnings!'"
		failedct=`expr $failedct + 1`
	    fi
	fi
    else
	echo ${asm} | grep -v err >/dev/null
       	if test $? -gt 0; then
	    # YASM detected errors but shouldn't have!
	    echo $ECHO_N "E"
            eval "failed$failedct='E: ${a} returned an error code!'"
	    failedct=`expr $failedct + 1`
	else
	    ./test_hd results/${o} > results/${oh}
	    diff ${og} results/${oh} > /dev/null
	    if test $? -eq 0; then
		diff -w ${eg} results/${e} > /dev/null
		if test $? -eq 0; then
		    # Both object file and error/warnings match, it passes!
		    echo $ECHO_N "."
		    passedct=`expr $passedct + 1`
		else
		    # Error/warnings don't match.
		    echo $ECHO_N "W"
                    eval "failed$failedct='W: ${a} did not match errors and warnings!'"
		    failedct=`expr $failedct + 1`
		fi
	    else
		# Object file doesn't match.
		echo $ECHO_N "O"
                eval "failed$failedct='O: ${a} did not match object file!'"
		failedct=`expr $failedct + 1`
	    fi
	fi
    fi
done

ct=`expr $failedct + $passedct`
per=`expr 100 \* $passedct / $ct`

echo " +$passedct-$failedct/$ct $per%"
i=0
while test $i -lt $failedct; do
    eval "failure=\$failed$i"
    echo " ** $failure"
    i=`expr $i + 1`
done

exit $failedct
