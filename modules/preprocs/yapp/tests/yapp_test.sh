#! /bin/sh

case `echo "testing\c"; echo 1,2,3`,`echo -n testing; echo 1,2,3` in
  *c*,-n*) ECHO_N= ECHO_C='
' ECHO_T='	' ;;
  *c*,*  ) ECHO_N=-n ECHO_C= ECHO_T= ;;
  *)       ECHO_N= ECHO_C='\c' ECHO_T= ;;
esac

mkdir results >/dev/null 2>&1

#
# Verify that all test cases match
# (aside from whitespace for now)
#

passedct=0
failedct=0

YT="yapp_test"


echo $ECHO_N "Test $YT: $ECHO_C"
for asm in ${srcdir}/modules/preprocs/yapp/tests/*.asm
do
    a=`echo ${asm} | sed 's,^.*/,,;s,.asm$,,'`
    o=${a}.yp
    og=`echo ${asm} | sed 's,.asm$,.pre,'`
    e=${a}.ew
    eg=`echo ${asm} | sed 's,.asm$,.errwarn,'`
    if test \! -f ${eg}; then
	eg=/dev/null
    fi

    srcdir_re=`echo ${srcdir} | sed 's,\.,\\.,'`

    # Run within a subshell to prevent signal messages from displaying.
    sh -c "sed 's,\./,${srcdir}/,' ${asm} | ./yasm -e -r yapp - 2>results/${e} | sed 's,${srcdir_re}/,./,' > results/${o}" >/dev/null 2>/dev/null
    status=$?
    if test $status -gt 128; then
	# We should never get a coredump!
	echo $ECHO_N "C$ECHO_C"
        eval "failed$failedct='C: ${a} crashed!'"
	failedct=`expr $failedct + 1`
    elif test $status -gt 0; then
	echo ${asm} | grep err >/dev/null
       	if test $? -gt 0; then
	    # YASM detected errors but shouldn't have!
	    echo $ECHO_N "E$ECHO_C"
            eval "failed$failedct='E: ${a} returned an error code!'"
	    failedct=`expr $failedct + 1`
	else
	    # We got errors, check to see if they match:
	    if diff -w ${eg} results/${e} >/dev/null; then
		# Error/warnings match, it passes!
		echo $ECHO_N ".$ECHO_C"
		passedct=`expr $passedct + 1`
	    else
		# Error/warnings don't match.
		echo $ECHO_N "W$ECHO_C"
                eval "failed$failedct='W: ${a} did not match errors and warnings!'"
		failedct=`expr $failedct + 1`
	    fi
	fi
    else
	echo ${asm} | grep -v err >/dev/null
       	if test $? -gt 0; then
	    # YASM didn't detect errors but should have!
	    echo $ECHO_N "E$ECHO_C"
            eval "failed$failedct='E: ${a} did not return an error code!'"
	    failedct=`expr $failedct + 1`
	else
	    if diff -w ${og} results/${o} >/dev/null; then
		if diff -w ${eg} results/${e} >/dev/null; then
		    # Both output file and error/warnings match, it passes!
		    echo $ECHO_N ".$ECHO_C"
		    passedct=`expr $passedct + 1`
		else
		    # Error/warnings don't match.
		    echo $ECHO_N "W$ECHO_C"
                    eval "failed$failedct='W: ${a} did not match errors and warnings!'"
		    failedct=`expr $failedct + 1`
		fi
	    else
		# Output file doesn't match.
		echo $ECHO_N "O$ECHO_C"
                eval "failed$failedct='O: ${a} did not match output file!'"
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
