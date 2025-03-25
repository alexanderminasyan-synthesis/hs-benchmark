#!/bin/bash

if [ $# -ne 3 ]; then
    echo "Usage: $0 <number_of_files> <file_size_in_bytes> <directory_path>"
    exit 1
fi

num_files=$1
file_size=$2
dir_path=$3

mkdir -p "$dir_path"
> file_list.txt

for i in $(seq 0 $((num_files-1)))
do
    dd if=/dev/random of="$dir_path/file_$i" bs=$file_size count=1
    echo "$dir_path/file_$i" >> file_list.txt
done

echo "Created $num_files files of size $file_size bytes in $dir_path and generated file list"
