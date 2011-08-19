#!/usr/bin/perl
use POSIX qw(strftime);

$|++;

my $version = '0.0.1';
my $todaydate = strftime "%Y%m%d", localtime;
my $snapshot_dir = "/home/yasm/public_html/snapshots/$todaydate";
my $cvs_dir = '/usr/local/cvs2/yasm/chrooted-cvs/cvs';
my $build_dir = '/home/yasm/build';

# Create snapshot directory
system('mkdir', $snapshot_dir);
# Relink "latest" symlink
system('rm', "$snapshot_dir/../latest");
system('ln', '-s', $snapshot_dir, "$snapshot_dir/../latest");
system('rm', "$snapshot_dir/../../latest");
system('ln', '-s', $snapshot_dir, "$snapshot_dir/../../latest");

# Build CVS tree tarball
chdir("$cvs_dir/..") || die "Could not change to CVS directory";
system('tar', '-czf', "$snapshot_dir/yasm-cvstree.tar.gz", 'cvs') == 0
    or die "CVS tree tarball build failed: $?.";

# Build CVS checkout tarball
chdir($build_dir) || die "Could not change to build directory";
system('chmod', '-R', 'u+w', 'yasm') == 0
    or die "Could not chmod old CVS checkout directory: $?.";
system('rm', '-Rf', 'yasm') == 0
    or die "Could not remove old CVS checkout directory: $?.";
system('mkdir', 'yasm') == 0
    or die "Could not create CVS checkout directory: $?.";
chdir('yasm') || die "Could not change to CVS checkout directory.";
system('cvs', '-q', "-d$cvs_dir", 'checkout', '.') == 0
    or die "CVS checkout failed: $?.";
chdir('..');
system('tar', '-czf', "$snapshot_dir/yasm-cvs.tar.gz", 'yasm') == 0
    or die "CVS checkout tarball build failed: $?.";
chdir('yasm');

# Update and build distribution tarball
system("./autogen.sh >$snapshot_dir/autogen-log.txt 2>$snapshot_dir/autogen-errwarn.txt") == 0
    or die "autogen.sh failed: $?.";
system("gmake distcheck >$snapshot_dir/make-log.txt 2>$snapshot_dir/make-errwarn.txt") == 0
    or die "build failed: $?.";
system("cp *.tar.gz $snapshot_dir/") == 0
    or die "dist tarball copy failed.";
