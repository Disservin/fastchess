# Fast-Chess

Fast-Chess is a cli tool for creating tournaments between chess engines. Written in mostly C++17 and using [doctest](https://github.com/doctest/doctest) as a testing framework. Compilation can be easily done with a recent C++ Compiler and using the Makefile in /src.
The POSIX implementation was tested with very high concurrency (250) and very short TC (0.2+0.002) and had very little timeout problems, only 10 matches out of 20000 timed out.

## Example usage

```
fast-chess.exe -engine cmd=Engine1.exe name=Engine1 -engine cmd=Engine2.exe name=Engine2 -each tc=10+0.1 -rounds 200 -repeat -concurrency 4
```

## Command line options

```
Options:
  -concurrency N
  -draw movenumber=NUMBER movecount=COUNT score=SCORE
  -engine OPTIONS
  -each OPTIONS
    apply OPTIONS to both engines.
  -site SITE
  -event NAME
  -games N
    This should be set to 1 or 2, each round will play n games with, setting this higher than 2 does not really make sense.
  -openings file=NAME format=FORMAT order=ORDER plies=PLIES start=START
    format can be "epd" and order "sequential" or "random", start specifies the offset.
  -option.OPTION=VALUE
  -resign movecount=COUNT score=SCORE
  -rounds N
  -repeat
    This has the same effect as -games 2 and is the default.
  -ratinginterval N
    print the current elo estimation every N rounds.
  -sprt elo0=ELO0 elo1=ELO1 alpha=ALPHA beta=BETA
  -srand SEED
  -pgnout notation=NOTATION file=FILE tracknodes
    NOTATION defaults to san, alternatively you can choose lan, default file output is
    fast-chess.pgn, tracknodes adds the node count to the pgn.
  -log file=NAME

Engine;
  name=NAME
  cmd=COMMAND
  plies=N
  tc=TC
  
```

## Contributing

__Contributions__ are very welcome.
The entire code is formatted with clang-format using the __Microsoft Style__.  
If you create pull requests make sure the code is formatted using that style.  
Changes have to pass CI and be benificial to the development.  

To __contribute__ you need a recent __g++ compiler__ with support of C++17. As well as a way to run Makefile.  
__Tests__ can be run locally by going into the /tests directory and running `make -j` there. Then run the `./fast-chess-tests` executable.
