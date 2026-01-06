# DeliRium Engine - Setup Guide

## Quick Start (30 minutes)

### 1. Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake git
sudo apt install libsdl2-dev libglew-dev
```

**Fedora:**
```bash
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake git SDL2-devel glew-devel
```

**macOS:**
```bash
brew install cmake sdl2 glew
```

### 2. Create Directory Structure
```bash
chmod +x CREATE_STRUCTURE.sh
./CREATE_STRUCTURE.sh
```

### 3. Copy Files
Place all 17 provided engine files in their locations (see FILE_CHECKLIST.md)

### 4. Copy Your Existing Code
Copy these 11 files from your existing codebase:
- math_types.h
- object_descriptor_buffers.h
- surface_buffer_evaluator.h/cpp
- mathematical_color_evaluator.h/cpp
- bone_evaluator.h/cpp
- anchorpoint_evaluator.h/cpp
- surface_buffers.h

### 5. Update CMakeLists.txt
Uncomment the ENGINE_EXISTING_SOURCES lines for your .cpp files

### 6. Validate Setup
```bash
chmod +x validate_setup.sh
./validate_setup.sh
```

### 7. Build
```bash
cd build
cmake ..
make -j8
./delirium
```

## Expected Output

Window should open with:
- Title: "DeliRium Engine v0.1"
- Dark blue background
- FPS counter in title bar
- ESC to close

## Troubleshooting

**CMake can't find SDL2:**
```bash
sudo apt install libsdl2-dev
```

**Missing math_types.h:**
- Copy from your existing code to `engine/include/delirium/`

**Compilation errors:**
- Check all 11 of your files are copied
- Verify CMakeLists.txt lists your .cpp files

## Next Steps

After successful build:
1. Test loading a .del file
2. Implement basic renderer
3. Add camera controls
4. Create demo scene

See COMPLETE_FILE_SUMMARY.md for full documentation.