#!/bin/sh

prefix="-f ./cs_sock -p"
#client="valgrind --leak-check=full --track-origins=yes $1 $prefix"
#client="valgrind --leak-check=full --track-origins=yes --quiet $1 $prefix"
client="$1 $prefix"

# SEND DIRECTORY
$client -w ./tdir/bigdir
$client -w ./tdir/simpledir -t 200 -w ./tdir/genericdir
$client -w ./tdir/recursivedir -t 200 -w ./tdir/longdir,n=5

# SEND FILE / FILES
$client -W ./tdir/file1.txt,./tdir/file2.txt -t 200 -W ./tdir/smallfile1.txt

# READ RND FILES
# $client -R -d ./out
$client -R n=5 -d ./out
$client -R n=5 -d ./out

# READ FILES
$client -r ./tdir/file1.txt,./tdir/smallfile1.txt -t 200 -r ./tdir/file1.txt

# LOCK UNLOCk
$client -l ./tdir/file1.txt -t 200 &
$client -l ./tdir/file1.txt -t 100 &
$client -l ./tdir/file1.txt -t 100 &
$client -l ./tdir/file1.txt -t 100 &
$client -l ./tdir/file1.txt -t 100 &
$client -l ./tdir/file1.txt -t 100 &
$client -l ./tdir/file1.txt -t 100 &
$client -l ./tdir/file1.txt -t 200 -u ./tdir/file1.txt &
$client -l ./tdir/file1.txt,./tdir/smallfile1.txt

# REMOVE
$client -c ./tdir/file2.txt -t 200 -c ./tdir/smallfile1.txt

wait

# echo "SENDING SIGHUP SIGNAL"
