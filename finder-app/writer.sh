#!/bin/bash
#bin
#Perform checks
if [ "$#" -ne 2 ]; then
    echo "Illegal number of parameters"
    exit 1
fi


result=$(mkdir -p $(dirname $1))
if [ "$((result))" -ne "0" ]; then
    echo "Couldnt create directory"
    exit 1
fi

result=$(touch $1)
if [ "$((result))" -ne "0" ]; then
    echo "Couldnt create file"
    exit 1
fi


echo $2 > $1