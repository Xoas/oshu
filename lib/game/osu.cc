/**
 * \file lib/game/osu.cc
 * \ingroup game_osu
 *
 * Implement the osu! standard mode.
 */

#include "game/osu.h"

#include "game/base.h"
#include "video/texture.h"

#include <assert.h>

osu_game::osu_game(const char *beatmap_path)
: oshu::game::base(beatmap_path)
{
}

/**
 * Find the first clickable hit object that contains the given x/y coordinates.
 *
 * A hit object is clickable if it is close enough in time and not already
 * clicked.
 *
 * If two hit objects overlap, yield the oldest unclicked one.
 */
static struct oshu_hit* find_hit(struct osu_game *game, oshu_point p)
{
	struct oshu_hit *start = oshu_look_hit_back(game, game->beatmap.difficulty.approach_time);
	double max_time = game->clock.now + game->beatmap.difficulty.approach_time;
	for (struct oshu_hit *hit = start; hit->time <= max_time; hit = hit->next) {
		if (!(hit->type & (OSHU_CIRCLE_HIT | OSHU_SLIDER_HIT)))
			continue;
		if (hit->state != OSHU_INITIAL_HIT)
			continue;
		if (std::abs(p - hit->p) <= game->beatmap.difficulty.circle_radius)
			return hit;
	}
	return NULL;
}

/**
 * Free the texture associated to a slider, when the slider gets old.
 *
 * It is unlikely that once a slider is marked as good or missed, it will ever
 * be shown once again. The main exception is when the user seeks.
 */
static void jettison_hit(struct oshu_hit *hit)
{
	if (hit->texture) {
		oshu_destroy_texture(hit->texture);
		free(hit->texture);
		hit->texture = NULL;
	}
}

/**
 * Release the held slider, either because the held key is released, or because
 * a new slider is activated (somehow).
 */
static void release_slider(struct osu_game *game)
{
	struct oshu_hit *hit = game->current_slider;
	if (!hit)
		return;
	assert (hit->type & OSHU_SLIDER_HIT);
	if (game->clock.now < oshu_hit_end_time(hit) - game->beatmap.difficulty.leniency) {
		hit->state = OSHU_MISSED_HIT;
	} else {
		hit->state = OSHU_GOOD_HIT;
		oshu_play_sound(&game->library, &hit->slider.sounds[hit->slider.repeat], &game->audio);
	}
	jettison_hit(hit);
	oshu_stop_loop(&game->audio);
	game->current_slider = NULL;
}

/**
 * Make the sliders emit a sound when reaching an end point.
 *
 * When the last one is reached, release the slider.
 */
static void sonorize_slider(struct osu_game *game)
{
	struct oshu_hit *hit = game->current_slider;
	if (!hit)
		return;
	assert (hit->type & OSHU_SLIDER_HIT);
	int t = (game->clock.now - hit->time) / hit->slider.duration;
	int prev_t = (game->clock.before - hit->time) / hit->slider.duration;
	if (game->clock.now > oshu_hit_end_time(hit)) {
		release_slider(game);
	} else if (t > prev_t && prev_t >= 0) {
		assert (t <= hit->slider.repeat);
		oshu_play_sound(&game->library, &hit->slider.sounds[t], &game->audio);
	}
}

/**
 * Get the audio position and deduce events from it.
 *
 * For example, when the audio is past a hit object and beyond the threshold of
 * tolerance, mark that hit as missed.
 *
 * Also mark sliders as missed if the mouse is too far from where it's supposed
 * to be.
 */
int osu_game::check()
{
	/* Ensure the mouse follows the slider. */
	sonorize_slider(this); /* < may release the slider! */
	if (this->current_slider && mouse) {
		struct oshu_hit *hit = this->current_slider;
		double t = (this->clock.now - hit->time) / hit->slider.duration;
		oshu_point ball = oshu_path_at(&hit->slider.path, t);
		oshu_point m = mouse->position();
		if (std::abs(ball - m) > this->beatmap.difficulty.slider_tolerance) {
			oshu_stop_loop(&this->audio);
			this->current_slider = NULL;
			hit->state = OSHU_MISSED_HIT;
			jettison_hit(hit);
		}
	}
	/* Mark dead notes as missed. */
	double left_wall = this->clock.now - this->beatmap.difficulty.leniency;
	while (this->hit_cursor->time < left_wall) {
		struct oshu_hit *hit = this->hit_cursor;
		if (!(hit->type & (OSHU_CIRCLE_HIT | OSHU_SLIDER_HIT))) {
			hit->state = OSHU_UNKNOWN_HIT;
		} else if (hit->state == OSHU_INITIAL_HIT) {
			hit->state = OSHU_MISSED_HIT;
			jettison_hit(hit);
		}
		this->hit_cursor = hit->next;
	}
	return 0;
}

/**
 * Mark a hit as active.
 *
 * For a circle hit, mark it as good. For a slider, mark it as sliding.
 * Unknown hits are marked as unknown.
 *
 * The key is the held key, relevant only for sliders. In autoplay mode, it's
 * value doesn't matter.
 */
static void activate_hit(struct osu_game *game, struct oshu_hit *hit, enum oshu_finger key)
{
	if (hit->type & OSHU_SLIDER_HIT) {
		release_slider(game);
		hit->state = OSHU_SLIDING_HIT;
		game->current_slider = hit;
		game->held_key = key;
		oshu_play_sound(&game->library, &hit->sound, &game->audio);
		oshu_play_sound(&game->library, &hit->slider.sounds[0], &game->audio);
	} else if (hit->type & OSHU_CIRCLE_HIT) {
		hit->state = OSHU_GOOD_HIT;
		oshu_play_sound(&game->library, &hit->sound, &game->audio);
	} else {
		hit->state = OSHU_UNKNOWN_HIT;
	}
}

/**
 * Automatically activate the sliders on time.
 */
int osu_game::check_autoplay()
{
	sonorize_slider(this);
	while (this->hit_cursor->time < this->clock.now) {
		activate_hit(this, this->hit_cursor, OSHU_UNKNOWN_KEY);
		this->hit_cursor = this->hit_cursor->next;
	}
	return 0;
}

/**
 * Get the current mouse position, get the hit object, and change its state.
 *
 * Play a sample depending on what was clicked, and when.
 */
int osu_game::press(enum oshu_finger key)
{
	if (!mouse)
		return 0;
	oshu_point m = mouse->position();
	struct oshu_hit *hit = find_hit(this, m);
	if (!hit)
		return 0;
	if (fabs(hit->time - this->clock.now) < this->beatmap.difficulty.leniency) {
		activate_hit(this, hit, key);
		hit->offset = this->clock.now - hit->time;
	} else {
		hit->state = OSHU_MISSED_HIT;
		jettison_hit(hit);
	}
	return 0;
}

/**
 * When the user is holding a slider or a hold note in mania mode, release the
 * thing.
 */
int osu_game::release(enum oshu_finger key)
{
	if (this->held_key == key)
		release_slider(this);
	return 0;
}

int osu_game::relinquish()
{
	if (this->current_slider) {
		this->current_slider->state = OSHU_INITIAL_HIT;
		oshu_stop_loop(&this->audio);
		this->current_slider = NULL;
	}
	return 0;
}
