FAST-CHESS (alpha-0.6.3)            General Commands Manual             FAST-CHESS (alpha-0.6.3)

NAME
       fast-chess - a command-line tool for managing chess games with engines

SYNOPSIS
       fast-chess [OPTIONS]

DESCRIPTION 
        Fast-chess is a command-line tool designed to manage and orchestrate chess games between engines.
        It provides a range of options to configure game settings,
        engine parameters, concurrency, and output formats.

OPTIONS
        The following options are available:

        -config file=NAME [discard=true]
            Save or load engine configurations to resume games from previous sessions.
            Use 'discard=true' to discard saved game results.

        -concurrency N
            Set the concurrency level to N.

        -rounds N
            Play N rounds of games.

        -games N
            Play N games in each round. This should be set to 1 or 2; each round will play N games.
            Setting this higher than 2 does not provide meaningful results.

        -each OPTIONS
            Apply specified OPTIONS to both engines.

        -draw movenumber=NUMBER movecount=COUNT score=SCORE
            Set conditions for a draw based on the number of moves, move count, and score threshold.
              
                NUMBER - the number of moves before checking for a draw.
                COUNT  - the number of moves below the score threshold to adjudicate a draw.
                SCORE  - the score threshold for a draw.

        -resign movecount=COUNT score=SCORE
            Configure when the engine should resign based on move count and score threshold.

                COUNT - the number of moves to make before resigning.
                SCORE - the score threshold to resign at.

        -openings file=NAME format=FORMAT [order=ORDER] [plies=PLIES] [start=START]
            Specify an opening book file and its format for game starting positions.

                FORMAT - the file format, either epd or pgn.
                ORDER  - the order of moves to be played, either random or sequential.
                PLIES  - the number of plies to use.
                START  - the starting offset.

        -output format=FORMAT
            Choose the output format for game results (cutechess or fastchess).

        -pgnout notation=NOTATION file=FILE [nodes=true] [seldepth=true] [nps=true]
            Export games in PGN format with specified notations and optional tracking of nodes, seldepth, and nps.

                NOTATION
                    san - Standard Algebraic Notation
                    lan - Long Algebraic Notation
                    uci - Universal Chess Interface

                FILE - defaults to fast-chess.pgn
                nodes - defaults to false, track node count.
                seldepth - defaults to false, track seldepth.
                nps - defaults to false, track nps.

        -quick cmd=ENGINE1 cmd=ENGINE2 book=BOOK
            Shortcut for predefined game settings involving two engines, time controls,
            rounds, and concurrency.

        -sprt elo0=ELO0 elo1=ELO1 alpha=ALPHA beta=BETA
            Set parameters for the Sequential Probability Ratio Test (SPRT).

        -srand SEED
            Set the seed for the random number generator.

        -log file=NAME level=LEVEL
            Specify a log file with a specific log level (warn, info, err, fatal).

        -no-affinity
            Disable thread affinity for running multiple instances of fast-chess in parallel.

        -engine OPTIONS
            Apply specified OPTIONS to the next engine.

            cmd=COMMAND
                Specify the engine command.

            name=NAME
                Set the engine name. Must be unique.

            [args="ARGS"]
                If you want to pass multiple arguments, use args="ARG1 ARG2 ARG3".
                Please note that double quotes inside the string must be escaped, 
                e.g., args="single words \"multiple words\"" -> your engine will receive

                single
                words
                multiple words

            [tc=TC] 
                TC uses the same format as Cute-Chess. For example,
                10+0.1 would be 10 seconds with 100 millisecond increments.

            [option.name=VALUE]
                This can be used to set engine options. Note that the engine must support the option.
                For example, to set the hash size to 128MB, use option.Hash=128.


EXAMPLES
        To start a game between two engines using specific configurations:
            $ fast-chess -engine cmd=Engine1.exe name=Engine1 -engine cmd=Engine2.exe \
              name=Engine2 -each tc=10+0.1 -rounds 200 -repeat -concurrency 4

        To resume a game from a saved configuration:
            $ fast-chess -config file=saved_game.conf

AUTHOR
        fast-chess was written by Disservin, Szil and PGG106.

REPORTING BUGS
        Report any bugs to https://github.com/Disservin/fast-chess/issues.

COPYRIGHT
        This software is licensed under the MIT license. See the LICENSE file for details.

VERSION
        fast-chess version alpha-0.6.3