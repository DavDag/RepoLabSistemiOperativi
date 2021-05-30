#!/bin/bash

prefix="-f ./cs_sock -p"
client="$1 $prefix"
idir="$(pwd)/tdir"

# SEND DIRECTORY
$client -w $idir/simpledir -t 200 -w $idir/genericdir
$client -w $idir/recursivedir -t 200 -w $idir/longdir
$client -w $idir/bigdir,n=10 -D ./out/capacitymisses

# SEND FILE / FILES
$client -W $idir/file1.txt,$idir/file2.txt -t 200 -W $idir/smallfile1.txt,$idir/smallfile2.txt

# READ RND FILES
$client -R -d ./out/rn0
$client -R n=5 -d ./out/rn5
$client -R n=7 -d ./out/rn7
$client -R n=2 -d ./out/rn2
$client -R n=3 -d ./out/rn3

# READ FILES
$client -r $idir/file1.txt,$idir/smallfile1.txt -t 200 -r $idir/file1.txt

# LOCK / UNLOCK
$client -l $idir/file1.txt -t 200 &
$client -l $idir/file1.txt -t 100 &
$client -l $idir/file1.txt -t 100 &
$client -l $idir/file1.txt -t 200 -u $idir/file1.txt &
$client -l $idir/file1.txt -t 100 &
$client -l $idir/file1.txt -t 100 &
$client -l $idir/file1.txt -t 100 &
$client -l $idir/file1.txt -t 100 &
$client -l $idir/file1.txt,$idir/smallfile1.txt
$client -t 200 -l $idir/file2.txt
$client -t 200 -l $idir/file2.txt

# REMOVE
$client -c $idir/file2.txt -t 200 -c $idir/smallfile1.txt

# APPEND
$client -a $idir/smallfile1.txt,$idir/smallfile2.txt,./out/app -t200 

wait

# echo "SENDING SIGHUP SIGNAL"
