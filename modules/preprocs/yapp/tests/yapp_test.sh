#! /bin/sh
# $IdPath$

case `echo "testing\c"; echo 1,2,3`,`echo -n testing; echo 1,2,3` in
  *c*,-n*) ECHO_N= ECHO_C='
' ECHO_T='	' ;;
  *c*,*  ) ECHO_N=-n ECHO_C= ECHO_T= ;;
  *)       ECHO_N= ECHO_C='\c' ECHO_T= ;;
esac

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
    a=`echo ${asm} | sed 's,^.*/,,;s,.asm$,,'`
    y=${a}.yp
    p=`echo ${asm} | sed 's,.asm$,.pre,'`

    echo $ECHO_N "$YT: Testing yapp for ${a} ... $ECHO_C"
    if sed "s,\./,${srcdir}/," ${asm} | ./yasm -e |
	sed "s,${srcdir}/,./," > ${y}; then
	if diff -w ${p} ${y} > /dev/null; then
	    echo "PASS."
	    passedct=`expr $passedct + 1`
	    passedlist="${passedlist}${a} "
	else
	    echo "FAIL."
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
