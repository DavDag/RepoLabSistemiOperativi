#!/bin/bash

prefix="-f ./cs_sock -p"
client="$1 $prefix"
# client="valgrind --leak-check=full $1 $prefix"
idir="$(pwd)/tdir"
delay=200

# SEND DIRECTORY
$client -w $idir/simpledir -t $delay
$client -w $idir/genericdir -t $delay
$client -w $idir/recursivedir -t $delay
$client -w $idir/longdir -t $delay

# SEND FILES
$client -W $idir/smallfile1.txt -t $delay
$client -W $idir/smallfile2.txt,$idir/file1.txt,$idir/file2.txt -t $delay
 
# READ RND FILES
$client -R -d ./out/rn0
$client -R n=5 -d ./out/rn5
$client -R n=7 -d ./out/rn7
$client -R n=2 -d ./out/rn2
$client -R n=3 -d ./out/rn3

# READ FILES
$client -r $idir/file1.txt -t $delay
$client -r $idir/file1.txt,$idir/smallfile1.txt,$idir/file2.txt -t $delay

# LOCK / UNLOCK
$client -l $idir/file1.txt -t $delay &
$client -l $idir/file1.txt -t $delay &
$client -l $idir/file1.txt -t $delay -u $idir/file1.txt &
$client -l $idir/file1.txt -t $delay &
$client -l $idir/file1.txt -t $delay &
$client -l $idir/file1.txt,$idir/smallfile1.txt -t $delay
$client -l $idir/file1.txt -t $delay

# REMOVE
$client -c $idir/file2.txt -t $delay
$client -c $idir/smallfile1.txt -t $delay

# APPEND
$client -W $idir/smallfile1.txt -t $delay
$client -a $idir/smallfile2.txt,$idir/smallfile1.txt -t $delay
$client -r $idir/smallfile1.txt -d ./out/app -t $delay

wait

# echo "SENDING SIGHUP SIGNAL"
