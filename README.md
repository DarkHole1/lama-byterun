# Building and running

```
make
./interpreter [bytecode]
```

# Comparsion

```
$ time ./interpreter ./Sort.bc < /dev/null
./interpreter ./Sort.bc < /dev/null  0,73s user 0,02s system 99% cpu 0,757 total
$ time ./tests/lamac.sh -i ./performance/Sort.lama < /dev/null
./tests/lamac.sh -i ./performance/Sort.lama < /dev/null  5,92s user 0,03s system 99% cpu 5,981 total
```
