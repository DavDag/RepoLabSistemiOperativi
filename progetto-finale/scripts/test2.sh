#!/bin/bash

prefix="-f ./cs_sock -p"
client="$1 $prefix"
idir="$(pwd)/tdir"

$client -w $idir/longdir,n=16 -W $idir/bigdir/file05.txt -D ./out

wait
