#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "libretro.h"

#define FONT "cairo:monospace"
#define FONT_SIZE 20
#define SPACING    (FONT_SIZE * 0.4)
#define TILE_SIZE (FONT_SIZE * 4)
#define TILE_ANIM_SPEED 5

#define GRID_WIDTH   4
#define GRID_HEIGHT  4
#define GRID_SIZE    (GRID_WIDTH * GRID_HEIGHT)

#define BOARD_WIDTH  (SPACING + TILE_SIZE * GRID_WIDTH  + SPACING * (GRID_WIDTH  - 1) + SPACING)
#define BOARD_HEIGHT (SPACING + TILE_SIZE * GRID_HEIGHT + SPACING * (GRID_HEIGHT - 1) + SPACING)

#define BOARD_OFFSET_Y  SPACING + TILE_SIZE + SPACING

#define SCREEN_WIDTH   SPACING + BOARD_WIDTH + SPACING
#define SCREEN_HEIGHT  BOARD_OFFSET_Y + BOARD_HEIGHT + SPACING

#define PI 3.14159
extern int SCREEN_PITCH;

typedef struct
{
   int up;
   int down;
   int left;
   int right;
   int start;
   int select;
} key_state_t;

extern retro_environment_t environ_cb;
extern retro_video_refresh_t video_cb;
extern struct retro_log_callback logging;

void game_calculate_pitch(void);

void game_init(void);
void game_deinit(void);
void game_reset(void);
void game_update(float delta, key_state_t *new_ks);
void *game_data(void);
void *game_save_data(void);
unsigned game_data_size(void);
void game_render(void);
int game_init_pixelformat(void);

#endif // GAME_H
