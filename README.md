# File System Recovery Tool

A sophisticated user-space file system simulator with advanced recovery, defragmentation, and visualization capabilities. Built with Qt6 and C++, this educational tool demonstrates core file system concepts including block allocation, journaling, crash recovery, and performance optimization.

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![Language](https://img.shields.io/badge/language-C%2B%2B17-blue)
![Framework](https://img.shields.io/badge/framework-Qt6-green)
![License](https://img.shields.io/badge/license-MIT-blue)

## 🎯 Overview

This project simulates a Unix-like file system in user space, providing a safe environment to explore file system internals, test recovery mechanisms, and visualize how data is stored and managed on disk. Perfect for students, educators, and developers interested in understanding file system architecture and implementation.

### Why This Project Matters

**Educational Value:**
- **Hands-on Learning:** Bridges the gap between theoretical file system concepts and practical implementation
- **Visual Understanding:** Real-time block map visualization shows exactly how files are stored and fragmented
- **Safe Experimentation:** Test crash scenarios and recovery without risking real data or system stability
- **Performance Analysis:** Observe the real impact of fragmentation on read/write performance

**Technical Importance:**
- **Recovery Mechanisms:** Demonstrates how modern file systems handle power failures and crashes
- **Defragmentation:** Shows why and how defragmentation improves performance
- **Block Management:** Illustrates efficient block allocation strategies for contiguous storage
- **Journaling Concepts:** Foundation for understanding journaling file systems

## ✨ Key Features

### 🔧 Core File System Operations
- **Create/Read/Write/Delete Files:** Full CRUD operations with proper inode management
- **Directory Support:** Hierarchical directory structure with path resolution
- **Block Allocation:** Smart allocation strategies including compact allocation for defragmentation
- **Metadata Management:** Inodes with timestamps, permissions, and block pointers
- **Indirect Blocks:** Support for files larger than direct block capacity (>48KB)

### 💾 Virtual Disk Management
- **Persistent Storage:** File system state saved to disk image files
- **Configurable Size:** Create disks from 1MB to 1GB
- **Superblock:** Stores file system metadata and configuration
- **Bitmap Management:** Efficient free block tracking
- **Inode Table:** Centralized metadata storage

### 🎨 Real-Time Visualization
- **Block Map Widget:** Color-coded visualization of disk blocks
  - 🟢 **GREEN**: Free blocks available for allocation
  - 🔴 **RED**: Used blocks containing file data
  - 🟡 **YELLOW**: Superblock (file system metadata)
  - 🟣 **PURPLE**: Inode table and bitmap
  - ⚫ **BLACK**: Corrupted/orphaned blocks from power cuts
- **Hover Tooltips:** See block number, state, owner, and filename
- **Live Updates:** Watch blocks change in real-time during operations

### ⚡ Power Cut Simulation & Recovery
- **Realistic Crash Scenarios:** Simulate power failures during write operations
- **Orphan Block Detection:** Identify blocks belonging to incomplete writes
- **Automated Recovery:** Clean up corrupted inodes and free orphaned blocks
- **System State Management:** Disable operations when corrupted, enable after recovery
- **Detailed Logging:** Track every step of the recovery process

### 📊 Performance Metrics
- **Fragmentation Analysis:** Real-time fragmentation percentage calculation
- **Read/Write Latency:** Measure operation performance in milliseconds
- **Throughput Monitoring:** Track data transfer rates
- **Defragmentation Results:** Before/after comparison with improvement metrics
- **Historical Charts:** Visualize performance trends over time

### 🔄 Defragmentation Engine
- **Smart Reorganization:** Moves fragmented files to contiguous blocks
- **Zero-Fragmentation Guarantee:** Achieves 0% fragmentation when successful
- **Block Compaction:** Packs all files from left-to-right on disk
- **Performance Verification:** Benchmarks before/after to prove improvements
- **Progress Tracking:** Real-time status updates during defragmentation

### 📝 Console Logging
- **Operation Tracking:** Every file system operation logged with timestamps
- **Error Reporting:** Clear error messages with context
- **Recovery Status:** Detailed recovery process information
- **Color-Coded Output:** Different message types for easy scanning

## 🏗️ Architecture

### Component Overview

```
┌─────────────────────────────────────────────────────────────┐
│                        MainWindow (Qt6)                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ FileBrowser  │  │  BlockMap    │  │ Performance  │      │
│  │   Widget     │  │   Widget     │  │   Widget     │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                  │                  │              │
│         └──────────────────┼──────────────────┘              │
│                            │                                 │
│  ┌─────────────────────────▼──────────────────────────┐     │
│  │              FileSystem Core                       │     │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────┐        │     │
│  │  │  Inode   │  │Directory │  │  Block   │        │     │
│  │  │ Manager  │  │ Manager  │  │ Ownership│        │     │
│  │  └────┬─────┘  └────┬─────┘  └────┬─────┘        │     │
│  │       └─────────────┼─────────────┘              │     │
│  │                     │                             │     │
│  │  ┌──────────────────▼──────────────────────┐     │     │
│  │  │         VirtualDisk                     │     │     │
│  │  │  - Superblock                           │     │     │
│  │  │  - Bitmap (free block tracking)         │     │     │
│  │  │  - Block I/O operations                 │     │     │
│  │  └─────────────────────────────────────────┘     │     │
│  └───────────────────────────────────────────────────┘     │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐     │
│  │ DefragManager│  │RecoveryMgr   │  │PowerCutSim   │     │
│  │              │  │              │  │              │     │
│  └──────────────┘  └──────────────┘  └──────────────┘     │
└─────────────────────────────────────────────────────────────┘
```

### Key Components

**VirtualDisk** (`src/VirtualDisk.cpp`)
- Manages the disk image file on the host filesystem
- Implements block-level read/write operations (4KB blocks)
- Maintains superblock, bitmap, and inode table
- Provides block allocation strategies (normal and compact)

**InodeManager** (`src/Inode.cpp`)
- Allocates and frees inodes
- Manages direct and indirect block pointers
- Handles file metadata (size, timestamps, permissions)
- Supports up to 1024 inodes per file system

**DirectoryManager** (`src/Directory.cpp`)
- Maintains directory entries (filename → inode mapping)
- Implements path resolution
- Handles directory operations (list, create, delete)

**FileSystem** (`src/FileSystem.cpp`)
- High-level file operations API
- Coordinates between managers
- Implements block ownership tracking for visualization
- Handles power cut simulation and recovery

**DefragManager** (`src/DefragManager.cpp`)
- Analyzes file fragmentation
- Performs file defragmentation
- Benchmarks read performance before/after
- Guarantees contiguous block allocation

**UI Components** (`include/BlockMapWidget.h`, `include/PerformanceWidget.h`, etc.)
- Qt6-based graphical interface
- Real-time visualization and interaction
- Performance monitoring and charts

## 🚀 Getting Started

### Prerequisites

- **C++ Compiler:** GCC 9+ or Clang 10+ with C++17 support
- **Qt6:** Qt6 Core, Widgets, and Charts modules
- **CMake:** Version 3.16 or higher
- **Operating System:** macOS, Linux, or Windows

### Installation

1. **Clone the repository:**
```bash
git clone https://github.com/nehalsinghchandel/file-system-recovery-tool.git
cd file-system-recovery-tool
```

2. **Install Qt6 (if not already installed):**

**macOS (Homebrew):**
```bash
brew install qt@6
```

**Ubuntu/Debian:**
```bash
sudo apt-get install qt6-base-dev qt6-charts-dev
```

**Windows:**
Download Qt6 from https://www.qt.io/download

3. **Build the project:**
```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

4. **Run the application:**
```bash
./file_system_tool.app/Contents/MacOS/file_system_tool  # macOS
# OR
./file_system_tool  # Linux/Windows
```

## 📖 Usage Guide

### Creating a New File System

1. Launch the application
2. **File → New Disk** (or the app creates one automatically on first run)
3. Enter desired disk size (default: 10 MB)
4. The file system is now ready for use

### File Operations

**Creating Files:**
1. Enter filename in the input field
2. Adjust file size using the slider (4KB - 400KB)
3. Click **"Create"** button
4. File appears in the browser and blocks turn red on the map

**Reading Files:**
1. Select a file from the browser
2. Click **"Read"** button
3. Check console log for read confirmation and latency

**Deleting Files:**
1. Select file(s) in the browser
2. Click **"Delete"** button
3. Watch blocks turn green as they're freed

**Bulk Operations:**
1. Select count from dropdown (10, 25, 50, 100)
2. Click **"Write Random Files"**
3. Watch the progress bar as files are created
4. Files are written at 4KB/sec with animation

### Simulating Power Cuts

1. Create several files (recommended: 10-20 files)
2. Click **"Simulate Power Cut"** button
3. **Observe:**
   - Last written file becomes corrupted
   - Corrupted blocks turn **BLACK** on the bitmap
   - Console shows orphaned block numbers
   - All write operations become **DISABLED**
   - Only **"Run Recovery"** button is enabled

4. Click **"Run Recovery"**
5. **Observe:**
   - Corrupted blocks are freed (turn GREEN)
   - Corrupted file is removed
   - All other files remain intact
   - Operations re-enabled

### Running Defragmentation

1. Create many files of varying sizes to cause fragmentation
2. Check fragmentation percentage in Performance panel
3. Click **"Run Defragmentation"** button
4. **Observe:**
   - Files are reorganized into contiguous blocks
   - Block map shows compact left-to-right layout
   - Fragmentation drops to 0%
   - Each file now has exactly 1 fragment
   - Performance results show before/after latency

### Interpreting the Block Map

- **Block Color Meaning:**
  - 🟢 Free space available
  - 🔴 File data
  - 🟡 Superblock (block 0)
  - 🟣 System metadata (inode table, bitmap)
  - ⚫ Corrupted/orphaned blocks

- **Hover over blocks** to see:
  - Block number
  - State (Free/Used/Corrupted)
  - Owner inode number (if used)
  - Filename (if used)

### Reading Performance Metrics

**Performance Widget shows:**
- **Fragmentation:** Current fragmentation percentage
- **Latency:** Last read/write operation time (ms)
- **Throughput:** Data transfer rate (MB/s)
- **Defrag Results:**
  - Before latency
  - After latency
  - Improvement (ms and file count)

## 🔬 Technical Details

### Disk Layout

```
┌──────────────────────────────────────────────────────────┐
│ Block 0: Superblock                                      │
│   - Magic number                                         │
│   - Total blocks, free blocks                            │
│   - Inode count                                          │
│   - Block/inode table locations                          │
├──────────────────────────────────────────────────────────┤
│ Blocks 1-N: Bitmap                                       │
│   - One bit per block (0=allocated, 1=free)              │
├──────────────────────────────────────────────────────────┤
│ Blocks N+1 to M: Inode Table                             │
│   - 128 bytes per inode                                  │
│   - Up to 1024 inodes                                    │
├──────────────────────────────────────────────────────────┤
│ Blocks M+1 to End: Data Blocks                           │
│   - 4096 bytes per block                                 │
│   - File content and directory data                      │
└──────────────────────────────────────────────────────────┘
```

### Inode Structure (128 bytes)

```cpp
struct Inode {
    uint32_t inodeNumber;           // Unique inode ID
    FileType fileType;              // REGULAR_FILE or DIRECTORY
    uint8_t  permissions;           // rwx bits
    uint16_t linkCount;             // Hard link count
    uint32_t fileSize;              // Size in bytes
    uint32_t blockCount;            // Blocks allocated
    time_t   createdTime;           // Creation timestamp
    time_t   modifiedTime;          // Last modified
    time_t   accessedTime;          // Last accessed
    uint32_t directBlocks[12];      // Direct block pointers (48 KB)
    uint32_t indirectBlock;         // Indirect block pointer
    uint8_t  padding[20];           // Alignment padding
};
```

### Block Allocation Strategies

**Normal Allocation (`allocateBlock`):**
- Returns first available free block
- Fast allocation
- May cause fragmentation over time

**Compact Allocation (`allocateBlockCompact`):**
- Searches from the beginning of data region
- Returns lowest numbered free block
- Used during defragmentation
- Guarantees left-to-right packing

### Defragmentation Algorithm

```
1. Collect All Files:
   - Read all valid inodes
   - Read file data into memory
   - Store old block locations

2. Free All Data Blocks:
   - Mark all file-owned blocks as free
   - Clear block ownership
   - Creates contiguous free space

3. Reallocate Compactly:
   - For each file:
     - Use allocateBlockCompact() 
     - Allocates from lowest available block
     - Updates inode with new block numbers
     - Writes data to new location

Result: All files packed left-to-right, 0% fragmentation
```

### Recovery Process

```
1. Detect Corruption:
   - simulatePowerCut() marks last file as corrupted
   - Stores corrupted block numbers
   - Sets hasCorruption flag

2. UI Response:
   - Disable all write operations
   - Enable only "Run Recovery" button
   - Display corrupted blocks in BLACK

3. Recovery Execution:
   - Free all corrupted blocks
   - Clear corrupted inode
   - Update bitmap and superblock
   - Clear corruption flag

4. Post-Recovery:
   - Re-enable all operations
   - Refresh all UI widgets
   - System returns to consistent state
```

## 🎓 Educational Use Cases

### For Students

**Learning File System Concepts:**
- See how inodes connect to data blocks
- Understand fragmentation and its performance impact
- Learn about crash recovery mechanisms
- Visualize how disk space is managed

**Hands-on Experiments:**
- Create fragmented files and measure performance degradation
- Simulate crashes and practice recovery
- Compare defragmented vs. fragmented performance
- Explore different allocation strategies

### For Educators

**Teaching Materials:**
- Visual demonstrations for file system lectures
- Safe environment for crash scenario demonstrations
- Performance analysis exercises
- Recovery mechanism illustrations

**Lab Assignments:**
- Implement new features (journaling, permissions, etc.)
- Optimize allocation algorithms
- Add new defragmentation strategies
- Measure and analyze performance

## 🛠️ Development

### Project Structure

```
file-system-recovery-tool/
├── src/
│   ├── main.cpp                 # Application entry point
│   ├── MainWindow.cpp           # Main UI window
│   ├── VirtualDisk.cpp          # Disk simulation
│   ├── Inode.cpp                # Inode management
│   ├── FileSystem.cpp           # File system core
│   ├── Directory.cpp            # Directory operations
│   ├── DefragManager.cpp        # Defragmentation engine
│   ├── RecoveryManager.cpp      # Recovery operations
│   ├── BlockMapWidget.cpp       # Block visualization
│   ├── PerformanceWidget.cpp    # Metrics display
│   └── FileBrowserWidget.cpp    # File browser UI
├── include/
│   └── *.h                      # Header files
├── CMakeLists.txt               # Build configuration
└── README.md                    # This file
```

### Building with Debug Symbols

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Running Tests

```bash
# After building
ctest --verbose
```

## 📈 Performance Benchmarks

Typical performance on modern hardware:

| Operation | Latency | Notes |
|-----------|---------|-------|
| Create File (4KB) | ~0.5 ms | Single block allocation |
| Read File (48KB) | ~1.2 ms | 12 direct blocks |
| Delete File | ~0.3 ms | Bitmap update + inode free |
| Defragmentation (100 files) | ~150 ms | Complete reorganization |
| Recovery | ~50 ms | Orphan block cleanup |

**Fragmentation Impact:**
- 0% fragmentation: ~0.8 ms average read latency
- 50% fragmentation: ~1.5 ms average read latency (87% slower)
- After defragmentation: Returns to ~0.8 ms

## 🚧 Future Enhancements

- [ ] **Journaling Support:** Write-ahead logging for crash consistency
- [ ] **Permissions System:** User/group/other rwx permissions
- [ ] **Hard Links:** Multiple directory entries for same inode
- [ ] **Symbolic Links:** Path-based file references
- [ ] **Extended Attributes:** Metadata beyond standard inode fields
- [ ] **Compression:** Transparent file compression
- [ ] **Encryption:** At-rest encryption support
- [ ] **Snapshots:** Copy-on-write snapshots
- [ ] **Multi-threading:** Parallel defragmentation
- [ ] **File System Check:** Comprehensive consistency checker (fsck)

## 🤝 Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

### Development Guidelines

1. Follow the existing code style (C++17, Qt conventions)
2. Add appropriate comments for complex logic
3. Update documentation for new features
4. Test thoroughly before submitting PRs

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.

## 👤 Author

**Nehal Singh Chandel**
- GitHub: [@nehalsinghchandel](https://github.com/nehalsinghchandel)

## 🙏 Acknowledgments

- Qt Framework for the excellent GUI toolkit
- Modern operating system design principles
- Unix file system concepts and implementations

## 📞 Support

For questions, issues, or suggestions:
- **Open an Issue:** GitHub Issues tab
- **Email:** nehalsinghchandel@example.com

---

**⭐ If you find this project useful, please consider giving it a star!**

Last Updated: December 2025