#!/bin/sh

#
# Verify that all test cases match
# (aside from whitespace for now)
#

passedct=0
passedlist=''
failedct=0
failedlist=''

YT=" - YAPP_TEST"


for asm in ${srcdir}/src/preprocs/yapp/tests/*.asm
do
    a=`echo ${asm} | sed -e 's,^.*/,,' | sed -e 's,.asm$,,'`
    y=${a}.yp
    p=`echo ${asm} | sed -e 's,.asm$,.pre,'`

    echo "$YT: Testing yapp for ${a}"
    ./yasm -e ${asm} > ${y}
    if diff -w ${p} ${y} > /dev/null; then
	passedct=`expr $passedct + 1`
	passedlist="${passedlist}${a} "
    else
	failedct=`expr $failedct + 1`
	failedlist="${failedlist}${a} "
    fi
    #rm ${y}
done

test $passedct -gt 0 && echo "$YT: PASSED $passedct: $passedlist"
test $failedct -gt 0 && echo "$YT: FAILED $failedct: $failedlist"

exit $failedct
