#!/bin/sh
# $IdPath$
# Run this to set up the build system: configure, makefiles, etc.
# (based on the version in enlightenment's cvs)

package="yasm"

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

cd "$srcdir"
DIE=0

(gettextize --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have gettext installed to compile $package."
	echo "Download the appropriate package for your system,"
	echo "or get the source from one of the GNU ftp sites"
	echo "listed in http://www.gnu.org/order/ftp.html"
        DIE=1
}

(aclocal --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have aclocal installed to compile $package."
	echo "Download the appropriate package for your system,"
	echo "or get the source from one of the GNU ftp sites"
	echo "listed in http://www.gnu.org/order/ftp.html"
        DIE=1
}

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have autoconf installed to compile $package."
	echo "Download the appropriate package for your system,"
	echo "or get the source from one of the GNU ftp sites"
	echo "listed in http://www.gnu.org/order/ftp.html"
        DIE=1
}

(autoheader --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have autoheader installed to compile $package."
	echo "Download the appropriate package for your system,"
	echo "or get the source from one of the GNU ftp sites"
	echo "listed in http://www.gnu.org/order/ftp.html"
        DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
        echo
        echo "You must have automake installed to compile $package."
	echo "Download the appropriate package for your system,"
	echo "or get the source from one of the GNU ftp sites"
	echo "listed in http://www.gnu.org/order/ftp.html"
        DIE=1
}

if test "$DIE" -eq 1; then
        exit 1
fi

if test -z "$*"; then
        echo "I am going to run ./configure with --enable-dev - if you wish "
        echo "to pass any other args to it, please specify them on the $0"
	echo "command line."
fi

echo "Generating configuration files for $package, please wait...."

echo "  aclocal $ACLOCAL_FLAGS"
aclocal $ACLOCAL_FLAGS || exit 1
echo "  gettextize -f"
echo "N" | gettextize -f || exit 1
echo "  autoheader"
autoheader || exit 1
echo "  automake -a"
automake -a || exit 1
echo "  autoconf"
autoconf || exit 1
echo "  configure --enable-dev $@"
$srcdir/configure --enable-dev "$@" || exit 1

