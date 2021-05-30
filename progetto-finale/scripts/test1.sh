#!/bin/bash

prefix="-f ./cs_sock -p"
client="$1 $prefix"
idir="$(pwd)/tdir"

# SEND DIRECTORY
$client -w $idir/simpledir -t 200
$client -w $idir/genericdir -t 200
$client -w $idir/recursivedir -t 200
$client -w $idir/longdir -t 200

# SEND FILES
$client -W $idir/smallfile1.txt -t 200
$client -W $idir/smallfile2.txt,$idir/file1.txt,$idir/file2.txt -t 200

# READ RND FILES
$client -R -d ./out/rn0
$client -R n=5 -d ./out/rn5
$client -R n=7 -d ./out/rn7
$client -R n=2 -d ./out/rn2
$client -R n=3 -d ./out/rn3

# READ FILES
$client -r $idir/file1.txt -t 200
$client -r $idir/file1.txt,$idir/smallfile1.txt,$idir/file2.txt -t 200

# LOCK / UNLOCK
$client -l $idir/file1.txt -t 200 &
$client -l $idir/file1.txt -t 200 &
$client -l $idir/file1.txt -t 200 -u $idir/file1.txt &
$client -l $idir/file1.txt -t 200 &
$client -l $idir/file1.txt -t 200 &
$client -l $idir/file1.txt,$idir/file2.txt -t 200 &
$client -l $idir/file1.txt,$idir/smallfile1.txt -t 200

# REMOVE
$client -c $idir/file2.txt -t 200 -c $idir/smallfile1.txt

# APPEND
$client -w $idir/smallfile1.txt
$client -a $idir/smallfile2.txt,$idir/smallfile1.txt
$client -r $idir/smallfile1.txt -d ./out/app

wait

# echo "SENDING SIGHUP SIGNAL"
