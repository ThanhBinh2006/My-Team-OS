#!/bin/bash

# ═══════════════════════════════════════════════════════════
#  Test Runner with per-testcase timeout & module filtering
# ═══════════════════════════════════════════════════════════

# Per-testcase timeout in seconds (default: 30s, override with TESTCASE_TIMEOUT env var)
TIMEOUT=${TESTCASE_TIMEOUT:-90}

CONFIG_FILE="test_config.conf"

# ─── Read config ───
if [ -f "$CONFIG_FILE" ]; then
  # Source only the MODULES line (strip comments)
  MODULES=$(grep -E '^MODULES=' "$CONFIG_FILE" | head -1 | cut -d'=' -f2 | tr -d ' ')
  echo "📋 Config loaded: MODULES=$MODULES"
else
  echo "⚠  Config file '$CONFIG_FILE' not found, defaulting to MODULES=all"
  MODULES="all"
fi

# ─── Classify a testcase name into its module group ───
# Returns: syscall, scheduler, memory, paging, synchronization, or unknown
classify_testcase() {
  local name="$1"

  # Order matters: more specific patterns first

  # Paging tests: contain "paging" in the name
  if echo "$name" | grep -qiE 'paging'; then
    echo "paging"
    return
  fi

  # Syscall tests: contain "syscall" or "_sc" (but not "sched")
  if echo "$name" | grep -qiE '(syscall|_sc($|[^h]))'; then
    echo "syscall"
    return
  fi

  # Memory tests: contain "memory"
  if echo "$name" | grep -qiE '(memory)'; then
    echo "memory"
    return
  fi

  # Scheduler tests: contain "sched" or "mlq"
  if echo "$name" | grep -qiE '(sched|mlq)'; then
    echo "scheduler"
    return
  fi

  # Synchronization tests: contain "sync"
  if echo "$name" | grep -qiE '(sync)'; then
    echo "synchronization"
    return
  fi

  echo "unknown"
}

# ─── Check if a module is enabled in the config ───
is_module_enabled() {
  local module="$1"

  # "all" enables everything
  if [ "$MODULES" = "all" ]; then
    return 0
  fi

  # Check if the module appears in the comma-separated list
  echo ",$MODULES," | grep -qi ",$module,"
}

# ─── Clean output files only (preserve expected/ subdirectory) ───
mkdir -p output
find output -maxdepth 1 -name '*.output' -type f -delete
echo "🧹 Cleaned output/*.output files (preserved output/expected/)"

TOTAL=0
RAN=0
SKIPPED=0
PASSED=0
FAILED=0
TIMEOUT_COUNT=0
FAILED_LIST=""
TIMEOUT_LIST=""
SKIPPED_LIST=""

echo ""
echo "════════════════════════════════════════"
echo "  TEST RUNNER — Module Filter + Timeout"
echo "════════════════════════════════════════"
echo "  Enabled modules: $MODULES"
echo "  Timeout per test: ${TIMEOUT}s"
echo "════════════════════════════════════════"
echo ""

for file in input/*; do
  if [ -f "$file" ]; then
    base=$(basename "$file")
    TOTAL=$((TOTAL + 1))

    # Classify and filter
    module=$(classify_testcase "$base")

    if ! is_module_enabled "$module"; then
      SKIPPED=$((SKIPPED + 1))
      SKIPPED_LIST="${SKIPPED_LIST}\n  - ${base} [${module}]"
      continue
    fi

    RAN=$((RAN + 1))
    echo "────────────────────────────────────────"
    echo "▶ [$module] $base (timeout: ${TIMEOUT}s)"

    # Run with timeout; capture exit code
    # stdbuf -oL forces line-buffered stdout so output is flushed before kill
    # --signal=TERM gives process a chance to flush; --kill-after=2s escalates to KILL
    timeout --signal=TERM --kill-after=2s "${TIMEOUT}s" stdbuf -oL ./os "$base" > "output/${base}.output" 2>&1
    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 137 ] || [ $EXIT_CODE -eq 124 ]; then
      # 124 = timeout sent TERM, 137 = KILL (128+9)
      echo "  ⏰ TIMEOUT — exceeded ${TIMEOUT}s, killed"
      TIMEOUT_COUNT=$((TIMEOUT_COUNT + 1))
      TIMEOUT_LIST="${TIMEOUT_LIST}\n  - ${base} [${module}]"
    elif [ $EXIT_CODE -ne 0 ]; then
      echo "  ✗ CRASHED — exit code $EXIT_CODE"
      FAILED=$((FAILED + 1))
      FAILED_LIST="${FAILED_LIST}\n  - ${base} [${module}] (exit: ${EXIT_CODE})"
    else
      echo "  ✓ COMPLETED"
      PASSED=$((PASSED + 1))
    fi
  fi
done

echo ""
echo "════════════════════════════════════════"
echo "  TEST EXECUTION SUMMARY"
echo "════════════════════════════════════════"
echo "  Total testcases: $TOTAL"
echo "  ────────────────────────────────"
echo "  Ran:       $RAN"
echo "    Passed:    $PASSED"
echo "    Crashed:   $FAILED"
echo "    Timeout:   $TIMEOUT_COUNT"
echo "  ────────────────────────────────"
echo "  Skipped:   $SKIPPED  (module not enabled)"
echo "════════════════════════════════════════"

if [ -n "$FAILED_LIST" ]; then
  echo ""
  echo "  Crashed tests:"
  echo -e "$FAILED_LIST"
fi

if [ -n "$TIMEOUT_LIST" ]; then
  echo ""
  echo "  Timed-out tests:"
  echo -e "$TIMEOUT_LIST"
fi

if [ -n "$SKIPPED_LIST" ]; then
  echo ""
  echo "  Skipped tests:"
  echo -e "$SKIPPED_LIST"
fi

echo ""
if [ $FAILED -gt 0 ] || [ $TIMEOUT_COUNT -gt 0 ]; then
  echo "⚠  Some tests did not complete successfully."
else
  echo "✅ All executed tests completed successfully."
fi

# Always exit 0 so the workflow continues to the diff step
exit 0
