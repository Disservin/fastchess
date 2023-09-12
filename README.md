# fast-chess

[![FastChess](https://github.com/Disservin/fast-chess/actions/workflows/fastchess.yml/badge.svg?branch=master)](https://github.com/Disservin/fast-chess/actions/workflows/fastchess.yml)

fast-chess is a versatile command-line tool designed for running chess engine
tournaments. Written primarily in C++17, it utilizes [doctest](https://github.com/doctest/doctest) as its testing
framework.

With fast-chess, you can effortlessly orchestrate chess tournaments, configure
time controls, and execute matches concurrently for optimal time efficiency.
Extensively tested for high concurrency (supporting up to 250 threads) with
short time controls (0.2+0.002s), it exhibits minimal timeout issues, with only
10 matches out of 20,000 experiencing timeouts.

## What's New

- **Enhanced Cutechess Output**: The Cutechess output support has been refined to
  enhance compatibility with other tools. Simply switch the `-output` option to
  cutechess to enable it.
- **Extended PGN Data**: You can now track nodes, seldepth, and nps (nodes per
  second) in the PGN output. Refer to the `-pgnout` option for detailed
  information.
- **Quick Match Option**: We've introduced a `-quick` option for running quick
  matches between two engines with an EPD book. Specify
  `-quick cmd=ENGINE1 cmd=ENGINE2 book=BOOK` to swiftly initiate a match.

## Quick start

### Building from source

Building Fast-Chess from source is straightforward. Just follow these steps:

1. Clone the repository `git clone https://github.com/Disservin/fast-chess.git`
2. Navigate to the fast-chess directory `cd fast-chess`
3. Build the executable `make -j`

### Download the latest release

Prefer a pre-compiled version?

Download the latest release from our [release page](https://github.com/Disservin/fast-chess/releases).

### Example usage

Here's an example of how to use fast-chess:

```
fast-chess.exe -engine cmd=Engine1.exe name=Engine1 -engine cmd=Engine2.exe
name=Engine2 -each tc=10+0.1 -rounds 200 -repeat -concurrency 4
```

In this command, we've added two engines using `-engine`, specified their names
and executables, and set common options like time control (10 seconds with
0.1-second increment) and hash size using `-each`.

We're running 200 rounds with each engine playing two games per round via
`-rounds` and `-repeat`, respectively.

Finally, we've set the concurrency to 4 threads with `-concurrency`.

_Note that setting concurrency higher than your CPU core/thread
count may not yield significant performance gains and could potentially have the
opposite effect._

## Command line options

fast-chess supports many other command-line options:

- `-config file=NAME [discard=true]`  
   After a CTRL+C event or finishing all games the current config is
  automatically saved. You can restart fast-chess using this argument. This can
  be used to conveniently load engine configurations or/and to resume from a
  previous stop. In case you want to discard the saved game results you can
  specify `discard`. Note that this must be specified after and only after file.
  <br/>
- `-concurrency N`
  <br/>
- `-draw movenumber=NUMBER movecount=COUNT score=SCORE`

  - `NUMBER` - number of moves to make before checking for a draw
  - `COUNT` - number of moves to be below the score threshold to adjudicate a
    draw
  - `SCORE` - score threshold
    <br/>

- `-engine OPTIONS`

  - `OPTIONS` - apply [OPTIONS](#Options) to the next engine.
    <br/>

- `-each OPTIONS`

  - `OPTIONS` - apply [OPTIONS](#Options) to both engines.
    <br/>

- `-site SITE`
  <br/>
- `-event NAME`
  <br/>
- `-games N`

  - `N` - number of games to play
    This should be set to 1 or 2, each round will play n games with, setting this
    higher than 2 does not really make sense.

- `-openings file=NAME format=FORMAT [order=ORDER] [plies=PLIES] [start=START]`
  If no opening book is specified, every game will start from the standard
  position.

  - `NAME` - name of the file containing the openings
  - `FORMAT` - format of the file
    - `epd` - EPD format
    - `pgn` - PGN format
  - `ORDER` - order of the openings
    - `sequential` - sequential order
    - `random` - random order
  - `PLIES` - number of plies to use
  - `START` - starting offset

    <br/>

- `-output format=FORMAT`

  - `FORMAT`
    - `cutechess` - Cute-Chess output format
    - `fastchess` - fast-chess output format
      <br/>

- `-resign movecount=COUNT score=SCORE`

  - `COUNT` - number of moves to make before resigning
  - `SCORE` - score threshold to resign at
    <br/>

- `-repeat`
  This has the same effect as -games 2 and is the default.
  <br/>
- `-rounds N`

  - `N` - number of rounds to play
    <br/>

- `-sprt elo0=ELO0 elo1=ELO1 alpha=ALPHA beta=BETA`
  <br/>
- `-srand SEED`

  - `SEED` - seed for the random number generator

  <br/>

- `-pgnout notation=NOTATION file=FILE [nodes=true] [seldepth=true] [nps=true]`
  - `NOTATION`
    - `san` - Standard Algebraic Notation
    - `lan` - Long Algebraic Notation
    - `uci` - Universal Chess Interface
  - `FILE` - defaults to fast-chess.pgn
  - `nodes` - defaults to `false`, track node count
  - `seldepth` - defaults to `false`, track seldepth
  - `nps` - defaults to `false`, track nps
- `-log file=NAME`
  <br/>
- `-quick cmd=ENGINE1 cmd=ENGINE2 book=BOOK`  
   This is a shortcut for
  ````
  -engine cmd=ENGINE1 -engine cmd=ENGINE1 -engine cmd=ENGINE2 -each tc=10+0.1 -rounds 25000 -repeat -concurrency max - 2 -openings file=BOOK format=epd order=random -draw movecount=8 score=8 movenumber=30```
  ````

### Options

- `cmd=COMMAND`
- `args="ARGS"`
  If you want to pass multiple arguments, you can use `args="ARG1 ARG2 ARG3"`.
  Please keep in mind that double quotes inside the string must be escaped. 
  i.e. `args="single words \"multiple words\""` -> your engine will receive
  ```
  single
  words
  multiple words
  ```
- `[name=NAME]`
- `[tc=TC]`
  TC uses the same format as Cute-Chess. For example 10+0.1 would be 10
  seconds with 100 millisecond increments.
- `[option.name=VALUE]`
  This can be used to set engine options. Note that the engine must support
  the option. For example, to set the hash size to 128MB, you can use
  `option.Hash=128 option`. Note that the option name is case sensitive.

## Contributing

We welcome contributions to fast-chess! Please ensure that any changes you make
are **beneficial** to the development and **pass the CI tests**.

The code follows the Google style and is formatted with clang-format. When
creating pull requests, please format your code accordingly.

To contribute, you'll need a recent GCC compiler that supports C++17 and the
ability to run the Makefile. You can locally test your changes by running
`make -j tests`, followed by executing the `./fast-chess-tests` executable to
verify your changes pass the tests.

## Maintainers

The following people have push access to the repository:

- [Disservin](https://github.com/Disservin)
- [Szil](https://github.com/SzilBalazs)
- [PGG106](https://github.com/PGG106)
