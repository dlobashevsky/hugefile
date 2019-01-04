#!/bin/bash

valgrind --tool=memcheck --leak-check=full --num-callers=24 --show-reachable=yes --track-fds=yes ../src/hugefile -i -d data.out/db |& tee $0.log
