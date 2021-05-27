#!/bin/sh

client=$1
prefix="-f ./cs_sock -p" 

sleep 1

# SEND DIRECTORY
# $client $prefix -w ./tdir/simpledir -t 200 -w ./tdir/genericdir
# $client $prefix -w ./tdir/recursivedir -t 200 -w ./tdir/longdir,n=5
# # $client $prefix -w ./tdir/bigdir
# 
# # SEND FILE / FILES
# $client $prefix -W ./tdir/file1.txt,./tdir/file2.txt -t 200 -W ./tdir/smallfile1.txt
$client $prefix -W ./tdir/file1.txt
# 
# # READ RND FILES
# # $client $prefix -R -d ./out
# $client $prefix -R n=5 -d ./out
# $client $prefix -R n=5 -d ./out
# 
# # READ FILES
# $client $prefix -r ./tdir/file1.txt,./tdir/smallfile1.txt -t 200 -r ./tdir/file1.txt
# 
# # LOCK UNLOCk
#$client $prefix -l ./tdir/file1.txt -t 200 &
$client $prefix -l ./tdir/file1.txt -t 100 &
$client $prefix -l ./tdir/file1.txt -t 100 &
$client $prefix -l ./tdir/file1.txt -t 100 &
$client $prefix -l ./tdir/file1.txt -t 100 &
$client $prefix -l ./tdir/file1.txt -t 100 &
$client $prefix -l ./tdir/file1.txt -t 100 &
$client $prefix -l ./tdir/file1.txt -t 100 &
$client $prefix -l ./tdir/file1.txt -t 100 &
$client $prefix -l ./tdir/file1.txt -t 100 &

# $client $prefix -l ./tdir/file1.txt -t 200 &
# $client $prefix -l ./tdir/file1.txt -t 200 -u ./tdir/file1.txt &
# $client $prefix -l ./tdir/file1.txt,./tdir/smallfile1.txt
# 
# # REMOVE
# $client $prefix -c ./tdir/file2.txt -t 200 -c ./tdir/smallfile1.txt

wait

# echo "SENDING SIGHUP SIGNAL"
