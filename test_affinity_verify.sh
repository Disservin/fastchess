#!/bin/bash
# Enhanced test to check process affinity

# Create a dummy engine that sleeps longer for inspection
cat > /tmp/slow_dummy_engine.sh << 'EOF'
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
            # Sleep longer to allow inspection
            sleep 5
            echo "bestmove e2e4"
            ;;
    esac
done
EOF

chmod +x /tmp/slow_dummy_engine.sh

echo "Running fastchess with affinity enabled..."
cd rust

# Run tournament with affinity in background
./target/release/fastchess \
  -engine cmd=/tmp/slow_dummy_engine.sh name=engine1 \
  -engine cmd=/tmp/slow_dummy_engine.sh name=engine2 \
  -each tc=1+0.1 \
  -rounds 1 -games 2 -concurrency 2 -use-affinity 2>&1 &

FASTCHESS_PID=$!

# Wait for engines to start
sleep 1

# Check CPU affinity of running dummy engines
echo ""
echo "=== Checking CPU affinity of running engines ==="
pgrep -f slow_dummy_engine.sh | while read pid; do
    if [ -f /proc/$pid/status ]; then
        echo "Engine PID $pid:"
        grep -E "Cpus_allowed_list" /proc/$pid/status
    fi
done

# Wait for fastchess to finish
wait $FASTCHESS_PID

echo ""
echo "=== Test completed ==="
