
[spec]

; Format and options of this spec file:
options = "+Freeciv-spec-Devel-2019-Jul-03"

[info]

artists = "
    Ricardo Subtil[~]
"

[file]
gfx = "hexemplio/cityextras"

[grid_main]

x_top_left = 1
y_top_left = 1
dx = 90
dy = 9
pixel_border = 1

tiles = { "row", "column", "tag"
; Unit activity letters:  (note unit icons have just "u.")

; City hit-point bars: approx percent of hp remaining
; [~]

  0,  0, "city.hp_100"
  1,  0, "city.hp_95"
  2,  0, "city.hp_90"
  3,  0, "city.hp_85"
  0,  1, "city.hp_80"
  1,  1, "city.hp_75"
  2,  1, "city.hp_70"
  3,  1, "city.hp_65"
  0,  2, "city.hp_60"
  1,  2, "city.hp_55"
  2,  2, "city.hp_50"
  3,  2, "city.hp_45"
  0,  3, "city.hp_40"
  1,  3, "city.hp_35"
  2,  3, "city.hp_30"
  3,  3, "city.hp_25"
  0,  4, "city.hp_20"
  1,  4, "city.hp_15"
  2,  4, "city.hp_10"
  3,  4, "city.hp_5"

}
