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

#result=$(grep -c $searchstr $filesdir/*.*)

total=0
files=0


files=$(find $filesdir -type f | wc -l)
total=$(grep $searchstr $(find $filesdir -type f)| wc -l)

echo "The number of files are $files and the number of matching lines are $total"