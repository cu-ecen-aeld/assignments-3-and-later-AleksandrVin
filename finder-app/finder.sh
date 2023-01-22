#!/bin/sh

filedir=$1
searchstr=$2

if [ -z $filedir ] || [ -z $searchstr ]
then
	echo "filedir and searchstr should be provided as args"
	exit 1
fi

if [ ! -d $filedir ]
then
	echo "filedir is not a dirrectory"
	exit 1
fi

x=$(find $filedir -type f | wc -l)
y=$(grep -r $searchstr $filedir | wc -l)

echo "The number of files are $x and the number of matching lines are $y"
	
