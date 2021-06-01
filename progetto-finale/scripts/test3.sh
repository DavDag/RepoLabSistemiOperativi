#!/bin/bash

prefix="-f ./cs_sock"
client="$1 $prefix "
idir="$(pwd)/tdir"

# 
bigdirfiles=( $(ls $idir/bigdir) )
for i in ${!bigdirfiles[@]}
do
    $client -W $idir/bigdir/${bigdirfiles[$i]} &
done

# 
longdirfiles=( $(ls $idir/longdir) )
for i in ${!longdirfiles[@]}
do
    longdirfiles[$i]="$idir/longdir/${longdirfiles[$i]}"
done

# 
genericdirfiles=( $(ls $idir/genericdir) )
for i in ${!genericdirfiles[@]}
do
    genericdirfiles[$i]="$idir/genericdir/${genericdirfiles[$i]}"
done

# 
currInUse=()
files=( "${longdirfiles[@]}" "${genericdirfiles[@]}" )
for i in ${!files[@]}
do
    echo "${files[$i]}" >> "Available"
done
availableCount=$(( ${#files[@]} ))
files=()

# Spawn random client
spawnClient() {
    # Count available files
    availableCount=$(wc -l < Available)

    # Randomly choose a number 'n'
    [[ $availableCount -eq 0 ]] && num_file=$(( 0 )) || num_file=$(( $RANDOM % (availableCount / 4) ))

    # vars
    client_cmd="$client"
    selected=()

    # For 1 to n "consume 1 line" and add "-W file" to command
    for i in $(seq 1 $num_file)
    do
        # Add "-R n=$n" with 50% chance (to slow the client)
        n=$(( $RANDOM % 128 + 32 ))
        [[ $(( $RANDOM % 100 )) -lt 50 ]] && rncmd="-R n=$n" || rncmd=""
        # Add "-W file"
        file=$(head -n 1 Available)
        sed -i 1d Available
        selected[${#selected[@]}]=$file
        client_cmd+="-W $file $rncmd "
    done

    # From 1 to n add "-c file" to command
    for i in $(seq 1 $num_file)
    do
        # Add "-R n=$n" with 50% chance (to slow the client)
        n=$(( $RANDOM % 128 + 32 ))
        [[ $(( $RANDOM % 100 )) -lt 50 ]] && rncmd="-R n=$n" || rncmd=""
        # Add "-c file"
        client_cmd+="-c ${selected[$(( $i - 1 ))]} $rncmd "
    done

    # echo "$availableCount"

    # Save file used by client $1
    currInUse[$1]="${selected[@]}"
    # Run client in bg
    runClient "$client_cmd" $1 &
}

# Run client command and call clientTerminated at the end
runClient() {
    # echo "$1"
    $1
    clientTerminated $2
}

# Restore file sent from client $1
clientTerminated() {
    # Retrieve file used
    tmp=(${currInUse[$1]})
    # And restore them
    for i in ${!tmp[@]}
    do
        echo "${tmp[$i]}" >> "Available"
    done
    currInUse[$1]=""
}

# Stop after 30 seconds
cid=0
end=$(( SECONDS + 30 ))
while [ $SECONDS -lt $end ]
do
    # Count child processes
    num_childrens=( $(ps --no-headers -o pid --ppid=$$ | wc -w) )
    if [ $num_childrens -lt 16 ]
    then
        # Spawn client if needed
        spawnClient $cid
        cid=$(( $cid + 1 ))
    fi
    #echo $num_childrens
done

for i in ${!bigdirfiles[@]}
do
    $client -c $idir/bigdir/${bigdirfiles[$i]} &
done

wait

rm -f Available;
