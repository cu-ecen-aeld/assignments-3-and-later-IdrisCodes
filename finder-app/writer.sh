#!/bin/sh

if [ "$#" -ne 2 ]; then
    echo "Illegal number of parameters"
    exit 1
fi


writefile=$1
writestr=$2



mkdir -p $(dirname $writefile)

touch $writefile

if ! [ -f $writefile ]; then
    echo "Could not create file"
    exit 1
fi

echo $writestr > $writefile