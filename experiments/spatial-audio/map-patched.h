/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Exported from the October 2026 FH6 experiments, see THIRD_PARTY_NOTICES.md
 */
/*
 * Copyright 2020 Andrew Eikum for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */
/* Extracted from the actual build source for focused channel-map tests */
static UINT32 AudioObjectType_to_index(AudioObjectType type)
{
    UINT32 o = 0;
    while(type){
        type >>= 1;
        ++o;
    }
    return o - 2;
}
static void static_mask_to_channels(AudioObjectType static_mask, WORD *count, DWORD *mask, UINT32 *map)
{
    static const struct { AudioObjectType type; DWORD speaker; } table[] = {
        { AudioObjectType_FrontLeft, SPEAKER_FRONT_LEFT },
        { AudioObjectType_FrontRight, SPEAKER_FRONT_RIGHT },
        { AudioObjectType_FrontCenter, SPEAKER_FRONT_CENTER },
        { AudioObjectType_LowFrequency, SPEAKER_LOW_FREQUENCY },
        { AudioObjectType_SideLeft, SPEAKER_SIDE_LEFT },
        { AudioObjectType_SideRight, SPEAKER_SIDE_RIGHT },
        { AudioObjectType_BackLeft, SPEAKER_BACK_LEFT },
        { AudioObjectType_BackRight, SPEAKER_BACK_RIGHT },
        { AudioObjectType_TopFrontLeft, SPEAKER_TOP_FRONT_LEFT },
        { AudioObjectType_TopFrontRight, SPEAKER_TOP_FRONT_RIGHT },
        { AudioObjectType_TopBackLeft, SPEAKER_TOP_BACK_LEFT },
        { AudioObjectType_TopBackRight, SPEAKER_TOP_BACK_RIGHT },
        { AudioObjectType_BackCenter, SPEAKER_BACK_CENTER },
    };
    UINT32 i;

    *count = 0;
    *mask = 0;
    for (i = 0; i < ARRAY_SIZE(table); i++)
        if (static_mask & table[i].type) {
            *mask |= table[i].speaker;
            *count += 1;
        }
    /* WAVEFORMATEXTENSIBLE channels follow the speaker bits in ascending order, so a static object's
     * channel is the number of speakers in the mask below its own: back left/right (0x10, 0x20) and
     * back centre (0x100) come before side left/right (0x200, 0x400). */
    for (i = 0; i < 17; i++)
        map[i] = ~0;
    for (i = 0; i < ARRAY_SIZE(table); i++) {
        if (static_mask & table[i].type) {
            UINT32 idx = AudioObjectType_to_index(table[i].type);
            map[idx] = __builtin_popcount(*mask & (table[i].speaker - 1));
            TRACE("mapping 0x%x to %u\n", table[i].type, map[idx]);
        }
    }
}
