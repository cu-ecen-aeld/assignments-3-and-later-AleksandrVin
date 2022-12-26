#!/bin/bash

writefile=$1
writestr=$2

if [ -z $writefile ] || [ -z $writestr ]
then
	echo privide writefile and writestr as args
	exit 1
fi

mkdir -p $(dirname $writefile)

echo $writestr > $writefile
