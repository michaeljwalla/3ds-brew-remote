# Compiling 3DS Brew Remote

## Setup

First, ensure your development environment is configured:

```bash
source ~/.bashrc
```

In order to setup devkitpro dependencies.\
You can see a guide here to install them: https://devkitpro.org/wiki/Getting_Started

## Compiling the 3DSX (Homebrew Launcher Format)

From the `3ds-brew-remote` root directory:

```bash
cd 3ds
mkdir -p build
cd build
cmake -DNINTENDO_3DS=TRUE -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/3DS.cmake ..
make
```

The output file will be: **`3ds-remote.3dsx`**

## Compiling the CIA (Installable Title)

From the same `3ds-brew-remote/3ds/build` directory:

```bash
make 3ds-remote.cia
```

The output file will be: **`3ds-remote.cia`**

**Note:** This requires `bannertool` and `makerom` to be installed and available on your PATH. If not available, the CIA target will be automatically disabled.

## Output Files

- **3ds-remote.3dsx** - Homebrew launcher format (~173KB)
- **3ds-remote.elf** - Main executable (~1.4MB)
- **3ds-remote.cia** - Installable title (if tools available)
- **3ds-remote.smdh** - Metadata file

## Clean Build

To do a clean rebuild:

```bash
cd 3ds/build
rm -rf *
cmake -DNINTENDO_3DS=TRUE -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/3DS.cmake ..
make
```
