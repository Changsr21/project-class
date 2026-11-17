#!/usr/bin/env bash
# 一键：编译 -> 运行实验（test.tif -> test1.tif）-> 独立校验 -> 边界测试
set -euo pipefail
cd "$(dirname "$0")"
IN=${1:-data/test.tif}; OUT=${2:-output/test1.tif}; SUB=${3:-10}
mkdir -p output results
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/ImageTest "$IN" "$OUT" "$SUB" | tee results/run_log.txt
python scripts/verify.py "$IN" "$OUT" "$SUB" results | tee results/verify_log.txt
python scripts/edge_test.py build/ImageTest | tee results/edge_test_log.txt
echo "ALL DONE"
