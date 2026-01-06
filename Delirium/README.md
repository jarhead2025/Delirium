# DeliRium Engine

**Mathematical Surface-Based Game Engine for Low-End Hardware**

## Overview

DeliRium is a game engine that represents 3D objects as mathematical surfaces instead of traditional polygon meshes. This approach achieves **6x+ compression** compared to standard mesh formats while enabling infinite LOD and algebraic physics.

## Key Features

- **6x+ File Compression**: Objects stored as mathematical equations
- **Infinite LOD**: Generate any detail level on demand
- **Algebraic Physics**: Fast collision detection using surface equations
- **127+ Procedural Colors**: Mathematical color patterns from 3 bytes
- **GTX 1650 Optimized**: Designed for budget hardware

## Quick Start

```bash
# 1. Install dependencies (Ubuntu/Debian)
sudo apt install build-essential cmake libsdl2-dev libglew-dev

# 2. Create structure
chmod +x CREATE_STRUCTURE.sh && ./CREATE_STRUCTURE.sh

# 3. Copy your existing code files (see FILE_CHECKLIST.md)

# 4. Build
cd build
cmake ..
make -j8

# 5. Run
./delirium
```

## Project Structure

```
DeliRium_Local/
├── engine/
│   ├── include/delirium/  # Engine headers
│   └── src/               # Engine source
├── assets/                # .del files
├── docs/                  # Documentation
└── build/                 # Build output
```

## Documentation

- **SETUP_GUIDE.md** - Detailed setup instructions
- **FILE_CHECKLIST.md** - Complete file list
- **FILE_PLACEMENT_MAP.txt** - Quick reference

## Requirements

- C++17 compiler
- CMake 3.15+
- SDL2
- OpenGL 3.3+
- GLEW

## Current Status

**v0.1-alpha** - Foundation complete
- ✅ Core engine loop
- ✅ Scene graph
- ✅ Transform system
- ✅ Asset loading (.del format)
- ✅ Basic physics
- 🔄 Renderer (in progress)

## Innovation

Traditional mesh: **5 KB → 5 KB**
DeliRium format: **5 KB → 0.8 KB** (6.2x compression)

Your mathematical surface evaluation code is integrated and ready to use!

## License

[To be determined - add when open sourcing]

## Contact

[Your contact info when ready for beta]
