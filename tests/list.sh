#!/bin/bash

valgrind --tool=memcheck --leak-check=full --num-callers=24 --show-reachable=yes --track-fds=yes ../src/hugefile -l -d data.out/db -o data.out/file.list |& tee $0.log
