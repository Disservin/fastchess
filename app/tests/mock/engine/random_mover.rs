use chess_library_rs::board::Board;
use chess_library_rs::movegen;
use chess_library_rs::movelist::Movelist;
use chess_library_rs::uci;

use rand::rngs::StdRng;
use rand::{Rng, SeedableRng};
use std::io::{self, BufRead, Write};

struct RandomMover {
    rng: StdRng,
    board: Board,
}

impl RandomMover {
    fn new(seed: Option<u64>) -> Self {
        let rng = match seed {
            Some(s) => {
                eprintln!("Using provided seed: {}", s);
                StdRng::seed_from_u64(s)
            }
            None => {
                let s = rand::random::<u64>();
                eprintln!("Using random seed: {}", s);
                StdRng::seed_from_u64(s)
            }
        };

        RandomMover {
            rng,
            board: Board::new(),
        }
    }

    fn random_move(&mut self) -> String {
        let mut moves = Movelist::new();
        movegen::legalmoves(
            &mut moves,
            &self.board,
            movegen::MoveGenType::All,
            movegen::PieceGenType::ALL,
        );

        if moves.is_empty() {
            return "0000".to_string();
        }

        let idx = self.rng.gen_range(0..moves.size());
        let mv = moves[idx];
        uci::move_to_uci(mv, self.board.chess960())
    }

    fn handle_position(&mut self, line: &str) {
        let tokens: Vec<&str> = line.split_whitespace().collect();
        if tokens.len() < 2 {
            return;
        }

        if tokens[1] == "startpos" {
            self.board = Board::new();
            if tokens.len() > 2 && tokens[2] == "moves" {
                for i in 3..tokens.len() {
                    let mv = uci::uci_to_move(&self.board, tokens[i]);
                    if mv.raw() != 0 {
                        self.board.make_move(mv, false);
                    }
                }
            }
        } else if tokens[1] == "fen" {
            let mut fen_parts = Vec::new();
            let mut i = 2;
            while i < tokens.len() && tokens[i] != "moves" {
                fen_parts.push(tokens[i]);
                i += 1;
            }
            let fen = fen_parts.join(" ");
            self.board.set_fen(&fen);

            if i < tokens.len() && tokens[i] == "moves" {
                i += 1;
                while i < tokens.len() {
                    let mv = uci::uci_to_move(&self.board, tokens[i]);
                    if mv.raw() != 0 {
                        self.board.make_move(mv, false);
                    }
                    i += 1;
                }
            }
        }
    }

    fn handle_uci(&self) {
        println!("id name random_move");
        println!("id author fastchess");
        println!("option name Hash type spin default 16 min 1 max 33554432");
        println!("uciok");
    }

    fn handle_isready(&self) {
        println!("readyok");
    }

    fn handle_ucinewgame(&mut self) {
        self.board = Board::new();
    }

    fn handle_go(&mut self) {
        let mv = self.random_move();
        println!("info depth 1 pv {} score cp 0", mv);
        println!("bestmove {}", mv);
    }

    fn run(&mut self) {
        let stdin = io::stdin();
        let stdout = io::stdout();
        let mut stdout_lock = stdout.lock();

        for line in stdin.lock().lines() {
            let line = match line {
                Ok(l) => l,
                Err(_) => break,
            };

            let line = line.trim();

            if line == "quit" {
                break;
            } else if line == "uci" {
                self.handle_uci();
            } else if line == "isready" {
                self.handle_isready();
            } else if line == "ucinewgame" {
                self.handle_ucinewgame();
            } else if line.starts_with("position") {
                self.handle_position(line);
            } else if line.starts_with("go") {
                self.handle_go();
            }

            stdout_lock.flush().unwrap();
        }
    }
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let seed = args.get(1).and_then(|s| s.parse::<u64>().ok());
    let mut mover = RandomMover::new(seed);
    mover.run();
}
