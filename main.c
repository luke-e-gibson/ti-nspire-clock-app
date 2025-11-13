#ifndef __TINSPIRE__
#define __TINSPIRE__
#endif

#include <libndls.h>
#include <os.h>
#include <SDL/SDL.h>
#include <SDL/SDL_config.h>
#include <SDL/SDL_video.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Ndless SDK maps for RTC and keypad usage.
static volatile uint32_t *const rtc_seconds = (volatile uint32_t *)0x90090000;

// SDL line helpers rely on explicit pi to avoid math library macros missing.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 (M_PI / 2.0)
#endif

static void put_pixel(SDL_Surface *surface, int x, int y, Uint32 color) {
	if (x < 0 || y < 0 || x >= surface->w || y >= surface->h)
		return;
	uint8_t *pixel_addr = (uint8_t *)surface->pixels + y * surface->pitch + x * surface->format->BytesPerPixel;
	switch (surface->format->BytesPerPixel) {
	case 1:
		*pixel_addr = (uint8_t)color;
		break;
	case 2:
		*(uint16_t *)pixel_addr = (uint16_t)color;
		break;
	case 3:
		pixel_addr[0] = (uint8_t)(color);
		pixel_addr[1] = (uint8_t)(color >> 8);
		pixel_addr[2] = (uint8_t)(color >> 16);
		break;
	case 4:
		*(uint32_t *)pixel_addr = color;
		break;
	default:
		break;
	}
}

static void draw_circle(SDL_Surface *surface, int cx, int cy, int radius, Uint32 color) {
	int x = radius;
	int y = 0;
	int err = 1 - x;
	while (x >= y) {
		put_pixel(surface, cx + x, cy + y, color);
		put_pixel(surface, cx + y, cy + x, color);
		put_pixel(surface, cx - y, cy + x, color);
		put_pixel(surface, cx - x, cy + y, color);
		put_pixel(surface, cx - x, cy - y, color);
		put_pixel(surface, cx - y, cy - x, color);
		put_pixel(surface, cx + y, cy - x, color);
		put_pixel(surface, cx + x, cy - y, color);
		y++;
		if (err < 0) {
			err += 2 * y + 1;
		} else {
			x--;
			err += 2 * (y - x + 1);
		}
	}
}

static void draw_line(SDL_Surface *surface, int x0, int y0, int x1, int y1, Uint32 color) {
	int dx = abs(x1 - x0);
	int dy = -abs(y1 - y0);
	int sx = x0 < x1 ? 1 : -1;
	int sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;
	while (true) {
		put_pixel(surface, x0, y0, color);
		if (x0 == x1 && y0 == y1)
			break;
		int e2 = 2 * err;
		if (e2 >= dy) {
			err += dy;
			x0 += sx;
		}
		if (e2 <= dx) {
			err += dx;
			y0 += sy;
		}
	}
}

static void draw_tick(SDL_Surface *surface, int cx, int cy, int radius, int length, double angle, Uint32 color) {
	double s = sin(angle);
	double c = cos(angle);
	int x0 = (int)(cx + s * (radius - length));
	int y0 = (int)(cy - c * (radius - length));
	int x1 = (int)(cx + s * radius);
	int y1 = (int)(cy - c * radius);
	draw_line(surface, x0, y0, x1, y1, color);
}

static void draw_hand(SDL_Surface *surface, int cx, int cy, int length, double angle, Uint32 color, int thickness) {
	int x1 = (int)(cx + sin(angle) * length);
	int y1 = (int)(cy - cos(angle) * length);
	for (int offset = -thickness; offset <= thickness; ++offset) {
		// Offset hand by rotating perpendicular direction for simple width effect.
		double perp_angle = angle + M_PI_2;
		int shift_x = (int)(cos(perp_angle) * offset);
		int shift_y = (int)(sin(perp_angle) * offset);
		draw_line(surface, cx + shift_x, cy + shift_y, x1 + shift_x, y1 + shift_y, color);
	}
}

static void render_clock(SDL_Surface *screen, nSDL_Font *font, const struct tm *tm_info, int32_t offset_seconds) {
	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 12, 20, 32));
	bool locked = false;
	if (SDL_MUSTLOCK(screen)) {
		if (SDL_LockSurface(screen) == 0)
			locked = true;
		else
			return;
	}

	const int cx = 160;
	const int cy = 95;
	const int radius = 80;
	const int hour_length = (radius * 55) / 100;
	const int minute_length = (radius * 80) / 100;
	const int second_length = (radius * 90) / 100;
	Uint32 border_color = SDL_MapRGB(screen->format, 220, 220, 220);
	Uint32 tick_major = SDL_MapRGB(screen->format, 255, 255, 255);
	Uint32 tick_minor = SDL_MapRGB(screen->format, 160, 160, 160);
	Uint32 hour_color = SDL_MapRGB(screen->format, 255, 230, 128);
	Uint32 minute_color = SDL_MapRGB(screen->format, 128, 220, 255);
	Uint32 second_color = SDL_MapRGB(screen->format, 255, 96, 96);

	draw_circle(screen, cx, cy, radius, border_color);
	for (int mark = 0; mark < 60; ++mark) {
		double angle = (mark / 60.0) * (2.0 * M_PI);
		int length = (mark % 5 == 0) ? 12 : 6;
		draw_tick(screen, cx, cy, radius - 4, length, angle, mark % 5 == 0 ? tick_major : tick_minor);
	}

	double seconds_fraction = tm_info->tm_sec / 60.0;
	double minutes_fraction = (tm_info->tm_min + seconds_fraction) / 60.0;
	double hours_fraction = ((tm_info->tm_hour % 12) + minutes_fraction) / 12.0;
	double second_angle = seconds_fraction * 2.0 * M_PI;
	double minute_angle = minutes_fraction * 2.0 * M_PI;
	double hour_angle = hours_fraction * 2.0 * M_PI;

	draw_hand(screen, cx, cy, hour_length, hour_angle, hour_color, 2);
	draw_hand(screen, cx, cy, minute_length, minute_angle, minute_color, 1);
	draw_hand(screen, cx, cy, second_length, second_angle, second_color, 0);
	draw_circle(screen, cx, cy, 4, border_color);

	if (locked)
		SDL_UnlockSurface(screen);

	char digital[48];
	char offset_str[48];
	if (!strftime(digital, sizeof(digital), "%Y-%m-%d %H:%M:%S", tm_info)) {
		strncpy(digital, "--:--:--", sizeof(digital));
		digital[sizeof(digital) - 1] = '\0';
	}
	int32_t minutes_offset = offset_seconds / 60;
	int32_t seconds_offset = offset_seconds % 60;
	if (seconds_offset < 0 && minutes_offset > 0) {
		seconds_offset += 60;
		minutes_offset -= 1;
	} else if (seconds_offset > 0 && minutes_offset < 0) {
		seconds_offset -= 60;
		minutes_offset += 1;
	}
	snprintf(offset_str, sizeof(offset_str), "Offset: %+ld min %+ld sec", (long)minutes_offset, (long)seconds_offset);
	nSDL_DrawString(screen, font, 50, 190, digital);
	nSDL_DrawString(screen, font, 50, 210, offset_str);
	nSDL_DrawString(screen, font, 50, 225, "+/- adjust offset, ESC exits");
	SDL_Flip(screen);
}

int main(void) {
	assert_ndless_rev(801);

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
		return 1;

	SDL_Surface *screen = SDL_SetVideoMode(320, 240, has_colors ? 16 : 8, SDL_SWSURFACE);
	if (!screen) {
		SDL_Quit();
		return 1;
	}

	nSDL_Font *font = nSDL_LoadFont(NSDL_FONT_TINYTYPE, 255, 255, 255);
	if (!font) {
		SDL_Quit();
		return 1;
	}

	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	uint32_t last_second = (uint32_t)-1;
	int32_t time_offset = 0;
	bool running = true;
	bool redraw = true;
	SDL_Event event;

	while (running) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_KEYDOWN) {
				switch (event.key.keysym.sym) {
				case SDLK_ESCAPE:
					running = false;
					break;
				case SDLK_PLUS:
				case SDLK_EQUALS:
				case SDLK_KP_PLUS:
					time_offset += 60;
					redraw = true;
					break;
				case SDLK_MINUS:
				case SDLK_UNDERSCORE:
				case SDLK_KP_MINUS:
					time_offset -= 60;
					redraw = true;
					break;
				default:
					break;
				}
			}
		}

		if (isKeyPressed(KEY_NSPIRE_ESC))
			running = false;
		uint32_t raw = *rtc_seconds;
		if (raw != last_second) {
			last_second = raw;
			redraw = true;
		}

		if (redraw) {
			int64_t adjusted = (int64_t)raw + time_offset;
			if (adjusted < 0)
				adjusted = 0;
			time_t rtc_time = (time_t)adjusted;
			struct tm *tm_info = gmtime(&rtc_time);
			if (tm_info)
				render_clock(screen, font, tm_info, time_offset);
			redraw = false;
		}

		SDL_Delay(60);
	}

	nSDL_FreeFont(font);
	SDL_Quit();
	return 0;
}
