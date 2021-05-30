#!/bin/bash

prefix="-f ./cs_sock -p"
client="$1 $prefix"
idir="$(pwd)/tdir"

$client -w $idir/longdir,n=16 -D ./out/1
$client -W $idir/bigdir/file00.txt -D ./out/2
$client -W $idir/file2.txt,$idir/smallfile1.txt -D ./out/3

wait
