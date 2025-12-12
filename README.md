# File System Recovery Tool

A user-space file system simulator with crash recovery, journaling, and defragmentation capabilities, featuring real-time visualization.

 ## Features

### Module 1: Core File System Engine
- **Virtual Disk Management**: 100MB binary file simulating a physical disk
- **Block Allocation**: 4KB blocks with bitmap-based free space tracking
- **Inode System**: File metadata with direct and indirect block pointers
- **Directory Support**: Hierarchical directory structure with path resolution
- **CRUD Operations**: Complete file create, read, update, delete functionality

### Module 2: Recovery & Optimization
- **Journaling System**: Write-ahead logging for crash recovery
- **Consistency Checker**: Detects orphan blocks and invalid inodes
- **Crash Simulation**: Test recovery by simulating incomplete operations
- **Defragmentation**: Block compaction algorithm with performance benchmarking
- **Automated Recovery**: Journal replay and file system repair

### Module 3: Visual Interface
- **Block Map Visualization**: Real-time grid display with color-coded blocks
  - 🟢 Green: Free blocks
  - 🔴 Red: Used blocks
  - 🟡 Yellow: Corrupted blocks
  - 🔵 Blue: Journal blocks
  - 🟣 Purple: Inode table
  - 🔷 Teal: Superblock
- **Performance Metrics**: Live charts for read/write latency
- **Control Panel**: Buttons for all file system operations
- **File Browser**: Interactive directory and file listing

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Main Window (Qt GUI)                     │
├─────────────────┬───────────────────┬───────────────────────┤
│  Block Map      │  Performance      │   File Browser        │
│  Visualization  │  Charts           │   & Details           │
├─────────────────┴───────────────────┴───────────────────────┤
│              Control Panel & Log Output                     │
└─────────────────────────────────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
  ┌─────▼─────┐      ┌──────▼──────┐     ┌─────▼──────┐
  │ Recovery  │      │    Defrag   │     │ File System│
  │ Manager   │      │   Manager   │     │   Engine   │
  └───────────┘      └─────────────┘     └─────┬──────┘
                                               │
                           ┌───────────────────┼──────────────┐
                           │                   │              │
                     ┌─────▼──────┐     ┌──────▼─────┐  ┌────▼────┐
                     │   Inode    │     │ Directory  │  │ Journal │
                     │  Manager   │     │  Manager   │  └─────────┘
                     └────────────┘     └────────────┘
                                  │
                            ┌─────▼──────┐
                            │  Virtual   │
                            │   Disk     │
                            │ (disk.bin) │
                            └────────────┘
```

## Building

### Prerequisites
- **C++ Compiler**: GCC 9+ or Clang 10+ (C++17 support)
- **CMake**: 3.16 or later
- **Qt 6**: Core, Widgets, and Charts modules

### Installing Qt on macOS
```bash
# Using Homebrew
brew install qt@6

# Set Qt path (add to ~/.zshrc for persistence)
export Qt6_DIR="/usr/local/opt/qt@6"
```

### Build Steps
```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake ..

# Build
cmake --build .

# Run
./file_system_tool
```

## Usage

### 1. Create a New Disk
- **File → New Disk...**
- Choose location for `disk.bin`
- Creates a formatted 100MB virtual disk

### 2. Perform File Operations
- **Create File**: Enter path (e.g., `/test.txt`) and click "Create File"
- **Write Random Files**: Generate test data for fragmentation testing
- **Delete File**: Remove files from the file system

### 3. Simulate Crash & Recovery
1. Click **"Simulate Crash"** to create an inconsistent state
2. Restart application and open same disk
3. Click **"Run Recovery"** to repair file system
4. Check log output for recovery results

### 4. Defragmentation
1. Generate fragmented files using **"Write Random Files"**
2. Observe fragmentation score in Performance Metrics
3. Click **"Run Defragmentation"**
4. Compare before/after read latency

### 5. Visual Monitoring
- **Block Map**: Hover over blocks to see details
- **Performance Charts**: Monitor real-time read/write latency
- **File Browser**: Navigate directory structure

## Technical Details

### Disk Layout
```
┌──────────────┬──────────┬──────────────┬─────────┬──────────────┐
│  Superblock  │  Bitmap  │ Inode Table  │ Journal │ Data Blocks  │
│   (1 block)  │          │              │ (64 bl) │              │
└──────────────┴──────────┴──────────────┴─────────┴──────────────┘
```

### File System Parameters
- **Total Size**: 100 MB (configurable)
- **Block Size**: 4 KB
- **Total Blocks**: ~25,000
- **Inode Count**: ~3,125 (12.5% of blocks)
- **Journal Size**: 64 blocks (~256 KB)

### Key Algorithms
- **Block Allocation**: First-fit with bitmap scanning
- **Defragmentation**: Single-pass compaction (O(n) where n = file count)
- **Crash Recovery**: Write-ahead logging with idempotent replay
- **Consistency Check**: Two-pass scan (mark referenced blocks, free unmarked)

## Testing Scenarios

### Scenario 1: Basic Operations
```bash
1. Create disk
2. Create files: /file1.txt, /file2.txt
3. Write data to files
4. Read files back
5. Delete /file1.txt
6. Verify file2.txt still accessible
```

### Scenario 2: Crash Recovery
```bash
1. Create disk
2. Create large file
3. Click "Simulate Crash" during operation
4. Reopen disk
5. Run recovery
6. Verify no orphan blocks
```

### Scenario 3: Defragmentation
```bash
1. Write 50 random files
2. Delete every other file (creates gaps)
3. Write 25 more files (fragments across gaps)
4. Note read latency
5. Run defragmentation
6. Observe 50%+ latency improvement
```

## Project Structure

```
file-system-recovery-tool/
├── include/               # Header files
│   ├── VirtualDisk.h
│   ├── Inode.h
│   ├── Directory.h
│   ├── FileSystem.h
│   ├── Journal.h
│   ├── RecoveryManager.h
│   ├── DefragManager.h
│   ├── MainWindow.h
│   └── [GUI widgets...]
├── src/                   # Implementation files
│   ├── VirtualDisk.cpp
│   ├── [all implementations...]
│   └── main.cpp
├── CMakeLists.txt         # Build configuration
└── README.md              # This file
```

## Contributing

This is an educational project demonstrating file system concepts. Feel free to:
- Add new features (symbolic links, permissions, etc.)
- Improve defragmentation algorithms
- Add more visualization options
- Implement additional recovery strategies

## License

Educational/Open Source - See LICENSE file

## Authors

Created as part of a file system concepts learning project.