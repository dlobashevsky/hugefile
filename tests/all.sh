#!/bin/bash

pushd data.out
./clean.sh
popd

echo "Build test:" ; ./build.sh >/dev/null
echo "Stat test:" ; ./stat.sh >/dev/null
echo "Dump test:" ; ./dump.sh >/dev/null
echo "Extract test:" ; ./extract.sh >/dev/null
echo "Extract with filter test:" ; ./extract2.sh >/dev/null
echo "List test:" ; ./list.sh >/dev/null

failed=`fgrep 'ERROR SUMMARY:' *.log | fgrep -v '0 errors from 0 contexts (suppressed: 0 from 0)' | wc -l`
echo "Done," $failed "tests failed"

