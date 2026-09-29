# Shared by the Spread scenes, which define probe() and kad(). Positions come
# from where Spread draws the cards, so no scene assumes the tablet's size.

# Carry the card beside the centred one onto it and rest there, as a person
# joins two cards into a Stack. The card is left held and armed to join; the
# caller lets go, and can read the release as it settles. The card it joins is
# found again where it stands once the carry has begun.
carry_onto_centre() {
    local id=$1 context width centre_id centre_x centre_y pick_x finger target_x held_x
    context=$(kad workspaceContext)
    width=$(jq '.displayContext.displays[] | select(.role == "tablet") | .geometry.width' <<<"$context")
    centre_id=$(jq -r '[.applications[] | select(.hasCard and .selected)][0].windowId' <<<"$context")
    read -r centre_x centre_y < <(jq -r '[.applications[] | select(.hasCard and .selected)][0].spreadRect
        | "\((.x + .width / 2) | floor) \((.y + .height / 2) | floor)"' <<<"$context")
    # The middle of what shows of the card beside it.
    pick_x=$(jq -r --argjson width "$width" \
        '[.applications[] | select(.hasCard and (.selected | not) and .spreadRect)][0].spreadRect
         | (([.x, 0] | max) + ([.x + .width, $width] | min)) / 2 | floor' <<<"$context")
    probe down "$id" "$pick_x" "$centre_y"
    sleep .4
    finger=$((pick_x < centre_x ? pick_x + 30 : pick_x - 30))
    probe motion "$id" "$finger" "$centre_y"
    sleep .5
    target_x=$(kad workspaceContext | jq -r --arg id "$centre_id" \
        '.applications[] | select(.windowId == $id) | .spreadRect | (.x + .width / 2) | floor')
    held_x=$(kad nativeCarryState | jq -r '.lineRect | (.x + .width / 2) | floor')
    probe motion "$id" "$((finger + target_x - held_x))" "$centre_y"
    sleep .6
    kad nativeCarryState | jq -e '.lineCarrying and .carryAim == "join"' >/dev/null
}
