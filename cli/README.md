# NXP Simtemp CLI

Command-line interface for reading and monitoring temperature data from the NXP simulated temperature sensor driver.

## Features

- Real-time temperature monitoring
- Multiple output formats (table, JSON, CSV)
- Statistics calculation (min, max, average)
- Threshold detection with visual alerts
- Continuous or fixed-sample modes
- Efficient polling-based I/O
- Clean signal handling

## Building
```bash
make
```

## Usage

### Basic Examples
```bash
# Read 10 samples (default)
./simtemp_cli

# Read 20 samples
./simtemp_cli -n 20

# Continuous mode with statistics
./simtemp_cli -c -s

# JSON output
./simtemp_cli -n 50 -f json

# CSV output for data analysis
./simtemp_cli -n 100 -f csv > temperature_data.csv
```

### Options

- `-c, --continuous`: Run until Ctrl+C
- `-n, --samples=N`: Read N samples
- `-i, --interval=MS`: Delay between samples
- `-f, --format=FORMAT`: Output format (table/json/csv)
- `-s, --stats`: Show statistics
- `-v, --verbose`: Verbose output
- `-h, --help`: Show help

## Installation
```bash
sudo make install
```

Then you can run `simtemp_cli` from anywhere.

## Requirements

- NXP simtemp kernel module loaded
- Read permissions on `/dev/simtemp`
