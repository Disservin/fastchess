# FastChess

FastChess is a cli tool for creating tournaments between chess engines.

## Example usage

```
fast-chess.exe -engine cmd=Engine1.exe name=Engine1 -engine cmd=Engine2.exe name=Engine2 -each tc=10+0.1 -rounds 200 -repeat -concurrency 4
```

## Command line options

```
Options:
  -engine OPTIONS
  -each OPTIONS
  -concurrency N
  -draw movenumber=NUMBER movecount=COUNT score=SCORE
  -resign movecount=COUNT score=SCORE
  -rounds N
  -games N
    This should be set to 1 or 2, each round will play n games with, setting this higher than 2 does not really make sense.
  -repeat
    This has the same effect as -games 2.
  -ratinginterval N
  -openings file=FILE format=FORMAT order=ORDER plies=PLIES start=START
  -pgnout FILE
  -event
  -option.OPTION=VALUE

Engine;
  name=NAME
  cmd=COMMAND
  plies=N
  
```
