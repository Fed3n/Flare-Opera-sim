#!/bin/bash

#checks difference between files in output/ and raw/ with same name matching pattern given as argument
name=$1

ls --recursive output | grep $name | while read -r line ; do
    lines=$(wc -l < output/"$line")
    echo "Comparing output/$line and raw/$line in first $lines lines..."
    diff -q output/$line <(head -n $lines raw/$line) > /dev/null
    if [[ $? == "0" ]]
    then
        echo "Files are the same."
    else
        echo "!!! Files are different!"
    fi
    echo ""
done
