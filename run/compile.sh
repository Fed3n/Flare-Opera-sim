#!/bin/bash

#get relative path
PARENT_PATH=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )
cd "$PARENT_PATH"

cd ../src/opera ; make clean ; make ; cd datacenter/ ; make clean ; make
cd "$PARENT_PATH"
cd ../src/clos ; make clean ; make ; cd datacenter/ ; make clean ; make
