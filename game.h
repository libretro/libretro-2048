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

typedef enum
{
   DIR_NONE,
   DIR_UP,
   DIR_RIGHT,
   DIR_DOWN,
   DIR_LEFT
} direction_t;

typedef enum
{
   STATE_TITLE,
   STATE_PLAYING,
   STATE_GAME_OVER,
   STATE_WON,
   STATE_PAUSED
} game_state_t;

typedef struct vector
{
   int x;
   int y;
} vector_t;

typedef struct cell {
   int value;
   vector_t pos;
   vector_t old_pos;
   float move_time;
   float appear_time;
   struct cell *source;
} cell_t;

typedef struct game {
   int score;
   int best_score;
   game_state_t state;
   key_state_t old_ks;
   direction_t direction;
   cell_t grid[GRID_SIZE];
} game_t;

extern retro_environment_t environ_cb;
extern retro_video_refresh_t video_cb;
extern retro_log_printf_t log_cb;

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

void render_playing(void);
void render_title(void);
void render_win_or_game_over(void);
void render_paused(void);

#endif // GAME_H
