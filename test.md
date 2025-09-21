~/fastchess -engine cmd=stockfish name=stockfish-nosyzygy \
-engine cmd=stockfish name=stockfish-syzygy option.SyzygyPath=/dev/shm/3-6men/ \
-concurrency 100 -rounds 40000 -repeat \
-openings file=UHO_XXL_+0.90_+1.19.epd format=epd \
-each proto=uci option.Threads=1 option.Hash=16 tc=10+0.1 dir=~/Stockfish/src/



~/fastchess -engine cmd=stockfish-15 name=stockfish-nosyzygy \
-engine cmd=stockfish-15 name=stockfish-syzygy option.SyzygyPath=/dev/shm/3-6men/ \
-concurrency 100 -rounds 40000 -repeat \
-openings file=UHO_XXL_+0.90_+1.19.epd format=epd \
-each proto=uci option.Threads=1 option.Hash=16 tc=10+0.1 dir=~/Stockfish/src/
