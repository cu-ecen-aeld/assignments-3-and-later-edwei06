#!/bin/bash

# Check if exactly two arguments are provided
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <filesdir> <searchstr>"
    exit 1
fi

filesdir="$1"
searchstr="$2"

# Verify that the provided directory exists
if [ ! -d "$filesdir" ]; then
    echo "Error: $filesdir is not a directory"
    exit 1
fi

# Count the number of files (including in subdirectories)
num_files=$(find "$filesdir" -type f | wc -l)

# Count the number of lines matching the search string
# Redirect grep errors to /dev/null in case some files are not searchable.
num_lines=$(grep -R "$searchstr" "$filesdir" 2>/dev/null | wc -l)

echo "The number of files are $num_files and the number of matching lines are $num_lines"

