"""Generate all flash-resident, antialiased dashboard fonts.

Usage: python3 tools/generate_smooth_digits.py [font.otf]
Requires Pillow. TTF and OTF source fonts are supported.
"""

from argparse import ArgumentParser
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
DIGIT_CHARS = "0123456789-"
LABEL_CHARS = "0123456789-%ABCDEFGHIJKLMNOPQRSTUVWXYZ"
UNIT_CHARS = "0123456789-%ABCDEFGHIJKLMNOPQRSTUVWXYZaghmp"


def render_font(font_path: Path, size: int, chars: str):
    font = ImageFont.truetype(str(font_path), size)
    packed: list[int] = []
    glyphs: list[tuple[int, int, int, int, int, int]] = []
    for char in chars:
        left, top, right, bottom = font.getbbox(char, anchor="ls")
        width, height = right - left, bottom - top
        advance = round(font.getlength(char))
        if not (0 < width <= 255 and 0 < height <= 255 and
                -128 <= left <= 127 and -128 <= top <= 127 and
                0 < advance <= 255):
            raise ValueError(f"glyph {char!r} does not fit SmoothGlyph metadata")

        bitmap = Image.new("L", (width, height), 0)
        ImageDraw.Draw(bitmap).text(
            (-left, -top), char, font=font, fill=255, anchor="ls"
        )
        alpha = [(value * 15 + 127) // 255 for value in bitmap.tobytes()]
        offset = len(packed)
        for index in range(0, len(alpha), 2):
            packed.append(
                (alpha[index] << 4) |
                (alpha[index + 1] if index + 1 < len(alpha) else 0)
            )
        glyphs.append((offset, width, height, left, top, advance))
    return glyphs, packed


def append_set(lines: list[str], prefix: str, chars: str, glyphs, packed) -> None:
    lines.append(f'static const char {prefix}_CHARS[] = "{chars}";')
    lines.append(f"static const SmoothGlyph {prefix}_GLYPHS[] PROGMEM = {{")
    for char, glyph in zip(chars, glyphs):
        lines.append("    {" + ", ".join(map(str, glyph)) + "}, // " + char)
    lines.extend(["};", "", f"static const uint8_t {prefix}_PIXELS[] PROGMEM = {{"])
    for index in range(0, len(packed), 16):
        lines.append("    " + ", ".join(f"0x{value:02X}" for value in packed[index:index + 16]) + ",")
    lines.extend(["};", ""])


def main() -> None:
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("font", nargs="?", type=Path, default=ROOT / "font.otf",
                        help="path to a TTF or OTF font (default: ./font.otf)")
    parser.add_argument("--size", type=int, default=166, help="focus digit source size in pixels")
    args = parser.parse_args()

    digit_lines = [
        "#pragma once", "#include <Arduino.h>", "",
        f"// Four-bit coverage digits generated from {args.font.name} at {args.size} px.",
        "// Generated bitmap data; see the source font's license before redistribution.",
        "struct SmoothGlyph {",
        "    uint32_t offset;", "    uint8_t width;", "    uint8_t height;",
        "    int8_t xOffset;", "    int8_t yOffset;", "    uint8_t advance;",
        "};", "",
    ]
    glyphs, packed = render_font(args.font, args.size, DIGIT_CHARS)
    append_set(digit_lines, "SMOOTH_DIGIT", DIGIT_CHARS, glyphs, packed)
    digit_output = ROOT / "smooth_digits.h"
    digit_output.write_text("\n".join(digit_lines).rstrip() + "\n")
    print(f"Wrote {digit_output} ({len(packed)} flash bytes)")

    text_lines = [
        "#pragma once", '#include "smooth_digits.h"', "",
        f"// Four-bit coverage text fonts generated from {args.font.name}.",
        "// Generated bitmap data; see the source font's license before redistribution.", "",
    ]
    total_bytes = 0
    for prefix, chars, size in (
        ("SMOOTH_LABEL", LABEL_CHARS, 16),
        ("SMOOTH_TITLE", LABEL_CHARS, 28),
        ("SMOOTH_SIDE", DIGIT_CHARS, 38),
        ("SMOOTH_SIDE_COMPACT", DIGIT_CHARS, 30),
        ("SMOOTH_SIDE_TINY", DIGIT_CHARS, 24),
        ("SMOOTH_UNIT", UNIT_CHARS, 18),
        ("SMOOTH_FOCUS_UNIT", "%CHagmp", 48),
        ("SMOOTH_SUBSCRIPT", "2O", 11),
    ):
        glyphs, packed = render_font(args.font, size, chars)
        append_set(text_lines, prefix, chars, glyphs, packed)
        total_bytes += len(packed)
    text_output = ROOT / "smooth_text.h"
    text_output.write_text("\n".join(text_lines).rstrip() + "\n")
    print(f"Wrote {text_output} ({total_bytes} flash bytes)")


if __name__ == "__main__":
    main()
