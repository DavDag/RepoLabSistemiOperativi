#!/bin/bash

export LC_NUMERIC="en_US.UTF-8"

# Setup
LOG_FILE=$1

# Vars
TOTAL_RFSUM=0    # Total read file bytes 
TOTAL_RFCOUNT=0  # Total read file op
TOTAL_RNSUM=0    # Total read n bytes
TOTAL_RNCOUNT=0  # Total read n op
TOTAL_WSUM=0     # Total write file bytes
TOTAL_WCOUNT=0   # Total write file op
TOTAL_LCOUNT=0   # Total lock file op
TOTAL_UCOUNT=0   # Total unlock file op
TOTAL_RMCOUNT=0  # Total remove file op
TOTAL_OLCOUNT=0  # Total open-lock file op
TOTAL_CCOUNT=0   # Total close file op
TOTAL_OFCOUNT=0  # Total open file op 
TOTAL_OSCOUNT=0  # Total open session op 
TOTAL_CSCOUNT=0  # Total close session op 
TOTAL_LNCOUNT=0  # Total lock notification sent
MAX_SIZE_BYTES=0 # Maximum size reached in bytes (file-system)
MAX_SIZE_SLOT=0  # Maximum size reached in slot (file-system)
TOTAL_CACOUNT=0  # Total capacity misses
TOTAL_REQ_PTH=() # Total requests per thread
MAX_SAME_CC=0    # Maximum client connected togheter

#exec 3<$LOG_FILE

# Read splitting on '|'
while IFS='|' read -a tmp
do
    for i in ${!tmp[@]}
    do
        # Extract key and value
        k=(${tmp[$i]%:*})
        v=(${tmp[$i]#*:})
        key="${k[@]}"
        value="${v[@]}"

        # Handle based on type
        case $key in
            # Time since server started
            T)
                # Nothing
                ;;

            # Time to read-perform-write req
            ms)
                # Nothing
                ;;

            # Thread id and operation type
            \[#**\]*{**})
                thread=${key:2:2}
                if [[ $thread == "LK" ]]
                then
                    TOTAL_LNCOUNT=$(( TOTAL_LNCOUNT + 1 ))
                else
                    TOTAL_REQ_PTH[$thread]=$(( TOTAL_REQ_PTH[$thread] + 1))
                fi
                optype=${key:7:2}
                case $optype in 
                    LF)
                        TOTAL_LCOUNT=$(( TOTAL_LCOUNT + 1 ))
                        ;;
                    UF)
                        TOTAL_UCOUNT=$(( TOTAL_UCOUNT + 1 ))
                        ;;
                    RM)
                        TOTAL_RMCOUNT=$(( TOTAL_RMCOUNT + 1 ))
                        ;;
                    OL)
                        TOTAL_OLCOUNT=$(( TOTAL_OLCOUNT + 1 ))
                        ;;
                    OS)
                        TOTAL_OSCOUNT=$(( TOTAL_OSCOUNT + 1 ))
                        ;;
                    O*)
                        TOTAL_OFCOUNT=$(( TOTAL_OFCOUNT + 1 ))
                        ;;
                    CF)
                        TOTAL_CCOUNT=$(( TOTAL_CCOUNT + 1 ))
                        ;;
                    CS)
                        TOTAL_CSCOUNT=$(( TOTAL_CSCOUNT + 1 ))
                        ;;
                esac
                ;;
            
            # Client id
            C-ID)
                # Nothing
                ;;
            
            # Num connected clients
            CC)
                MAX_SAME_CC=$((MAX_SAME_CC > value ? MAX_SAME_CC : value))
                ;;
            
            # Bytes wrote
            W)
                if [[ $optype == "RF" ]]
                then
                    TOTAL_RFSUM=$(( TOTAL_RFSUM + value ))
                    TOTAL_RFCOUNT=$(( TOTAL_RFCOUNT + 1 ))
                elif [[ $optype == "RN" ]]
                then
                    TOTAL_RNSUM=$(( TOTAL_RNSUM + value ))
                    TOTAL_RNCOUNT=$(( TOTAL_RNCOUNT + 1 ))
                fi
                ;;
            
            # Bytes read
            R)
                if [[ $optype == "WF" ]]
                then
                    TOTAL_WSUM=$(( TOTAL_WSUM + value ))
                    TOTAL_WCOUNT=$(( TOTAL_WCOUNT + 1 ))
                fi
                ;;
            
            # File System size (Bytes)
            FS-B)
                MAX_SIZE_BYTES=$((MAX_SIZE_BYTES > value ? MAX_SIZE_BYTES : value))
                ;;
            
            # File System size (Slot)
            FS-S)
                MAX_SIZE_SLOT=$((MAX_SIZE_SLOT > value ? MAX_SIZE_SLOT : value))
                ;;
            
            # File System capacity misses count
            FS-M)
                TOTAL_CACOUNT=$(( value ))
                ;;
        esac
    done

done < $LOG_FILE

#exec 3<&-

# DO NOT AVERAGE BYTES WITH DECIMAL PRECISION.
AVG_RF=$(( TOTAL_RFCOUNT == 0 ? 0 : TOTAL_RFSUM / TOTAL_RFCOUNT ))
AVG_RN=$(( TOTAL_RNCOUNT == 0 ? 0 : TOTAL_RNSUM / TOTAL_RNCOUNT ))
AVG_WF=$(( TOTAL_WCOUNT == 0 ? 0 : TOTAL_WSUM / TOTAL_WCOUNT ))

conv_bytes() {
    if [[ $1 -lt 1024 ]]
    then
        unit=" B"
        byt=$(echo "$1" | bc -l)
    elif [[ $1 -lt 1024*1024 ]]
    then
        unit="KB"
        byt=$(echo "$1/1024" | bc -l)
    elif [[ $1 -lt 1024*1024*1024 ]]
    then
        unit="MB"
        byt=$(echo "$1/1024/1024" | bc -l)
    elif [[ $1 -lt 1024*1024*1024*1024 ]]
    then
        unit="GB"
        byt=$(echo "$1/1024/1024/1024" | bc -l)
    fi
    echo "$(printf "%7.2f $unit" $byt)"
}

echo "+--------------------------------------+"
echo "| Avg bytes (READ-FILE)  => $(conv_bytes $AVG_RF) |"
echo "| Avg bytes (READ-N)     => $(conv_bytes $AVG_RN) |"
echo "| Avg bytes (WRITE-FILE) => $(conv_bytes $AVG_WF) |"
echo "| Total bytes (READ)     => $(conv_bytes $(( $TOTAL_RNSUM + $TOTAL_RFSUM )) ) |"
echo "| Total bytes (WRITTEN)  => $(conv_bytes $TOTAL_WSUM) |"
echo "+--------------------------------------+"
echo "| Count (READ-FILE)      => $(printf "%10d" $TOTAL_RFCOUNT) |"
echo "| Count (READ-N)         => $(printf "%10d" $TOTAL_RNCOUNT) |"
echo "| Count (WRITE-FILE)     => $(printf "%10d" $TOTAL_WCOUNT) |"
echo "| Count (LOCK-FILE)      => $(printf "%10d" $TOTAL_LCOUNT) |"
echo "| Count (UNLOCK-FILE)    => $(printf "%10d" $TOTAL_UCOUNT) |"
echo "| Count (REMOVE-FILE)    => $(printf "%10d" $TOTAL_RMCOUNT) |"
echo "| Count (OPEN-FILE)      => $(printf "%10d" $TOTAL_OFCOUNT) |"
echo "| Count (OPEN-LOCK-FILE) => $(printf "%10d" $TOTAL_OLCOUNT) |"
echo "| Count (CLOSE-FILE)     => $(printf "%10d" $TOTAL_CCOUNT) |"
echo "| Count (OPEN-SESSION)   => $(printf "%10d" $TOTAL_OSCOUNT) |"
echo "| Count (CLOSE-SESSION)  => $(printf "%10d" $TOTAL_CSCOUNT) |"
echo "| Count (LOCK-NOT)       => $(printf "%10d" $TOTAL_LNCOUNT) |"
echo "+--------------------------------------+"
echo "| Max FS size (Bytes)    => $(conv_bytes $MAX_SIZE_BYTES) |"
echo "| Max FS size (Slots)    => $(printf "%10d" $MAX_SIZE_SLOT) |"
echo "| Count capacity misses  => $(printf "%10d" $TOTAL_CACOUNT) |"
echo "| Max connected clients  => $(printf "%10d" $MAX_SAME_CC) |"
echo "+--------------------------------------+"
for i in ${!TOTAL_REQ_PTH[@]}
do
    echo "| Req handled by t #$(printf "%.3d" $i)  => $(printf "%10d" ${TOTAL_REQ_PTH[$i]}) |"
done
echo "+--------------------------------------+"
