use std::fs::{File, OpenOptions};
use std::io::Write;
use std::sync::Mutex;

/// Thread-safe file writer with optional CRC32 checksumming.
///
/// Mirrors the C++ `util::FileWriter` which protects writes with a mutex
/// and optionally appends a CRC32 checksum.
pub struct FileWriter {
    inner: Mutex<FileWriterInner>,
}

struct FileWriterInner {
    file: File,
    crc_enabled: bool,
}

impl FileWriter {
    /// Create a new FileWriter.
    ///
    /// - `path`: file path to write to
    /// - `append`: if true, append to existing file; otherwise truncate
    /// - `crc`: if true, append CRC32 checksum after each write
    pub fn new(path: &str, append: bool, crc: bool) -> std::io::Result<Self> {
        let file = OpenOptions::new()
            .create(true)
            .write(true)
            .append(append)
            .truncate(!append)
            .open(path)?;

        Ok(Self {
            inner: Mutex::new(FileWriterInner {
                file,
                crc_enabled: crc,
            }),
        })
    }

    /// Write a string to the file. Thread-safe.
    pub fn write(&self, content: &str) {
        let mut inner = self.inner.lock().unwrap();
        if inner.crc_enabled {
            let checksum = crc32fast::hash(content.as_bytes());
            let _ = write!(inner.file, "{{CRC:{:08X}}}\n{}", checksum, content);
        } else {
            let _ = write!(inner.file, "{}", content);
        }
    }
}
