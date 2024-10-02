#!/bin/bash

set -x

# Compile fastchess
make clean
make -j USE_CUTE=true

# Create a directory for testing
mkdir -p cutechess-testing
cp fastchess cutechess-testing
cd cutechess-testing

# Get a copy of SF
if [[ ! -f ./sf_2 ]]; then
   echo "Downloading SF 16.1"
   wget --no-verbose https://github.com/official-stockfish/Stockfish/releases/download/sf_16.1/stockfish-ubuntu-x86-64-avx2.tar
   tar -xvf stockfish-ubuntu-x86-64-avx2.tar
   cp stockfish/stockfish-ubuntu-x86-64-avx2 ./sf_1
   cp stockfish/stockfish-ubuntu-x86-64-avx2 ./sf_2
fi

# Get a copy of cutechess
if [[ ! -f ./cutechess-cli ]]; then
   echo "Downloading cutechess-cli"
   wget --no-verbose https://github.com/ppigazzini/static-cutechess-cli/releases/download/v1.3.1/cutechess-cli-linux-64bit.zip
   unzip cutechess-cli-linux-64bit.zip
fi

# Get a suitable book without move counters
if [[ ! -f ./UHO_4060_v2.epd ]]; then
   echo "Downloading book"
   wget --no-verbose https://github.com/official-stockfish/books/raw/master/UHO_4060_v2.epd.zip
   unzip UHO_4060_v2.epd.zip
fi

# Actual testing
rm -f *-out.pgn

for binary in ./cutechess-cli ./fastchess
do

(

$binary         -recover -repeat -games 2 -rounds 100\
                -pgnout $binary-out.pgn\
                -srand $RANDOM  -resign movecount=3 score=600 -draw movenumber=34 movecount=8 score=20\
                -variant standard -concurrency 4 -openings file=UHO_4060_v2.epd format=epd order=sequential \
                -engine name=sf_1 tc=inf depth=6 cmd=./sf_1 dir=.\
                -engine name=sf_2 tc=inf depth=8 cmd=./sf_2 dir=.\
                -each proto=uci option.Threads=1

) | grep "Finished game" | sort -n -k 3 > $binary-out.finished

done

diff ./cutechess-cli-out.finished ./fastchess-out.finished
if [[ $? -ne 0 ]]; then
   echo "Finished games have different termination status"
   exit 1
fi

grep PlyCount cutechess-cli-out.pgn | grep -o "[0-9]*" | sort -n > cutechess-cli-out.plycount
grep PlyCount fastchess-out.pgn | grep -o "[0-9]*" | sort -n > fastchess-out.plycount
diff ./cutechess-cli-out.plycount ./fastchess-out.plycount
if [[ $? -ne 0 ]]; then
   echo "Not all games have the same PlyCount, adjudication might differ"
   exit 1
fi


cd ..
