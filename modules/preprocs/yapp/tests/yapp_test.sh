#!/bin/sh

#
# Verify that all test cases match
# (aside from whitespace for now)
#

passedct=0
passedlist=''
failedct=0
failedlist=''
errorct=0
errorlist=''

YT="yapp_test"


for asm in ${srcdir}/src/preprocs/yapp/tests/*.asm
do
    a=`echo ${asm} | sed -e 's,^.*/,,' | sed -e 's,.asm$,,'`
    y=${a}.yp
    p=`echo ${asm} | sed -e 's,.asm$,.pre,'`

    echo -n "$YT: Testing yapp for ${a} ..."
    if sed -e "s,\./,${srcdir}/," ${asm} | ./yasm -e |
	sed -e "s,${srcdir}/,./," > ${y}; then
	if diff -w ${p} ${y} > /dev/null; then
	    echo " PASS."
	    passedct=`expr $passedct + 1`
	    passedlist="${passedlist}${a} "
	else
	    echo " FAIL."
	    failedct=`expr $failedct + 1`
	    failedlist="${failedlist}${a} "
	fi
    else
	errorct=`expr $errorct + 1`
	errorlist="${errorlist}${a} "
    fi
done

ct=`expr $failedct + $passedct + $errorct`
per=`expr 100 \* $passedct / $ct`

echo "$YT: $per%: Checks: $ct, Failures $failedct, Errors: $errorct"
#test $passedct -gt 0 && echo "$YT: PASSED $passedct: $passedlist"
#test $failedct -gt 0 && echo "$YT: FAILED $failedct: $failedlist"
#test $errorct -gt 0 && echo "$YT: ERRORED $errorct: $errorlist"

exit `expr $failedct + $errorct`
