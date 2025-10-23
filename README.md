# NXP Simulated Temperature Sensor Driver

A Linux kernel driver that simulates a temperature sensor for testing purposes.

**Developer:** Edgar Valencia
**Date:** October 2024

---

## What is this?

This project creates a simulated temperature sensor in Linux. It has two main parts:

1. **Kernel Driver** - Creates `/dev/simtemp` device that generates temperature data
2. **CLI Tool** - Program to read and display temperatures

---

## Features

- Generates random temperatures (25°C to 45°C)
- New sample every 100ms automatically
- Multiple output formats: table, JSON, CSV
- Can detect when temperature is too high (threshold alert)
- Works with standard Linux tools (cat, dd, etc.)

---

## Requirements

- Linux system (tested on Ubuntu 24.04)
- Kernel version 6.14 or similar
- GCC compiler
- Make build tool
- Root access (sudo)

---

## Quick Start

### 1. Build Everything
```bash
./scripts/build.sh
```

### 2. Run Demo
```bash
sudo ./scripts/demo.sh
```

### 3. Use CLI Tool
```bash
# Read 10 temperature samples
sudo ./cli/simtemp_cli -n 10

# Continuous monitoring with statistics
sudo ./cli/simtemp_cli -c -s

# JSON output
sudo ./cli/simtemp_cli -n 20 -f json

# Get help
./cli/simtemp_cli --help
```

---

## Project Structure
```
nxp-simtemp-challenge/
├── kernel/              # Kernel driver source code
│   ├── nxp_simtemp.c   # Main driver file
│   └── Makefile        # Build configuration
├── cli/                 # CLI application
│   ├── simtemp_cli.c   # Main program
│   └── Makefile        # Build configuration
├── scripts/             # Helper scripts
│   ├── build.sh        # Compile everything
│   ├── demo.sh         # Run demonstration
│   ├── cleanup.sh      # Remove compiled files
│   └── test.sh         # Run tests
├── docs/                # Documentation
│   ├── AI_NOTES.md     # Development notes
│   ├── DESIGN.md       # Technical design
│   └── TESTPLAN.md     # Test documentation
└── README.md            # This file
```

---

## How It Works
```
Timer (every 100ms)
      ↓
Generate random temperature
      ↓
Store in ring buffer (64 samples)
      ↓
User reads from /dev/simtemp
      ↓
Get temperature data
```

### Temperature Generation

- **Base temperature:** 35°C
- **Variation:** ±10°C (random)
- **Result range:** 25°C - 45°C
- **Default threshold:** 45°C

---

## CLI Options
```
Usage: simtemp_cli [OPTIONS]

Options:
  -c, --continuous       Run until Ctrl+C
  -n, --samples=N        Read N samples (default: 10)
  -i, --interval=MS      Wait MS milliseconds between reads
  -f, --format=FORMAT    Output format: table, json, csv
  -s, --stats            Show min/max/average at end
  -v, --verbose          Show detailed information
  -h, --help             Show help message
```

---

## Testing

### Run All Tests
```bash
sudo ./scripts/test.sh
```

### Manual Testing
```bash
# Load kernel module
sudo insmod kernel/nxp_simtemp.ko

# Check device exists
ls -l /dev/simtemp

# Read with standard tools
sudo cat /dev/simtemp | hexdump -C | head -5

# Unload module
sudo rmmod nxp_simtemp
```

---

## Cleanup
```bash
# Remove all compiled files and unload module
./scripts/cleanup.sh
```

---

## Documentation

- **[AI_NOTES.md](docs/AI_NOTES.md)** - Development process and decisions
- **[DESIGN.md](docs/DESIGN.md)** - Technical design details
- **[TESTPLAN.md](docs/TESTPLAN.md)** - Testing strategy and results
- **Demo Video:** [Watch on YouTube](https://drive.google.com/file/d/1Uq5fEQtYhkVCP1npYmgbdLocQ5HTNwB1/view?usp=sharing)
- **Submission Document:** [NXP_Submission_EdgarValencia.pdf](./docs/Report/NXP_Submission_EdgarValencia.pdf)

---

## Development Status

### ✅ Completed (Phase 2A, 2B, 2C)

- Temperature generation with random values
- Ring buffer for storing samples
- High-resolution timer (100ms)
- Blocking and non-blocking reads
- Poll/select support
- CLI application with multiple formats
- Statistics calculation
- Automated build and demo scripts

### ⏳ Future Work (Phase 2D)

- IOCTL support for runtime configuration
- Device Tree overlay
- Multiple device instances
- Advanced statistics

---

## Known Issues

None at this time. The driver is stable and working correctly.

---

## License

GPL v2 (same as Linux Kernel)

---

## Contact

**Edgar Valencia**  
GitHub: [EdgarValencia95/nxp-simtemp-challenge](https://github.com/EdgarValencia95/nxp-simtemp-challenge)

---

## Acknowledgments

- NXP Semiconductors for the challenge
- Linux kernel community for documentation
- AI assistance for technical guidance

---

**Last Updated:** October 22, 2024
