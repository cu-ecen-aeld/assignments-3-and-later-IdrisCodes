#!/bin/sh

#Perform checks
if [ "$#" -ne 2 ]; then
    echo "Illegal number of parameters"
    exit 1
fi

if ! [ -d "$1" ]; then
   echo "Provided argument is not a directory"
   exit 1
fi

filesdir="$1"
searchstr="$2"

result=$(grep -c $searchstr $filesdir/*.*)

total=0
files=0
for i in $result
do
num=$(grep -oE [0-9]+$ <<< $i)
total=$((total + num))
if [ $num -gt "0" ]; then
files=$((files + 1))
fi

done

echo "The number of files are $files and the number of matching lines are $total"