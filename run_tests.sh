#!/bin/bash
echo "Building test application..."
rm -rf build/
idf.py -DTEST_MODE=1 build | tee ./logs/test_build.log
echo "Flashing and Monitoring tests..."
idf.py flash monitor -p $1 | tee ./logs/test_flash_monitor.log
