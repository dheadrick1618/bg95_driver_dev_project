#!/bin/bash
echo "Building main application..."
rm -rf build/
idf.py build | tee ./logs/main_build.log
echo "Flashing and Monitoring main application..."
idf.py flash monitor -p $1 | tee ./logs/main_flash_monitor.log
