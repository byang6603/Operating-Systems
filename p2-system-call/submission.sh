#!/bin/bash

# Navigate to the solution directory and clean up
cd solution
make clean
cd ..

# Check if slipdays.txt exists in the current directory
if [ -f slipdays.txt ]; then
    echo "slipdays.txt found. Including it in the archive."
    tar -zcf p2.tar ./solution README.md slipdays.txt
else
    echo "slipdays.txt not found. Proceeding without it."
    tar -zcf p2.tar ./solution README.md
fi
