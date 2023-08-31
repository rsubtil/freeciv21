
[spec]

; Format and options of this spec file:
options = "+Freeciv-spec-Devel-2019-Jul-03"

[info]

artists = "
    ev1lbl0w
"

[file]
gfx = "misc/jury_vote"

[grid_main]

x_top_left = 0
y_top_left = 0
dx = 48
dy = 48

tiles = { "row", "column", "tag"

; Agree/disagree thumbs:

  0,  0, "jury_vote.none"
  0,  1, "jury_vote.yes"
  0,  2, "jury_vote.no"
  0,  3, "jury_vote.abstain"

}
