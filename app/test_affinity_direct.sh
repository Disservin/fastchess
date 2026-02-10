#!/bin/bash
cat > /tmp/test_affinity.rs << 'RUST_EOF'
use fastchess::affinity::AffinityManager;

fn main() {
    let mgr = AffinityManager::new(true, &[], 1);
    println!("AffinityManager created with is_active: {}", mgr.is_active());
    
    match mgr.consume() {
        Ok(guard) => {
            let cpus = guard.cpus();
            println!("Consumed CPUs: {:?}", cpus);
        }
        Err(e) => {
            println!("Failed to consume: {}", e);
        }
    }
}
RUST_EOF

rustc --edition 2021 -L target/release/deps --extern fastchess=target/release/libfastchess.rlib /tmp/test_affinity.rs -o /tmp/test_affinity 2>&1
/tmp/test_affinity
