#!/usr/bin/env python3
#
# SPDX-FileCopyrightText: 2026 Roger Ortiz <roger@r0rt1z2.com>
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Encode a video into the frame blob played by 'fastboot oem badapple'.
#
# Frames are 1-bit images (row-aligned, MSB first), each XORed against the
# previous frame and then PackBits compressed, which is what the player in
# board/tecno/board-lh8n.c decodes. Output is a C header.
#
# Requires ffmpeg for video input (demo mode does not).
#
#   ./utils/badapple.py --demo -o board/tecno/badapple_frames.h
#   ./utils/badapple.py badapple.mp4 -c 120 -r 44 -f 8 -o board/tecno/badapple_frames.h
#
# The kaeru payload is limited to MAX_STAGE2_SIZE (128 KiB, see
# stage1/main.c) and the code is ~11 KiB, so keep the blob under ~115 KiB.
# Lower -c/-r/-f, or use --start/--duration, until the reported size fits.
#

import argparse
import math
import os
import shutil
import subprocess
import sys


def ffmpeg_exe() -> str:
    """Find ffmpeg: $BADAPPLE_FFMPEG, then PATH, then imageio-ffmpeg's copy."""
    env = os.environ.get("BADAPPLE_FFMPEG")
    if env:
        return env

    found = shutil.which("ffmpeg")
    if found:
        return found

    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except Exception:
        sys.exit("ffmpeg not found. Install it (apt install ffmpeg), or\n"
                 "  pip install imageio-ffmpeg\n"
                 "or point BADAPPLE_FFMPEG at the binary.")


BAYER4 = (
    (0, 8, 2, 10),
    (12, 4, 14, 6),
    (3, 11, 1, 9),
    (15, 7, 13, 5),
)


def packbits(data: bytes) -> bytes:
    """Runs of zeroes + literals, mirrored exactly by the player's decoder.

    Zero runs are located with bytes.find/rstrip so the scan stays in C: this is
    the hottest path on colour frames (a frame is several hundred KiB).
    """
    out = bytearray()
    i, n = 0, len(data)

    while i < n:
        z = data.find(b"\x00\x00", i)

        if z == i:
            # Run of zeroes, at most 128 per code (longer runs just emit more).
            # NB: leading zeros -> lstrip (rstrip would give 0 whenever the
            # chunk doesn't end in zero, which is an infinite loop).
            chunk = data[i:min(i + 128, n)]
            run = len(chunk) - len(chunk.lstrip(b"\x00"))

            out.append((257 - run) & 0xFF)
            i += run
            continue

        # Literals up to the next worthwhile run, at most 128 per code.
        stop = z if z != -1 else n

        while i < stop:
            end = min(i + 128, stop)

            out.append(end - i - 1)
            out.extend(data[i:end])
            i = end

    return bytes(out)


def xor_bytes(a: bytes, b: bytes) -> bytes:
    """Frame delta: one big-int XOR instead of a per-byte Python loop."""
    return (int.from_bytes(a, "little") ^ int.from_bytes(b, "little")).to_bytes(len(a), "little")


def frame_from_gray(gray: bytes, cols: int, rows: int, threshold: int, invert: bool,
                    dither: bool) -> bytes:
    row_bytes = (cols + 7) // 8
    frame = bytearray(row_bytes * rows)

    for y in range(rows):
        base = y * cols
        brow = BAYER4[y & 3]
        for x in range(cols):
            if dither:
                # Ordered (Bayer 4x4) dithering: much closer to the real
                # dithered look than a hard threshold.
                on = gray[base + x] > brow[x & 3] * 17
            else:
                on = gray[base + x] >= threshold
            if invert:
                on = not on
            if on:
                frame[y * row_bytes + (x >> 3)] |= 0x80 >> (x & 7)

    return bytes(frame)


def rgb565_tables(mask: int):
    """The two output bytes of RGB565 as 256-entry translate tables.

    v = (r5 << 11) | (g6 << 5) | b5, so
      high byte = (r5 << 3) | (g6 >> 3)
      low  byte = ((g6 & 7) << 5) | b5
    """
    rk = (mask >> 11) & 0x1F
    gk = (mask >> 5) & 0x3F
    bk = mask & 0x1F

    hi_r = bytearray(256)
    hi_g = bytearray(256)
    lo_g = bytearray(256)
    lo_b = bytearray(256)

    for v in range(256):
        r5 = ((v & 0xF8) >> 3) & rk
        g6 = ((v & 0xFC) >> 2) & gk
        b5 = (v >> 3) & bk

        hi_r[v] = r5 << 3
        hi_g[v] = g6 >> 3
        lo_g[v] = (g6 & 7) << 5
        lo_b[v] = b5

    return bytes(hi_r), bytes(hi_g), bytes(lo_g), bytes(lo_b)


_RGB565_TABLES: dict = {}


def frame_from_rgb565(rgb: bytes, cols: int, rows: int, mask: int = 0xFFFF) -> bytes:
    """rgb: 3 bytes/pixel, out: RGB565 little-endian, 2 bytes/pixel.

    `mask` zeroes low bits per channel (RGB565 + quantisation), which is what
    keeps the frame-to-frame XOR delta small: video noise stops flipping pixels.

    Done with translate tables + big-int OR instead of a pixel loop -- this is
    the dominant cost for a 360x540 frame (~195k pixels).
    """
    n = cols * rows
    t = _RGB565_TABLES.get(mask)

    if t is None:
        t = _RGB565_TABLES[mask] = rgb565_tables(mask)

    hi_r, hi_g, lo_g, lo_b = t
    green = rgb[1::3]

    hi = (int.from_bytes(rgb[0::3].translate(hi_r), "little") |
          int.from_bytes(green.translate(hi_g), "little")).to_bytes(n, "little")
    lo = (int.from_bytes(rgb[2::3].translate(lo_b), "little") |
          int.from_bytes(green.translate(lo_g), "little")).to_bytes(n, "little")

    out = bytearray(2 * n)

    out[0::2] = lo
    out[1::2] = hi

    return bytes(out)


def read_raw_frames(path: str, cols: int, rows: int, fps: float, color: bool,
                    start: str | None, duration: str | None, denoise: bool = False):
    """Decode a video straight to raw frames with ffmpeg (gray or rgb24).

    `denoise` runs a temporal denoiser (hqdn3d) before scaling. Without it the
    source's compression noise makes ~60% of the picture differ slightly every
    frame, so the player's dirty rectangle is most of the frame and every byte of
    it is painted, cache-flushed and pushed to the panel -- which is the
    difference between 25 and 30 fps at 1080x1620.
    """
    cmd = [ffmpeg_exe(), "-v", "error"]

    if start:
        cmd += ["-ss", start]
    cmd += ["-i", path]
    if duration:
        cmd += ["-t", duration]

    px = "rgb24" if color else "gray"
    chain = "fps=%s" % fps

    if denoise:
        chain += ",hqdn3d=1.5:1.5:8:8"

    chain += (",scale=%d:%d:force_original_aspect_ratio=increase,crop=%d:%d,format=%s"
              % (cols, rows, cols, rows, px))

    cmd += ["-vf", chain, "-f", "rawvideo", "-pix_fmt", px, "-"]

    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    want = cols * rows * (3 if color else 1)

    try:
        while True:
            buf = proc.stdout.read(want)
            if len(buf) < want:
                break
            yield buf
    finally:
        proc.stdout.close()
        err = proc.stderr.read().decode("utf-8", "replace").strip()
        proc.wait()
        if proc.returncode != 0 and err:
            sys.exit("ffmpeg failed:\n%s" % err)


def demo_frames(cols: int, rows: int, count: int):
    """Synthetic animation, so the command works before a real video is used."""
    cx, cy = cols / 2.0, rows / 2.0

    for f in range(count):
        t = f / count
        r = 2.0 + (min(cols, rows * 2) / 2.0 - 2.0) * t
        box = (f * 3) % max(1, cols - 6)
        out = bytearray(cols * rows)

        for y in range(rows):
            for x in range(cols):
                dx, dy = x - cx, (y - cy) * 2.0
                inside = abs(math.hypot(dx, dy) - r) < 1.2
                if box <= x < box + 6 and (rows // 2 - 3) <= y < (rows // 2 + 3):
                    inside = True
                out[y * cols + x] = 255 if inside else 0

        yield bytes(out)


def main() -> None:
    p = argparse.ArgumentParser(description="Encode frames for 'fastboot oem badapple'")
    p.add_argument("input", nargs="?", help="video file (omit with --demo)")
    p.add_argument("-o", "--output", default="board/tecno/badapple_frames.h")
    p.add_argument("--blob", metavar="FILE",
                   help="also write the raw frame stream to FILE (for the partition-"
                        "backed player); the C header then omits the data array")
    p.add_argument("-c", "--cols", type=int, default=180)
    p.add_argument("-r", "--rows", type=int, default=410)
    p.add_argument("-f", "--fps", type=float, default=8.0)
    p.add_argument("-t", "--threshold", type=int, default=128)
    p.add_argument("-s", "--start", help="start timestamp, e.g. 00:14")
    p.add_argument("-d", "--duration", help="duration, e.g. 00:30")
    p.add_argument("-n", "--count", type=int, help="stop after N frames")
    p.add_argument("--invert", action="store_true", help="swap on/off pixels")
    p.add_argument("--dither", action="store_true",
                   help="ordered (Bayer) dithering instead of a hard threshold")
    p.add_argument("--color", action="store_true",
                   help="RGB565 frames instead of 1-bit (colour; ~10-20x the data)")
    p.add_argument("--quant", type=int, default=5, metavar="BITS",
                   help="colour only: keep BITS per channel (5 = native RGB565, "
                        "4 trims video noise so deltas shrink)")
    p.add_argument("--denoise", action="store_true",
                   help="temporal denoise (hqdn3d) before scaling, for picture quality; "
                        "it does NOT shrink the dirty rectangle (measured)")
    p.add_argument("--demo", action="store_true", help="generate a synthetic animation")
    p.add_argument("--char", default="#", help="character the player draws for 'on'")
    args = p.parse_args()

    if not args.demo and not args.input:
        p.error("an input video is required unless --demo is used")
    if args.cols < 8 or args.cols > 1024 or args.rows < 2 or args.rows > 1024:
        p.error("cols must be 8..1024 and rows 2..1024")

    bpp = 2 if args.color else 1
    # 1-bit rows are packed into bytes; colour rows are bpp bytes per pixel.
    row_bytes = (args.cols + 7) // 8 if bpp == 1 else args.cols * bpp
    frame_bytes = row_bytes * args.rows

    # Per-channel quantisation mask for colour (RGB565 has 5/6/5 bits).
    qmask = 0xFFFF

    if 1 <= args.quant < 5:
        rb, gb, bb = min(5, args.quant), min(6, args.quant), min(5, args.quant)
        qmask = (((0x1F << (5 - rb)) & 0x1F) << 11) | \
                (((0x3F << (6 - gb)) & 0x3F) << 5) | \
                ((0x1F << (5 - bb)) & 0x1F)

    if args.demo:
        raw_frames = demo_frames(args.cols, args.rows, args.count or 48)
    else:
        raw_frames = read_raw_frames(args.input, args.cols, args.rows, args.fps,
                                     args.color, args.start, args.duration, args.denoise)

    offsets = [0]
    blob = bytearray()
    prev = bytes(frame_bytes)
    count = 0

    for raw in raw_frames:
        if args.color:
            cur = frame_from_rgb565(raw, args.cols, args.rows, qmask)
        else:
            cur = frame_from_gray(raw, args.cols, args.rows, args.threshold, args.invert,
                                  args.dither)
        delta = xor_bytes(cur, prev)
        blob += packbits(delta)
        prev = cur
        count += 1
        offsets.append(len(blob))

        if args.count and count >= args.count:
            break
        if count % 50 == 0:
            print("  %d frames, %d bytes..." % (count, len(blob)), file=sys.stderr)

    if count == 0:
        sys.exit("no frames were produced")

    with open(args.output, "w") as fh:
        fh.write("//\n// Generated by utils/badapple.py -- do not edit.\n//\n\n")
        fh.write("#define BADAPPLE_W %d\n" % args.cols)
        fh.write("#define BADAPPLE_H %d\n" % args.rows)
        fh.write("#define BADAPPLE_BPP %d\n" % bpp)
        fh.write("#define BADAPPLE_FPS %d\n" % int(round(args.fps)))
        fh.write("#define BADAPPLE_FRAMES %d\n" % count)
        fh.write("#define BADAPPLE_CHAR '%s'\n" % args.char)
        fh.write("\nstatic const uint32_t badapple_offsets[BADAPPLE_FRAMES + 1] = {\n")
        for i in range(0, len(offsets), 16):
            fh.write("    " + ", ".join(str(o) for o in offsets[i:i + 16]) + ",\n")
        fh.write("};\n")

        if args.blob:
            # When the frames live in a partition, the payload only needs the
            # geometry and the offset index, not the data itself.
            fh.write("\n// frame data lives in the '%s' partition\n" % args.blob)
        else:
            fh.write("\nstatic const uint8_t badapple_data[] = {\n")
            for i in range(0, len(blob), 16):
                fh.write("    " + ", ".join("0x%02x" % b for b in blob[i:i + 16]) + ",\n")
            fh.write("};\n")

    if args.blob:
        with open(args.blob, "wb") as fh:
            fh.write(blob)
        print("blob: %d bytes -> %s" % (len(blob), args.blob))

    print("%d frames, %dx%d, %d bytes (%d KiB) -> %s"
          % (count, args.cols, args.rows, len(blob), len(blob) // 1024, args.output))


if __name__ == "__main__":
    main()
