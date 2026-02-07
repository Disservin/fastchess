#!/bin/bash
# Test script to verify CPU affinity is being set

# Create a simple dummy UCI engine that sleeps for testing
cat > /tmp/dummy_engine.sh << 'EOF'
#!/bin/bash
echo "id name DummyEngine"
echo "id author Test"
echo "uciok"

while IFS= read -r line; do
    case "$line" in
        "uci")
            echo "id name DummyEngine"
            echo "id author Test"
            echo "uciok"
            ;;
        "isready")
            echo "readyok"
            ;;
        "ucinewgame")
            echo "readyok"
            ;;
        "quit")
            exit 0
            ;;
        "go"*)
            # Sleep to simulate thinking
            sleep 0.1
            echo "bestmove e2e4"
            ;;
    esac
done
EOF

chmod +x /tmp/dummy_engine.sh

echo "Running fastchess with affinity enabled..."
cd rust

# Run tournament with affinity in background
timeout 10s cargo run --release -- \
  -engine cmd=/tmp/dummy_engine.sh name=engine1 \
  -engine cmd=/tmp/dummy_engine.sh name=engine2 \
  -each tc=0.01+0.001 \
  -rounds 1 -games 2 -concurrency 2 -affinity \
  -log all 2>&1 | tee /tmp/fastchess_affinity_test.log &

FASTCHESS_PID=$!

# Wait a moment for engines to start
sleep 2

# Check CPU affinity of running dummy engines
echo ""
echo "=== Checking CPU affinity of running engines ==="
pgrep -f dummy_engine.sh | while read pid; do
    if [ -f /proc/$pid/status ]; then
        echo "Engine PID $pid:"
        grep -E "Cpus_allowed_list" /proc/$pid/status || echo "  (affinity not set or not available)"
    fi
done

# Wait for fastchess to finish
wait $FASTCHESS_PID

echo ""
echo "=== Test completed. Check above for CPU affinity settings ==="
echo "Expected: Each engine should have a limited CPU list (e.g., Cpus_allowed_list: 0 or 1)"
echo "If all CPUs are allowed (e.g., 0-15), affinity is NOT working."
