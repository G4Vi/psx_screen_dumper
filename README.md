psx_screen_dumper
=================

Dump data from a psx (Sony PlayStation) through the screen

See [screen_data_reader](https://github.com/G4Vi/screen_data_reader/) for the reader.

[Demo video](https://www.youtube.com/watch?v=dxf2u5wmo0I)

## Requirements
* NTSC PlayStation and ability to capture cleanish video from it. The video must have the correct aspect ratio and decent lighting. A phone camera with focus lock on a flatscreen TV or a capture card can work. A CRT probably won't work. Otherwise it can be tested with an emulator.

* Method of booting psx homebrew i.e modchip, FreePSXBoot, tonyhax, swap trick, etc.

* A copy of `psx_screen_dumper` depending on booting method.

* [screen_data_reader](https://github.com/G4Vi/screen_data_reader/). For small videos you may be able to use the web app linked on that page.

* [Optional] For easily converting the raw dumped memory card save images [psx_mc_cli](https://github.com/G4Vi/psx_mc_cli)

## Dumping a Memory Card Save via video camera

1. Launch `psx_screen_dumper` on your PlayStation.
2. Select Dump mc# saves where # refers to the card number that has your desired save (Press X)
3. Scroll to your desired save if needed
4. Hold up your camera and verify you have a good picture; correct aspect ratio, clear and visible text. On devices[phones] with autofocus you may need to lock the focus so that it doesn't go crazy when the data patterns changes.
5. Start recording, try to keep the camera as steady as possible centered on the TV.
6. Press X to start the data patterns, record until you return to the menu.
7. Run `screen_data_reader` on your video file. If all goes well it should create a raw memory card file such as `BASLUS-01066TNHXG01` and print the crc32.
8. If it didn't go well check the log to see if it printed any `append frame` messages. Possibly adjust video camera settings and retry.
9. [Optional] Convert your save to `mcs` or make a `mcd` memory card image with `psx_mc_cli`

## Dumping your psx BIOS via capture card
1. Launch `psx_screen_dumper` on your PlayStation.
2. Verify output from your capture card is legible
3. Start recording from capture card.
4. Select `Dump bios`
5. Stop recording
6. Run `screen_data_reader` on your video file. `./screen_data_reader.py -i cap.avi`. If it worked it should create `PSX-BIOS.ROM` and print the crc32.
7. If it didn't go well check the log to see if it printed any `append frame` messages. Possibly adjust video capture settings and retry.
8. [Optional] Verify your bios crc32 with [FreePSXBoot BIOS CRC32 table](https://github.com/brad-lin/FreePSXBoot#downloads) to confirm you got a good dump. If you have a custom or patched bios it probably won't match.

## LICENSE
MIT, see `LICENSE`.


BIOS charset usage and crc32 adapted from [tonyhax](https://github.com/socram8888/tonyhax/).

## Help

Feel free to ask in the [PSXDEV discord](https://discord.gg/QByKPpH)

## Building (Linux)

Install the MIPS toolchain

`sudo apt-get install gcc-mipsel-linux-gnu g++-mipsel-linux-gnu binutils-mipsel-linux-gnu`

Download the nugget submodule

`cd thirdparty/nugget && git submodule init && git submodule update`

Install the converted PsyQ 4.7 libs. Currently SHA1 `36E7A1D606568363F1EBE67E1B499E61FA48DE00`

`wget http://psx.arthus.net/sdk/Psy-Q/psyq-4.7-converted-full.7z`

`7z x -o./thirdparty/nugget/psyq/ psyq-4.7-converted-full.7z`

`make` builds a `.ps-exe`

`make iso` builds an iso

`make nocash` builds a `.ps-exe` with `.exe` extension instead for loading in `no$psx`

`make actualclean` cleans

### Thanks

 Nicolas Noble for build environment. ABelliqueux for easy PSYQ setup tutorial. Many others in the PSXDEV discord.

### Wishlist
- PAL support