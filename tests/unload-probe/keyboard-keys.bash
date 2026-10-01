# Shared by the keyboard scenes. Where the Shuffle Keyboard draws a key, from
# the panel KWin reports and the Keyboard's own sums, so a scene taps a key
# rather than a fraction of whatever layout was current when it was written.
# The card spans the screen less a 10 px gutter either side, under a 26 px top
# strip; four rows of square keys share a twelve-and-a-half-unit block centred
# in it. Prints "x y".
#   hide      the Hide key, at the bottom row's right end
#   z         Z, the first letter of the third row, low in the key, where a
#             card drawn over the keys' top rows does not reach
keyboard_key_point() {
    jq -r --arg key "$1" '
        def clamp(lo; hi; v): [lo, ([hi, v] | min)] | max;
        (.width + 20) as $w
        | clamp(5; 10; $w * 0.0062) as $gap
        | clamp(6; 12; $w * 0.007) as $outer
        | ((.height - 26 - $gap * 3 - $outer) / 4) as $row
        | ([$row, (($w - 20 - $outer * 2 + $gap) / 12.5 - $gap)] | min) as $unit
        | ($unit + $gap) as $pitch
        | (12.5 * $pitch - $gap) as $field
        | (.x + $outer + ((.width - $outer * 2) - $field) / 2) as $left
        | (.y + 26) as $top
        | if $key == "hide" then
              [$left + 11 * $pitch + (1.5 * $pitch - $gap) / 2, $top + 3 * ($row + $gap) + $row / 2]
          elif $key == "z" then
              [$left + 1.75 * $pitch + $unit / 2, $top + 2 * ($row + $gap) + $row * 0.85]
          else error("no key " + $key) end
        | map(floor) | "\(.[0]) \(.[1])"' <<<"$2"
}
