#!/bin/sh

YVF=YASM-VERSION-FILE
DEF_VER=v1.1.0

LF='
'

# First see if there is a version file (included in release tarballs),
# then try git-describe, then default.
if test -f version
then
	VN=$(cat version) || VN="$DEF_VER"
elif test -d .git -o -f .git &&
	VN=$(git describe --match "v[0-9]*" --abbrev=4 HEAD 2>/dev/null) &&
	case "$VN" in
	*$LF*) (exit 1) ;;
	v0.1.0*)
		# Special handling until we get a more recent tag on the
		# master branch
		MERGE_BASE=$(git merge-base $DEF_VER HEAD 2>/dev/null)
		VN1=$(git rev-list $MERGE_BASE..HEAD | wc -l 2>/dev/null)
		VN2=$(git rev-list --max-count=1 --abbrev-commit --abbrev=4 HEAD 2>/dev/null)
		VN=$(echo "v$DEF_VER-$VN1-g$VN2" | sed -e 's/ //g')
		git update-index -q --refresh
		test -z "$(git diff-index --name-only HEAD --)" ||
		VN="$VN-dirty" ;;
	v[0-9]*)
		git update-index -q --refresh
		test -z "$(git diff-index --name-only HEAD --)" ||
		VN="$VN-dirty" ;;
	esac
then
	VN=$(echo "$VN" | sed -e 's/-/./g');
else
	VN="$DEF_VER"
fi

VN=$(expr "$VN" : v*'\(.*\)')

if test -r $YVF
then
	VC=$(cat $YVF)
else
	VC=unset
fi
test "$VN" = "$VC" || {
	echo >&2 "$VN"
	echo "$VN" >$YVF
	echo "#define PACKAGE_STRING \"yasm $VN\"" > YASM-VERSION.h
	echo "#define PACKAGE_VERSION \"$VN\"" >> YASM-VERSION.h
}
