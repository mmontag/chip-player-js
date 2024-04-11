#!/bin/bash
# Unzip all zip files in current directory into their own directories
for f in *.zip; do unzip -unq "$f" -d "$(basename -s .zip "$f")"; done;
