# Quick Start Guide - File System Recovery Tool

## ✅ Build Complete!

The file system recovery tool has been successfully built and is ready to run.

**Executable Location:**
```
/Users/nehalsinghchandel/file-system-recovery-tool/build/file_system_tool.app/Contents/MacOS/file_system_tool
```

**Size:** 666 KB

## Running the Application

### Option 1: Launch from Terminal
```bash
cd /Users/nehalsinghchandel/file-system-recovery-tool/build
./file_system_tool.app/Contents/MacOS/file_system_tool
```

### Option 2: Open as macOS App
```bash
open /Users/nehalsinghchandel/file-system-recovery-tool/build/file_system_tool.app
```

## First Steps

1. **Launch the application** (choose either option above)
2. **Create a new disk**:
   - Click **File → New Disk...**
   - Choose a location (e.g., `~/Documents/test_disk.bin`)
   - Click Save
3. **Explore the interface**:
   - **Left Panel**: Block Map visualization (watch blocks change color)
   - **Top Right**: Performance metrics
   - **Bottom Right**: File browser  
   - **Bottom Panel**: Control panel with operations

## Quick Test Scenarios

### Test 1: Create Files
```
1. In Control Panel, enter filename: /test1.txt
2. Click "Create File"
3. Watch a green block turn red in Block Map
4. Enter filename: /test2.txt
5. Click "Create File"
6. Check File Browser - should show 2 files
```

### Test 2: Bulk Files + Fragmentation
```
1. Select "50" in dropdown
2. Click "Write Random Files"
3. Watch Block Map fill with red blocks
4. Check "Fragmentation" score in Performance Metrics
5. Click "Run Defragmentation" (green button)
6. Observe blocks reorganizing
7. Check performance improvement in log
```

### Test 3: Crash Recovery
```
1. Click "Simulate Crash" (red button)
   - Creates inconsistent state
2. Close application
3. Reopen application
4. File → Open Disk (select your disk.bin)
5. Notice warning: "not cleanly unmounted"
6. Click "Run Recovery" (blue button)
7. Check log for orphan blocks fixed
```

## Understanding the UI

### Block Map Colors
- 🟢 **Green**: Free blocks (available)
- 🔴 **Red**: Used blocks (contain file data)
- 🔵 **Blue**: Journal blocks (for recovery)
- 🟣 **Purple**: Inode table (file metadata)
- 🔷 **Teal**: Superblock (disk metadata)

### Control Panel Buttons
- **Create File**: Add single file
- **Delete File**: Remove file
- **Write Random Files**: Generate test data
- 🔴 **Simulate Crash**: Create corruption
- 🔵 **Run Recovery**: Fix file system
- 🟢 **Run Defragmentation**: Optimize layout

## Expected Behavior

### Fragmentation
- **Without defrag**: Read latency ~50-100ms
- **After defrag**: Read latency ~10-20ms  
- **Improvement**: >50% faster

### Recovery
- Detects orphan blocks (marked used but unreferenced)
- Fixes invalid inodes
- Replays or rolls back incomplete transactions

## Troubleshooting

### Issue: Application Crashes on Startup
**Solution**: Make sure Qt6 is properly installed
```bash
brew list qt@6  # Verify Qt installation
export Qt6_DIR="/opt/homebrew/opt/qt@6"  # Set path
```

### Issue: Cannot Create Disk
**Solution**: Check file permissions and disk space
```bash
df -h ~  # Check free space
touch ~/test.bin && rm ~/test.bin  # Test write permission
```

### Issue: "File not found" errors
**Solution**: Always use absolute paths for files
```
❌ test.txt
✅ /test.txt

❌ dir/test.txt  
✅ /dir/test.txt
```

## Advanced Usage

### Create Directory
```
1. Create file: /test_dir  (Note: directories use special inode type)
2. Use file browser to navigate
```

### Monitor Performance
1. Create several files
2. Read/write operations show in Performance charts
3. Latency displayed in real-time

### Stress Test
```
1. Write Random Files: 100
2. Creates highly fragmented disk
3. Run defragmentation
4. Compare before/after metrics
```

## Technical Details

- **Disk Size**: 100 MB (configurable in code)
- **Block Size**: 4 KB
- **Total Blocks**: ~25,000
- **Max File Size**: ~4 MB per file
- **Journal**: 64 blocks (~256 KB)

## Next Steps

- Experiment with different file operations
- Test crash recovery scenarios
- Measure defragmentation improvements  
- Explore visual block allocation patterns

For full documentation, see [README.md](file:///Users/nehalsinghchandel/file-system-recovery-tool/README.md)

---

**Enjoy exploring file system internals!** 🚀
