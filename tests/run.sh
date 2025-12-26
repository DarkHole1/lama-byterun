#!/bin/bash

ulimit -Sv 500000

pushd tests

for SOURCE in ./*.lama
do
    BC="${SOURCE%.lama}.bc"
    INPUT="${SOURCE%.lama}.input"
    OUTPUT="${SOURCE%.lama}.output"
    TEST="${SOURCE%.lama}.t1"
    if $LAMAC -b "$SOURCE"
    then
        ../interpreter "$BC" > "$OUTPUT" < "$INPUT" 2>&1
        cmp --silent $TEST $OUTPUT || echo "Different output for $SOURCE"
    else
        echo "File $SOURCE failed to compile"
    fi
done

popd