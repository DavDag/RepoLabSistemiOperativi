#!/bin/bash

prefix="-f ./cs_sock -p"
client="$1 $prefix"
idir="$(pwd)/tdir"

sleep 1

$client -w $idir/longdir,n=50
$client -W $idir/smallfile1.txt

$client -l $idir/smallfile1.txt -t 500 &
$client -l $idir/smallfile1.txt -t 500 &
$client -l $idir/smallfile1.txt -t 500 &
$client -l $idir/smallfile1.txt -t 500 &
$client -l $idir/smallfile1.txt -t 500 &
$client -l $idir/smallfile1.txt -t 500 &
$client -l $idir/smallfile1.txt -t 500 &
$client -l $idir/smallfile1.txt -t 500 &
$client -l $idir/smallfile1.txt -t 500 &
$client -l $idir/smallfile1.txt -t 500 &
$client -l $idir/smallfile1.txt -t 500 &
$client -l $idir/smallfile1.txt -t 500 &

wait
