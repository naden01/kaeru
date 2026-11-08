//
// SPDX-FileCopyrightText: 2026 naden01 <naden.irsyad01@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Tecno KL5n (MT6768) -- stage 1 of a port, written by following the "Porting
// kaeru to another device" section of README-LH8N.md.
//
// State: config + board exist and build; the LK should load kaeru and answer
// `fastboot oem kaeru-version`. Deliberately no device-specific behaviour yet.
//
// Still to do, each verifiable before the next:
//
//   1. All addresses in the defconfig are now verified against this LK (see the
//      "Worked example" in README-LH8N.md). Only the panel geometry is missing.
//   2. Patches: the Transsion pattern searches were checked against this image --
//      ccci_ld_md_sec_ptr_hdr_verify and green_state match, orange_state_warning
//      and vbmeta_lock are absent. The search runs in memory at boot, so copying
//      the whole set from board-lh8n.c is safe: absent patterns are skipped and
//      `oem patches` reports which ones applied.
//   3. Panel: read the framebuffer base and stride from the device's DT
//      (reserved-memory/mblock-*-framebuffer/reg) and add them here. Then
//      `oem badapple test` validates base, stride and present in one shot.
//   4. Keys/boot menu: needs mtk_detect_key, so do it after step 1.
//   5. Streaming (Bad Apple): needs partition_read.
//
#include <board_ops.h>

void board_early_init(void) {
    printf("Entering early init for Tecno KL5n\n");
}

void board_late_init(void) {
    printf("Entering late init for Tecno KL5n\n");
}
