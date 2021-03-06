/**
 * \file video/view.h
 * \ingroup video_view
 */

#pragma once

#include "core/geometry.h"

struct oshu_display;

/**
 * \defgroup video_view View
 * \ingroup video
 *
 * \brief
 * Define and transform an affine coordinate system.
 *
 * ### Anatomy of a window
 *
 * SDL provides us with a physical window, whose every pixel is mapped to a
 * distinct pixel on the screen. Its size is arbitrary and the user may resize
 * it at any time. The actual position and size of objects drawn directly on
 * the window is predictable, but you have to take into account the window's
 * size if you don't want to overflow.
 *
 * Inside the window, we have a logicial *viewport*, whose virtual size is
 * always 640x480. When the window is bigger, the viewport is automatically
 * scaled to be as large as possible in the window, without losing its aspect
 * ratio. This is what you'd use to draw game components that should move and
 * scale when the window is resized.
 *
 * The *game area* is a 512x384 rectangle centered inside the viewport. Since
 * game coordinates are used throughout the beatmap, this will be your usual
 * reference.
 *
 * Note that different modes may use differents coordinate systems. The
 * following section will focus on the standard osu mode.
 *
 * ### Coordinate systems
 *
 * The coordinate system the beatmaps use is not the same as the coordinate
 * system of the SDL window. Let's take some time to define it.
 *
 * The original osu! viewport is 640x480 pixels, while the playable game zone
 * is 512x384. The game zone is centered in the viewport, leaving margin for
 * the notes at each corner of the game zone.
 *
 * ```
 * ┌───────────────────────────────────────────────────────────┐
 * │ ↖                                          ↑              │
 * │  (0,0) in viewport coordinates             | 48px         │
 * │  or (-64, -48) in game coordinates         ↓              │
 * │    ┌─────────────────────────────────────────────────┐    │
 * │    │ ↖                                               │    │
 * │    │  (0,0) in game coordinates                      │    │
 * │    │                                                 │    │
 * │    │                                                3│    │4
 * │    │                                                8│    │8
 * │    │                                                4│    │0
 * │    │                                                p│    │p
 * │    │                                                x│    │x
 * │    │                                                 │    │
 * │←——→│                                                 │←——→│
 * │64px│                                                 │64px│
 * │    │                      512px                      │    │
 * │    └─────────────────────────────────────────────────┘    │
 * │                                             ↑             │
 * │                                             | 48px        │
 * │                                             ↓             │
 * └───────────────────────────────────────────────────────────┘
 *                             640px
 * ```
 *
 * The above transformation is performed by #oshu_resize_view.
 *
 * When the window grows, this whole viewport is scaled, and not just the game
 * area. The viewport will be zoomed to fit the available space, while
 * preserving the aspect ratio and without cropping. This means that if the
 * ratio of the user's window doesn't have the right ratio, black bars are
 * added, like when playing cinemascope movies.
 *
 * This is what happens when the window is not wide enough:
 *
 * ```
 * ┌───────────────────────────────────────────────────────┐
 * │******    ↕ (window height - 480 × zoom) / 2     ******│
 * ┢━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┪
 * ┃                                                       ┃
 * ┃                                                       ┃
 * ┃↕ 480 × zoom                                           ┃
 * ┃                                                       ┃
 * ┃                     640 × zoom                        ┃
 * ┡━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┩
 * │******    ↕ (window height - 480 × zoom) / 2     ******│
 * └───────────────────────────────────────────────────────┘
 *                window width = 640 × zoom
 * ```
 *
 * This is done by #oshu_fit_view.
 *
 * \{
 */

/**
 * Define a coordinate system.
 *
 * More concretely, this is an affine transformation system, where zoom is the
 * factor and (x, y) the constant. `v(p) = z p + o`
 *
 * Transformation operations on the view like #oshu_resize_view,
 * #oshu_scale_view and #oshu_fit_view are composed on the right:
 * `v(v'(p)) = z (z' p + o') + o = z z' p + z o' + o`
 *
 * For ease of understanding, transformation of views are explained in terms of
 * logical coordinates and physical coordinates. Views convert logical
 * coordinates into physical coordinates, so the logical one is the input, and
 * physical one the output.
 */
struct oshu_view {
	double zoom;
	oshu_point origin;
	oshu_size size;
};

/**
 * Change the size of the view without zooming.
 *
 * The new view's center is aligned with the old one by translating the origin
 * accordingly.
 *
 * This lets you cut the view to introduce margins. Say, you're original view
 * is 400x300, and you resize it to 300x200, then you'll have 50px margins on
 * every side.
 *
 * The aspect ratio needs no be preserved.
 *
 * Definition:
 * - `v(x) = x + (physical width - logical width) / 2`
 *
 * Properties:
 * - `v(logical width / 2) = physical width / 2`
 *
 */
void oshu_resize_view(struct oshu_view *view, oshu_size size);

/**
 * Scale the coordinate system.
 *
 * Scaling by 2 means that a length of 10px will be displayed as 20px.
 *
 * The logical view is downscaled so that it still reflects the physical
 * screen. Scaling a 800x600 view by 2 means that you'll only have 400x300
 * pixels left to draw on.
 *
 * Definition:
 * - `v(x) = factor * x`
 * - `logical width = physical width / factor`
 *
 * Properties:
 * - `v(0) = 0`
 * - `v(logical width) = physical width`
 *
 */
void oshu_scale_view(struct oshu_view *view, double factor);

/**
 * Scale and resize the view while preserving the aspect ratio.
 *
 * The resulting view's logical size is *w × h*, and appears centered and as
 * big as possible in the window, without cutting it.
 *
 * Properties:
 * - The center of the view is the center of the window:
 *   `v(logical width / 2) = physical width / 2`
 * - The view is not cut:
 *   `0 ≤ v(0) ≤ v(logical width) ≤ physical width`
 */
void oshu_fit_view(struct oshu_view *view, oshu_size size);

/**
 * Reset the display's view to the identity view.
 *
 * The size of the window is automatically retrieved from the SDL window.
 *
 * The resulting view is stored in the display's #oshu_display::view attribute.
 */
void oshu_reset_view(struct oshu_display *display);

/**
 * Project a point from logical coordinates to physical coordinates.
 *
 * Applies the affine transformation: `v(p) = z p + o`.
 *
 * \sa oshu_unproject
 */
oshu_point oshu_project(struct oshu_view *view, oshu_point p);

/**
 * Unproject a point from physical coordinates to logical coordinates.
 *
 * This is the opposite operation of #oshu_project.
 *
 * From the definition of the view, `v(p) = z p + o`,
 * we deduce `p = (v(p) - o) / z`.
 */
oshu_point oshu_unproject(struct oshu_view *view, oshu_point p);

/** \} */
