    wrldview INFILE.ESM[,...] W H ARCHIVEPATH MATCDBPATH [OPTIONS...]

Interactively render a world, cell, or object from ESM file(s), terrain data, and archives. See also [render](render.md) for additional details. W and H are the width and height of the window to be created. A window that would be larger than the display dimensions and has even width and height is automatically downsampled to half resolution.

### General options

* **MATCDBPATH**: Material database file name(s), can be a comma separated list of multiple CDB files. Defaults to materials/materialsbeta.cdb if an empty string is specified.
* **--help** or **-h**: Print usage.
* **--list** or **--list-defaults**: Print defaults for all options, and exit. If used after other options, then the updated values are printed, including the light vector calculated from **-light**.
* **--**: Remaining options are file names.
* **-threads INT**: Set the number of threads to use.
* **-debug INT**: Set debug render mode (0: disabled, 1: reference form IDs as 0xRRGGBB, 2: depth \* 16 or 64, 3: normals, 4: diffuse texture only, 5: light only).
* **-w FORMID**: Form ID of world, cell, or object to render. A table of game and DLC world form IDs can be found in [SConstruct.maps](../SConstruct.maps).
* **-rq INT**: Set render quality and flags (0 to 2047, can be specified in hexadecimal format with 0x prefix, defaults to 0), using a sum of any of the following values:
  * 2: Render all supported object types other than decals, actors and markers (same as **-a**).
  * 0, 4, 8, or 12: Render quality from lowest to highest, 0 uses diffuse textures only on terrain and objects, 4 enables normal mapping, 8 also enables PBR on objects only, 12 enables PBR on terrain as well.
  * 16: Enable actors, this is only partly implemented and may not work correctly.
  * 32: Enable the rendering of projected decals (PDCL objects). This requires an additional buffer for normals, increasing memory usage from 8 to 12 bytes per pixel.
  * 64: Enable marker objects.
  * 128: Disable built-in exclude patterns for effect meshes.
  * 256: Disable the use of effect materials.
  * 512: Disable the use of object bounds data (OBND) for the purpose of testing if an object is visible.
  * 1024: Disable reordering objects, ensures deterministic output at the cost of worse threading performance.
* **-ft INT**: Minimum frame time in milliseconds. The display is updated after this amount of time during rendering.
* **-markers FILENAME**: Read marker definitions from the specified file, see [markers](markers.md) for details on the file format.

### Texture options

* **-textures BOOL**: Make all diffuse textures white if false.
* **-tc INT** or **-txtcache INT**: Texture cache size in megabytes.
* **-mc INT**: Model cache size, the number of models to load at the same time (1 to 64, defaults to 16).
* **-mip INT**: Base mip level for all textures other than cube maps and the water texture. Defaults to 2.
* **-env FILENAME.DDS**: Default environment map texture path in archives. Defaults to **textures/cubemaps/cell_spacecube.dds**. Use **baunpack ARCHIVEPATH --list /cubemaps/** to print the list of available cube map textures, and [cubeview](cubeview.md) to preview them.

### Terrain options

* **-btd FILENAME.BTD**: Read terrain data from Starfield .btd file.
* **-r X0 Y0 X1 Y1**: Terrain cell range, X0,Y0 = SW to X1,Y1 = NE. The default is to use all available terrain, limiting the range can sometimes be useful to reduce the load time and memory usage.
* **-l INT**: Level of detail to use from BTD file (0 to 4, default: 0), see [btddump](btddump.md).
* **-defclr 0x00RRGGBB**: Default color for untextured terrain.
* **-lmult FLOAT**: Land texture RGB level scale.
* **-ltxtres INT**: Maximum land texture resolution per cell, must be power of two, and in the range 2<sup>(7-l)</sup> to 4096.

### Model options

* **-mlod INT**: Set level of detail for models, 0 (default and best) to 4.
* **-vis BOOL**: Render only objects visible from distance.
* **-ndis BOOL**: If zero, also render initially disabled objects.
* **-minscale FLOAT**: Minimum view scale to render exterior objects at.
* **-xm STRING**: Add excluded model path name pattern. **-xm meshes** disables all solid objects.
* **-xm_clear**: Clear any previously added excluded model path name patterns.

### View options

* **-cam SCALE DIR X Y Z**: Set view scale from world to image coordinates, view direction (0 to 19, see [src/viewrtbl.cpp](../src/viewrtbl.cpp)), and camera position.
* **-cam SCALE -1 RX RY RZ X Y Z**: Set view scale from world to image coordinates, view direction using custom rotations (see [figure](view.png)), and camera position.
* **-zrange FLOAT**: Limit Z range in view space to less than the specified value.

### Lighting options

* **-light SCALE RY RZ**: Set overall brightness, and light vector Y, Z rotation in degrees (0, 0 = top). The light direction is rotated around the Y axis first (positive = east, negative = west), then around the Z axis (positive = counter-clockwise, from east towards north). The default direction of RY=70.5288 and RZ=135 translates to a light vector of -2/3, 2/3, 1/3.
* **-lcolor LMULT LCOLOR EMULT ECOLOR ACOLOR**: Set light source, environment, and ambient light colors and levels. LCOLOR, ECOLOR, and ACOLOR are in 0xRRGGBB format (sRGB color space), -1 is interpreted as white for LCOLOR and ECOLOR. LMULT and LCOLOR multiply diffuse and specular lighting, while EMULT and ECOLOR are applied to environment maps and also to ambient light. If ACOLOR is negative, the ambient light color is determined from the default cube map specified with **-env**, if one is available, otherwise it defaults to 0x404040.
* **-rscale FLOAT**: Scale factor to use when calculating a view vector (X \* -normalZ, Y \* -normalZ, H \* RSCALE) from the pixel coordinates and image height for cube mapping and specular reflections. A higher value simulates a narrower FOV for the purpose of reflections. Defaults to 2.0.

### Water options

* **-wtxt FILENAME.DDS**: Water normal map texture path in archives. Defaults to **textures/water/wavesdefault_normal.dds**. Setting the path to an empty string disables normal mapping on water.
* **-watercolor UINT32**: Water color (A7R8G8B8), 0 disables water, 0x7FFFFFFF (the default) uses values from the ESM file. The alpha channel determines the depth at which water reaches maximum opacity, maxDepth = (128 - a) \* (128 - a) / 128.
* **-wrefl FLOAT**: Water environment map scale, the default is 1.0.
* **-wscale INT**: Water texture tile size in meters, defaults to 31.

### Keyboard and mouse controls

* **0** to **5**: Set debug render mode. 0: disabled (default), 1: reference and cell form IDs as RGB colors, 2: depth, 3: normals, 4: diffuse texture only with no lighting, 5: light only (white textures).
* **+**, **-**: Zoom in or out.
* **Keypad 0**, **5**: Set view from the bottom or top (default = top).
* **Keypad 1** to **9**: Set isometric view from the SW to NE.
* **Shift** + **Keypad 0** to **9**: Set side view, or top/bottom view rotated by 45 degrees.
* **A**, **D**: Move to the left or right.
* **S**, **W**: Move backward or forward (-Z or +Z in the view space).
* **E**, **X**: Move up or down (-Y or +Y in the view space).
* **F11**: Save raw screenshot without downsampling or pixel format conversion.
* **F12** or **Print Screen**: Save screenshot.
* **P**: Print current **-light** and **-view** parameters (for use with [render](render.md) or [markers](markers.md)), and camera position. The information printed is also copied to the clipboard.
* **V**: Print all current settings, similarly to the 'list' console command.
* **R**: Print the list of view directions that can be used with 'cam'.
* **H**: Show help screen.
* **C**: Clear messages.
* **\`**: Open the console. In this mode, keyboard and mouse controls work similarly to [esmview](esmview.md) with SDL 2, and any of the command line options can be entered as a command (without the leading - character). Entering the hexadecimal form ID of a reference moves the camera position to its coordinates. The command **q** or **\`** returns to view mode.
* **Esc**: Quit program.

##### Mouse controls in view mode

* Double click with the left button: move camera position so that the selected pixel is at the center of the screen.
* Double click with the right button: similar to above, but also adjust the Z coordinate in view space so that the selected pixel is just in front of the new camera position.
* Single click with the left button, in debug mode 1 only: print the form ID of the selected object based on the color of the pixel, and also copy it to the clipboard.

### Example

    ./wrldview Starfield/Data/Starfield.esm 3200 1800 Starfield/Data "" -w 0x0001251B -light 1.0 45 90 -lcolor 1.5 -1 1.0 -1 -1 -rq 0x2F -minscale 1 -cam 2.828427 1 -1188.59 -1014.737 1428.076

