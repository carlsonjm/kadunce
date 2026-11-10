/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once
#include <algorithm>

namespace Kadunce {

// A pane a layout moves or resizes (CARD-LIFECYCLE.md §11, Connected) is
// drawn in a box that travels from its old place to its new one, while its
// client draws the old size or the new one at any moment. Its picture keeps
// the window's own scale, its top-left corner at the box's, and the box
// crops it, as a window being resized by hand is. It shrinks to cover a box
// smaller than it both ways, and never grows: a picture smaller than the box
// leaves the rest of the box to the card backing rather than being blown up.
struct PaneResizeRect {
    double x = 0.0, y = 0.0, width = 0.0, height = 0.0;
};

inline PaneResizeRect paneResizePicture(const PaneResizeRect &box, double pictureWidth, double pictureHeight)
{
    if (box.width <= 0.0 || box.height <= 0.0 || pictureWidth <= 0.0 || pictureHeight <= 0.0)
        return box;
    const double cover = std::max(box.width / pictureWidth, box.height / pictureHeight);
    const double scale = std::min(1.0, cover);
    return {box.x, box.y, pictureWidth * scale, pictureHeight * scale};
}

// Whether a picture leaves part of its box uncovered, a pixel's fraction aside.
inline bool paneResizeLeavesRoom(const PaneResizeRect &box, const PaneResizeRect &picture)
{
    return picture.width < box.width - 0.5 || picture.height < box.height - 0.5;
}

} // namespace Kadunce
