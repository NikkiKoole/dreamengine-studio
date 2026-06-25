// config for bunnymark.c — uses a REAL true-colour PNG sheet (bunnymark_sheet.png)
// instead of the palette-indexed sprite builder, because the bunny is a shaded
// grayscale drawing tinted into 5 arbitrary colours (white/yellow/pink/green/
// orange) that the 32-colour pico32 palette can't express. spr()/sspr() sample
// the sheet texture directly, so true-colour + alpha transparency just works.
//
// SOURCE ART: tools/carts/bunnymark_bunny.png (32×32 grayscale, alpha).
// The sheet is FIVE 32×32 tints of it, each = source × tint colour (multiply),
// laid out 2×2-slots apart so each is an sspr() of a 32×32 region:
//   slot-px (0,0) white · (32,0) yellow · (64,0) pink · (96,0) green · (0,32) orange
// Regenerate (ImageMagick) if the source art changes:
//   SRC=tools/carts/bunnymark_bunny.png; magick "$SRC" -alpha extract a.png
//   for HEX in FFF1E8 FFEC27 FF77A8 00E436 FFA300; do
//     magick "$SRC" -colorspace sRGB -type TrueColor -alpha off \
//       \( -size 32x32 xc:"#$HEX" \) -compose multiply -composite rgb.png
//     magick rgb.png a.png -alpha off -compose CopyOpacity -composite PNG32:tint_$HEX.png; done
//   # then composite the five into a 128×128 xc:none canvas at the px offsets above.

module.exports = { spritesPng: 'bunnymark_sheet.png' }
