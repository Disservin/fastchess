# fast-chess

[![FastChess](https://github.com/Disservin/fast-chess/actions/workflows/fastchess.yml/badge.svg?branch=master)](https://github.com/Disservin/fast-chess/actions/workflows/fastchess.yml)

fast-chess is a versatile command-line tool designed for running chess engine
tournaments, inspired by [Cute Chess](https://github.com/cutechess/cutechess). Written primarily in C++17, it utilizes [doctest](https://github.com/doctest/doctest) as its testing
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

Current dev versions are available as artifacts from the [CI](https://github.com/Disservin/fast-chess/actions?query=is%3Asuccess+event%3Apush+branch%3Amaster).

### Example usage

Here's an example of how to use fast-chess:

```bash
fast-chess.exe -engine cmd=Engine1.exe name=Engine1 -engine cmd=Engine2.exe
name=Engine2 -each tc=10+0.1 -rounds 200 -repeat -concurrency 4
```

_Note that setting concurrency higher than your CPU core/thread
count may not yield significant performance gains and could potentially have the
opposite effect. Also it is highly encouraged to use an opening book._


## Command line options

See [man](man) for a detailed description of all command line options.

## Contributing

We welcome contributions to fast-chess! Please ensure that any changes you make
are **beneficial** to the development and **pass the CI tests**.

The code follows the Google style and is formatted with clang-format. When
creating pull requests, please format your code accordingly.

To contribute, you'll need a recent GCC compiler that supports C++17 and the
ability to run the Makefile. You can locally test your changes by running
`make -j tests`, followed by executing the `./fast-chess-tests` executable to
verify your changes pass the tests.

You can format the code with clang-format by running `make format`.
After making changes to the man file, you need to run `make update-man`.

## Maintainers

The following people have push access to the repository:

- [Disservin](https://github.com/Disservin)
- [Szil](https://github.com/SzilBalazs)
- [PGG106](https://github.com/PGG106)
