#!/bin/sh
# $IdPath$

#
# Verify that all test cases match
#

passedct=0
failedct=0
errorct=0

OT="objfmt_test"


for asm in ${srcdir}/src/objfmts/bin/tests/*.asm
do
    a=`echo ${asm} | sed -e 's,^.*/,,' | sed -e 's,.asm$,,'`
    o=${a}
    oh=${o}.hx
    og=`echo ${asm} | sed -e 's,.asm$,.bin.hx,'`
    e=${a}.ew
    eg=`echo ${asm} | sed -e 's,.asm$,.errwarn,'`

    echo -n "$OT: Testing bin objfmt for ${a} return value ..."
    # Run within a subshell to prevent signal messages from displaying.
    sh -c "./yasm -f bin ${asm} -o ${o} 2>${e}" 2>/dev/null
    status=$?
    if test $status -gt 128; then
	# We should never get a coredump!
	echo " FAIL (crashed)."
	failedct=`expr $failedct + 1`
    elif test $status -gt 0; then
	echo ${asm} | grep err >/dev/null
       	if test $? -gt 0; then
	    # YASM detected errors but shouldn't have!
	    echo " FAIL."
	    failedct=`expr $failedct + 1`
	else
	    echo " PASS."
	    passedct=`expr $passedct + 1`
	    echo -n "$OT: Testing bin objfmt for ${a} error/warnings ..."
	    # We got errors, check to see if they match:
    	    cat ${e} | sed -e "s,${srcdir}/,./," >${e}.2
    	    mv ${e}.2 ${e}
	    if diff -w ${eg} ${e} > /dev/null; then
		# Error/warnings match, it passes!
		echo " PASS."
		passedct=`expr $passedct + 1`
	    else
		# Error/warnings don't match.
		echo " FAIL."
		failedct=`expr $failedct + 1`
	    fi
	fi
    else
	echo ${asm} | grep -v err >/dev/null
       	if test $? -gt 0; then
	    # YASM detected errors but shouldn't have!
	    echo " FAIL."
	    failedct=`expr $failedct + 1`
	else
	    echo " PASS."
	    passedct=`expr $passedct + 1`
	    echo -n "$OT: Testing bin objfmt for ${a} output file ..."
	    hexdump -e '1/1 "%02x " "\n"' ${o} > ${oh}
	    if diff -w ${og} ${oh} > /dev/null; then
		echo " PASS."
		passedct=`expr $passedct + 1`
		echo -n "$OT: Testing bin objfmt for ${a} error/warnings ..."
		cat ${e} | sed -e "s,${srcdir}/,./," >${e}.2
		mv ${e}.2 ${e}
		if diff -w ${eg} ${e} > /dev/null; then
		    # Both object file and error/warnings match, it passes!
		    echo " PASS."
		    passedct=`expr $passedct + 1`
		else
		    # Error/warnings don't match.
		    echo " FAIL."
		    failedct=`expr $failedct + 1`
		fi
	    else
		# Object file doesn't match.
		echo " FAIL."
		failedct=`expr $failedct + 1`
	    fi
	fi
    fi
done

ct=`expr $failedct + $passedct + $errorct`
per=`expr 100 \* $passedct / $ct`

echo "$OT: $per%: Checks: $ct, Failures $failedct, Errors: $errorct"

exit `expr $failedct + $errorct`
