# trs-tools

This is a tool that can be used for creating or extracting image from TRS files used by Illwinter Game Design in Dominions 4 and 5.

A good chunk of this program logic has been taken from [dominions-tools](https://github.com/dominions-tools/dominions-tools/blob/master/dump-trs-data).

## Building

You can use standard cmake commands to build these tools:

```bash
git clone https://github.com/iret-a/trs-tools.git
cd trs-tools
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release
cd build
make -j
```

Alternatively, you can also directly use your C compiler:
```bash
git clone https://github.com/iret-a/trs-tools.git
cd trs-tools
gcc -std=c99 -lm -O2 src/main.c src/trs.c -o trs-tools
```

## Usage

Extract images from a TRS file:
```bash
trs-tools x misc.trs
```

Creating a TRS file from all png images in current directoy:
```bash
trs-tools c monster.trs *png
```

## License
[GPL-3.0](https://www.gnu.org/licenses/gpl-3.0.txt)