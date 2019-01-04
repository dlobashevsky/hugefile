#!/bin/bash

valgrind --tool=memcheck --leak-check=full --num-callers=24 --show-reachable=yes --track-fds=yes ../src/hugefile -x -d data.out/db -o data.out/extract2 -f '*/deep/*' |& tee $0.log
