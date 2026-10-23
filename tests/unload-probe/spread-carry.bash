# Shared by the Spread scenes, which define probe() and kad(). Positions come
# from where Spread draws the cards, so no scene assumes the tablet's size.

# Carry the card beside the centred one onto it and rest there, as a person
# joins two cards into a Stack: the held card is brought to where the centred
# card showed before the pickup. The card is left held and armed to join; the
# caller lets go, and can read the release as it settles. A second argument,
# left or right, names the side the carried card comes from; without it, the
# nearer card beside the centred one is carried.
carry_onto_centre() {
    local id=$1 side=${2:-} context width centre_x centre_y pick_x finger held_x
    context=$(kad workspaceContext)
    width=$(jq '.displayContext.displays[] | select(.role == "tablet") | .geometry.width' <<<"$context")
    read -r centre_x centre_y < <(jq -r '[.applications[] | select(.hasCard and .selected)][0].spreadRect
        | "\((.x + .width / 2) | floor) \((.y + .height / 2) | floor)"' <<<"$context")
    # The middle of what shows of the nearest card beside it on that side.
    pick_x=$(jq -r --argjson width "$width" --argjson centre "$centre_x" --arg side "$side" \
        '[.applications[] | select(.hasCard and (.selected | not) and .spreadRect)
          | select($side == "" or (($side == "left") == (.spreadRect.x + .spreadRect.width / 2 < $centre)))]
         | sort_by((.spreadRect.x + .spreadRect.width / 2 - $centre) | if . < 0 then -. else . end)[0].spreadRect
         | (([.x, 0] | max) + ([.x + .width, $width] | min)) / 2 | floor' <<<"$context")
    probe down "$id" "$pick_x" "$centre_y"
    sleep .4
    finger=$((pick_x < centre_x ? pick_x + 30 : pick_x - 30))
    probe motion "$id" "$finger" "$centre_y"
    sleep .5
    held_x=$(kad nativeCarryState | jq -r '.lineRect | (.x + .width / 2) | floor')
    probe motion "$id" "$((finger + centre_x - held_x))" "$centre_y"
    sleep .6
    kad nativeCarryState | jq -e '.lineCarrying and .carryAim == "join"' >/dev/null
}
