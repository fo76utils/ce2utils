# ce2utils

Simple command line utilities for extracting data from Starfield game files.

**Note:** ce2utils is work in progress, and support for older games is deprecated in these tools. For Oblivion to Fallout 76, use [fo76utils](https://github.com/fo76utils/fo76utils/) instead.

* [baunpack](doc/baunpack.md) - list the contents of, or extract from .BA2 or .BSA archives.
* [bcdecode](doc/bcdecode.md) - convert BC1 to BC5 block compressed DDS textures to uncompressed RGBA image data in raw or DDS format. BC6H and BC7 decompression are also supported, using code from [detex](https://github.com/hglm/detex).
* [btddump](doc/btddump.md) - extract terrain data from Starfield .BTD files to raw height map or land textures.
* [cubeview](doc/cubeview.md) - view cube maps or other textures from archive files.
* [esmdump](doc/esmdump.md) - list records from .ESM files in text or TSV format.
* [esmview](doc/esmview.md) - interactive version of esmdump, also includes a NIF viewer for objects with an associated model (MODL).
* [findwater](doc/findwater.md) - create a height map of water bodies, optionally using .NIF meshes.
* [fo4land](doc/fo4land.md) - extract landscape data from Oblivion, Fallout 3, Skyrim, and Fallout 4.
* [landtxt](doc/landtxt.md) - create an RGB land texture from the output of btddump (formats 2 and 8) or fo4land (formats 2, 4, and 5), or directly from terrain data in ESM/BTD file(s), using a set of DDS texture files.
* [markers](doc/markers.md) - find references to a set of form IDs defined in a text file, and mark their locations on an RGBA format map, optionally using DDS icon files.
* [nif\_info](doc/nif_info.md) - list data from a set of .NIF files in .BA2 or .BSA archives, convert to .OBJ format, or render the model to a DDS file, or display it.
* [render](doc/render.md) - render a Starfield world, cell, or object from ESM file(s), terrain data, and archives. Currently does not support textures.
* [terrain](doc/terrain.md) - older and simpler program to render terrain and water only to an RGB image, using files created by btddump, findwater, or fo4land. Includes 2D and isometric mode.
* [wrldview](doc/wrldview.md) - interactive version of render.

Can be built with MSYS2 (https://www.msys2.org/) on 64-bit Windows, and also on Linux. Run "scons" to compile. mman.c and mman.h are from mman-win32 (https://github.com/alitrack/mman-win32). All source code is under the MIT license.

Running any of the programs without arguments prints detailed usage information.

### Building from source code on Windows

* Download and run the MSYS2 installer from https://www.msys2.org/.
* In the MSYS2 MSYS terminal, run these commands to install development packages:
  *     pacman -Syu
        pacman -Su
        pacman -S --needed base-devel mingw-w64-x86_64-toolchain
        pacman -S msys/scons
* Optionally, install SDL 2 for cubeview, wrldview, and the NIF viewer mode of esmview and nif\_info, matplotlib for plotting scripts, and SWIG for building the Python interface to libfo76utils:
  *     pacman -S mingw64/mingw-w64-x86_64-SDL2
        pacman -S mingw64/mingw-w64-x86_64-python-matplotlib
        pacman -S mingw64/mingw-w64-x86_64-swig
* The installed MSYS2 and MinGW packages can be updated anytime by running **pacman -Syu** again.
* In the MSYS2 MinGW x64 terminal, compile the utilities with **scons**. Use **scons -j 8** for building with 8 parallel jobs, and **scons -c** to clean up and delete the object files and executables. Running scons with the **rgb10a2=1** option compiles all tools that can render NIF files with RGB10A2 frame buffer format, and adding **pymodule=1** builds a Python interface to libfo76utils under scripts.
* If Visual Studio is also installed on the system, **tools=mingw** needs to be added to the scons options.
* By default, the code generated is compatible with Intel Sandy Bridge or newer CPUs. Adding **avx=0** or **avx2=1** disables instruction set extensions or also enables them for Haswell or newer, respectively.
* Optionally, for the makemap and icon extraction scripts only, download and install [ImageMagick](https://imagemagick.org/script/download.php#windows) and [SWFTools](http://www.swftools.org/download.html).

#### Example 1

    ./btddump terrain/newatlantis.btd newatlantis_hmap.dds 0 Starfield/Data
    ./btddump terrain/newatlantis.btd newatlantis_norm.dds 1 Starfield/Data
    ./btddump terrain/newatlantis.btd newatlantis_ltex.dds 2 Starfield/Data
    ./btddump terrain/newatlantis.btd newatlantis_txts.dds 8 Starfield/Data

#### Example 2

    ./baunpack Fallout76/Data -- textures/interface/season/season

#### Example 3

    ./esmdump Fallout4/Data/Fallout4.esm -u | python3 scripts/esmvcdisp.py -h 69 60 184 -

#### Example 4

    ./esmview Skyrim/Data/Skyrim.esm Skyrim/Data skyrim_english -F tes5cell.txt

#### Example 5

    # list options
    scons -f SConstruct.maps game=tes5 listvars=1
    # build Skyrim map
    scons -f SConstruct.maps game=tes5
    # extract the map icons only
    scons -f SConstruct.maps game=tes5 interface/tes5icon_02.dds

#### Example 6

    ./nif_info -render1920x1080 glassdome.dds Fallout76/Data palaceofthewindingpath/palace_bld_glassdome01.nif
    ./nif_info -render1920x1080 44.dds Fallout76/Data weapons/44/44load.nif
    ./nif_info -view1920x1080 Fallout76/Data meshes/test/testpbrmaterials01.nif
    ./cubeview 1280 720 Fallout76/Data textures/shared/cubemaps/outsideoldtownreflectcube_e.dds

#### Example 7

    ./render Fallout76/Data/SeventySix.esm whitespring.dds 4096 4096 Fallout76/Data -r -32 -32 32 32 -cam 0.125 54.7356 180 -135 53340 -99681 74002.25 -light 1.7 70.5288 135 -lcolor 1 0xFFFCF0 0.875 -1 -1 -ssaa 1 -rq 0x2F -ltxtres 512
    ./markers Fallout76/Data/SeventySix.esm fo76mmap.dds 4096,4096,0.125,54.7356,180,-135,4096,-256,16384 fo76icondefs.txt
    ./render Fallout76/Data/SeventySix.esm watoga.dds 9024 9024 Fallout76/Data -r 0 -71 71 0 -cam 0.25 180 0 0 146368.4 -141504.4 65536 -light 1.7 70.5288 135 -ssaa 1 -rq 0x2F -ltxtres 2048 -mip 1 -lmip 2

