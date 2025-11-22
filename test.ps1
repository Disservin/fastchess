&".\fastchess.exe" `
    -engine dir"=G:/Github/Stockfish/src/" name=b1 `
    -engine dir="G:/Github/Stockfish/src/" name=b2 `
    -each cmd="stockfish.exe" tc=1+0.01 `
    -recover `
    -concurrency 10 `
    -openings file=openings.epd `
    -rounds 5000 `
    -repeat `
    -pgnout "sprt-2582.pgn" `
    -log file=log.txt engine=true level=trace engine=true `
| tee last-sprt.txt
