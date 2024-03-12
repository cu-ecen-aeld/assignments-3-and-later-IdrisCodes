#!/bin/sh

if [ "$#" -ne 2 ]; then
    echo "Illegal number of parameters"
    exit 1
fi


filesdir=$1

if ! [ -d $filesdir ]; then
    echo "This is not a valid directory: $filesdir"
    exit 1
fi
searchstr=$2

X=$(find $filesdir -type f | wc -l)
Y=$(grep -e $searchstr -r $filesdir | wc -l)

echo "The number of files are $X and the number of matching lines are $Y"
