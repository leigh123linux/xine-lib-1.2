/*
 * Intel Indeo 2 codec
 * copyright (c) 2005 Konstantin Shishkov
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define IR2_CODES 143
static const uint16_t ir2_codes[IR2_CODES][2] = {
#ifdef ALT_BITSTREAM_READER_LE
{0x0000,  3}, {0x0004,  3}, {0x0006,  3}, {0x0001,  5},
{0x0009,  5}, {0x0019,  5}, {0x000D,  5}, {0x001D,  5},
{0x0023,  6}, {0x0013,  6}, {0x0033,  6}, {0x000B,  6},
{0x002B,  6}, {0x001B,  6}, {0x0007,  8}, {0x0087,  8},
{0x0027,  8}, {0x00A7,  8}, {0x0067,  8}, {0x00E7,  8},
{0x0097,  8}, {0x0057,  8}, {0x0037,  8}, {0x00B7,  8},
{0x00F7,  8}, {0x000F,  9}, {0x008F,  9}, {0x018F,  9},
{0x014F,  9}, {0x00CF,  9}, {0x002F,  9}, {0x012F,  9},
{0x01AF,  9}, {0x006F,  9}, {0x00EF,  9}, {0x01EF,  9},
{0x001F, 10}, {0x021F, 10}, {0x011F, 10}, {0x031F, 10},
{0x009F, 10}, {0x029F, 10}, {0x019F, 10}, {0x039F, 10},
{0x005F, 10}, {0x025F, 10}, {0x015F, 10}, {0x035F, 10},
{0x00DF, 10}, {0x02DF, 10}, {0x01DF, 10}, {0x03DF, 10},
{0x003F, 13}, {0x103F, 13}, {0x083F, 13}, {0x183F, 13},
{0x043F, 13}, {0x143F, 13}, {0x0C3F, 13}, {0x1C3F, 13},
{0x023F, 13}, {0x123F, 13}, {0x0A3F, 13}, {0x1A3F, 13},
{0x063F, 13}, {0x163F, 13}, {0x0E3F, 13}, {0x1E3F, 13},
{0x013F, 13}, {0x113F, 13}, {0x093F, 13}, {0x193F, 13},
{0x053F, 13}, {0x153F, 13}, {0x0D3F, 13}, {0x1D3F, 13},
{0x033F, 13}, {0x133F, 13}, {0x0B3F, 13}, {0x1B3F, 13},
{0x073F, 13}, {0x173F, 13}, {0x0F3F, 13}, {0x1F3F, 13},
{0x00BF, 13}, {0x10BF, 13}, {0x08BF, 13}, {0x18BF, 13},
{0x04BF, 13}, {0x14BF, 13}, {0x0CBF, 13}, {0x1CBF, 13},
{0x02BF, 13}, {0x12BF, 13}, {0x0ABF, 13}, {0x1ABF, 13},
{0x06BF, 13}, {0x16BF, 13}, {0x0EBF, 13}, {0x1EBF, 13},
{0x01BF, 13}, {0x11BF, 13}, {0x09BF, 13}, {0x19BF, 13},
{0x05BF, 13}, {0x15BF, 13}, {0x0DBF, 13}, {0x1DBF, 13},
{0x03BF, 13}, {0x13BF, 13}, {0x0BBF, 13}, {0x1BBF, 13},
{0x07BF, 13}, {0x17BF, 13}, {0x0FBF, 13}, {0x1FBF, 13},
{0x007F, 14}, {0x207F, 14}, {0x107F, 14}, {0x307F, 14},
{0x087F, 14}, {0x287F, 14}, {0x187F, 14}, {0x387F, 14},
{0x047F, 14}, {0x247F, 14}, {0x147F, 14}, {0x0002,  3},
{0x0011,  5}, {0x0005,  5}, {0x0015,  5}, {0x0003,  6},
{0x003B,  6}, {0x0047,  8}, {0x00C7,  8}, {0x0017,  8},
{0x00D7,  8}, {0x0077,  8}, {0x010F,  9}, {0x004F,  9},
{0x01CF,  9}, {0x00AF,  9}, {0x016F,  9},
#else
    {0x0000,  3}, {0x0001,  3}, {0x0003,  3}, {0x0010,  5},
    {0x0012,  5}, {0x0013,  5}, {0x0016,  5}, {0x0017,  5},
    {0x0031,  6}, {0x0032,  6}, {0x0033,  6}, {0x0034,  6},
    {0x0035,  6}, {0x0036,  6}, {0x00E0,  8}, {0x00E1,  8},
    {0x00E4,  8}, {0x00E5,  8}, {0x00E6,  8}, {0x00E7,  8},
    {0x00E9,  8}, {0x00EA,  8}, {0x00EC,  8}, {0x00ED,  8},
    {0x00EF,  8}, {0x01E0,  9}, {0x01E2,  9}, {0x01E3,  9},
    {0x01E5,  9}, {0x01E6,  9}, {0x01E8,  9}, {0x01E9,  9},
    {0x01EB,  9}, {0x01EC,  9}, {0x01EE,  9}, {0x01EF,  9},
    {0x03E0, 10}, {0x03E1, 10}, {0x03E2, 10}, {0x03E3, 10},
    {0x03E4, 10}, {0x03E5, 10}, {0x03E6, 10}, {0x03E7, 10},
    {0x03E8, 10}, {0x03E9, 10}, {0x03EA, 10}, {0x03EB, 10},
    {0x03EC, 10}, {0x03ED, 10}, {0x03EE, 10}, {0x03EF, 10},
    {0x1F80, 13}, {0x1F81, 13}, {0x1F82, 13}, {0x1F83, 13},
    {0x1F84, 13}, {0x1F85, 13}, {0x1F86, 13}, {0x1F87, 13},
    {0x1F88, 13}, {0x1F89, 13}, {0x1F8A, 13}, {0x1F8B, 13},
    {0x1F8C, 13}, {0x1F8D, 13}, {0x1F8E, 13}, {0x1F8F, 13},
    {0x1F90, 13}, {0x1F91, 13}, {0x1F92, 13}, {0x1F93, 13},
    {0x1F94, 13}, {0x1F95, 13}, {0x1F96, 13}, {0x1F97, 13},
    {0x1F98, 13}, {0x1F99, 13}, {0x1F9A, 13}, {0x1F9B, 13},
    {0x1F9C, 13}, {0x1F9D, 13}, {0x1F9E, 13}, {0x1F9F, 13},
    {0x1FA0, 13}, {0x1FA1, 13}, {0x1FA2, 13}, {0x1FA3, 13},
    {0x1FA4, 13}, {0x1FA5, 13}, {0x1FA6, 13}, {0x1FA7, 13},
    {0x1FA8, 13}, {0x1FA9, 13}, {0x1FAA, 13}, {0x1FAB, 13},
    {0x1FAC, 13}, {0x1FAD, 13}, {0x1FAE, 13}, {0x1FAF, 13},
    {0x1FB0, 13}, {0x1FB1, 13}, {0x1FB2, 13}, {0x1FB3, 13},
    {0x1FB4, 13}, {0x1FB5, 13}, {0x1FB6, 13}, {0x1FB7, 13},
    {0x1FB8, 13}, {0x1FB9, 13}, {0x1FBA, 13}, {0x1FBB, 13},
    {0x1FBC, 13}, {0x1FBD, 13}, {0x1FBE, 13}, {0x1FBF, 13},
    {0x3F80, 14}, {0x3F81, 14}, {0x3F82, 14}, {0x3F83, 14},
    {0x3F84, 14}, {0x3F85, 14}, {0x3F86, 14}, {0x3F87, 14},
    {0x3F88, 14}, {0x3F89, 14}, {0x3F8A, 14}, {0x0002,  3},
    {0x0011,  5}, {0x0014,  5}, {0x0015,  5}, {0x0030,  6},
    {0x0037,  6}, {0x00E2,  8}, {0x00E3,  8}, {0x00E8,  8},
    {0x00EB,  8}, {0x00EE,  8}, {0x01E1,  9}, {0x01E4,  9},
    {0x01E7,  9}, {0x01EA,  9}, {0x01ED,  9}
#endif
};

static const uint8_t ir2_luma_table[256] = {
 0x80, 0x80, 0x84, 0x84, 0x7C, 0x7C, 0x7F, 0x85,
 0x81, 0x7B, 0x85, 0x7F, 0x7B, 0x81, 0x8C, 0x8C,
 0x74, 0x74, 0x83, 0x8D, 0x7D, 0x73, 0x8D, 0x83,
 0x73, 0x7D, 0x77, 0x89, 0x89, 0x77, 0x89, 0x77,
 0x77, 0x89, 0x8C, 0x95, 0x74, 0x6B, 0x95, 0x8C,
 0x6B, 0x74, 0x7C, 0x90, 0x84, 0x70, 0x90, 0x7C,
 0x70, 0x84, 0x96, 0x96, 0x6A, 0x6A, 0x82, 0x98,
 0x7E, 0x68, 0x98, 0x82, 0x68, 0x7E, 0x97, 0xA2,
 0x69, 0x5E, 0xA2, 0x97, 0x5E, 0x69, 0xA2, 0xA2,
 0x5E, 0x5E, 0x8B, 0xA3, 0x75, 0x5D, 0xA3, 0x8B,
 0x5D, 0x75, 0x71, 0x95, 0x8F, 0x6B, 0x95, 0x71,
 0x6B, 0x8F, 0x78, 0x9D, 0x88, 0x63, 0x9D, 0x78,
 0x63, 0x88, 0x7F, 0xA7, 0x81, 0x59, 0xA7, 0x7F,
 0x59, 0x81, 0xA4, 0xB1, 0x5C, 0x4F, 0xB1, 0xA4,
 0x4F, 0x5C, 0x96, 0xB1, 0x6A, 0x4F, 0xB1, 0x96,
 0x4F, 0x6A, 0xB2, 0xB2, 0x4E, 0x4E, 0x65, 0x9B,
 0x9B, 0x65, 0x9B, 0x65, 0x65, 0x9B, 0x89, 0xB4,
 0x77, 0x4C, 0xB4, 0x89, 0x4C, 0x77, 0x6A, 0xA3,
 0x96, 0x5D, 0xA3, 0x6A, 0x5D, 0x96, 0x73, 0xAC,
 0x8D, 0x54, 0xAC, 0x73, 0x54, 0x8D, 0xB4, 0xC3,
 0x4C, 0x3D, 0xC3, 0xB4, 0x3D, 0x4C, 0xA4, 0xC3,
 0x5C, 0x3D, 0xC3, 0xA4, 0x3D, 0x5C, 0xC4, 0xC4,
 0x3C, 0x3C, 0x96, 0xC6, 0x6A, 0x3A, 0xC6, 0x96,
 0x3A, 0x6A, 0x7C, 0xBA, 0x84, 0x46, 0xBA, 0x7C,
 0x46, 0x84, 0x5B, 0xAB, 0xA5, 0x55, 0xAB, 0x5B,
 0x55, 0xA5, 0x63, 0xB4, 0x9D, 0x4C, 0xB4, 0x63,
 0x4C, 0x9D, 0x86, 0xCA, 0x7A, 0x36, 0xCA, 0x86,
 0x36, 0x7A, 0xB6, 0xD7, 0x4A, 0x29, 0xD7, 0xB6,
 0x29, 0x4A, 0xC8, 0xD7, 0x38, 0x29, 0xD7, 0xC8,
 0x29, 0x38, 0xA4, 0xD8, 0x5C, 0x28, 0xD8, 0xA4,
 0x28, 0x5C, 0x6C, 0xC1, 0x94, 0x3F, 0xC1, 0x6C,
 0x3F, 0x94, 0xD9, 0xD9, 0x27, 0x27, 0x80, 0x80
};
