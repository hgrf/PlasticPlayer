#!/usr/bin/env python3
from PIL import Image

import os
import struct

im = Image.open(os.path.join(os.path.dirname(__file__), "splash.bmp")).convert("RGBA")
w, h = im.size

with open(os.path.join(os.environ["BINARIES_DIR"], "splash.bin"), "wb") as f:
    for row in range(0, h // 8):
        for col in range(0, w):
            v = 0
            for n in range(8):
                r, g, b, a = im.getpixel((col, row * 8 + n))
                if r <= 128 or g <= 128 or b <= 128:
                    v |= 1 << n
            f.write(struct.pack('B', v))
