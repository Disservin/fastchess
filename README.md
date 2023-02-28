# FastChess

FastChess is a cli tool for creating tournaments between chess engines. Written in mostly C++17 and using [doctest](https://github.com/doctest/doctest) as a testing framework. Compilation can be easily done with a recent C++ Compiler and using the Makefile in /src.
The POSIX implementation was tested with very high concurrency (250) and very short TC (0.2+0.002) and had very little timeout problems, only 10 matches out of 20000 timed out.

## Example usage

```
fast-chess.exe -engine cmd=Engine1.exe name=Engine1 -engine cmd=Engine2.exe name=Engine2 -each tc=10+0.1 -rounds 200 -repeat -concurrency 4
```

## Command line options

```
Options:
  -engine OPTIONS
  -each OPTIONS
    apply OPTIONS to both engines
  -concurrency N
  -draw movenumber=NUMBER movecount=COUNT score=SCORE
  -resign movecount=COUNT score=SCORE
  -rounds N
  -games N
    This should be set to 1 or 2, each round will play n games with, setting this higher than 2 does not really make sense.
  -repeat
    This has the same effect as -games 2.
  -ratinginterval N
    print elo estimation every n
  -openings file=NAME format=FORMAT order=ORDER plies=PLIES start=START
    format can be "epd" and order "sequential" or "random", start specifies the offset
  -event NAME
  -option.OPTION=VALUE

Engine;
  name=NAME
  cmd=COMMAND
  plies=N
  
```
