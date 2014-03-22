#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define FONT "cairo:monospace"
#define FONT_SIZE 20
#define SPACING    (FONT_SIZE * 0.4)
#define TILE_SIZE (FONT_SIZE * 4)

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
} key_state_t;

void game_calculate_pitch(void);

void game_init(uint16_t *frame_buf);
void game_deinit(void);
void game_reset(void);
void game_update(float delta, key_state_t *new_ks);
void game_render(void);

#endif // GAME_H
