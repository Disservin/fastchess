# Fast-Chess

Fast-Chess is a command-line tool designed for creating chess engine
tournaments. It is mostly written in C++17 and uses
[doctest](https://github.com/doctest/doctest) as its testing framework.

With Fast-Chess, you can easily create and run chess tournaments with two
engines, set time controls, and run matches in parallel to save time. The POSIX
implementation has been extensively tested for high concurrency (250 threads)
with short time controls (0.2+0.002s) and has demonstrated minimal timeout
issues, with only 10 matches out of 20,000 timing out.

## News

- The cutechess output support has been refined and should now be more
  compatible with other tools. Change the `-output` option to `cutechess` to
  enable it.
- You can also track nodes, seldepth and nps in the pgn output read the
  `-pgnout` option for more information.
- There is now a `-quick` option that allows you to quickly run a match between
  two engines with am epd book. You only need to specify
  `-quick cmd=ENGINE1 cmd=ENGINE2 book=BOOK` and you're good to go.

## Quick start

### Building from source

Building Fast-Chess from source is simple and straightforward. Just follow these
steps:

1. Clone the repository `git clone https://github.com/Disservin/fast-chess.git`
2. Navigate to the fast-chess directory `cd fast-chess`
3. Build the executable `make -j`

### Download the latest release

If you prefer to download the latest release, you can do so from our
[release page](https://github.com/Disservin/fast-chess/releases). Just choose
the version that fits your needs, download it, and you're good to go!

### Example usage

Here's an example of how to use Fast-Chess:

```
fast-chess.exe -engine cmd=Engine1.exe name=Engine1 -engine cmd=Engine2.exe
name=Engine2 -each tc=10+0.1 -rounds 200 -repeat -concurrency 4
```

In this command we add two engines with `-engine`. We've specified the name of
the engines and set the executables with `name=NAME cmd=CMD`. Options following
an `-each` flag will be used for both engines, like a time control (in this case
10s with 0.1s increment per move) or a Hash size. We're also running 200 rounds
with each engine playing two games per round using the `-rounds` and `-repeat`
options, respectively. Finally, we've set the concurrency to 4 threads using the
`-concurrency` option. Note that setting the concurrency higher than your CPU's
core count may not result in any significant performance gains and may even have
the opposite effect.

## Command line options

Fast-Chess supports many other command-line options:

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
    - `fastchess` - Fast-Chess output format
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
- `[name=NAME]`
- `[tc=TC]`
  TC uses the same format as Cute-Chess. For example 10+0.1 would be 10
  seconds with 100 millisecond increments.
- `[option.name=VALUE]`
  This can be used to set engine options. Note that the engine must support
  the option. For example, to set the hash size to 128MB, you can use
  `option.Hash=128 option`. Note that the option name is case sensitive.

## Contributing

We welcome contributions to Fast-Chess! Please ensure that any changes you make
are **beneficial** to the development and **pass the CI tests**.

The entire code is formatted with **clang-format** using the **Google style**.
If you create pull requests, please make sure that the code is formatted using
this style.

To contribute, you need a recent GCC compiler that supports C++17, as well as a
way to run the Makefile. **You can run tests locally** by running
`make -j tests`. Then, run the ./fast-chess-tests executable to ensure your
changes pass the tests.

## Maintainers

The following people have push access to the repository:

- [Disservin](https://github.com/Disservin)
- [Szil](https://github.com/SzilBalazs)
- [PGG106](https://github.com/PGG106)
