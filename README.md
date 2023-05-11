# Fast-Chess

Fast-Chess is a command-line tool designed for creating chess engine tournaments. It is mostly written in C++17 and uses [doctest](https://github.com/doctest/doctest) as its testing framework.

With Fast-Chess, you can easily create and run chess tournaments with two engines,
set time controls, and run matches in parallel to save time.
The POSIX implementation has been extensively tested for high concurrency (250) with short time controls (0.2+0.002) and has demonstrated minimal timeout issues, with only 10 matches out of 20,000 timing out.

## Quick start

### Building from source

Building Fast-Chess from source is simple and straightforward. Just follow these steps:

1. Clone the repository `git clone https://github.com/Disservin/fast-chess.git`
2. Navigate to the fast-chess directory `cd fast-chess`
3. Build the executable `make -j`

### Download the latest release

If you prefer to download the latest release, you can do so from our [release page](https://github.com/Disservin/fast-chess/releases).
Just choose the version that fits your needs, download it, and you're good to go!

### Example usage

Here's an example of how to use Fast-Chess:

```
fast-chess.exe -engine cmd=Engine1.exe name=Engine1 -engine cmd=Engine2.exe name=Engine2
-each tc=10+0.1 -rounds 200 -repeat -concurrency 4
```

In this command we add two engines with `-engine`. We've specified the name of the
engines and set the executables with `name=NAME cmd=CMD`. Options following an `-each` flag will be
used for both engines, like a time control (in this case 10s with 0.1s increment per move) or a Hash size.
We're also running 200 rounds with each engine playing two games per round using the `-rounds` and `-repeat` options,
respectively. Finally, we've set the concurrency to 4 threads using the `-concurrency` option.
Note that setting the concurrency higher than your CPU's core count may not result in any significant performance gains
and may even have the opposite effect.

## Command line options

Fast-Chess supports many other command-line options:

```
Options:
  -config file=NAME discard=true
    After a CTRL+C event or finishing all games the current config is automatically saved.
    You can restart fast-chess using this argument. This can be used to conveniently load
    engine configurations or/and to resume from a previous stop. In case you want to discard
    the saved game results you can specify `discard`. Note that this must be specified after and
    only after file.
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
    FORMAT can be "epd" (or experimental pgn) and order "sequential" or"random", start specifies the offset.
    If plies is set to zero, as it is by default, the entire opening line will be used.
    FORMAT "pgn" is currently experimental.
  -option.OPTION=VALUE
  -output format=FORMAT
    FORMAT can be "cutechess" or "fastchess" (default) change this if you have scripts
    that only parse cutechess like output. This is experimental as of now.
  -resign movecount=COUNT score=SCORE
  -repeat
    This has the same effect as -games 2 and is the default.
  -rounds N
  -sprt elo0=ELO0 elo1=ELO1 alpha=ALPHA beta=BETA
  -srand SEED
  -pgnout notation=NOTATION file=FILE
    NOTATION defaults to san, alternatively you can choose lan or uci, default file output is
    fast-chess.pgn.
  -log file=NAME

Engine;
  name=NAME
  cmd=COMMAND
  plies=N
  tc=TC
    TC uses the same format as Cute-Chess. For example 10+0.1 would be 10 seconds with 100 millisecond increments.

```

## Contributing

We welcome contributions to Fast-Chess! Please ensure that any changes you make are **beneficial** to the development and **pass the CI tests**.

The entire code is formatted with **clang-format** using the **Google style**. If you create pull requests, please make sure that the code is formatted using this style.

To contribute, you need a recent GCC compiler that supports C++17, as well as a way to run the Makefile. **You can run tests locally** by navigating to the /tests directory and running make -j.
Then, run the ./fast-chess-tests executable to ensure your changes pass the tests.
