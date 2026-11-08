//
// SPDX-FileCopyrightText: 2026 naden01 <naden.irsyad01@gmail.com>
// SPDX-FileCopyrightText: 2026 KanagawaYamada <albert.wesley.dion@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <board_ops.h>
#include <lib/bootloader_message.h>
#include <lib/fastboot.h>
#include <lib/common.h>
#include <lib/thread.h>
#include <main/main.h>
#include <wdt/mtk_wdt.h>

#include "badapple_frames.h"

// MTK hardware keycodes, confirmed on this device with 'fastboot oem kscan':
//   Vol+ = 0, Vol- = 1, Power = 8
#define KEY_VOLUME_UP   0
#define KEY_VOLUME_DOWN 1
#define KEY_POWER       8

// Set while 'oem badapple' is playing so the boot-menu thread stands down
// instead of reacting to the same key presses and redrawing over the video.
static volatile int badapple_active;

// Set while a diagnostic repaints frames without pushing them to the panel.
static int badapple_no_present;

// Present the whole picture instead of just the repainted rectangle. The panel is
// only guaranteed to show a complete frame if the pushed region is the whole
// picture; many small overlapping region pushes can leave it showing a mix of the
// current and previous frame. Selected with 'oem badapple full'.
static int badapple_present_full;

// No timeout: the menu runs in a thread alongside fastboot, so it can sit
// there for as long as the bootloader session lasts.

// What each board patch found at boot (0 = pattern did not match). Stored then,
// because the patches overwrite the patterns they match.
enum {
    PATCH_CCCI,
    PATCH_ORANGE,
    PATCH_GREEN,
    PATCH_VBMETA,
    PATCH_COUNT
};
static uint32_t g_patch_addr[PATCH_COUNT];

// Ported from the Infinix X670 board (transsion-graveyard/kaeru_infinix_X670).
// Its byte pattern matches this LK exactly, so the function is the same CCCI
// modem-image verifier; forcing it to return 0 lets modem (md1rom/md3rom/...)
// images be loaded without passing signature verification, matching what the
// patcher already does for the other partitions.
void board_early_init(void) {
    printf("Entering early init for Tecno Pova 5 Pro 5G\n");

    uint32_t addr = SEARCH_PATTERN(LK_START, LK_END, 0xE92D, 0x41F0, 0x460A, 0x4604);
    g_patch_addr[PATCH_CCCI] = addr;

    if (addr) {
        printf("Found ccci_ld_md_sec_ptr_hdr_verify at 0x%08X\n", addr);
        FORCE_RETURN(addr, 0);
    } else {
        printf("ccci_ld_md_sec_ptr_hdr_verify: NOT FOUND\n");
    }
}

static void video_clean_screen(void) {
    ((void (*)(void))(0x4823cb74 | 1))();
}

static void video_set_cursor(int row, int col) {
    ((void (*)(int, int))(0x4823ca20 | 1))(row, col);
}

// Push a rectangle of the framebuffer to the panel. These panels are
// command-mode: the controller keeps showing whatever was pushed to it last, so
// every drawing path has to present itself. The Bad Apple player originally did
// not, and the screen only changed when the boot-menu thread happened to push --
// which looks exactly like uneven pacing plus fragments of earlier frames.
static void mt_disp_update_rect(int x, int y, int w, int h) {
    ((void (*)(uint32_t, uint32_t, uint32_t, uint32_t))(0x48200e8c | 1))(
            (uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h);
}

static void mt_disp_update(void) {
    mt_disp_update_rect(0, 0, 1080, 2460);
}

static void mdelay(unsigned long ms) {
    ((void (*)(unsigned long))(0x4825F554 | 1))(ms);
}

static void cmd_aria(const char* arg, void* data, unsigned sz) {
    // 1. Send text to the PC Terminal over USB (Synced with phone screen)
    fastboot_info("After all... even though we're friends,");
    fastboot_info("Aria the idol is sure to bring you way");
    fastboot_info("more surprises down the road.");

    // 2. Render text directly onto the phone's physical screen
    video_clean_screen();
    video_set_cursor(10, 0); // Start at row 10
    video_printf("After all... even though we're friends,\n");
    video_printf("Aria the idol is sure to bring you way\n");
    video_printf("more surprises down the road.\n");
    mt_disp_update(); // Push framebuffer to display hardware

    fastboot_okay("");
}

static void cmd_sunna(const char* arg, void* data, unsigned sz) {
    fastboot_info("Pfft- That's not crying, Xiao'Yu!");
    fastboot_info("That's just a whole performance");
    fastboot_info("piece with your face!");

    video_clean_screen();
    video_set_cursor(10, 0);
    video_printf("Pfft- That's not crying, Xiao'Yu!\n");
    video_printf("That's just a whole performance\n");
    video_printf("piece with your face!\n");
    mt_disp_update();

    fastboot_okay("");
}

static void cmd_nangongyu(const char* arg, void* data, unsigned sz) {
    fastboot_info("It's just a meet-and-greet,");
    fastboot_info("why so nervous? Worried");
    fastboot_info("you'll fall for me?");

    video_clean_screen();
    video_set_cursor(10, 0);
    video_printf("It's just a meet-and-greet,\n");
    video_printf("why so nervous? Worried\n");
    video_printf("you'll fall for me?\n");
    mt_disp_update();

    fastboot_okay("");
}

static void cmd_aod(const char* arg, void* data, unsigned sz) {
    fastboot_info("Yume o tsukande hanasanaide");
    fastboot_info("Kogare akogare utau no");
    fastboot_info("Shibireru guzo ni koi shiteru");
    fastboot_info("Jonetsu wa kitto");
    fastboot_info("uso ni wa naranai kara");

    video_clean_screen();
    video_set_cursor(10, 0);
    video_printf("Yume o tsukande hanasanaide\n");
    video_printf("Kogare akogare utau no\n");
    video_printf("Shibireru guzo ni koi shiteru\n");
    video_printf("Jonetsu wa kitto\n");
    video_printf("uso ni wa naranai kara\n");
    mt_disp_update();

    fastboot_okay("");
}

static void cmd_yamada(const char* arg, void* data, unsigned sz) {
    fastboot_info("1 + 1 = 2");
    fastboot_info("");
    fastboot_info("Hi, my name is Kanagawa Yamada.");
    fastboot_info("You can call me Yamada,");
    fastboot_info("VTeacher from Indonesia");
    fastboot_info("");
    fastboot_info("YT: @KanagawaYamada");

    video_clean_screen();
    video_set_cursor(10, 0);
    video_printf("1 + 1 = 2\n\n");
    video_printf("Hi, my name is Kanagawa Yamada.\n");
    video_printf("You can call me Yamada,\n");
    video_printf("VTeacher from Indonesia\n\n");
    video_printf("YT: @KanagawaYamada\n");
    mt_disp_update();

    fastboot_okay("");
}

static void cmd_naze(const char* arg, void* data, unsigned sz) {
    fastboot_info("naze was here :3");
    fastboot_okay("");
}

// Diagnostic: reports which hardware keycode is currently held, so the
// KEY_* defines above can be verified on a new device. Note the name: LK
// already registers 'oem key', and any prefix starting with that would be
// swallowed by it (hence 'oem kscan').
static void cmd_keys(const char* arg, void* data, unsigned sz) {
    char buf[32];
    int last = -1;

    fastboot_info("Hold a button; reporting its key index for 10s...");

    for (int t = 0; t < 100; t++) {
        int cur = -1;

        for (int k = 0; k <= 31; k++) {
            if (mtk_detect_key(k)) {
                cur = k;
                break;
            }
        }

        if (cur != last) {
            if (cur < 0) {
                fastboot_info("(released)");
            } else {
                npf_snprintf(buf, sizeof(buf), "key %d pressed", cur);
                fastboot_info(buf);
            }
            last = cur;
        }

        mdelay(100);
    }

    fastboot_okay("");
}

// Registered from the .fastboot_cmds section so it is available on every
// boot (including fastboot mode) via kaeru's early command registration.
FASTBOOT_CMD(kscan, "oem kscan", cmd_keys, 1);

// ---------------------------------------------------------------------------
// 'fastboot oem badapple': plays the 1-bit frames produced by
// utils/badapple.py straight into the panel framebuffer (real pixels, no text
// grid). The framebuffer is the reserved region the bootloader hands to the
// kernel; on LH8n it is mblock-19-framebuffer at 0x76950000.
// ---------------------------------------------------------------------------
#if BADAPPLE_BPP == 1
#define BADAPPLE_ROW_BYTES ((BADAPPLE_W + 7) / 8)
#else
#define BADAPPLE_ROW_BYTES (BADAPPLE_W * BADAPPLE_BPP)
#endif
#define BADAPPLE_FRAME_BYTES (BADAPPLE_ROW_BYTES * BADAPPLE_H)

// Frames are streamed out of a partition instead of embedded, so the payload
// stays ~small and stage2's 128 KiB/640 KiB ceiling stops applying. Write the
// blob from utils/badapple.py's --blob with e.g.
//   adb shell "su -c 'dd if=/sdcard/badapple.bin of=/dev/block/by-name/tranfs'"
#define BADAPPLE_STREAM 1
#define BADAPPLE_PART "tranfs"
#define BADAPPLE_PART_OFFSET 0

// Read-ahead window. Bigger = fewer partition reads (each costs ~10 ms regardless
// of size), but it is BSS: the payload's whole RAM budget is 640 KiB and the
// 540x405 colour frame buffer is 427 KiB, so this is kept at 64 KiB (~0.5 reads
// per frame at ~31 KB/delta, which is a couple of ms).
#define BADAPPLE_MAX_CHUNK 65536

// Probe/benchmark reads are always one sector.
#define BADAPPLE_IO_UNIT 4096

// Cap on the integer upscale; 0 = fill the screen with the largest integer scale
// that fits. The renderer pays for the *output* area (every blit writes,
// cache-flushes and presents the changed rectangle), so this is the smoothness
// lever if the phases below say the blit dominates. 3 = full width (1080x1620)
// for a 360x540 grid, 2 = 720x1080.
#define BADAPPLE_MAX_SCALE 3

#define FB_BASE 0x76950000UL
#define FB_WIDTH 1080
#define FB_HEIGHT 2460
// 1080 px * 4 B rounded up to a 64 B alignment, in pixels (matches the
// kernel's stride of 1088 px for this panel).
#define FB_STRIDE 1088

#define FB_FG 0xffffffffUL
#define FB_BG 0xff000000UL

static uint8_t badapple_frame[BADAPPLE_FRAME_BYTES];

#if BADAPPLE_STREAM
static uint8_t badapple_chunk[BADAPPLE_IO_UNIT];

// Read-ahead window: on this device a partition read costs ~10 ms regardless of
// size (measured: 32 x 4 KiB = 320 ms), so reading per frame would eat a big
// slice of the frame budget. One 128 KiB read serves many frames instead.
static uint8_t badapple_win[BADAPPLE_MAX_CHUNK];
static uint32_t badapple_win_base;
static uint32_t badapple_win_len;

// The current frame's delta range in the blob, and the read cursor inside it.
static uint32_t badapple_rd_pos;
static uint32_t badapple_rd_end;

// Rough per-phase cost accounting, printed when playback ends: without it we are
// guessing whether playback is read-, decode- or paint-bound.
static uint32_t badapple_prof_read;
static uint32_t badapple_prof_unpack;
static uint32_t badapple_prof_blit;
static uint32_t badapple_prof_reads;
static uint32_t badapple_prof_bytes;
static uint32_t badapple_prof_paint;
static uint32_t badapple_prof_present;
static uint32_t badapple_prof_kpix;
static uint32_t badapple_prof_dbytes;
static uint32_t badapple_prof_deltas;
static uint32_t badapple_prof_rows;
static uint32_t badapple_prof_cols;
static uint32_t badapple_prof_rects;

static uint32_t badapple_ticks(void);

// The single integer upscale used for both axes, capped by BADAPPLE_MAX_SCALE.
static int badapple_scale(void) {
    int s = FB_WIDTH / BADAPPLE_W;
    int sy = FB_HEIGHT / BADAPPLE_H;

    if (sy < s) {
        s = sy;
    }

#if BADAPPLE_MAX_SCALE > 0
    if (s > BADAPPLE_MAX_SCALE) {
        s = BADAPPLE_MAX_SCALE;
    }
#endif

    return s;
}

// LK's partition helpers, the same ones stage1 uses to load the payload.
// A partition that is not there reads as size 0 / -1, never as stale data.
static uint64_t part_size(const char* name) {
    return ((uint64_t (*)(const char*))(CONFIG_PARTITION_GET_SIZE_BY_NAME_ADDRESS | 1))(name);
}

static long part_read(const char* name, uint64_t off, uint8_t* buf, size_t len) {
    long n = ((long (*)(const char*, long long, uint8_t*, size_t))(
                      CONFIG_PARTITION_READ_ADDRESS | 1))(name, (long long)off, buf, len);

    // The read is a DMA-style transfer, so drop any stale cache lines.
    if (n > 0) {
        arch_clean_invalidate_cache_range((uint32_t)buf, (uint32_t)n);
    }

    return n;
}

static uint64_t badapple_part_size(void) {
    return part_size(BADAPPLE_PART);
}

static long badapple_part_read(uint64_t off, uint8_t* buf, size_t len) {
    return part_read(BADAPPLE_PART, off, buf, len);
}

// Next byte of the current frame's delta: 1 = byte, 0 = end of delta,
// -1 = the partition read failed.
//
// The window deliberately does *not* have to hold a whole delta: a delta can be
// nearly as large as the frame itself (a 360x540 colour frame is 380 KiB), so
// this refills mid-frame rather than refusing the frame. Refusing is what made
// playback abort mid-clip at 64 KiB with the earlier per-frame design.
static int badapple_rd_next(uint8_t* out) {
    if (badapple_rd_pos >= badapple_rd_end) {
        return 0;
    }

    if (badapple_rd_pos < badapple_win_base ||
        badapple_rd_pos >= badapple_win_base + badapple_win_len) {
        // Read ahead past the *end of this delta* -- up to the whole blob. Taking
        // the frame boundary as the read bound cost one ~27 ms partition read per
        // frame (measured: 590 reads for 579 deltas = 16.2 s of a 19.6 s run).
        // Crossing frame boundaries makes one window serve several frames.
        const uint32_t total = badapple_offsets[BADAPPLE_FRAMES];
        uint32_t n = BADAPPLE_MAX_CHUNK;

        if (badapple_rd_pos + n > total) {
            n = total - badapple_rd_pos;
        }

        uint32_t t0 = badapple_ticks();

        if (badapple_part_read(BADAPPLE_PART_OFFSET + badapple_rd_pos, badapple_win, n) !=
            (long)n) {
            return -1;
        }

        badapple_prof_read += badapple_ticks() - t0;
        badapple_prof_reads++;
        badapple_prof_bytes += n;

        badapple_win_base = badapple_rd_pos;
        badapple_win_len = n;
    }

    *out = badapple_win[badapple_rd_pos - badapple_win_base];
    badapple_rd_pos++;
    return 1;
}
#endif

// Rows/columns touched by the last decoded delta, so a frame only redraws
// (and only cache-flushes) what actually changed.
static int badapple_first_row;
static int badapple_last_row;
static int badapple_first_x;
static int badapple_last_x;

// One expanded framebuffer row, reused for the s copies of each source row.
static uint32_t badapple_line[FB_WIDTH];

// One framebuffer pixel for source pixel x of an already-decoded row.
static inline uint32_t badapple_pixel(const uint8_t* row, int x) {
#if BADAPPLE_BPP == 2
    // RGB565 little-endian -> the panel's ARGB8888 (alpha must be 0xFF, else
    // the display ignores the pixel and everything looks black).
    uint32_t v = (uint32_t)row[2 * x] | ((uint32_t)row[2 * x + 1] << 8);
    uint32_t r = (v >> 11) & 0x1f, g = (v >> 5) & 0x3f, b = v & 0x1f;

    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);

    return 0xff000000u | (r << 16) | (g << 8) | b;
#else
    return (row[x >> 3] & (0x80 >> (x & 7))) ? FB_FG : FB_BG;
#endif
}

// Forget the accumulated dirty rectangle. Called after a blit has brought the
// screen up to date, so the next batch of deltas starts clean.
static void badapple_rect_reset(void) {
    badapple_first_row = BADAPPLE_H;
    badapple_last_row = -1;
    badapple_first_x = BADAPPLE_W;
    badapple_last_x = -1;
}

// XOR the PackBits'd delta into the working frame: run codes skip (XOR with
// zero is a no-op), literals are XORed in. 0 = ok, -1 = the source failed.
//
// The dirty rectangle is *accumulated* across every delta applied since the last
// blit, and deliberately not reset here: when playback catches up it applies
// several deltas per pass, and resetting per delta would repaint only the last
// one's region -- leaving the earlier ones on screen as "the previous frame".
static int badapple_unpack(void) {
    int i = 0;

    while (i < BADAPPLE_FRAME_BYTES) {
        uint8_t b;
        int r = badapple_rd_next(&b);

        if (r < 0) {
            return -1;
        }
        if (r == 0) {
            break;
        }

        signed char c = (signed char)b;

        if (c >= 0) {
            int n = c + 1;

            while (n-- > 0 && i < BADAPPLE_FRAME_BYTES) {
                if (badapple_rd_next(&b) <= 0) {
                    break;
                }

                if (b) {
                    badapple_frame[i] ^= b;

                    int row = i / BADAPPLE_ROW_BYTES;
#if BADAPPLE_BPP == 2
                    int px = (i % BADAPPLE_ROW_BYTES) / 2;

                    if (px < badapple_first_x) {
                        badapple_first_x = px;
                    }
                    if (px > badapple_last_x) {
                        badapple_last_x = px;
                    }
#else
                    int bx = (i % BADAPPLE_ROW_BYTES) * 8;

                    if (bx < badapple_first_x) {
                        badapple_first_x = bx;
                    }
                    if (bx + 7 > badapple_last_x) {
                        badapple_last_x = bx + 7;
                    }
#endif

                    if (row < badapple_first_row) {
                        badapple_first_row = row;
                    }
                    if (row > badapple_last_row) {
                        badapple_last_row = row;
                    }
                }

                i++;
            }
        } else if (c != -128) {
            i += 1 - c;

            if (i > BADAPPLE_FRAME_BYTES) {
                i = BADAPPLE_FRAME_BYTES;
            }
        }
    }

    if (badapple_last_x >= BADAPPLE_W) {
        badapple_last_x = BADAPPLE_W - 1;
    }

    return 0;
}

// Fetch frame n's delta and XOR it into the working frame. The bytes come from
// the partition via the windowed reader above.
static int badapple_unpack_frame(int n) {
    uint32_t off = badapple_offsets[n];

    badapple_rd_pos = off;
    badapple_rd_end = badapple_offsets[n + 1];
    badapple_prof_dbytes += badapple_rd_end - off;
    badapple_prof_deltas++;

    if (badapple_unpack() != 0) {
        printf("badapple: frame %d read failed (off=%u len=%u)\n", n, (unsigned)off,
               (unsigned)(badapple_rd_end - off));
        return -1;
    }

    return 0;
}

// Fill the panel black once, so the area around the clip (if the grid is not
// the panel's aspect) is letterboxed rather than stale.
static void badapple_clear(void) {
    uint32_t* p = (uint32_t*)FB_BASE;
    uint32_t n = (uint32_t)FB_HEIGHT * FB_STRIDE;

    for (uint32_t i = 0; i < n; i++) {
        p[i] = FB_BG;
    }

    arch_clean_invalidate_cache_range(FB_BASE, n * 4);
    mt_disp_update();
}

// Nearest-neighbour upscale of the 1-bit frame into the framebuffer. A single
// integer scale is used on both axes and the image is centred, so the clip is
// never stretched. Only the rows the delta touched are redrawn and flushed.
static void badapple_blit(void) {
    if (badapple_last_row < 0) {
        return;  // identical to the previous frame
    }

    int s = badapple_scale();

    if (s < 1) {
        return;
    }

    const int ox = (FB_WIDTH - BADAPPLE_W * s) / 2;
    const int oy = (FB_HEIGHT - BADAPPLE_H * s) / 2;
    const int x0 = badapple_first_x;
    const int x1 = badapple_last_x;
    uint32_t t_paint = badapple_ticks();

    for (int y = badapple_first_row; y <= badapple_last_row; y++) {
        const uint8_t* row = badapple_frame + y * BADAPPLE_ROW_BYTES;

        // Expand only the changed column range, then memcpy it s times.
        int w = 0;

        for (int x = x0; x <= x1; x++) {
            uint32_t c = badapple_pixel(row, x);

            for (int j = 0; j < s; j++) {
                badapple_line[w++] = c;
            }
        }

        uint32_t* dst =
                (uint32_t*)(FB_BASE + (uint32_t)(oy + y * s) * FB_STRIDE * 4) + ox + x0 * s;

        for (int k = 0; k < s; k++) {
            memcpy(dst, badapple_line, (size_t)w * 4);
            dst += FB_STRIDE;
        }
    }

    // Push just the painted span of each changed row to RAM. Clean, not
    // clean+invalidate: the lines stay cached, and consecutive frames usually
    // redraw an overlapping rectangle. Whole-row flushes cost several times more
    // than the pixels actually written when the change is a narrow band.
    {
        const uint32_t span = (uint32_t)((x1 - x0 + 1) * s) * 4;
        uint32_t base = FB_BASE +
                ((uint32_t)(oy + badapple_first_row * s) * FB_STRIDE +
                 (uint32_t)(ox + x0 * s)) * 4;

        for (int r = badapple_first_row; r <= badapple_last_row; r++) {
            for (int k = 0; k < s; k++) {
                arch_clean_cache_range(base, span);
                base += FB_STRIDE * 4;
            }
        }
    }

    badapple_prof_paint += badapple_ticks() - t_paint;
    badapple_prof_kpix += (uint32_t)((badapple_last_row - badapple_first_row + 1) * s) *
                          (uint32_t)((x1 - x0 + 1) * s) / 1024;
    badapple_prof_rows += (uint32_t)(badapple_last_row - badapple_first_row + 1);
    badapple_prof_cols += (uint32_t)(x1 - x0 + 1);
    badapple_prof_rects++;

    // Present just the rectangle we repainted (see mt_disp_update_rect), or the
    // whole picture when 'oem badapple full' asked for guaranteed-complete frames.
    if (!badapple_no_present) {
        uint32_t t0 = badapple_ticks();

        if (badapple_present_full) {
            mt_disp_update_rect(ox, oy, BADAPPLE_W * s, BADAPPLE_H * s);
        } else {
            mt_disp_update_rect(ox + x0 * s, oy + badapple_first_row * s, (x1 - x0 + 1) * s,
                                (badapple_last_row - badapple_first_row + 1) * s);
        }

        badapple_prof_present += badapple_ticks() - t0;
    }
}

// Pixels of the picture area in RAM that differ from the decoded frame, i.e.
// stale content the display would still be scanning out. Used by 'verify'.
static uint32_t badapple_count_stale(int s, int ox, int oy, int* bad_y, int* bad_x) {
    uint32_t bad = 0;

    *bad_y = -1;
    *bad_x = -1;

    // Clean+invalidate first: write our dirty lines out, then read back the RAM
    // the display engine actually scans out.
    arch_clean_invalidate_cache_range(FB_BASE + (uint32_t)oy * FB_STRIDE * 4,
                                      (uint32_t)(BADAPPLE_H * s) * FB_STRIDE * 4);

    for (int y = 0; y < BADAPPLE_H; y++) {
        const uint8_t* row = badapple_frame + y * BADAPPLE_ROW_BYTES;

        for (int x = 0; x < BADAPPLE_W; x++) {
            const uint32_t c = badapple_pixel(row, x);
            const uint32_t* dp =
                    (const uint32_t*)(FB_BASE + (uint32_t)(oy + y * s) * FB_STRIDE * 4) +
                    ox + x * s;

            for (int k = 0; k < s; k++) {
                for (int j = 0; j < s; j++) {
                    if (dp[j] != c) {
                        if (!bad) {
                            *bad_y = y;
                            *bad_x = x;
                        }
                        bad++;
                    }
                }

                dp += FB_STRIDE;
            }
        }
    }

    return bad;
}

// LK timer: 0x4825F38C reads the current tick; 0x4825F500 converts ms -> ticks
// (both inside the LK's mdelay path).
static uint32_t badapple_ticks(void) {
    return ((uint32_t (*)(void))(0x4825F38C | 1))();
}

// Spin the clip on the panel until a key is pressed: the playback loop shared by
// 'oem badapple' and the menu entry. 'start' is the wall-clock origin and 'shown'
// the last frame already on screen (-1 when the screen is blank). The deltas
// applied and the blits drawn come back for the caller's report.
static void badapple_run(uint32_t start, uint32_t ticks_per_ms, uint32_t fps,
                         uint32_t period_ms, int shown, uint32_t* out_advanced,
                         uint32_t* out_drawn) {
    uint32_t advanced = 0;
    uint32_t drawn = 0;

    // The frame index is derived from the wall clock, not from a per-frame
    // delay: if a frame renders slowly we skip the ones we missed, so the clip
    // always keeps the source's timing instead of playing back slow.
    for (;;) {
        if (mtk_detect_key(KEY_POWER) || mtk_detect_key(KEY_VOLUME_UP) ||
            mtk_detect_key(KEY_VOLUME_DOWN)) {
            break;
        }

        uint32_t elapsed_ms = ticks_per_ms ? (badapple_ticks() - start) / ticks_per_ms : 0;
        uint32_t loop_ms = period_ms ? elapsed_ms % period_ms : 0;
        int target = (int)((loop_ms * fps) / 1000);

        if (target != shown) {
            int err = 0;
            uint32_t t0 = badapple_ticks();

            // Apply every delta between what is on screen and now (cheap), then
            // draw once.
            while (shown != target) {
                if (++shown >= BADAPPLE_FRAMES) {
                    shown = 0;
                    memset(badapple_frame, 0, sizeof(badapple_frame));
                    badapple_clear();
                    badapple_rect_reset();
                }

                if (badapple_unpack_frame(shown) != 0) {
                    err = 1;
                    break;
                }

                advanced++;
            }

            badapple_prof_unpack += badapple_ticks() - t0;

            if (err) {
                break;
            }

            t0 = badapple_ticks();
            badapple_blit();
            badapple_prof_blit += badapple_ticks() - t0;
            drawn++;

            // The screen is up to date now, so the next batch starts a new rect.
            badapple_rect_reset();
        }

        mdelay(2);
    }

    *out_advanced = advanced;
    *out_drawn = drawn;
}

static void cmd_badapple(const char* arg, void* data, unsigned sz) {
    const uint32_t ticks_per_ms = ((uint32_t (*)(uint32_t))(0x4825F500 | 1))(1);
    const uint32_t fps = BADAPPLE_FPS > 0 ? BADAPPLE_FPS : 30;
    const uint32_t period_ms = ((uint32_t)BADAPPLE_FRAMES * 1000) / fps;
    uint32_t start = badapple_ticks();
    int shown = -1;
    int from = 0;

    // 'oem badapple at <frame>' starts playback there. The deltas are an XOR
    // chain, so the earlier ones are replayed (invisibly) first -- that takes
    // about 1 ms/frame, then the whole picture is pushed once so the screen
    // matches the frame we start from.
    if (arg) {
        const char* p = strstr(arg, "at ");

        if (p) {
            p += 3;
            while (*p >= '0' && *p <= '9') {
                from = from * 10 + (*p++ - '0');
            }
            if (from < 0 || from >= BADAPPLE_FRAMES) {
                from = 0;
            }
        }
    }

    fastboot_info("Bad Apple!! -- press any key to stop");

#if BADAPPLE_STREAM
    // Pre-flight: is the partition visible, and is the blob actually there?
    {
        char msg[96];
        uint64_t psize = badapple_part_size();

        npf_snprintf(msg, sizeof(msg), "%s: %u KiB", BADAPPLE_PART,
                     (unsigned)(psize / 1024));
        fastboot_info(msg);

        if (psize == 0) {
            fastboot_fail("Party partition not found");
            return;
        }

        if (badapple_part_read(BADAPPLE_PART_OFFSET, badapple_chunk, 16) == 16) {
            npf_snprintf(msg, sizeof(msg), "blob: %02x %02x %02x %02x %02x %02x %02x %02x",
                         badapple_chunk[0], badapple_chunk[1], badapple_chunk[2],
                         badapple_chunk[3], badapple_chunk[4], badapple_chunk[5],
                         badapple_chunk[6], badapple_chunk[7]);
        } else {
            npf_snprintf(msg, sizeof(msg), "blob: probe read failed");
        }
        fastboot_info(msg);

        // How expensive is a partition read? (tells us if playback is read-bound)
        uint32_t t0 = badapple_ticks();

        for (int i = 0; i < 32; i++) {
            badapple_part_read(BADAPPLE_PART_OFFSET + (uint32_t)i * 4096,
                               badapple_chunk, 4096);
        }

        npf_snprintf(msg, sizeof(msg), "32x4K reads: %u ms",
                     ticks_per_ms ? (badapple_ticks() - t0) / ticks_per_ms : 0);
        fastboot_info(msg);
    }
#endif

    // 'oem badapple test' just validates our framebuffer assumptions.
    if (arg && strstr(arg, "test")) {
        for (int y = 0; y < FB_HEIGHT; y++) {
            uint32_t c = (y < FB_HEIGHT / 3) ? FB_FG : FB_BG;
            uint32_t* p = (uint32_t*)(FB_BASE + (uint32_t)y * FB_STRIDE * 4);

            for (int x = 0; x < FB_WIDTH; x++) {
                p[x] = c;
            }
        }

        arch_clean_invalidate_cache_range(FB_BASE, (uint32_t)FB_HEIGHT * FB_STRIDE * 4);
        mt_disp_update();
        fastboot_info("fb test: top third white, rest black");
        mdelay(4000);
        fastboot_okay("");
        return;
    }

    // 'oem badapple verify [start] [count]' replays the clip from the beginning
    // (the deltas are an XOR chain, so frame N needs all of them), repaints every
    // frame without pushing it to the panel, and after each frame in
    // [start, start+count) compares the picture *read back out of RAM* against
    // the decoded frame. Any stale pixel left by the dirty-rectangle logic shows
    // up here, at any point in the clip -- not just the opening frames.
    if (arg && strstr(arg, "verify")) {
        char msg[96];
        const char* p = strstr(arg, "verify") + 6;
        int start = 0;
        int count = 60;

        while (*p == ' ') {
            p++;
        }
        if (*p >= '0' && *p <= '9') {
            while (*p >= '0' && *p <= '9') {
                start = start * 10 + (*p++ - '0');
            }
            while (*p == ' ') {
                p++;
            }
            if (*p >= '0' && *p <= '9') {
                count = 0;
                while (*p >= '0' && *p <= '9') {
                    count = count * 10 + (*p++ - '0');
                }
            }
        }

        const int s = badapple_scale();

        if (s < 1) {
            fastboot_fail("scale 0");
            return;
        }
        if (start < 0 || start >= BADAPPLE_FRAMES) {
            fastboot_fail("start past the end");
            return;
        }
        if (count < 1) {
            count = 1;
        }
        if (start + count > BADAPPLE_FRAMES) {
            count = BADAPPLE_FRAMES - start;
        }

        const int ox = (FB_WIDTH - BADAPPLE_W * s) / 2;
        const int oy = (FB_HEIGHT - BADAPPLE_H * s) / 2;

        memset(badapple_frame, 0, sizeof(badapple_frame));
        badapple_clear();
        badapple_no_present = 1;
        badapple_rect_reset();

        uint32_t worst = 0;
        int bad_frame = -1;
        int bad_y = -1;
        int bad_x = -1;
        int decoded = 0;

        for (int f = 0; f < start + count; f++) {
            if (badapple_unpack_frame(f) != 0) {
                break;
            }

            badapple_blit();
            decoded = f + 1;
            badapple_rect_reset();

            if (f < start) {
                continue;
            }

            int fy = -1;
            int fx = -1;
            uint32_t bad = badapple_count_stale(s, ox, oy, &fy, &fx);

            if (bad > worst) {
                worst = bad;
            }
            if (bad && bad_frame < 0) {
                bad_frame = f;
                bad_y = fy;
                bad_x = fx;
            }
        }

        badapple_no_present = 0;

        if (decoded <= 0) {
            fastboot_fail("no frames decoded");
            return;
        }

        // Show the last frame we looked at (verify repaints without presenting).
        mt_disp_update_rect(ox, oy, BADAPPLE_W * s, BADAPPLE_H * s);

        npf_snprintf(msg, sizeof(msg), "verify %d..%d scale %d: worst %u bad px of %u", start,
                     decoded - 1, s, (unsigned)worst,
                     (unsigned)(BADAPPLE_W * BADAPPLE_H * s * s));
        fastboot_info(msg);

        if (bad_frame >= 0) {
            npf_snprintf(msg, sizeof(msg), "frame %d first bad at row %d col %d", bad_frame, bad_y,
                         bad_x);
            fastboot_info(msg);
            fastboot_fail("stale pixels in RAM");
        } else {
            fastboot_okay("framebuffer matched every verified frame");
        }

        return;
    }

    memset(badapple_frame, 0, sizeof(badapple_frame));

    // 'oem badapple full' presents the whole picture each frame instead of the
    // repainted rectangle (see badapple_present_full).
    if (arg && strstr(arg, "full")) {
        badapple_present_full = 1;
    }

    badapple_active = 1;
    badapple_clear();
    badapple_rect_reset();

    if (from > 0) {
        fastboot_info("seeking... (replaying deltas)");

        memset(badapple_frame, 0, sizeof(badapple_frame));
        badapple_no_present = 1;

        for (int f = 0; f < from; f++) {
            if (badapple_unpack_frame(f) != 0) {
                break;
            }
        }

        badapple_no_present = 0;

        // Push the whole picture once so the screen matches the frame we start on.
        badapple_first_row = 0;
        badapple_last_row = BADAPPLE_H - 1;
        badapple_first_x = 0;
        badapple_last_x = BADAPPLE_W - 1;
        badapple_blit();
        badapple_rect_reset();

        // Shift the wall-clock origin so the first target is `from`.
        start -= ((uint32_t)from * 1000u / fps) * ticks_per_ms;
        shown = from - 1;
    }

    uint32_t advanced = 0;
    uint32_t drawn = 0;

    badapple_run(start, ticks_per_ms, fps, period_ms, shown, &advanced, &drawn);

    // Measured pace: deltas/second lower than BADAPPLE_FPS means the renderer
    // (not the decoder) is the bottleneck and frames were skipped.
    {
        char msg[96];
        uint32_t ms = ticks_per_ms ? (badapple_ticks() - start) / ticks_per_ms : 0;

        npf_snprintf(msg, sizeof(msg), "%u deltas, %u blits, %u ms (%u fps of %u)",
                     (unsigned)advanced, (unsigned)drawn, (unsigned)ms,
                     (unsigned)(ms ? (advanced * 1000 + ms / 2) / ms : 0), (unsigned)fps);
        fastboot_info(msg);

        npf_snprintf(msg, sizeof(msg), "%u reads, %u KiB avg: read %u ms, decode %u ms",
                     (unsigned)badapple_prof_reads,
                     (unsigned)(badapple_prof_reads ?
                             (badapple_prof_bytes / 1024) / badapple_prof_reads : 0),
                     (unsigned)(ticks_per_ms ? badapple_prof_read / ticks_per_ms : 0),
                     (unsigned)(ticks_per_ms ?
                             (badapple_prof_unpack > badapple_prof_read ?
                                     badapple_prof_unpack - badapple_prof_read : 0) / ticks_per_ms : 0));
        fastboot_info(msg);

        npf_snprintf(msg, sizeof(msg), "paint %u ms (%u kpix/frame), present %u ms (%s)",
                     (unsigned)(ticks_per_ms ? badapple_prof_paint / ticks_per_ms : 0),
                     (unsigned)(drawn ? badapple_prof_kpix / drawn : 0),
                     (unsigned)(ticks_per_ms ? badapple_prof_present / ticks_per_ms : 0),
                     badapple_present_full ? "whole picture" : "changed rect");
        fastboot_info(msg);

        {
            uint32_t avg_d = badapple_prof_deltas ?
                    badapple_prof_dbytes / badapple_prof_deltas : 0;
            uint32_t avg_r = badapple_prof_rects ?
                    badapple_prof_rows / badapple_prof_rects : 0;
            uint32_t avg_c = badapple_prof_rects ?
                    badapple_prof_cols / badapple_prof_rects : 0;
            uint32_t pct = (avg_r * avg_c * 100) / (BADAPPLE_W * BADAPPLE_H);

            npf_snprintf(msg, sizeof(msg),
                         "avg delta %u B; avg rect %ux%u of %dx%d = %u%% of picture",
                         (unsigned)avg_d, (unsigned)avg_r, (unsigned)avg_c, (int)BADAPPLE_W,
                         (int)BADAPPLE_H, (unsigned)pct);
            fastboot_info(msg);
        }
    }

    badapple_active = 0;
    fastboot_okay("");
}

FASTBOOT_CMD(badapple, "oem badapple", cmd_badapple, 1);

// Reports what each board patch matched at boot (recorded then, because the
// patches overwrite the patterns they match). expdb only gets the LK log on
// abnormal boots, so this is the reliable way to check the port.
//
// The name is passed as a literal rather than looked up in a table: the
// payload is relocated by start.S, which fixes up the GOT only, so a global
// array of pointers would still hold link-time addresses.
static void report_patch(const char* name, uint32_t addr) {
    char buf[96];

    if (addr) {
        npf_snprintf(buf, sizeof(buf), "  %s @ 0x%08X", name, addr);
    } else {
        npf_snprintf(buf, sizeof(buf), "  %s NOT APPLIED", name);
    }

    fastboot_info(buf);
}

static void cmd_patches(const char* arg, void* data, unsigned sz) {
    fastboot_info("kaeru patches at boot:");

    report_patch("ccci_ld_md_sec_ptr_hdr_verify", g_patch_addr[PATCH_CCCI]);
    report_patch("orange_state_warning", g_patch_addr[PATCH_ORANGE]);
    report_patch("green_state", g_patch_addr[PATCH_GREEN]);
    report_patch("vbmeta_lock", g_patch_addr[PATCH_VBMETA]);

    fastboot_okay("");
}

FASTBOOT_CMD(patches, "oem patches", cmd_patches, 1);

// ---------------------------------------------------------------------------
// Fastboot-mode boot menu, mirroring board-RMX2156.c: a thread draws the menu
// and polls the keys while LK's fastboot loop keeps running, so fastboot is
// never blocked and there is no timeout to race against.
// Vol+/Vol- move the selection, Power invokes the selected action.
// ---------------------------------------------------------------------------
typedef struct {
    const char* label;
    uintptr_t action;
} menu_item_t;

// The reboot entries hand control to LK's own fastboot handlers, which then
// reset into the requested mode: they are the same handlers LK's 'continue',
// 'reboot-bootloader' and 'reboot-recovery' commands use. The Poweroff entry
// calls LK's own mt_power_off routine (the one its "mt_power_off new" log line
// comes from).
//
// The last four entries are kaeru's own. They are dispatched by hand in the
// menu thread (they are not LK functions), so their 'action' is unused.
static const menu_item_t menu_items[] = {
        {"Reboot System", 0x48230A2D},      // LK cmd_continue
        {"Reboot Recovery", 0x48230C6D},    // LK cmd_reboot_recovery
        {"Reboot Bootloader", 0x48231105},  // LK cmd_reboot_bootloader
        {"Poweroff", 0x4821C045},           // LK mt_power_off
        {"Backup boot images", 0},
        {"Restore boot images", 0},
        {"Flash staged image", 0},
        {"Copy partition", 0},
        {"Bad Apple!!", 0},
};

enum {
    MENU_REBOOT_SYSTEM,
    MENU_REBOOT_RECOVERY,
    MENU_REBOOT_BOOTLOADER,
    MENU_POWEROFF,
    MENU_BACKUP,
    MENU_RESTORE,
    MENU_FLASH,
    MENU_COPY,
    MENU_BADAPPLE,
};

#define MENU_COUNT ((int)ARRAY_SIZE(menu_items))

// The action in progress, shown on the status line while it runs. Plain
// literals rather than the table's labels: the labels live in an array of
// pointers this payload's relocation does not fix up (see the note below).
static const char* menu_doing(int sel) {
    switch (sel) {
        case MENU_REBOOT_SYSTEM:
            return "Reboot to System";
        case MENU_REBOOT_RECOVERY:
            return "Reboot to Recovery";
        case MENU_REBOOT_BOOTLOADER:
            return "Reboot to Bootloader";
        case MENU_POWEROFF:
            return "Powering off";
        case MENU_BACKUP:
            return "Backing up - do not power off";
        case MENU_RESTORE:
            return "Restoring - do not power off";
        case MENU_COPY:
            return "Copying - do not power off";
        case MENU_BADAPPLE:
            return "Bad Apple!!";
        default:
            return "Flash staged image";
    }
}

// On-device boot-image backup, defined below the menu (it draws its progress on
// the menu's status line).
static int kbk_backup(int ui);
static int kbk_restore(int ui);
static void kst_pick(int ui);
static void kcp_pick(int ui);

// Active slot letter, defined below; the menu shows it on the footer rule.
static char active_slot(void);

// Field width for the menu. Lines are right-aligned within it (padded on the
// LEFT), so every line's text ends at the same column: the block is flush
// with the right edge, like right-align in a word processor.
// Must be >= the longest line (the "Volume Up/Down ..." hint is 42).
#define MENU_COLS 42

// Column where the field ends (i.e. the right edge of the text). The menu
// itself starts at MENU_COL - MENU_COLS. Raise this to push the block further
// right; lower it if a line ever wraps.
// Column where the field ends, i.e. one past the last character. Calibrated
// on this panel: a 4-char marker at column 120 ran to the right edge and was
// still accepted, so the text layer is ~124 columns wide. (94 was only ~46%
// across; anything past ~124 is silently rejected by video_set_cursor.)
#define MENU_COL 124

// One-shot on-screen calibration: LK's video_set_cursor validates both
// arguments against runtime limits in RAM, so neither the panel geometry nor
// the argument names can be assumed. These markers reveal the mapping.
#define MENU_CALIBRATE 0

#if MENU_CALIBRATE
static void menu_calibrate(void) {
    char line[64];

    // The four words around LK's cursor-limit pair (video_set_cursor loads
    // them relative to 0x4833AC30); print them so the real limits are known.
    volatile uint32_t* w = (volatile uint32_t*)0x4833AC2C;

    video_set_cursor(1, 0);
    npf_snprintf(line, sizeof(line), "kaeru w: %u %u %u %u", w[0], w[1], w[2], w[3]);
    video_printf("%s\n", line);

    const int cols[] = {0, 60, 120, 180};

    for (int i = 0; i < 4; i++) {
        video_set_cursor(3 + i, cols[i]);
        npf_snprintf(line, sizeof(line), "<%d", cols[i]);
        video_printf("%s\n", line);
    }

    mt_disp_update();
}
#endif

// Power must be held this long to confirm, so a stray press during a flash
// cannot reboot the device.
#define MENU_HOLD_MS 1000

// Key poll period while the menu and pickers are idle. A tap is only seen if it
// is still down when the loop next samples the keys, so this bounds how short a
// press can still be missed. 20 ms is well under a human tap.
#define MENU_POLL_MS 20

// Holding a volume key slides the selection: one step on the press, a pause, then
// auto-repeat, so a tap moves once and a hold scrolls through the list.
#define MENU_REPEAT_DELAY_MS 400
#define MENU_REPEAT_MS       120

// Main-menu box rows, on the MENU_COLS-wide field menu_line() writes.
#define MENU_TOP     8
#define MENU_STATUS  (MENU_TOP + MENU_COUNT + 2)   // blank line, reused for status
#define MENU_HINT    (MENU_STATUS + 1)
#define MENU_BOTTOM  (MENU_HINT + 1)

// Stock video_printf() presents the whole 1080x2460 framebuffer after *every*
// call -- LK's video_puts tail-calls mt_disp_update_rect(0, 0, width, height).
// The menu redraws ~13 lines, so per-line printf means ~13 full-screen pushes
// (about a second here) after each key press, and the menu does not poll keys
// while a push is in flight. That is why taps were missed. Draw with the raw
// putc, which only touches the text buffer, and present once per redraw.
static void menu_putc(char c) {
    ((void (*)(char))(0x4823C7BC | 1))(c);
}

// The menu and both pickers only draw rows 2..26 of LK's 8x16 text grid, so
// presenting that band covers every write (menu_line pads each field and
// kst_clear blanks the block). A full-screen present is ~74 ms against ~26 ms.
#define MENU_BAND_H (27 * 16)

static void menu_present(void) {
    mt_disp_update_rect(0, 0, FB_WIDTH, MENU_BAND_H);
}

static void menu_line(int row, const char* s) {
    char buf[MENU_COLS + 1];
    int len = 0;

    while (s[len] && len < MENU_COLS) {
        len++;
    }

    // Pad on the left so the text is right-aligned inside the field; the
    // leading spaces also erase whatever the previous draw left behind.
    int i = 0;

    for (int pad = MENU_COLS - len; pad > 0; pad--) {
        buf[i++] = ' ';
    }

    for (int j = 0; j < len; j++) {
        buf[i++] = s[j];
    }

    buf[MENU_COLS] = '\0';

    // Set the start column for every line: a newline puts the cursor back at
    // column 0.
    video_set_cursor(row, MENU_COL - MENU_COLS);

    // Raw putc, not video_printf: printf would present the full panel here (see
    // menu_putc). The caller presents once for the whole redraw instead.
    for (int k = 0; k < MENU_COLS; k++) {
        menu_putc(buf[k]);
    }

    menu_putc('\n');
}

// One line of the main-menu box: 'left'/'right' are the rails ('|', or '+' on a
// rule) and 'body' is the text between them, padded to the inner width.
static void menu_box_line(int row, char left, char right, const char* body) {
    char buf[MENU_COLS + 1];
    int len = 0;
    int i = 0;

    while (body[len] && len < MENU_COLS - 2) {
        len++;
    }

    buf[i++] = left;

    for (int k = 0; k < MENU_COLS - 2; k++) {
        buf[i++] = (k < len) ? body[k] : ' ';
    }

    buf[i++] = right;
    buf[i] = '\0';

    menu_line(row, buf);
}

// A rule across the box with an optional centred label, e.g.
// "+==== slot b active ====+". Empty text gives a plain rule.
static void menu_rule_text(int row, const char* text) {
    char body[MENU_COLS - 1];
    int len = 0;
    int pad;

    while (text[len] && len < MENU_COLS - 4) {
        len++;
    }

    pad = ((MENU_COLS - 2) - len) / 2;

    for (int i = 0; i < MENU_COLS - 2; i++) {
        body[i] = '=';
    }

    for (int i = 0; i < len; i++) {
        body[pad + i] = text[i];
    }

    body[MENU_COLS - 2] = '\0';

    menu_box_line(row, '+', '+', body);
}

static void menu_rule(int row) {
    menu_rule_text(row, "");
}

static void menu_draw(int sel) {
    char line[64];
    char slot = active_slot();

    // No video_clean_screen() here: the splash drawn from logo.bin stays up and
    // the menu is drawn as a box over it.
    menu_rule_text(MENU_TOP, " kaeru boot menu ");
    menu_box_line(MENU_TOP + 1, '|', '|', "      Tecno Pova 5 Pro 5G (LH8n)");

    for (int i = 0; i < MENU_COUNT; i++) {
        // 'label' is a pointer in a static table, and this payload only has its
        // GOT relocated (see the note below), so it is stale until rebased.
        const char* label = (const char*)((uintptr_t)menu_items[i].label + kaeru_reloc_delta);
        npf_snprintf(line, sizeof(line), " %s %s", (i == sel) ? ">>" : "  ", label);
        menu_box_line(MENU_TOP + 2 + i, '|', '|', line);
    }

    menu_box_line(MENU_STATUS, '|', '|', "");
    menu_box_line(MENU_HINT, '|', '|', " Vol: tap/hold  Power: hold to select");

    if (slot) {
        npf_snprintf(line, sizeof(line), " slot %c active ", slot);
        menu_rule_text(MENU_BOTTOM, line);
    } else {
        menu_rule(MENU_BOTTOM);
    }

    menu_present();
}

static void menu_draw_cb(int sel, void* ctx) {
    (void)ctx;
    menu_draw(sel);
}

// Tap-or-hold movement for a list: one step on the press (a tap moves once),
// then auto-repeat while the key stays down, so a hold slides through the list.
// 'draw' repaints whenever the selection moves. Returns 1 if it moved.
static int menu_move_keys(int items, int* sel, void (*draw)(int sel, void* ctx), void* ctx) {
    int voldn = mtk_detect_key(KEY_VOLUME_DOWN);
    int volup = !voldn && mtk_detect_key(KEY_VOLUME_UP);
    int key;
    int dir;
    int held = 0;

    if (!voldn && !volup) {
        return 0;
    }

    key = voldn ? KEY_VOLUME_DOWN : KEY_VOLUME_UP;
    dir = voldn ? 1 : items - 1;  // +1 / -1 mod items

    *sel = (*sel + dir) % items;
    draw(*sel, ctx);

    while (mtk_detect_key(key)) {
        mdelay(MENU_POLL_MS);
        held += MENU_POLL_MS;

        if (held >= MENU_REPEAT_DELAY_MS) {
            *sel = (*sel + dir) % items;
            draw(*sel, ctx);
            held -= MENU_REPEAT_MS;
        }
    }

    return 1;
}

// True once Power has been held for MENU_HOLD_MS; waits for release first.
static int menu_hold_power(void) {
    int held = 0;

    while (mtk_detect_key(KEY_POWER) && held < MENU_HOLD_MS) {
        mdelay(MENU_POLL_MS);
        held += MENU_POLL_MS;
    }

    if (held < MENU_HOLD_MS) {
        return 0;
    }

    while (mtk_detect_key(KEY_POWER)) {
        mdelay(MENU_POLL_MS);
    }

    return 1;
}

// Is the clip actually streamed in the partition? The blob carries no magic of
// its own (utils/badapple.py writes only the deltas), so presence is a probe:
// the partition has to hold the whole stream, and its first bytes must not read
// back blank (all 0x00, or 0xFF on an erased one) -- no PackBits frame can
// start with either. The offsets table is compiled in, so the length is exact.
static int badapple_blob_missing(void) {
    uint8_t probe[16];
    uint8_t any = 0;
    uint8_t all = 0xFF;

    if (badapple_part_size() < badapple_offsets[BADAPPLE_FRAMES]) {
        return 1;
    }

    if (badapple_part_read(BADAPPLE_PART_OFFSET, probe, sizeof(probe)) != (long)sizeof(probe)) {
        return 1;
    }

    for (unsigned i = 0; i < sizeof(probe); i++) {
        any |= probe[i];
        all &= probe[i];
    }

    return any == 0 || all == 0xFF;
}

// The menu's Bad Apple entry: the same clip, drawn with the menu's own output
// instead of fastboot INFO lines. Refused with a plain notice when nothing is
// streamed in tranfs, the same short-circuit the staged-flash picker uses.
static void badapple_menu(void) {
    const uint32_t ticks_per_ms = ((uint32_t (*)(uint32_t))(0x4825F500 | 1))(1);
    const uint32_t fps = BADAPPLE_FPS > 0 ? BADAPPLE_FPS : 30;
    const uint32_t period_ms = ((uint32_t)BADAPPLE_FRAMES * 1000) / fps;
    uint32_t advanced = 0;
    uint32_t drawn = 0;

    if (badapple_blob_missing()) {
        menu_box_line(MENU_STATUS, '|', '|', " No Bad Apple stream in tranfs");
        menu_present();
        mdelay(2000);
        return;
    }

    memset(badapple_frame, 0, sizeof(badapple_frame));

    badapple_active = 1;
    badapple_clear();
    badapple_rect_reset();

    badapple_run(badapple_ticks(), ticks_per_ms, fps, period_ms, -1, &advanced, &drawn);

    badapple_active = 0;
}

// Runs alongside LK's fastboot loop: the menu never blocks fastboot, so the
// port stays usable while the device sits in the bootloader.
static int menu_thread(void* arg) {
    int sel = 0;
    int dirty = 1;

    (void)arg;

#if MENU_CALIBRATE
    menu_calibrate();
#endif

    // Ignore keys that are already down (e.g. the Power + Vol- combo that got
    // us here), so the menu cannot be driven straight into an action.
    while (mtk_detect_key(KEY_VOLUME_UP) || mtk_detect_key(KEY_VOLUME_DOWN) ||
           mtk_detect_key(KEY_POWER)) {
        mdelay(50);
    }

    for (;;) {
        if (badapple_active) {
            mdelay(100);
            dirty = 1;  // redraw once playback stops
            continue;
        }

        if (menu_move_keys(MENU_COUNT, &sel, menu_draw_cb, NULL)) {
            dirty = 0;  // menu_move_keys() already redrew
        } else if (mtk_detect_key(KEY_POWER)) {
            // Require a deliberate hold: a tap (e.g. an accidental bump while
            // flashing something) must not reboot the device.
            menu_box_line(MENU_STATUS, '|', '|', " Hold Power to confirm...");
            menu_present();

            if (menu_hold_power()) {
                // Say where we are going and push it to the panel *before* running
                // the action: the reboot normally never returns, and a
                // command-mode panel only shows what has been pushed to it.
                menu_box_line(MENU_STATUS, '|', '|', menu_doing(sel));
                menu_present();
                mdelay(400);

                if (sel == MENU_FLASH) {
                    // The picker draws its own full block and blanks it before
                    // returning, so nothing is left on the main layout.
                    kst_pick(1);
                } else if (sel == MENU_COPY) {
                    kcp_pick(1);
                } else if (sel == MENU_BADAPPLE) {
                    // Plays on the panel and returns here when a key stops it.
                    badapple_menu();
                } else if (sel != MENU_BACKUP && sel != MENU_RESTORE) {
                    // The PMIC power-off sequence takes long enough for the
                    // watchdog to reset the SoC mid-way, which would look like a
                    // reboot instead of a power-off.
                    if (sel == MENU_POWEROFF) {
                        mtk_wdt_disable();
                    }

                    ((void (*)(const char*, void*, unsigned))(menu_items[sel].action))(NULL, NULL, 0);

                    // Only reached if the command came back instead of rebooting.
                    menu_box_line(MENU_STATUS, '|', '|',
                                  (sel == MENU_POWEROFF) ? "Power off done" : "Reboot done");
                    menu_present();
                    mdelay(600);

                    break;
                } else {
                    // Backup and restore come back to the menu, so they can be
                    // repeated or followed by a reboot without another Power
                    // cycle.
                    int rc = (sel == MENU_BACKUP) ? kbk_backup(1) : kbk_restore(1);

                    menu_box_line(MENU_STATUS, '|', '|',
                                  rc ? "Failed - see the LK log"
                                     : (sel == MENU_BACKUP) ? "Backup done" : "Restore done");
                    menu_present();
                    mdelay(900);
                }
            }

            dirty = 1;  // released too early: back to the normal hint
        }

        if (dirty) {
            dirty = 0;
            menu_draw(sel);
        }

        mdelay(MENU_POLL_MS);
    }

    return 0;
}

static void menu_inject(void) {
    thread_t* thr = thread_create("kaeru-menu", menu_thread, NULL, DEFAULT_PRIORITY,
                                  DEFAULT_STACK_SIZE);

    if (thr) {
        thread_resume(thr);
    }
}

// ---------------------------------------------------------------------------
// Boot-image sanity check
//
// kaeru runs before LK's own boot code, so it can never see a boot attempt
// fail. What it *can* see is an image LK has no chance of booting: if one of
// the boot images of the active slot -- boot (kernel) or vendor_boot (vendor
// ramdisk) -- is gone or does not start with its boot header, the device is
// bricked, and the useful place to be then is the bootloader, which is
// otherwise only reachable with the Power + Vol- combo.
//
// init_boot (the generic ramdisk) is deliberately *not* one of them: this ROM
// keeps the generic ramdisk inside 'boot' -- its v4 header has a non-zero
// ramdisk_size -- and leaves init_boot as 8 MiB of zeros. Requiring its magic
// sent every normal boot of this device to the bootloader. A missing, zeroed or
// otherwise unrecognised init_boot is logged, never acted on.
//
// The active slot comes from the AOSP A/B metadata LK itself uses, in 'misc':
// struct bootloader_control { char slot_suffix[4]; uint32_t magic; ... } at
// offset 2048, magic 0x42414342 ("BCAB"). LK reads exactly that (its boot
// control code loads 0x20 bytes from 'misc' at 0x800), picks the slot from the
// priority nibbles and writes the suffix back, and it also passes the suffix on
// the kernel command line. Anything unexpected there means "don't guess" rather
// than "broken".
// ---------------------------------------------------------------------------
#define SLOT_META_OFFSET 2048
#define SLOT_META_SIZE   32

// 'a' / 'b', or 0 when the slot cannot be determined.
static char active_slot(void) {
    uint8_t buf[SLOT_META_SIZE];

    if (part_read("misc", SLOT_META_OFFSET, buf, sizeof(buf)) < (long)sizeof(buf)) {
        return 0;
    }

    // "BCAB" magic, compared byte-wise because the struct is packed.
    if (buf[4] != 'B' || buf[5] != 'C' || buf[6] != 'A' || buf[7] != 'B') {
        return 0;
    }

    // slot_suffix is NUL terminated, normally "_a" / "_b".
    for (int i = 0; i < 4; i++) {
        if (buf[i] == 'a' || buf[i] == 'b') {
            return (char)buf[i];
        }
        if (buf[i] == '\0') {
            break;
        }
    }

    return 0;
}

// A missing partition or an unreadable header counts as usable: this check must
// never force the bootloader out of its own uncertainty. hdr returns the first
// 8 bytes for logging, when they were readable.
static int image_has_magic(const char* name, const char* magic, uint8_t hdr[8]) {
    if (part_size(name) == 0) {
        return 1;
    }

    if (part_read(name, 0, hdr, sizeof(uint8_t) * 8) < 8) {
        return 1;
    }

    return memcmp(hdr, magic, sizeof(uint8_t) * 8) == 0;
}

// Raw header bytes of a partition that does not start with its boot magic:
// without them the log says only *which* image surprised us, never what was
// actually there.
static void log_header(const char* part, const uint8_t hdr[8]) {
    printf("kaeru: %s header %02x %02x %02x %02x %02x %02x %02x %02x\n", part, hdr[0],
           hdr[1], hdr[2], hdr[3], hdr[4], hdr[5], hdr[6], hdr[7]);
}

// ---------------------------------------------------------------------------
// On-device boot-image backup
//
// The menu has no host and no SD card, so the only place to keep a rescue copy
// of the boot images is a partition. 'tranfs' is 307 MiB of otherwise-unused
// zeros (the Bad Apple blob occupies its first ~198 MiB), so its tail is the
// backup store.
//
// This is a crude rescue, not an imaging tool: only the two images LK cannot
// boot without -- boot and vendor_boot of one slot, ~104 MiB together -- are
// kept. A header at a fixed offset names the images, their lengths and a
// checksum each, so a half-written backup is refused rather than written over a
// good boot image.
// ---------------------------------------------------------------------------
#define KBK_PART      "tranfs"
#define KBK_OFFSET    0x0C800000UL  // 200 MiB in, above the Bad Apple blob
#define KBK_HDR_BYTES 4096

#define KBK_MAGIC   0x314B424BUL  // "KBK1"
#define KBK_VERSION 1
#define KBK_NPARTS  2

// A few UFS sectors per transfer: the payload's RAM budget sets this, not
// throughput (a partition read costs ~10 ms whatever its size, and the whole
// copy runs unattended).
#define KBK_CHUNK 65536

struct kbk_header {
    uint32_t magic;
    uint32_t version;
    char     slot;
    uint8_t  pad[3];
    uint32_t count;
    struct {
        char     name[16];
        uint32_t size;
        uint32_t sum;
    } img[KBK_NPARTS];
};

static uint8_t kbk_hdr[KBK_HDR_BYTES];
static uint8_t kbk_buf[KBK_CHUNK];

// FNV-1a: enough to catch a truncated or shuffled copy, and cheap.
static uint32_t kbk_hash(const uint8_t* p, uint32_t n, uint32_t h) {
    while (n--) {
        h ^= *p++;
        h *= 16777619u;
    }

    return h;
}

// Read-only hash of a region, so a backup can be checked before any of it is
// written over a good image.
static int kbk_hash_region(const struct part_info* part, uint64_t off, uint32_t n,
                           uint32_t* out_sum) {
    uint32_t h = 2166136261u;

    while (n) {
        uint32_t c = (n < KBK_CHUNK) ? n : KBK_CHUNK;

        if (storage_part_read(part, kbk_buf, off, c) != (ssize_t)c) {
            return -1;
        }

        // The read is a DMA transfer: drop stale cache lines before hashing.
        arch_clean_invalidate_cache_range((uint32_t)kbk_buf, c);

        h = kbk_hash(kbk_buf, c, h);
        n -= c;
        off += c;
    }

    *out_sum = h;
    return 0;
}

// Copies 'n' bytes between two partitions in KBK_CHUNK steps, hashing what it
// reads. Returns 0 on success, with *out_sum the hash of the source. 'row' is
// the layout row its progress is drawn on.
static int kbk_copy(const struct part_info* from, uint64_t from_off,
                    const struct part_info* to, uint64_t to_off, uint32_t n,
                    uint32_t* out_sum, const char* what, int row, int ui) {
    uint32_t h = 2166136261u;
    uint32_t total = n;
    uint32_t done = 0;

    while (n) {
        uint32_t c = (n < KBK_CHUNK) ? n : KBK_CHUNK;

        if (storage_part_read(from, kbk_buf, from_off, c) != (ssize_t)c) {
            return -1;
        }

        // DMA in, then out: invalidate after the read, clean before the write.
        arch_clean_invalidate_cache_range((uint32_t)kbk_buf, c);

        h = kbk_hash(kbk_buf, c, h);

        arch_clean_invalidate_cache_range((uint32_t)kbk_buf, c);

        if (storage_part_write(to, kbk_buf, to_off, c) != (ssize_t)c) {
            return -1;
        }

        n -= c;
        done += c;
        from_off += c;
        to_off += c;

        // Refresh the panel every 8 MiB (a whole copy runs for tens of seconds).
        if (ui && (done == total || (done & 0x7FFFFFu) == 0)) {
            char line[MENU_COLS + 1];

            // MiB, not a percentage: LK's libc has no 64-bit divide, and
            // done * 100 overflows 32 bits.
            npf_snprintf(line, sizeof(line), " %s - %u/%u MiB", what,
                         (unsigned)(done >> 20), (unsigned)(total >> 20));
            menu_line(row, line);
            menu_present();
        }
    }

    *out_sum = h;
    return 0;
}

// 'backup': copy the active slot's boot images into the tail of 'tranfs'.
static int kbk_backup(int ui) {
    const struct part_info* bak = storage_part_find(KBK_PART);
    struct kbk_header* h = (struct kbk_header*)kbk_hdr;
    const struct part_info* part[KBK_NPARTS];
    char slot = active_slot();
    uint64_t off = KBK_OFFSET + KBK_HDR_BYTES;
    uint64_t total = KBK_HDR_BYTES;

    if (!bak || !slot) {
        printf("kaeru: backup: no %s partition or no active slot\n", KBK_PART);
        return -1;
    }

    memset(h, 0, KBK_HDR_BYTES);
    h->magic = KBK_MAGIC;
    h->version = KBK_VERSION;
    h->slot = slot;
    h->count = KBK_NPARTS;

    for (int i = 0; i < KBK_NPARTS; i++) {
        npf_snprintf(h->img[i].name, sizeof(h->img[i].name), "%s_%c",
                     (i == 0) ? "boot" : "vendor_boot", slot);
        part[i] = storage_part_find(h->img[i].name);
        if (!part[i]) {
            printf("kaeru: backup: %s not found\n", h->img[i].name);
            return -1;
        }
        h->img[i].size = (uint32_t)((uint64_t)part[i]->size_blocks * BLOCK_SIZE);
        total += h->img[i].size;
    }

    if (KBK_OFFSET + total > (uint64_t)bak->size_blocks * BLOCK_SIZE) {
        printf("kaeru: backup: %u MiB will not fit in %s\n", (unsigned)(total >> 20),
               KBK_PART);
        return -1;
    }

    // Images first, header last: a header with a valid magic must never
    // describe images that were not fully written. If the copy dies half-way,
    // the old header's checksums no longer match, so restore refuses it.
    for (int i = 0; i < KBK_NPARTS; i++) {
        if (kbk_copy(part[i], 0, bak, off, h->img[i].size, &h->img[i].sum, "Backing up",
                     11 + MENU_COUNT, ui)) {
            printf("kaeru: backup: copying %s failed\n", h->img[i].name);
            return -1;
        }
        off += h->img[i].size;
    }

    if (storage_part_write(bak, kbk_hdr, KBK_OFFSET, KBK_HDR_BYTES) != KBK_HDR_BYTES) {
        printf("kaeru: backup: writing the header failed\n");
        return -1;
    }

    printf("kaeru: backup: slot %c saved to %s at 0x%08x\n", slot, KBK_PART,
           (unsigned)KBK_OFFSET);
    return 0;
}

// 'restore': write the stored boot images back to their own partitions (the
// names in the header, not the current active slot).
static int kbk_restore(int ui) {
    const struct part_info* bak = storage_part_find(KBK_PART);
    struct kbk_header* h = (struct kbk_header*)kbk_hdr;
    const struct part_info* part[KBK_NPARTS];
    uint64_t off = KBK_OFFSET + KBK_HDR_BYTES;

    if (!bak) {
        printf("kaeru: restore: no %s partition\n", KBK_PART);
        return -1;
    }

    if (storage_part_read(bak, kbk_hdr, KBK_OFFSET, KBK_HDR_BYTES) != KBK_HDR_BYTES) {
        printf("kaeru: restore: header unreadable\n");
        return -1;
    }

    if (h->magic != KBK_MAGIC || h->version != KBK_VERSION || h->count != KBK_NPARTS) {
        printf("kaeru: restore: no usable backup (magic %08x)\n", h->magic);
        return -1;
    }

    // Check every image -- size and checksum -- before writing any of them, so a
    // torn backup cannot replace one good image and leave the other bad.
    for (int i = 0; i < KBK_NPARTS; i++) {
        uint32_t sum = 0;

        part[i] = storage_part_find(h->img[i].name);
        if (!part[i] || h->img[i].size == 0 ||
            h->img[i].size > (uint32_t)((uint64_t)part[i]->size_blocks * BLOCK_SIZE)) {
            printf("kaeru: restore: %s missing or too small\n", h->img[i].name);
            return -1;
        }

        if (kbk_hash_region(bak, off, h->img[i].size, &sum) || sum != h->img[i].sum) {
            printf("kaeru: restore: %s checksum mismatch\n", h->img[i].name);
            return -1;
        }

        off += h->img[i].size;
    }

    off = KBK_OFFSET + KBK_HDR_BYTES;

    for (int i = 0; i < KBK_NPARTS; i++) {
        uint32_t sum = 0;

        if (kbk_copy(bak, off, part[i], 0, h->img[i].size, &sum, "Restoring",
                     11 + MENU_COUNT, ui)) {
            printf("kaeru: restore: writing %s failed\n", h->img[i].name);
            return -1;
        }

        off += h->img[i].size;
    }

    printf("kaeru: restore: %s (%c) restored from %s\n", h->img[0].name, h->slot, KBK_PART);
    return 0;
}

// ---------------------------------------------------------------------------
// Flash a staged image from tranfs into a partition
//
// The last-resort counterpart to the backup above: a user with a rooted Android
// drops an image into a partition LK can still read when Android is gone, then
// boots the menu and picks the boot image to write it to. The menu offers only
// kst_allow; 'oem flash-staged' still takes any name the denylist allows.
// Staging shares the boot backup's store -- the two cannot coexist, and staging
// overwrites whatever backup was there (the magic at KBK_OFFSET tells them apart).
//
// The header is one text line, "KST1 <size> [<cksum> [<name>]]", so the
// on-device command is stock `cksum` plus two `dd`s, no PC:
//
//   IMG=/sdcard/recovery.img; set -- $(cksum "$IMG")
//   printf 'KST1 %s %s %s\n' "$2" "$1" "${IMG##*/}" | dd of=/dev/block/by-name/tranfs bs=4096 seek=$((0xC800000/4096)) conv=notrunc
//   dd if="$IMG" of=/dev/block/by-name/tranfs bs=4096 seek=$((0xC801000/4096)) conv=fsync
//
// The checksum is optional: omit the second field for a size-only flash. When
// present it is verified before anything is written, so a half-staged image
// cannot be flashed over a good one. The filename is display-only -- it lets the
// menu say what is staged -- and is read only when the checksum is present, so a
// name that starts with a digit cannot be mistaken for one. A header without it
// (hand-rolled dd, older staging) just shows as "staged image".
// ---------------------------------------------------------------------------
#define KST_HEADER "KST1"

// Partitions whose loss would leave a device this menu cannot recover: the
// staging store itself, the boot chain that runs before it, and the
// per-device identity/calibration blobs. Matched by prefix, so slot-suffixed
// names are covered too.
static const char* const kst_deny[] = {
        "tranfs", "preloader", "preloader_backup", "proinfo", "nvcfg",
        "nvdata", "flashinfo", "pgpt", "sgpt", "misc",
};

// Rows of the list shown at once. The rest are reached by scrolling; a scroll
// bar is not worth the pixels.
#define KST_VISIBLE 20

static uint32_t kst_crc_tbl[256];
static int kst_crc_ready;

// POSIX cksum's CRC-32 (poly 0x04C11DB7, not reflected, length folded in at the
// end), so the staging command is Android's stock `cksum`. LK's own hash is
// FNV-1a, which no shell tool on the phone can reproduce.
static uint32_t kst_crc_update(uint32_t crc, const uint8_t* p, uint32_t n) {
    if (!kst_crc_ready) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i << 24;

            for (int k = 0; k < 8; k++) {
                c = (c & 0x80000000u) ? (c << 1) ^ 0x04C11DB7u : (c << 1);
            }

            kst_crc_tbl[i] = c;
        }

        kst_crc_ready = 1;
    }

    while (n--) {
        crc = (crc << 8) ^ kst_crc_tbl[((crc >> 24) ^ *p++) & 0xFFu];
    }

    return crc;
}

static uint32_t kst_crc_finish(uint32_t crc, uint32_t len) {
    while (len) {
        crc = (crc << 8) ^ kst_crc_tbl[((crc >> 24) ^ len) & 0xFFu];
        len >>= 8;
    }

    return ~crc;
}

// cksum the staged region: same read loop as kbk_hash_region.
static int kst_crc_region(const struct part_info* part, uint64_t off, uint32_t n,
                          uint32_t* out) {
    uint32_t crc = 0;
    uint32_t len = n;

    while (n) {
        uint32_t c = (n < KBK_CHUNK) ? n : KBK_CHUNK;

        if (storage_part_read(part, kbk_buf, off, c) != (ssize_t)c) {
            return -1;
        }

        arch_clean_invalidate_cache_range((uint32_t)kbk_buf, c);

        crc = kst_crc_update(crc, kbk_buf, c);
        n -= c;
        off += c;
    }

    *out = kst_crc_finish(crc, len);
    return 0;
}

// Parse the header sector. Returns 0 and the image size, and the checksum (0
// when the staging command left it out).
// Read the staged image's size and checksum. 'name' (optional, may be NULL) also
// receives the staged filename when the header carries one -- see the staging
// recipe above -- and is set to "" otherwise.
static int kst_read_header(uint32_t* size, uint32_t* sum, char* name, size_t name_len) {
    const struct part_info* bak = storage_part_find(KBK_PART);
    char* p;
    char* end = NULL;
    unsigned long sz;

    if (name && name_len) {
        name[0] = '\0';
    }

    if (!bak) {
        return -1;
    }

    if (storage_part_read(bak, kbk_hdr, KBK_OFFSET, KBK_HDR_BYTES) != KBK_HDR_BYTES) {
        return -1;
    }

    arch_clean_invalidate_cache_range((uint32_t)kbk_hdr, KBK_HDR_BYTES);
    kbk_hdr[KBK_HDR_BYTES - 1] = '\0';

    if (memcmp(kbk_hdr, KST_HEADER, 4) != 0) {
        return -1;
    }

    p = (char*)kbk_hdr + 4;
    sz = strtoul(p, &end, 10);

    if (end == p || sz == 0) {
        return -1;
    }

    *size = (uint32_t)sz;
    *sum = (uint32_t)strtoul(end, &p, 10);  // 0 when the field is absent

    // The filename is whatever follows the checksum. It is read only when a
    // checksum is present, so a name starting with a digit cannot parse as one.
    if (name && name_len && *sum) {
        size_t i = 0;

        while (*p == ' ') {
            p++;
        }

        while (*p && *p != '\r' && *p != '\n' && i < name_len - 1) {
            name[i++] = *p++;
        }

        name[i] = '\0';
    }

    return 0;
}

static int kst_denied(const char* name) {
    for (unsigned i = 0; i < ARRAY_SIZE(kst_deny); i++) {
        // The table is baked in at the link base; rebase it like every other
        // pointer table, since stage 1 loads us at an arbitrary address.
        const char* deny = (const char*)((uintptr_t)kst_deny[i] + kaeru_reloc_delta);

        if (strncmp(name, deny, strlen(deny)) == 0) {
            return 1;
        }
    }

    return 0;
}

// What the picker offers: the boot images a no-PC user restores this way, plus
// the AVB metadata that has to be re-flashed alongside a re-signed boot image.
// init_boot and dtbo are left out: this ROM keeps the generic ramdisk inside
// 'boot' and its init_boot is 8 MiB of zeros (see the sanity check below), and
// a wrong dtbo is recoverable the same way a wrong boot is.
// Exact names, not prefixes -- a "boot" prefix would also pull in boot_para.
// Rebased like kst_deny, for the same reason.
static const char* const kst_allow[] = {
        "boot_a", "boot_b", "vendor_boot_a", "vendor_boot_b",
        "vbmeta_a", "vbmeta_b", "vbmeta_system_a", "vbmeta_system_b",
        "vbmeta_vendor_a", "vbmeta_vendor_b",
};

static int kst_allowed(const char* name) {
    for (unsigned i = 0; i < ARRAY_SIZE(kst_allow); i++) {
        const char* allow = (const char*)((uintptr_t)kst_allow[i] + kaeru_reloc_delta);

        if (strcmp(name, allow) == 0) {
            return 1;
        }
    }

    return 0;
}

// Why the last kst_flash() failed, short enough to show in a menu row. Both the
// picker and 'oem flash-staged' show it; the LK log is not readable from a host.
static char kst_reason[64];

#define KST_ERR(...)                                               \
    do {                                                           \
        npf_snprintf(kst_reason, sizeof(kst_reason), __VA_ARGS__); \
        printf("kaeru: flash: %s\n", kst_reason);                  \
        return -1;                                                 \
    } while (0)

// Write the staged image to 'name'. Returns 0 on success. 'row' is where the
// progress goes; 'ui' is 0 when called from fastboot.
static int kst_flash(const char* name, int row, int ui) {
    const struct part_info* src = storage_part_find(KBK_PART);
    const struct part_info* dst = storage_part_find(name);
    uint32_t size = 0;
    uint32_t sum = 0;
    uint32_t want = 0;
    uint32_t back = 0;
    uint64_t cap;
    char staged[48];

    if (!src) {
        KST_ERR("no %s partition", KBK_PART);
    }

    if (!dst) {
        KST_ERR("no %s partition", name);
    }

    if (kst_read_header(&size, &sum, staged, sizeof(staged))) {
        KST_ERR("nothing staged in %s", KBK_PART);
    }

    cap = (uint64_t)src->size_blocks * BLOCK_SIZE;

    if (size > cap - KBK_HDR_BYTES) {
        KST_ERR("staged %u bytes do not fit in %s", (unsigned)size, KBK_PART);
    }

    if (size > (uint64_t)dst->size_blocks * BLOCK_SIZE) {
        KST_ERR("staged %u bytes do not fit in %s", (unsigned)size, name);
    }

    if (sum && (kst_crc_region(src, KBK_OFFSET + KBK_HDR_BYTES, size, &want) || want != sum)) {
        KST_ERR("staged checksum %08X != %08X", (unsigned)want, (unsigned)sum);
    }

    if (kbk_copy(src, KBK_OFFSET + KBK_HDR_BYTES, dst, 0, size, &want, "Flashing", row, ui)) {
        KST_ERR("writing %s failed", name);
    }

    // Read the target back off the storage and compare it with the source hash
    // kbk_copy() computed. This unit refuses 'oem dump', so an unverified flash
    // is otherwise taken entirely on trust; the read-back is a fresh DMA
    // transfer, not a cache hit.
    if (ui) {
        menu_line(row, " Verifying...");
        menu_present();
    }

    if (kbk_hash_region(dst, 0, size, &back) || back != want) {
        KST_ERR("%s read back %08X != written %08X", name, (unsigned)back, (unsigned)want);
    }

    printf("kaeru: flash: %s: %u bytes written and verified in %s\n",
           staged[0] ? staged : "staged image", (unsigned)size, name);
    return 0;
}

// Blank the picker's block, so the main menu redraw does not sit on top of
// leftovers. Only the field the menu uses is cleared, same as menu_line.
static void kst_clear(void) {
    for (int r = 2; r <= 26; r++) {
        menu_line(r, "");
    }
}

// The pickers and the staged-flash screen share the same box: a title and a
// subtitle line, then the visible window of the list, then the hint.
#define PICK_TOP     2
#define PICK_TITLE   3
#define PICK_SUB     4
#define PICK_ITEM0   5
#define PICK_HINT    (PICK_ITEM0 + KST_VISIBLE)   // 25
#define PICK_BOTTOM  (PICK_HINT + 1)              // 26

// What menu_pick() draws, and what pick_draw() needs to render one frame.
struct pick_state {
    const struct part_info* const* parts;  // entries after the "Back" row
    int n;                                 // partitions follow row 0
    const char* back;                      // row 0's label
    const char* title;
    const char* sub;
    const char* hint;
    int top;                               // first visible row
};

// One frame of a boxed list picker: header, the scrolled window, footer. Called
// on every move so a held key slides through the list.
static void pick_draw(int sel, void* v) {
    struct pick_state* s = v;
    int r = PICK_ITEM0;

    // Keep the selected row inside the visible window.
    if (sel < s->top) {
        s->top = sel;
    }
    if (sel >= s->top + KST_VISIBLE) {
        s->top = sel - KST_VISIBLE + 1;
    }

    menu_rule(PICK_TOP);
    menu_box_line(PICK_TITLE, '|', '|', s->title);
    menu_box_line(PICK_SUB, '|', '|', s->sub);

    for (int i = 0; i < KST_VISIBLE; i++) {
        int idx = s->top + i;
        char line[64];

        if (idx == 0) {
            npf_snprintf(line, sizeof(line), " %s %s", (idx == sel) ? ">>" : "  ", s->back);
        } else if (idx <= s->n) {
            const struct part_info* p = s->parts[idx - 1];

            npf_snprintf(line, sizeof(line), " %s %-20s %4uM", (idx == sel) ? ">>" : "  ", p->name,
                         (unsigned)(((uint64_t)p->size_blocks * BLOCK_SIZE) >> 20));
        } else {
            line[0] = '\0';
        }

        menu_box_line(r + i, '|', '|', line);
    }

    menu_box_line(PICK_HINT, '|', '|', s->hint);
    menu_rule(PICK_BOTTOM);
    menu_present();
}

// The shared list picker: taps and holds on the volume keys move the selection,
// a Power hold commits. Returns the selected index (0 is the "Back" row).
static int menu_pick(struct pick_state* s) {
    int sel = 0;

    pick_draw(sel, s);

    for (;;) {
        // menu_move_keys() redraws each step, so only act when nothing moved.
        if (!menu_move_keys(s->n + 1, &sel, pick_draw, s)) {
            if (!mtk_detect_key(KEY_POWER)) {
                mdelay(MENU_POLL_MS);
                continue;
            }

            menu_box_line(PICK_HINT, '|', '|', " Hold Power to select...");
            menu_present();

            if (menu_hold_power()) {
                return sel;
            }

            pick_draw(sel, s);  // released too early
        }

        mdelay(MENU_POLL_MS);
    }
}

// The target picker: the boot images in kst_allow, with "Back to menu" first so
// it is always escapable. Runs inside the menu thread, so fastboot keeps
// working alongside it.
static void kst_pick(int ui) {
    const struct part_info* parts[PARTS_MAX];
    struct pick_state st;
    char sub[64];
    char line[64];
    char staged[48];
    uint32_t size = 0;
    uint32_t sum = 0;
    int have = (kst_read_header(&size, &sum, staged, sizeof(staged)) == 0);
    int count = storage_part_count();
    int n = 0;
    int sel;

    for (int i = 0; i < count && n < (int)ARRAY_SIZE(parts); i++) {
        const struct part_info* p = storage_part_get(i);

        if (p && kst_allowed(p->name)) {
            parts[n++] = p;
        }
    }

    if (have && staged[0]) {
        npf_snprintf(sub, sizeof(sub), " %s%s", staged, sum ? " (cksum)" : "");
    } else if (have) {
        npf_snprintf(sub, sizeof(sub), " staged %u MiB%s", (unsigned)(size >> 20),
                     sum ? ", cksum" : "");
    } else {
        npf_snprintf(sub, sizeof(sub), " no image staged in %s", KBK_PART);
    }

    if (!have) {
        // Nothing to flash, so skip the target list: let the user pick a target
        // and the only possible outcome is kst_flash()'s error.
        menu_rule(PICK_TOP);
        menu_box_line(PICK_TITLE, '|', '|', " Flash staged image");
        menu_box_line(PICK_SUB, '|', '|', sub);
        menu_box_line(PICK_HINT, '|', '|', " stage one with utils/stage.sh");
        menu_rule(PICK_BOTTOM);
        menu_present();
        mdelay(1800);
        kst_clear();
        menu_present();
        return;
    }

    st.parts = parts;
    st.n = n;
    st.back = "Back to menu";
    st.title = " Flash staged image";
    st.sub = sub;
    st.hint = " Vol: tap/hold  Power: hold to flash";
    st.top = 0;

    sel = menu_pick(&st);

    if (sel > 0) {
        const struct part_info* t = parts[sel - 1];
        const char* nm = staged[0] ? staged : "staged image";
        size_t nlen = strlen(nm);
        int rc;

        // Keep the target visible: the box clips from the right, so long names
        // show only their tail, which is the part that tells images apart.
        if (nlen > 20) {
            nm += nlen - 20;
        }

        npf_snprintf(line, sizeof(line), " %s -> %s", nm, t->name);
        menu_box_line(PICK_HINT, '|', '|', line);
        menu_present();

        rc = kst_flash(t->name, PICK_HINT, ui);

        menu_box_line(PICK_HINT, '|', '|', rc ? kst_reason : "Flashed + verified");
        menu_present();
        mdelay(1200);
    }

    kst_clear();
    menu_present();
}

// ---------------------------------------------------------------------------
// Copy one partition into another
//
// The backup/restore pair above moves the boot images through tranfs; this
// moves any partition straight into any other, with no staging and no host. It
// is the offline rescue for a device whose Android is gone: boot the menu, pick
// a good 'recovery' (or a good slot) as the source, and write it over a broken
// 'boot'.
//
// The whole source partition is copied, so it is refused unless it fits the
// target, and the target is refused for the same partitions the staged flash
// refuses (kst_deny).
// ---------------------------------------------------------------------------

// Why the last kcp_copy() failed, short enough for a menu row. Shares
// kst_reason: the two flashes never run at once.
#define KCP_ERR(...)                                               \
    do {                                                           \
        npf_snprintf(kst_reason, sizeof(kst_reason), __VA_ARGS__); \
        printf("kaeru: copy: %s\n", kst_reason);                   \
        return -1;                                                 \
    } while (0)

static int kcp_copy(const char* src_name, const char* dst_name, int row, int ui) {
    const struct part_info* src = storage_part_find(src_name);
    const struct part_info* dst = storage_part_find(dst_name);
    uint64_t n;
    uint32_t sum = 0;

    if (!src) {
        KCP_ERR("no %s partition", src_name);
    }

    if (!dst) {
        KCP_ERR("no %s partition", dst_name);
    }

    if (strcmp(src->name, dst->name) == 0) {
        KCP_ERR("source and target are the same");
    }

    if (kst_denied(dst_name)) {
        KCP_ERR("%s is protected", dst_name);
    }

    n = (uint64_t)src->size_blocks * BLOCK_SIZE;

    // kbk_copy takes a 32-bit length; no real image partition comes close.
    if ((n >> 32) != 0) {
        KCP_ERR("%s is larger than 4 GiB", src_name);
    }

    if (n == 0) {
        KCP_ERR("%s is empty", src_name);
    }

    if (n > (uint64_t)dst->size_blocks * BLOCK_SIZE) {
        KCP_ERR("%s does not fit in %s", src_name, dst_name);
    }

    if (kbk_copy(src, 0, dst, 0, (uint32_t)n, &sum, "Copying", row, ui)) {
        KCP_ERR("copying %s failed", src_name);
    }

    printf("kaeru: copy: %s -> %s (%u bytes)\n", src_name, dst_name, (unsigned)n);
    return 0;
}

// One side of the picker: scroll the partitions that pass the filter, return the
// chosen one, or NULL when the user backs out via 'back'. 'skip' is left out of
// the list (the source, so the target cannot be the same partition); 'deny' also
// drops the protected destinations. Draws the same block the flash picker does.
static const struct part_info* kcp_choose(const char* title, const char* sub, const char* back,
                                          const struct part_info* skip, int deny) {
    const struct part_info* parts[PARTS_MAX];
    struct pick_state st;
    int count = storage_part_count();
    int n = 0;
    int sel;

    for (int i = 0; i < count && n < (int)ARRAY_SIZE(parts); i++) {
        const struct part_info* p = storage_part_get(i);

        if (!p) {
            continue;
        }

        if (skip && strcmp(p->name, skip->name) == 0) {
            continue;
        }

        if (deny && kst_denied(p->name)) {
            continue;
        }

        parts[n++] = p;
    }

    st.parts = parts;
    st.n = n;
    st.back = back;
    st.title = title;
    st.sub = sub;
    st.hint = " Vol: tap/hold  Power: hold to select";
    st.top = 0;

    sel = menu_pick(&st);

    return (sel == 0) ? NULL : parts[sel - 1];
}

// The copy's two pickers: source first, then a target that is neither the source
// nor a protected partition. Backing out of the target picker returns to the
// source picker, so a wrong source does not send the user all the way back to
// the main menu. Runs inside the menu thread, so fastboot keeps working.
static void kcp_pick(int ui) {
    const struct part_info* src = NULL;
    const struct part_info* dst;
    char sub[64];
    char line[64];
    int rc;

    for (;;) {
        if (!src) {
            src = kcp_choose(" Copy partition", " Pick the source", "Back to menu", NULL, 0);

            if (!src) {
                break;  // Back from the source picker: leave the copy flow
            }
        }

        npf_snprintf(sub, sizeof(sub), " Source: %s", src->name);
        dst = kcp_choose(" Copy: pick target", sub, "Back to source", src, 1);

        if (!dst) {
            src = NULL;  // Back to re-pick the source
            continue;
        }

        npf_snprintf(line, sizeof(line), " Copying %s -> %s", src->name, dst->name);
        menu_box_line(PICK_HINT, '|', '|', line);
        menu_present();

        rc = kcp_copy(src->name, dst->name, PICK_HINT, ui);

        menu_box_line(PICK_HINT, '|', '|', rc ? kst_reason : "Copy done");
        menu_present();
        mdelay(1200);
        break;
    }

    kst_clear();
    menu_present();
}

// 'oem copy <source> <target>': the pickers' copy, for a scripted or host-driven
// run. Both partitions are named by the parsed table.
static void cmd_copy(const char* arg, void* data, unsigned sz) {
    char src[PART_NAME_MAX + 1];
    char dst[PART_NAME_MAX + 1];

    (void)data;
    (void)sz;

    arg = next_token(arg, src, sizeof(src));
    next_token(arg, dst, sizeof(dst));

    if (!src[0] || !dst[0]) {
        fastboot_fail("Usage: oem copy <source> <target>");
        return;
    }

    fastboot_info(kcp_copy(src, dst, 12 + MENU_COUNT, 0) ? kst_reason : "partition copied");
    fastboot_okay("");
}

// 'oem info': everything kaeru can tell about this boot, in one place. Read
// only, and it never writes the BCB.
static void cmd_info(const char* arg, void* data, unsigned sz) {
    static struct bootloader_message bcb; // 2 KiB, too much for LK's stack
    char buf[96];

    (void)arg;
    (void)data;
    (void)sz;

    fastboot_info("kaeru info:");
    npf_snprintf(buf, sizeof(buf), "  version %s", KAERU_VERSION);
    fastboot_info(buf);

    npf_snprintf(buf, sizeof(buf), "  boot mode %d (%s)", (int)get_bootmode(),
                 bootmode2str(get_bootmode()));
    fastboot_info(buf);

    char slot = active_slot();
    npf_snprintf(buf, sizeof(buf), "  active slot %c", slot ? slot : '?');
    fastboot_info(buf);

    if (read_bootloader_message(&bcb)) {
        bcb.command[sizeof(bcb.command) - 1] = '\0';
        npf_snprintf(buf, sizeof(buf), "  misc BCB \"%s\"", bcb.command);
        fastboot_info(buf);
    } else {
        fastboot_info("  misc BCB unreadable");
    }

    fastboot_okay("");
}

// 'oem clear-bcb': drop a pending misc BCB command. The escape hatch for a
// leftover 'fastboot_please'/'bootonce-bootloader' the OS never consumed.
static void cmd_clear_bcb(const char* arg, void* data, unsigned sz) {
    (void)arg;
    (void)data;
    (void)sz;

    fastboot_info(clear_bootloader_command(true) ? "misc BCB command cleared"
                                                 : "misc BCB not writable");
    fastboot_okay("");
}

// 'oem backup' / 'oem restore': the boot menu's two rescue entries, exposed
// over fastboot too, so they can be scripted or run with no hands on the
// device. Both run to completion inside the fastboot handler.
static void cmd_backup(const char* arg, void* data, unsigned sz) {
    (void)arg;
    (void)data;
    (void)sz;

    fastboot_info(kbk_backup(0) ? "backup failed - see the LK log"
                                : "boot images backed up to tranfs");
    fastboot_okay("");
}

static void cmd_restore(const char* arg, void* data, unsigned sz) {
    (void)arg;
    (void)data;
    (void)sz;

    fastboot_info(kbk_restore(0) ? "restore failed - see the LK log"
                                 : "boot images restored");
    fastboot_okay("");
}

// 'oem flash-staged <partition>': the menu picker's flash, for a scripted or
// host-driven run. The image is whatever is staged in tranfs (see the section
// above); nothing is uploaded here.
static void cmd_flash_staged(const char* arg, void* data, unsigned sz) {
    char name[PART_NAME_MAX + 1];

    (void)data;
    (void)sz;

    next_token(arg, name, sizeof(name));

    if (!name[0]) {
        fastboot_fail("Usage: oem flash-staged <partition>");
        return;
    }

    if (kst_denied(name)) {
        fastboot_fail("Partition is protected");
        return;
    }

    fastboot_info(kst_flash(name, 12 + MENU_COUNT, 0) ? kst_reason : "staged image flashed and verified");
    fastboot_okay("");
}

// 'oem verify <partition>': read the partition back and compare it with the
// staged image, so a flash that happened at some earlier point -- or through a
// path that could not check itself -- can be confirmed without a working
// readback command from the host. Flashing already verifies what it writes.
static void cmd_verify(const char* arg, void* data, unsigned sz) {
    const struct part_info* src = storage_part_find(KBK_PART);
    const struct part_info* dst;
    char name[PART_NAME_MAX + 1];
    char staged[48];
    char buf[FASTBOOT_INFO_MAX + 1];
    uint32_t size = 0;
    uint32_t want = 0;
    uint32_t back = 0;
    uint32_t sum = 0;

    (void)data;
    (void)sz;

    next_token(arg, name, sizeof(name));

    if (!name[0]) {
        fastboot_fail("Usage: oem verify <partition>");
        return;
    }

    dst = storage_part_find(name);

    if (!src || !dst) {
        fastboot_fail("Partition not found");
        return;
    }

    if (kst_read_header(&size, &sum, staged, sizeof(staged))) {
        fastboot_info("nothing staged in tranfs");
        fastboot_okay("");
        return;
    }

    if (size > (uint64_t)dst->size_blocks * BLOCK_SIZE) {
        fastboot_info("staged image is larger than the partition");
        fastboot_okay("");
        return;
    }

    if (kbk_hash_region(src, KBK_OFFSET + KBK_HDR_BYTES, size, &want) ||
        kbk_hash_region(dst, 0, size, &back) || back != want) {
        npf_snprintf(buf, sizeof(buf), "%s does not match %s", name, staged[0] ? staged : "staged image");
        fastboot_info(buf);
        fastboot_okay("");
        return;
    }

    npf_snprintf(buf, sizeof(buf), "%s matches %s (%u MiB)", name, staged[0] ? staged : "staged image",
                 (unsigned)(size >> 20));
    fastboot_info(buf);
    fastboot_okay("");
}

// 'oem help': every command kaeru registers, straight out of this table, so a
// new command is documented by adding one line here. Only kaeru's commands are
// listed; the vendor's own predate us and are not ours to describe (the full
// list of them is in README-LH8N.md).
//
// The entries hold pointers, and start.S relocates the GOT only, so each one has
// to be rebased with kaeru_reloc_delta before use, exactly as
// lib/fastboot/command.c does for the .fastboot_cmds section.
static const struct {
    const char* cmd;
    const char* desc;
} oem_descriptions[] = {
        {"oem help", "This list"},
        {"oem info", "Active slot, boot mode, pending BCB command"},
        {"oem clear-bcb", "Drop a pending misc BCB command"},
        {"oem backup", "Back up boot/vendor_boot to tranfs"},
        {"oem restore", "Restore boot/vendor_boot from tranfs"},
        {"oem flash-staged", "Flash the tranfs-staged image to a partition"},
        {"oem verify", "Compare a partition with the staged image"},
        {"oem copy", "Copy one partition's contents into another"},
        {"oem kaeru-version", "Print the kaeru version string"},
        {"oem mem", "Read/write a 32-bit word at an address"},
        {"oem partitions", "Dump the parsed GPT partition table"},
        {"oem patches", "Show the boot patches kaeru applied"},
        {"oem kscan", "Print pressed hardware keycodes"},
        {"oem badapple", "Play Bad Apple!! from tranfs"},
        {"oem naze", "naze was here :3"},
        {"oem aria", "Aria quote + on-screen text"},
        {"oem sunna", "Sunna quote + on-screen text"},
        {"oem nangongyu", "Nangong Yu quote + on-screen text"},
        {"oem aod", "AOD lyrics + on-screen text"},
        {"oem yamada", "Yamada intro + on-screen text"},
};

static void cmd_help(const char* arg, void* data, unsigned sz) {
    char line[FASTBOOT_INFO_MAX + 1];

    (void)arg;
    (void)data;
    (void)sz;

    fastboot_info("kaeru commands:");

    for (unsigned i = 0; i < ARRAY_SIZE(oem_descriptions); i++) {
        const char* cmd =
                (const char*)((uintptr_t)oem_descriptions[i].cmd + kaeru_reloc_delta);
        const char* desc =
                (const char*)((uintptr_t)oem_descriptions[i].desc + kaeru_reloc_delta);

        npf_snprintf(line, sizeof(line), "  %s - %s", cmd, desc);
        fastboot_info(line);
    }

    fastboot_okay("");
}

void board_late_init(void) {
    // Register custom fastboot commands
    fastboot_register("oem help", cmd_help, 1);
    fastboot_register("oem naze", cmd_naze, 1);
    fastboot_register("oem aria", cmd_aria, 1);
    fastboot_register("oem sunna", cmd_sunna, 1);
    fastboot_register("oem nangongyu", cmd_nangongyu, 1);
    fastboot_register("oem aod", cmd_aod, 1);
    fastboot_register("oem yamada", cmd_yamada, 1);
    fastboot_register("oem info", cmd_info, 1);
    fastboot_register("oem clear-bcb", cmd_clear_bcb, 1);
    fastboot_register("oem backup", cmd_backup, 1);
    fastboot_register("oem restore", cmd_restore, 1);
    fastboot_register("oem flash-staged", cmd_flash_staged, 1);
    fastboot_register("oem verify", cmd_verify, 1);
    fastboot_register("oem copy", cmd_copy, 1);

    // Android asks for the bootloader by leaving 'bootonce-bootloader' in the
    // misc BCB rather than rebooting into fastboot directly. Stock LK only knows
    // the boot-recovery/boot-fastboot commands, so it boots the OS and the
    // request is silently lost. Act on it here; this also clears it, so it
    // cannot pin the device in fastboot. A key combo below overrides it.
    read_and_set_bootmode_from_message();

    // Boot key combos, so a bootlooping device can still reach a usable mode:
    //   Power + Vol-  -> bootloader (fastboot)
    //   Power + Vol+  -> recovery
    if (mtk_detect_key(KEY_POWER)) {
        if (mtk_detect_key(KEY_VOLUME_DOWN)) {
            set_bootmode(BOOTMODE_FASTBOOT);
            video_printf("\n Key combo: forcing bootloader\n");
            mt_disp_update();
        } else if (mtk_detect_key(KEY_VOLUME_UP)) {
            set_bootmode(BOOTMODE_RECOVERY);
            video_printf("\n Key combo: forcing recovery\n");
            mt_disp_update();
        }
    }

    // If the images we are about to hand to the OS are not boot images, the
    // device is bricked: go to the bootloader rather than sit on a boot that
    // cannot work. boot is the kernel, vendor_boot the vendor ramdisk, and LK
    // needs both. A key combo above still wins, and so does a real recovery
    // request.
    if (get_bootmode() == BOOTMODE_NORMAL) {
        char slot = active_slot();
        char part[16];
        char bad[16] = {0};
        uint8_t hdr[8];

        if (slot) {
            for (int i = 0; i < 3 && bad[0] == '\0'; i++) {
                const char* base = "boot";

                if (i == 1) {
                    base = "init_boot";
                } else if (i == 2) {
                    base = "vendor_boot";
                }

                npf_snprintf(part, sizeof(part), "%s_%c", base, slot);

                if (image_has_magic(part, (i == 2) ? "VNDRBOOT" : "ANDROID!", hdr)) {
                    continue;
                }

                log_header(part, hdr);

                // i == 1 is the optional init_boot: logged above, never diverting.
                if (i != 1) {
                    npf_snprintf(bad, sizeof(bad), "%s", part);
                }
            }
        } else if (!image_has_magic("boot_a", "ANDROID!", hdr) &&
                   !image_has_magic("boot_b", "ANDROID!", hdr)) {
            // The slot suffix could not be read, so divert only a device with
            // nothing at all left to boot: a slot we merely failed to read must
            // never send a bootable device to the bootloader.
            npf_snprintf(bad, sizeof(bad), "boot_a/boot_b");
        }

        if (bad[0] != '\0') {
            set_bootmode(BOOTMODE_FASTBOOT);
            printf("kaeru: %s is not a boot image, forcing bootloader\n", bad);
            video_printf("\n Boot image broken: forcing bootloader\n");
            mt_disp_update();
        } else if (slot) {
            printf("kaeru: slot %c boot images ok\n", slot);
        } else {
            printf("kaeru: boot images ok (active slot unknown)\n");
        }
    }

    // The branding banner only makes sense when we are actually leaving LK for
    // the OS; in the bootloader it would just sit there and get in the way.
    if (get_bootmode() != BOOTMODE_FASTBOOT) {
        video_clean_screen();
        video_set_cursor(15, 0);
        video_printf(" Hold Your Dreams Don't Let Them Go \n");
        video_printf("====================================\n");
        video_printf("Angel of Delusion | ReDreaming Angel\n");
        mt_disp_update();
    }

    // When the device is heading into the bootloader, hang the menu off
    // LK's fastboot loop so fastboot stays fully usable alongside it.
    if (get_bootmode() == BOOTMODE_FASTBOOT) {
        menu_inject();
    }

    // ---------------------------------------------------------
    // DYNAMIC PATCHING (OTA SURVIVABLE)
    // ---------------------------------------------------------

    // Disable Orange State Warning dynamically
    uint32_t orange_addr = SEARCH_PATTERN(CONFIG_BOOTLOADER_BASE, CONFIG_BOOTLOADER_BASE + CONFIG_BOOTLOADER_SIZE,
                                          0xb508, 0xf7ea, 0xff7f, 0xf7ea, 0xff77, 0x2100);
    if (orange_addr) {
        printf("Found orange_state_warning at 0x%08X\n", orange_addr);
        FORCE_RETURN(orange_addr, 0);
    } else {
        printf("orange_state_warning: NOT FOUND\n");
    }
    g_patch_addr[PATCH_ORANGE] = orange_addr;

    // Refresh mode, as the boot menu may have changed it
    bootmode_t mode = get_bootmode();
    printf("kaeru: bootmode = %d (%s), spy mode%s\n", mode, bootmode2str(mode),
           (mode == BOOTMODE_FASTBOOT) ? " [menu injected]" : "");

    // If booting to Normal System (Not Recovery/TWRP):
    // Spoof verifiedbootstate to green and vbmeta to locked
    if (mode != BOOTMODE_RECOVERY) {
        // Dynamically search for the Green State check and spoof it
        uint32_t green_addr = SEARCH_PATTERN(CONFIG_BOOTLOADER_BASE, CONFIG_BOOTLOADER_BASE + CONFIG_BOOTLOADER_SIZE,
                                             0x4b18, 0x447b, 0x681b, 0x681b, 0x2b03, 0xd807);
        if (green_addr) {
            printf("Found green_state at 0x%08X\n", green_addr);
            WRITE16(green_addr + 6, 0x2300); // Spoof boot state to green
        } else {
            printf("green_state: NOT FOUND\n");
        }
        g_patch_addr[PATCH_GREEN] = green_addr;

        // Dynamically search for the VBMeta device_state generator and spoof it
        uint32_t lock_addr = SEARCH_PATTERN(CONFIG_BOOTLOADER_BASE, CONFIG_BOOTLOADER_BASE + CONFIG_BOOTLOADER_SIZE,
                                            0x2801, 0xd0f5, 0xb9a0, 0x9b09);
        if (lock_addr) {
            printf("Found vbmeta_lock at 0x%08X\n", lock_addr);
            WRITE16(lock_addr + 8, 0xBF00);  // Spoof VBMeta to locked
        } else {
            printf("vbmeta_lock: NOT FOUND\n");
        }
        g_patch_addr[PATCH_VBMETA] = lock_addr;
    } else {
        printf("Recovery boot: leaving boot state untouched for TWRP/adbd\n");
    }
    // If booting to TWRP (mode == BOOTMODE_RECOVERY):
    // We leave the boot state as orange and unlocked.
    // Spoofing locked state while booting TWRP will cause it to hang!
}
