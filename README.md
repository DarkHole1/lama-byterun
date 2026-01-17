# Building and running

```
make
./interpreter [bytecode]
```

for analysis use analyser binary

```
./analyser [bytecode]
```

# Comparsion

```
$ time ./interpreter ./Sort.bc < /dev/null
./interpreter ./Sort.bc < /dev/null  5,61s user 0,03s system 99% cpu 5,657 total
$ time ./tests/lamac.sh -i ./performance/Sort.lama < /dev/null
./tests/lamac.sh -i ./performance/Sort.lama < /dev/null  6,31s user 0,05s system 99% cpu 6,394 total
$ time ./interpreter -v Sort.bc < /dev/null                   
./interpreter -v Sort.bc < /dev/null  2,99s user 0,03s system 99% cpu 3,032 total
$ time ./tests/lamac.sh -s ./performance/Sort.lama < /dev/null
./tests/lamac.sh -s ./performance/Sort.lama < /dev/null  2,36s user 0,03s system 99% cpu 2,406 total
```
