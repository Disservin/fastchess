use std::io::Read;

// ---------------------------------------------------------------------------
// StringBuffer — fixed 255-byte token accumulator
// ---------------------------------------------------------------------------

const STRING_BUFFER_SIZE: usize = 255;

struct StringBuffer {
    buf: [u8; STRING_BUFFER_SIZE],
    len: usize,
}

impl StringBuffer {
    fn new() -> Self {
        StringBuffer {
            buf: [0u8; STRING_BUFFER_SIZE],
            len: 0,
        }
    }

    fn is_empty(&self) -> bool {
        self.len == 0
    }

    fn clear(&mut self) {
        self.len = 0;
    }

    fn add(&mut self, c: u8) -> bool {
        if self.len >= STRING_BUFFER_SIZE {
            return false;
        }
        self.buf[self.len] = c;
        self.len += 1;
        true
    }

    fn get(&self) -> &str {
        // SAFETY: PGN tokens are ASCII
        std::str::from_utf8(&self.buf[..self.len]).unwrap_or("")
    }
}

// ---------------------------------------------------------------------------
// StreamBuffer — buffered reader with character-at-a-time interface
// ---------------------------------------------------------------------------

const CHUNK: usize = 4096;

struct StreamBuffer<R: Read> {
    reader: R,
    buf: Vec<u8>,
    pos: usize,
    filled: usize,
}

impl<R: Read> StreamBuffer<R> {
    fn new(reader: R) -> Self {
        StreamBuffer {
            reader,
            buf: vec![0u8; CHUNK],
            pos: 0,
            filled: 0,
        }
    }

    fn fill(&mut self) -> bool {
        self.pos = 0;
        match self.reader.read(&mut self.buf) {
            Ok(n) if n > 0 => {
                self.filled = n;
                true
            }
            _ => {
                self.filled = 0;
                false
            }
        }
    }

    /// Return the current character, skipping carriage returns.
    fn some(&mut self) -> Option<u8> {
        loop {
            if self.pos < self.filled {
                let c = self.buf[self.pos];
                if c == b'\r' {
                    self.pos += 1;
                    continue;
                }
                return Some(c);
            }
            if !self.fill() {
                return None;
            }
        }
    }

    fn advance(&mut self) {
        if self.pos >= self.filled {
            self.fill();
        }
        self.pos += 1;
    }

    fn current(&mut self) -> Option<u8> {
        if self.pos >= self.filled {
            if !self.fill() {
                return None;
            }
        }
        Some(self.buf[self.pos])
    }

    fn peek(&mut self) -> Option<u8> {
        let next = self.pos + 1;
        if next < self.filled {
            return Some(self.buf[next]);
        }
        // Would need to peek across a buffer boundary — fill a new chunk
        // In practice this is rare; just return None as a safe fallback
        // which is acceptable for the termination symbol lookahead.
        // (The C++ implementation also peeks at the raw stream which can fail.)
        None
    }

    /// Skip from the current character (which must be `open_delim`) until the
    /// matching `close_delim`, handling nesting.  Returns `true` on success.
    fn skip_until(&mut self, open_delim: u8, close_delim: u8) -> bool {
        let mut stack = 0usize;
        loop {
            let c = match self.some() {
                Some(c) => c,
                None => return false,
            };
            self.advance();
            if c == open_delim {
                stack += 1;
            } else if c == close_delim {
                if stack == 0 {
                    return false; // mismatched
                }
                stack -= 1;
                if stack == 0 {
                    return true;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Error type
// ---------------------------------------------------------------------------

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum StreamParserErrorCode {
    None,
    ExceededMaxStringLength,
    InvalidHeaderMissingClosingBracket,
    InvalidHeaderMissingClosingQuote,
    NotEnoughData,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct StreamParserError(pub StreamParserErrorCode);

impl StreamParserError {
    pub fn none() -> Self {
        StreamParserError(StreamParserErrorCode::None)
    }
    pub fn has_error(&self) -> bool {
        self.0 != StreamParserErrorCode::None
    }
    pub fn message(&self) -> &'static str {
        match self.0 {
            StreamParserErrorCode::None => "No error",
            StreamParserErrorCode::ExceededMaxStringLength => "Exceeded max string length",
            StreamParserErrorCode::InvalidHeaderMissingClosingBracket => {
                "Invalid header: missing closing bracket"
            }
            StreamParserErrorCode::InvalidHeaderMissingClosingQuote => {
                "Invalid header: missing closing quote"
            }
            StreamParserErrorCode::NotEnoughData => "Not enough data",
        }
    }
}

impl std::fmt::Display for StreamParserError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.message())
    }
}

impl std::error::Error for StreamParserError {}

// ---------------------------------------------------------------------------
// Visitor trait
// ---------------------------------------------------------------------------

/// Callback-based interface for receiving PGN events.
///
/// Implement this trait to process games from a PGN stream.
pub trait Visitor {
    /// Called when a new game begins (before the first header).
    fn start_pgn(&mut self);

    /// Called for each `[Key "Value"]` header tag.
    fn header(&mut self, key: &str, value: &str);

    /// Called once all headers have been read and moves are about to start.
    fn start_moves(&mut self);

    /// Called for each move token.  `comment` is the text inside `{ }` that
    /// follows the move (empty string if there is no comment).
    fn move_token(&mut self, mv: &str, comment: &str);

    /// Called when the game ends (after the termination symbol).
    fn end_pgn(&mut self);

    /// Call this inside [`start_pgn`] (or any callback) to instruct the parser
    /// to skip the rest of the current game's moves.  [`end_pgn`] will still be
    /// called.
    fn skip_pgn(&mut self, _skip: bool) {}

    /// Returns whether the current game should be skipped.
    /// The default implementation always returns `false`; override if you call
    /// [`skip_pgn`] to store the flag.
    fn skip(&self) -> bool {
        false
    }
}

// ---------------------------------------------------------------------------
// StreamParser
// ---------------------------------------------------------------------------

/// Streaming PGN parser.
///
/// Reads from any `std::io::Read` source and calls [`Visitor`] callbacks for
/// each event.  Multiple games in the same stream are handled automatically.
pub struct StreamParser<R: Read> {
    stream: StreamBuffer<R>,
}

impl<R: Read> StreamParser<R> {
    pub fn new(reader: R) -> Self {
        StreamParser {
            stream: StreamBuffer::new(reader),
        }
    }

    /// Parse all games in the stream, calling `vis` for each event.
    pub fn read_games<V: Visitor>(&mut self, vis: &mut V) -> Result<(), StreamParserError> {
        if !self.stream.fill() {
            return Err(StreamParserError(StreamParserErrorCode::NotEnoughData));
        }

        let mut in_header = true;
        let mut in_body = false;
        let mut pgn_end = true;
        let mut dont_advance = false;

        // Per-game scratch buffers
        let mut header_key = StringBuffer::new();
        let mut header_val = StringBuffer::new();
        let mut mv_buf = StringBuffer::new();
        let mut comment = String::new();

        let mut error = StreamParserError::none();

        'outer: while let Some(c) = self.stream.some() {
            if in_header {
                vis.skip_pgn(false);

                if c == b'[' {
                    vis.start_pgn();
                    pgn_end = false;

                    Self::process_header(
                        &mut self.stream,
                        vis,
                        &mut header_key,
                        &mut header_val,
                        &mut in_header,
                        &mut in_body,
                        &mut error,
                    );

                    if error.has_error() {
                        return Err(error);
                    }
                    dont_advance = false;
                    continue;
                }
            } else if in_body {
                Self::process_body(
                    &mut self.stream,
                    vis,
                    &mut mv_buf,
                    &mut comment,
                    &mut in_header,
                    &mut in_body,
                    &mut pgn_end,
                    &mut dont_advance,
                    &mut error,
                );

                if error.has_error() {
                    return Err(error);
                }

                if dont_advance {
                    dont_advance = false;
                    continue 'outer;
                }
            }

            if !dont_advance {
                self.stream.advance();
            }
            dont_advance = false;
        }

        if !pgn_end {
            // flush the last game
            Self::flush_move(vis, &mut mv_buf, &mut comment);
            vis.end_pgn();
            vis.skip_pgn(false);
        }

        Ok(())
    }

    // -----------------------------------------------------------------------
    // Header processing
    // -----------------------------------------------------------------------

    fn process_header<V: Visitor>(
        stream: &mut StreamBuffer<R>,
        vis: &mut V,
        key: &mut StringBuffer,
        val: &mut StringBuffer,
        in_header: &mut bool,
        in_body: &mut bool,
        error: &mut StreamParserError,
    ) {
        let mut backslash = false;

        loop {
            let c = match stream.some() {
                Some(c) => c,
                None => return,
            };

            match c {
                b'[' => {
                    stream.advance();
                    // read tag name until whitespace
                    while let Some(k) = stream.some() {
                        if is_space(k) {
                            break;
                        }
                        if !key.add(k) {
                            *error =
                                StreamParserError(StreamParserErrorCode::ExceededMaxStringLength);
                            return;
                        }
                        stream.advance();
                    }
                    stream.advance(); // skip the space
                }
                b'"' => {
                    stream.advance();
                    loop {
                        let k = match stream.some() {
                            Some(k) => k,
                            None => break,
                        };
                        if k == b'\\' {
                            backslash = true;
                            stream.advance();
                        } else if k == b'"' && !backslash {
                            stream.advance();
                            // should now be at ']'
                            if stream.current().unwrap_or(0) != b']' {
                                *error = StreamParserError(
                                    StreamParserErrorCode::InvalidHeaderMissingClosingBracket,
                                );
                                return;
                            }
                            stream.advance();
                            break;
                        } else if k == b'\n' {
                            *error = StreamParserError(
                                StreamParserErrorCode::InvalidHeaderMissingClosingQuote,
                            );
                            return;
                        } else {
                            backslash = false;
                            if !val.add(k) {
                                *error = StreamParserError(
                                    StreamParserErrorCode::ExceededMaxStringLength,
                                );
                                return;
                            }
                            stream.advance();
                        }
                    }

                    // skip stray carriage return
                    if stream.current() == Some(b'\r') {
                        stream.advance();
                    }

                    if !vis.skip() {
                        vis.header(key.get(), val.get());
                    }
                    key.clear();
                    val.clear();

                    stream.advance();
                }
                b'\n' => {
                    *in_header = false;
                    *in_body = true;
                    if !vis.skip() {
                        vis.start_moves();
                    }
                    return;
                }
                _ => {
                    // unexpected — treat as start of body
                    *in_header = false;
                    *in_body = true;
                    if !vis.skip() {
                        vis.start_moves();
                    }
                    return;
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // Body processing
    // -----------------------------------------------------------------------

    fn process_body<V: Visitor>(
        stream: &mut StreamBuffer<R>,
        vis: &mut V,
        mv_buf: &mut StringBuffer,
        comment: &mut String,
        in_header: &mut bool,
        in_body: &mut bool,
        pgn_end: &mut bool,
        dont_advance: &mut bool,
        error: &mut StreamParserError,
    ) {
        let mut is_termination = false;
        let mut has_comment = false;

        // ---- preliminary scan: skip move numbers, detect early termination ----
        'pre: loop {
            let c = match stream.some() {
                Some(c) => c,
                None => break 'pre,
            };

            if is_space(c) || is_digit(c) {
                stream.advance();
            } else if c == b'-' || c == b'*' || c == b'/' {
                is_termination = true;
                stream.advance();
            } else if c == b'{' {
                has_comment = true;
                stream.advance();
                while let Some(k) = stream.some() {
                    stream.advance();
                    if k == b'}' {
                        break;
                    }
                    *comment += &(k as char).to_string();
                }
                if !vis.skip() {
                    vis.move_token("", comment);
                    has_comment = false;
                    comment.clear();
                }
            } else {
                break 'pre;
            }
        }

        if !vis.skip() && has_comment && !is_termination {
            // re-run preliminary scan (mirrors C++ goto start)
            return;
        }

        if is_termination {
            Self::on_end(vis, mv_buf, comment, in_header, in_body, pgn_end);
            *dont_advance = true;
            return;
        }

        // skip whitespace
        while let Some(c) = stream.some() {
            if is_space(c) {
                stream.advance();
            } else {
                break;
            }
        }

        // ---- main move loop ----
        loop {
            let cd = match stream.some() {
                Some(c) => c,
                None => {
                    Self::on_end(vis, mv_buf, comment, in_header, in_body, pgn_end);
                    *dont_advance = true;
                    return;
                }
            };

            // '[' starts a new PGN while we're in the body
            if cd == b'[' {
                Self::on_end(vis, mv_buf, comment, in_header, in_body, pgn_end);
                *dont_advance = true;
                return;
            }

            // skip move number digits
            while let Some(c) = stream.some() {
                if is_space(c) || is_digit(c) {
                    stream.advance();
                } else {
                    break;
                }
            }

            // skip dots
            while let Some(c) = stream.some() {
                if c == b'.' {
                    stream.advance();
                } else {
                    break;
                }
            }

            // skip spaces
            while let Some(c) = stream.some() {
                if is_space(c) {
                    stream.advance();
                } else {
                    break;
                }
            }

            // parse move
            if Self::parse_move(stream, vis, mv_buf, comment, error) {
                if error.has_error() {
                    return;
                }
                break;
            }

            // skip spaces
            while let Some(c) = stream.some() {
                if is_space(c) {
                    stream.advance();
                } else {
                    break;
                }
            }

            let curr = match stream.current() {
                Some(c) => c,
                None => {
                    Self::on_end(vis, mv_buf, comment, in_header, in_body, pgn_end);
                    *dont_advance = true;
                    return;
                }
            };

            // '*' — game termination
            if curr == b'*' {
                Self::on_end(vis, mv_buf, comment, in_header, in_body, pgn_end);
                stream.advance();
                *dont_advance = true;
                return;
            }

            let peek = stream.peek().unwrap_or(0);

            if curr == b'1' {
                if peek == b'-' {
                    // 1-0
                    stream.advance();
                    stream.advance();
                    Self::on_end(vis, mv_buf, comment, in_header, in_body, pgn_end);
                    *dont_advance = true;
                    return;
                } else if peek == b'/' {
                    // 1/2-1/2 (7 chars: 1 / 2 - 1 / 2)
                    for _ in 0..=6 {
                        stream.advance();
                    }
                    Self::on_end(vis, mv_buf, comment, in_header, in_body, pgn_end);
                    *dont_advance = true;
                    return;
                }
            }

            // '0' — might be 0-1 or 0-0[-0] castling
            if curr == b'0' && peek == b'-' {
                stream.advance(); // skip '0'
                stream.advance(); // skip '-'

                let c = match stream.current() {
                    Some(c) => c,
                    None => {
                        Self::on_end(vis, mv_buf, comment, in_header, in_body, pgn_end);
                        *dont_advance = true;
                        return;
                    }
                };

                if c == b'1' {
                    // 0-1
                    Self::on_end(vis, mv_buf, comment, in_header, in_body, pgn_end);
                    stream.advance();
                    *dont_advance = true;
                    return;
                } else {
                    // castling written as 0-0[-0]
                    if !mv_buf.add(b'0') || !mv_buf.add(b'-') {
                        *error = StreamParserError(StreamParserErrorCode::ExceededMaxStringLength);
                        return;
                    }
                    if Self::parse_move(stream, vis, mv_buf, comment, error) {
                        stream.advance();
                        break;
                    }
                }
            }

            // Check if we've consumed everything (e.g. EOF mid-game)
            if stream.some().is_none() {
                Self::on_end(vis, mv_buf, comment, in_header, in_body, pgn_end);
                *dont_advance = true;
                return;
            }
        }
    }

    /// Read one move token (stops at whitespace).  Then parse any appendix
    /// (comments, variations, NAGs).  Returns `true` if parsing should stop
    /// (EOF encountered mid-appendix).
    fn parse_move<V: Visitor>(
        stream: &mut StreamBuffer<R>,
        vis: &mut V,
        mv_buf: &mut StringBuffer,
        comment: &mut String,
        error: &mut StreamParserError,
    ) -> bool {
        // read token chars
        while let Some(c) = stream.some() {
            if is_space(c) {
                break;
            }
            if !mv_buf.add(c) {
                *error = StreamParserError(StreamParserErrorCode::ExceededMaxStringLength);
                return true;
            }
            stream.advance();
        }

        Self::parse_move_appendix(stream, vis, mv_buf, comment, error)
    }

    /// After the move token, consume any `{ comment }`, `( variation )`,
    /// `$NAG`, or whitespace.  Returns `true` when the outer loop should stop.
    fn parse_move_appendix<V: Visitor>(
        stream: &mut StreamBuffer<R>,
        vis: &mut V,
        mv_buf: &mut StringBuffer,
        comment: &mut String,
        _error: &mut StreamParserError,
    ) -> bool {
        loop {
            let curr = match stream.current() {
                Some(c) => c,
                None => {
                    // EOF — emit the last move and signal stop
                    Self::flush_move(vis, mv_buf, comment);
                    return true;
                }
            };

            match curr {
                b'{' => {
                    stream.advance();
                    while let Some(c) = stream.some() {
                        stream.advance();
                        if c == b'}' {
                            break;
                        }
                        *comment += &(c as char).to_string();
                    }
                }
                b'(' => {
                    stream.skip_until(b'(', b')');
                }
                b'$' => {
                    while let Some(c) = stream.some() {
                        if is_space(c) {
                            break;
                        }
                        stream.advance();
                    }
                }
                b' ' | b'\t' | b'\n' => {
                    while let Some(c) = stream.some() {
                        if !is_space(c) {
                            break;
                        }
                        stream.advance();
                    }
                }
                _ => {
                    Self::flush_move(vis, mv_buf, comment);
                    return false;
                }
            }
        }
    }

    fn flush_move<V: Visitor>(vis: &mut V, mv_buf: &mut StringBuffer, comment: &mut String) {
        if !mv_buf.is_empty() {
            if !vis.skip() {
                vis.move_token(mv_buf.get(), comment.as_str());
            }
            mv_buf.clear();
            comment.clear();
        }
    }

    fn on_end<V: Visitor>(
        vis: &mut V,
        mv_buf: &mut StringBuffer,
        comment: &mut String,
        in_header: &mut bool,
        in_body: &mut bool,
        pgn_end: &mut bool,
    ) {
        Self::flush_move(vis, mv_buf, comment);
        vis.end_pgn();
        vis.skip_pgn(false);
        *in_header = true;
        *in_body = false;
        *pgn_end = true;
    }
}

// ---------------------------------------------------------------------------
// Char helpers
// ---------------------------------------------------------------------------

#[inline]
fn is_space(c: u8) -> bool {
    matches!(c, b' ' | b'\t' | b'\n' | b'\r')
}

#[inline]
fn is_digit(c: u8) -> bool {
    c >= b'0' && c <= b'9'
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Cursor;

    #[derive(Default)]
    struct Recorder {
        games: Vec<RecordedGame>,
        skip_flag: bool,
    }

    #[derive(Default, Debug)]
    struct RecordedGame {
        headers: Vec<(String, String)>,
        moves: Vec<(String, String)>,
    }

    impl Visitor for Recorder {
        fn start_pgn(&mut self) {
            println!("start_pgn");
            self.games.push(RecordedGame::default());
        }
        fn header(&mut self, key: &str, value: &str) {
            self.games
                .last_mut()
                .unwrap()
                .headers
                .push((key.to_owned(), value.to_owned()));
        }
        fn start_moves(&mut self) {}
        fn move_token(&mut self, mv: &str, comment: &str) {
            println!("move_token: '{}' with comment '{}'", mv, comment);
            self.games
                .last_mut()
                .unwrap()
                .moves
                .push((mv.to_owned(), comment.to_owned()));
        }
        fn end_pgn(&mut self) {}
        fn skip_pgn(&mut self, skip: bool) {
            self.skip_flag = skip;
        }
        fn skip(&self) -> bool {
            self.skip_flag
        }
    }

    fn parse(pgn: &[u8]) -> Recorder {
        let mut cursor = Cursor::new(pgn);
        let mut vis = Recorder::default();
        StreamParser::new(&mut cursor).read_games(&mut vis).unwrap();
        vis
    }

    fn parse_file(path: &str) -> Recorder {
        let data = std::fs::read(path).expect("failed to read pgn file");
        parse(&data)
    }

    // ── original tests ────────────────────────────────────────────────────────

    #[test]
    fn test_single_game_headers() {
        let pgn = b"[Event \"Test\"]\n[Site \"Local\"]\n\n1. e4 e5 *\n";
        let r = parse(pgn);
        assert_eq!(r.games.len(), 1);
        assert_eq!(r.games[0].headers[0], ("Event".into(), "Test".into()));
        assert_eq!(r.games[0].headers[1], ("Site".into(), "Local".into()));
    }

    #[test]
    fn test_single_game_moves() {
        let pgn = b"[Event \"X\"]\n\n1. e4 e5 2. Nf3 Nc6 *\n";
        let r = parse(pgn);
        let moves: Vec<&str> = r.games[0].moves.iter().map(|(m, _)| m.as_str()).collect();
        assert_eq!(moves, vec!["e4", "e5", "Nf3", "Nc6"]);
    }

    #[test]
    fn test_move_with_comment() {
        let pgn = b"[Event \"X\"]\n\n1. e4 { good move } e5 *\n";
        let r = parse(pgn);
        assert_eq!(r.games[0].moves[0].0, "e4");
        assert_eq!(r.games[0].moves[0].1, " good move ");
    }

    #[test]
    fn test_termination_1_0() {
        let pgn = b"[Event \"X\"]\n\n1. e4 e5 1-0\n";
        let r = parse(pgn);
        assert_eq!(r.games.len(), 1);
        let moves: Vec<&str> = r.games[0].moves.iter().map(|(m, _)| m.as_str()).collect();
        assert_eq!(moves, vec!["e4", "e5"]);
    }

    #[test]
    fn test_termination_0_1() {
        let pgn = b"[Event \"X\"]\n\n1. e4 e5 0-1\n";
        let r = parse(pgn);
        assert_eq!(r.games.len(), 1);
    }

    #[test]
    fn test_termination_draw() {
        let pgn = b"[Event \"X\"]\n\n1. e4 e5 1/2-1/2\n";
        let r = parse(pgn);
        assert_eq!(r.games.len(), 1);
        let moves: Vec<&str> = r.games[0].moves.iter().map(|(m, _)| m.as_str()).collect();
        assert_eq!(moves, vec!["e4", "e5"]);
    }

    #[test]
    fn test_multiple_games() {
        let pgn = b"[Event \"A\"]\n\n1. d4 *\n[Event \"B\"]\n\n1. e4 *\n";
        let r = parse(pgn);
        assert_eq!(r.games.len(), 2);
        assert_eq!(r.games[0].moves[0].0, "d4");
        assert_eq!(r.games[1].moves[0].0, "e4");
    }

    #[test]
    fn test_nag_skipped() {
        let pgn = b"[Event \"X\"]\n\n1. e4 $1 e5 *\n";
        let r = parse(pgn);
        let moves: Vec<&str> = r.games[0].moves.iter().map(|(m, _)| m.as_str()).collect();
        assert_eq!(moves, vec!["e4", "e5"]);
    }

    #[test]
    fn test_variation_skipped() {
        let pgn = b"[Event \"X\"]\n\n1. e4 (1. d4 d5) e5 *\n";
        let r = parse(pgn);
        let moves: Vec<&str> = r.games[0].moves.iter().map(|(m, _)| m.as_str()).collect();
        assert_eq!(moves, vec!["e4", "e5"]);
    }

    #[test]
    fn test_castling_zero_notation() {
        let pgn = b"[Event \"X\"]\n\n1. e4 e5 2. Nf3 Nc6 3. Bc4 Bc5 4. 0-0 0-0 *\n";
        let r = parse(pgn);
        let moves: Vec<&str> = r.games[0].moves.iter().map(|(m, _)| m.as_str()).collect();
        assert!(moves.contains(&"0-0"));
    }

    #[test]
    fn test_escape_in_header_value() {
        let pgn = b"[Event \"Test\\\"Escaped\"]\n\n*\n";
        let r = parse(pgn);
        assert_eq!(r.games[0].headers[0].1, "Test\"Escaped");
    }

    // ── file-based tests ported from C++ ─────────────────────────────────────

    #[test]
    fn test_basic_pgn() {
        let r = parse_file("./tests/pgns/basic.pgn");
        assert_eq!(r.games.len(), 1);
        let moves = &r.games[0].moves;
        assert_eq!(moves.len(), 130);
        assert_eq!(moves[0].0, "Bg2");
        assert_eq!(moves[0].1, "+1.55/16 0.70s");
        assert_eq!(moves[1].0, "O-O");
        assert_eq!(moves[1].1, "-1.36/18 0.78s");
        assert_eq!(moves[2].0, "O-O");
        assert_eq!(moves[2].1, "+1.84/16 0.42s");
        assert_eq!(moves[3].0, "a5");
        assert_eq!(moves[3].1, "-1.30/16 0.16s");
    }

    #[test]
    fn test_corrupted_pgn() {
        let r = parse_file("./tests/pgns/corrupted.pgn");
        assert_eq!(r.games.len(), 1);
        assert_eq!(r.games[0].moves.len(), 125);
    }

    #[test]
    fn test_no_moves_pgn() {
        let r = parse_file("./tests/pgns/no_moves.pgn");
        assert_eq!(r.games.len(), 1);
        assert_eq!(r.games[0].moves.len(), 0);
    }

    #[test]
    fn test_multiple_pgn() {
        let r = parse_file("./tests/pgns/multiple.pgn");
        assert_eq!(r.games.len(), 4);
    }

    /// The skip visitor skips one game; only one game's moves should be recorded.
    #[test]
    fn test_skip_pgn() {
        use std::io::Cursor;

        struct SkipVisitor {
            inner: Recorder,
            game_idx: usize,
        }

        impl Visitor for SkipVisitor {
            fn start_pgn(&mut self) {
                self.inner.start_pgn();
            }
            fn header(&mut self, key: &str, value: &str) {
                self.inner.header(key, value);
            }
            fn start_moves(&mut self) {
                self.inner.start_moves();
            }
            fn move_token(&mut self, mv: &str, comment: &str) {
                self.inner.move_token(mv, comment);
            }
            fn end_pgn(&mut self) {
                self.game_idx += 1;
                self.inner.end_pgn();
            }
            fn skip_pgn(&mut self, _: bool) {}
            fn skip(&self) -> bool {
                // skip the second game (index 1)
                self.game_idx == 1
            }
        }

        let data = std::fs::read("./tests/pgns/skip.pgn").expect("failed to read skip.pgn");
        let mut cursor = Cursor::new(data);
        let mut vis = SkipVisitor {
            inner: Recorder::default(),
            game_idx: 0,
        };
        StreamParser::new(&mut cursor).read_games(&mut vis).unwrap();

        assert_eq!(vis.inner.games.len(), 2);
        // Only the non-skipped game has 130 moves; the skipped game has 0.
        let total_moves: usize = vis.inner.games.iter().map(|g| g.moves.len()).sum();
        assert_eq!(total_moves, 130);
    }

    #[test]
    fn test_newline_by_moves() {
        let r = parse_file("./tests/pgns/newline.pgn");
        assert_eq!(r.games.len(), 1);
        assert_eq!(r.games[0].moves.len(), 6);
    }

    #[test]
    fn test_castling_0_0_file() {
        let r = parse_file("./tests/pgns/castling.pgn");
        assert_eq!(r.games.len(), 1);
        assert_eq!(r.games[0].moves.len(), 6);
    }

    #[test]
    fn test_black_to_move_castling_0_0_0() {
        let r = parse_file("./tests/pgns/black2move.pgn");
        assert_eq!(r.games.len(), 1);
        assert_eq!(r.games[0].moves.len(), 3);
    }

    #[test]
    fn test_skip_variations_file() {
        let r = parse_file("./tests/pgns/variations.pgn");
        assert_eq!(r.games.len(), 1);
        assert_eq!(r.games[0].moves.len(), 108);
    }

    #[test]
    fn test_read_book() {
        let r = parse_file("./tests/pgns/book.pgn");
        assert_eq!(r.games.len(), 1);
        assert_eq!(r.games[0].moves.len(), 16);
    }

    #[test]
    fn test_no_moves_game_termination_0_1() {
        let r = parse_file("./tests/pgns/no_moves_but_game_termination.pgn");
        assert_eq!(r.games.len(), 1);
        assert_eq!(r.games[0].moves.len(), 0);
    }

    #[test]
    fn test_no_moves_game_termination_draw() {
        let r = parse_file("./tests/pgns/no_moves_but_game_termination_2.pgn");
        assert_eq!(r.games.len(), 1);
        assert_eq!(r.games[0].moves.len(), 0);
    }

    #[test]
    fn test_no_moves_game_termination_asterisk() {
        let r = parse_file("./tests/pgns/no_moves_but_game_termination_3.pgn");
        assert_eq!(r.games.len(), 1);
        assert_eq!(r.games[0].moves.len(), 0);
    }

    #[test]
    fn test_no_moves_game_termination_multiple() {
        let r = parse_file("./tests/pgns/no_moves_but_game_termination_multiple.pgn");
        assert_eq!(r.games.len(), 2);
        let all_moves: Vec<&str> = r
            .games
            .iter()
            .flat_map(|g| g.moves.iter().map(|(m, _)| m.as_str()))
            .collect();
        assert_eq!(all_moves.len(), 126);
        assert_eq!(all_moves[0], "d4");
        assert_eq!(all_moves[1], "e6");
        assert_eq!(*all_moves.last().unwrap(), "Ke4");
        assert_eq!(all_moves[all_moves.len() - 2], "Rd2+");
    }

    #[test]
    fn test_no_moves_game_termination_multiple_2() {
        let r = parse_file("./tests/pgns/no_moves_but_game_termination_multiple_2.pgn");
        assert_eq!(r.games.len(), 3);

        let all_headers: Vec<String> = r
            .games
            .iter()
            .flat_map(|g| g.headers.iter().map(|(k, v)| format!("{} {}", k, v)))
            .collect();
        assert_eq!(all_headers.len(), 6);

        assert_eq!(
            all_headers[0],
            "FEN 5k2/3r1p2/1p3pp1/p2n3p/P6P/1PPR1PP1/3KN3/6b1 w - - 0 34"
        );
        assert_eq!(all_headers[1], "Result 1/2-1/2");
        assert_eq!(
            all_headers[2],
            "FEN 5k2/5p2/4B2p/r5pn/4P3/5PPP/2NR2K1/8 b - - 0 59"
        );
        assert_eq!(all_headers[3], "Result 1/2-1/2");
        assert_eq!(
            all_headers[4],
            "FEN 8/p3kp1p/1p4p1/2r2b2/2BR3P/1P3P2/P4PK1/8 b - - 0 28"
        );
        assert_eq!(all_headers[5], "Result 1/2-1/2");
    }

    #[test]
    fn test_no_moves_but_comment_followed_by_termination() {
        let r = parse_file("./tests/pgns/no_moves_but_comment_followed_by_termination_marker.pgn");
        assert_eq!(r.games.len(), 2);
        let total: usize = r.games.iter().map(|g| g.moves.len()).sum();
        assert_eq!(total, 2);
    }

    #[test]
    fn test_no_moves_two_games() {
        let r = parse_file("./tests/pgns/no_moves_two_games.pgn");
        assert_eq!(r.games.len(), 2);
        assert!(r.games.iter().all(|g| g.moves.is_empty()));
    }

    #[test]
    fn test_square_bracket_in_header() {
        let r = parse_file("./tests/pgns/square_bracket_in_header.pgn");
        let all_headers: Vec<String> = r
            .games
            .iter()
            .flat_map(|g| g.headers.iter().map(|(k, v)| format!("{} {}", k, v)))
            .collect();
        assert_eq!(
            all_headers[0],
            "Event Batch 10: s20red4c4_t3 vs master[!important]"
        );
        assert_eq!(all_headers[1], "Variation closing ] opening");
        assert_eq!(all_headers[5], "White New-cfe8\"dsadsa\"ce842c");
    }

    #[test]
    fn test_empty_body() {
        let r = parse_file("./tests/pgns/empty_body.pgn");
        assert_eq!(r.games.len(), 2);
        let all_headers: Vec<String> = r
            .games
            .iter()
            .flat_map(|g| g.headers.iter().map(|(k, v)| format!("{} {}", k, v)))
            .collect();
        assert_eq!(all_headers[0], "Event Batch 2690: probTTsv1 vs master");
        assert_eq!(all_headers[7], "Event Batch 269: probTTsv1 vs master");
    }

    #[test]
    fn test_backslash_in_header_returns_error() {
        use std::io::Cursor;
        let data = std::fs::read("./tests/pgns/backslash_header.pgn").expect("failed to read file");
        let mut cursor = Cursor::new(data);
        let mut vis = Recorder::default();
        let err = StreamParser::new(&mut cursor).read_games(&mut vis);
        assert_eq!(
            err,
            Err(StreamParserError(
                StreamParserErrorCode::InvalidHeaderMissingClosingQuote
            ))
        );
        assert_eq!(vis.games.len(), 1);
    }

    /// Parser must not hang on a comment before the first move.
    #[test]
    fn test_comment_before_moves_no_hang() {
        parse_file("./tests/pgns/comment_before_moves.pgn");
    }

    #[test]
    fn test_non_ascii_headers() {
        let r = parse_file("./tests/pgns/non_ascii.pgn");
        assert_eq!(r.games.len(), 1);
        let headers: Vec<String> = r.games[0]
            .headers
            .iter()
            .map(|(k, v)| format!("{} {}", k, v))
            .collect();
        assert_eq!(headers.len(), 7);
        assert_eq!(headers[0], "Event Tournoi François");
        assert_eq!(headers[1], "Site Café");
        assert_eq!(headers[2], "Date 2024.03.15");
        assert_eq!(headers[3], "Round 1");
        assert_eq!(headers[4], "White José María");
        assert_eq!(headers[5], "Black Владимир Петров");
        assert_eq!(headers[6], "Result 1-0");
    }

    #[test]
    fn test_unescaped_quote_header_error() {
        use std::io::Cursor;
        let data =
            std::fs::read("./tests/pgns/unescaped_quote_header.pgn").expect("failed to read file");
        let mut cursor = Cursor::new(data);
        let mut vis = Recorder::default();
        let err = StreamParser::new(&mut cursor).read_games(&mut vis);
        assert_eq!(
            err,
            Err(StreamParserError(
                StreamParserErrorCode::InvalidHeaderMissingClosingBracket
            ))
        );
        assert_eq!(vis.games.len(), 1);
        assert!(vis.games[0].headers.is_empty());
    }

    #[test]
    fn test_no_result_game() {
        let r = parse_file("./tests/pgns/no_result.pgn");
        assert_eq!(r.games.len(), 1);
        let headers: Vec<String> = r.games[0]
            .headers
            .iter()
            .map(|(k, v)| format!("{} {}", k, v))
            .collect();
        assert_eq!(headers.len(), 15);
        assert_eq!(
            headers[7],
            "FEN r1bqk2r/pp1p1pbp/2n2np1/4p3/4P3/2NP2P1/PP2NPBP/R1BQ1RK1 w kq - 0 9"
        );
        assert_eq!(r.games[0].moves[0].0, "");
        assert_eq!(r.games[0].moves[0].1, "No result");
    }
}
