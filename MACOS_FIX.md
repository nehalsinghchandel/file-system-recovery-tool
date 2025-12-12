# macOS File Dialog Fix

## Problem
QFileDialog is not showing on macOS - returns immediately with empty string.

## Root Cause
When running Qt apps from terminal on macOS, file dialogs may fail due to:
1. macOS sandboxing/permissions
2. Not running as proper app bundle
3. Missing entitlements

## Solution

### Option 1: Use `open` Command (Recommended)
Instead of running the executable directly, launch as a proper macOS app:

```bash
cd /Users/nehalsinghchandel/file-system-recovery-tool/build
open file_system_tool.app
```

This properly initializes macOS app permissions.

### Option 2: Use Native File Dialog
Add this flag when creating file dialogs (already updated in code):

```cpp
QFileDialog::Options options = QFileDialog::DontUseNativeDialog;
```

### Option 3: Grant Terminal Permissions
1. Open **System Settings** → **Privacy & Security** → **Files and Folders**
2. Find **Terminal** in the list
3. Enable access to **Desktop** and **Documents**

## Quick Test

```bash
# Stop current app (Ctrl+C)
# Launch properly:
cd /Users/nehalsinghchandel/file-system-recovery-tool/build
open file_system_tool.app

# Then try File → New Disk...
```

## Alternative: Use Non-Native Dialog

I've updated the code to use Qt's own file dialog instead of macOS native one, which should work regardless of permissions.
