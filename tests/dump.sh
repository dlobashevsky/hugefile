#!/bin/bash

valgrind --tool=memcheck --leak-check=full --num-callers=24 --show-reachable=yes --track-fds=yes ../src/hugefile -p -d data.out/db -o data.out/dump |& tee $0.log
