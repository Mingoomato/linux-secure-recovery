#!/bin/sh
set -eu

fuzzer=$1
seed_dir=$2
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir "$tmp/corpus"
cp -R "$seed_dir"/. "$tmp/corpus/"
"$fuzzer" -runs=10000 -max_len=4096 "$tmp/corpus"
