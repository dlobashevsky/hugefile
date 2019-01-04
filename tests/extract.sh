#!/bin/bash

valgrind --tool=memcheck --leak-check=full --num-callers=24 --show-reachable=yes --track-fds=yes ../src/hugefile -x -d data.out/db -o data.out/extract |& tee $0.log
