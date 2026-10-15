#!/usr/bin/env bash
# Download one day of Binance spot trades from data.binance.vision, check Binance's published
# sha256, and write:
#   data/binance_trades.txt  the CSV (trade_id,price,qty,quote_qty,time,is_buyer_maker,
#                            is_best_match), for the column parser
#   data/binance.txt         the price column, one per line, for the per-number parsers
# Usage: scripts/fetch_binance.sh [SYMBOL] [YYYY-MM-DD]   (default BTCUSDT 2026-10-14)
set -euo pipefail
cd "$(dirname "$0")/.."

symbol=${1:-BTCUSDT}
day=${2:-2026-10-14}
name="$symbol-trades-$day"
url="https://data.binance.vision/data/spot/daily/trades/$symbol/$name.zip"
dir=data/binance # downloads stay in their own directory
mkdir -p "$dir"

if [ ! -f "$dir/$name.zip" ]; then
    curl -fsSL --retry 3 -o "$dir/$name.zip.CHECKSUM" "$url.CHECKSUM"
    curl -fsSL --retry 3 -o "$dir/$name.zip.part" "$url"
    mv "$dir/$name.zip.part" "$dir/$name.zip"
fi
(cd "$dir" && sha256sum -c --quiet "$name.zip.CHECKSUM")
unzip -p "$dir/$name.zip" "$name.csv" >data/binance_trades.txt
cut -d, -f2 data/binance_trades.txt >data/binance.txt
echo "data/binance_trades.txt: $(wc -l <data/binance_trades.txt) trades ($symbol $day)"
