#! /bin/sh
# $IdPath$

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
errorct=0

for asm in ${srcdir}/$2/*.asm
do
    a=`echo ${asm} | sed 's,^.*/,,;s,.asm$,,'`
    o=${a}$5
    oh=${a}.hx
    og=`echo ${asm} | sed 's,.asm$,.hex,'`
    e=${a}.ew
    eg=`echo ${asm} | sed 's,.asm$,.errwarn,'`

    echo $ECHO_N "$1: Testing $3 for ${a} return value ... $ECHO_C"
    # Run within a subshell to prevent signal messages from displaying.
    sh -c "cat ${asm} | ./yasm $4 -o results/${o} 2>results/${e}" 2>/dev/null
    status=$?
    if test $status -gt 128; then
	# We should never get a coredump!
	echo "FAIL (crashed)."
	failedct=`expr $failedct + 1`
    elif test $status -gt 0; then
	echo ${asm} | grep err >/dev/null
       	if test $? -gt 0; then
	    # YASM detected errors but shouldn't have!
	    echo "FAIL."
	    failedct=`expr $failedct + 1`
	else
	    echo "PASS."
	    passedct=`expr $passedct + 1`
	    echo $ECHO_N "$1: Testing $3 for ${a} error/warnings ... $ECHO_C"
	    # We got errors, check to see if they match:
    	    #cat ${e} | sed "s,${srcdir}/,./," >${e}.2
    	    #mv ${e}.2 ${e}
	    diff -w ${eg} results/${e} > /dev/null
	    if test $? -eq 0; then
		# Error/warnings match, it passes!
		echo "PASS."
		passedct=`expr $passedct + 1`
	    else
		# Error/warnings don't match.
		echo "FAIL."
		failedct=`expr $failedct + 1`
	    fi
	fi
    else
	echo ${asm} | grep -v err >/dev/null
       	if test $? -gt 0; then
	    # YASM detected errors but shouldn't have!
	    echo "FAIL."
	    failedct=`expr $failedct + 1`
	else
	    echo "PASS."
	    passedct=`expr $passedct + 1`
	    echo $ECHO_N "$1: Testing $3 for ${a} output file ... $ECHO_C"
	    ${PERL} ${srcdir}/test_hd.pl results/${o} > results/${oh}
	    diff ${og} results/${oh} > /dev/null
	    if test $? -eq 0; then
		echo "PASS."
		passedct=`expr $passedct + 1`
		echo $ECHO_N "$1: Testing $3 for ${a} error/warnings ... $ECHO_C"
		#cat ${e} | sed "s,${srcdir}/,./," >${e}.2
		#mv ${e}.2 ${e}
		diff -w ${eg} results/${e} > /dev/null
		if test $? -eq 0; then
		    # Both object file and error/warnings match, it passes!
		    echo "PASS."
		    passedct=`expr $passedct + 1`
		else
		    # Error/warnings don't match.
		    echo "FAIL."
		    failedct=`expr $failedct + 1`
		fi
	    else
		# Object file doesn't match.
		echo "FAIL."
		failedct=`expr $failedct + 1`
	    fi
	fi
    fi
done

ct=`expr $failedct + $passedct + $errorct`
per=`expr 100 \* $passedct / $ct`

echo "$1: $per%: Checks: $ct, Failures $failedct, Errors: $errorct"

exit `expr $failedct + $errorct`
