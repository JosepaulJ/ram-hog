
# RAM Hog

A cross-platform memory allocation utility written in C that allocates memory in fixed-size chunks until either memory is exhausted or a configurable maximum limit is reached. Perfect for stress testing, memory analysis, and system resource evaluation.

## Table of Contents
- [Features](#features)
- [Installation](#installation)
- [Usage](#usage)
- [Contributing](#contributing)
- [License](#license)
- [Contact](#contact)

## Features
- **Configurable chunk allocation**: Set custom chunk sizes (default: 100MB)
- **Memory limit control**: Set maximum allocation limits with MB/GB unit support
- **Speed modes**: Choose between aggressive (fast) or gentle (delayed) allocation
- **Progress tracking**: Real-time logging of allocation progress
- **Graceful shutdown**: Clean memory release on Ctrl+C interruption
- **Cross-platform**: Compatible with Windows, Linux, and macOS
- **Error handling**: Robust malloc failure detection and recovery
- **Memory persistence**: Holds allocated memory indefinitely until termination

## Installation
Clone the repository and compile the program:

```bash
git clone https://github.com/elxecutor/ram-hog.git
cd ram-hog
gcc -O2 ramhog.c -o ramhog
```

## Usage
1. **Basic usage** (allocate 100MB chunks until memory exhausted):
   ```bash
   ./ramhog
   ```

2. **Custom chunk size** (50MB chunks):
   ```bash
   ./ramhog -c 50
   ```

3. **Set allocation limit** (allocate up to 2GB):
   ```bash
   ./ramhog -m 2G
   ```

4. **Gentle allocation mode** (100ms delay between chunks):
   ```bash
   ./ramhog -s gentle
   ```

5. **Combined options**:
   ```bash
   ./ramhog -c 25 -m 1G -s aggressive
   ```

6. **View help**:
   ```bash
   ./ramhog --help
   ```

### Command Line Options
- `-c, --chunk-size SIZE`: Chunk size in MB (default: 100)
- `-m, --max-alloc SIZE`: Maximum allocation (MB/GB, 0=unlimited, default: 0)
- `-s, --speed MODE`: Allocation speed: `aggressive` or `gentle` (default: aggressive)
- `-h, --help`: Show help message

### Size Format Examples
- `100` - 100 MB
- `2G` - 2 GB  
- `1024M` - 1024 MB

## Contributing
We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) and [Code of Conduct](CODE_OF_CONDUCT.md) for details.

## License
This project is licensed under the [MIT License](LICENSE).

## Contact
For questions or support, please open an issue or contact the maintainer via [X](https://x.com/elxecutor/).
