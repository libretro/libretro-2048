
#include "game.h"
#include "libretro.h"

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <cairo/cairo.h>

int SCREEN_PITCH = 0;

static cairo_surface_t *surface = NULL;
static cairo_t *ctx = NULL;

static cairo_pattern_t* color_lut[13];

static const char* label_lut[13] =
{
   "",
   "2", "4", "8", "16",
   "32", "64", "128", "256",
   "512", "1024", "2048",
   "XXX"
};

static enum
{
   DIR_NONE,
   DIR_UP,
   DIR_RIGHT,
   DIR_DOWN,
   DIR_LEFT
} direction;

typedef struct cell {
   int value;
   int grid_x, grid_y;
   struct cell *origin;
} cell_t;

static cell_t grid[GRID_SIZE];/* =
{
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0
};*/

static int game_score = 0;
static float frame_time = 0.016;

static key_state_t old_ks = {0};

static void set_source_rgb(cairo_t *ctx, int r, int g, int b)
{
   cairo_set_source_rgb(ctx, r / 255.0, g / 255.0, b / 255.0);
}

static void set_source_rgba(cairo_t *ctx, int r, int g, int b, float a)
{
   cairo_set_source_rgba(ctx, r / 255.0, g / 255.0, b / 255.0, a);
}

static void filled_rectangle(cairo_t *ctx, int x, int y, int w, int h)
{
   cairo_rectangle(ctx, x, y, w, h);
   cairo_fill(ctx);
}

static float lerp(float v0, float v1, float t)
{
   return v0 * (1 - t) + v1 * t;
}

static void move_tiles(void)
{
   int vec_x, vec_y;

   switch (direction) {
      case DIR_UP:
         vec_x = 0; vec_y = -1;
         break;
      case DIR_DOWN:
         vec_x = 0; vec_y = 1;
         break;
      case DIR_RIGHT:
         vec_x = 1; vec_y = 0;
         break;
      case DIR_LEFT:
         vec_x = -1; vec_y = 0;
         break;
      default:
         return;
         break;
   }

   int col_begin = 0;
   int col_end   = 4;
   int col_inc   = 1;
   int row_begin = 0;
   int row_end   = 4;
   int row_inc   = 1;

   if (vec_x > 0) {
      col_begin = 3;
      col_end   = -1;
      col_inc   = -1;
   }

   if (vec_y > 0) {
      row_begin = 3;
      row_end   = -1;
      row_inc   = -1;
   }

   for (int row = row_begin; row != row_end; row += row_inc) {
      for (int col = col_begin; col != col_end; col += col_inc) {

         cell_t *cell = &grid[row * 4 + col];
         if (!cell->value)
            continue;

         cell_t *farthest;
         cell_t *next = cell;

         int new_row = row , new_col = col;

         do {
            farthest = next;

            new_row += vec_y;
            new_col += vec_x;

            if (new_row < 0 || new_col < 0 || new_row > 3 || new_col > 3)
               break;

            next = &grid[new_row * 4 + new_col];
         } while (!next->value);

         // TODO: check for multiple merges (only one allowed)
         if (next->value && next->value == cell->value && next != cell) {
            next->value = cell->value + 1;
            next->origin = cell;
            cell->value = 0;
            game_score += 2 << next->value;
         } else if (farthest != cell) {
            farthest->value = cell->value;
            cell->value = 0;
         }
      }
   }
}

static void add_tile(void) {
   cell_t *empty[GRID_SIZE];

   int j = 0;
   for (int i = 0; i < GRID_SIZE; i++) {
      empty[j] = NULL;
      if (!grid[i].value)
         empty[j++] = &grid[i];
   }

   if (j)
      empty[random() % j]->value = (random() / RAND_MAX) < 0.9 ? 1 : 2;
   else
      puts("Game Over!");
      //logging.log(RETRO_LOG_INFO, "Game Over!");
}

static void handle_input(key_state_t *ks)
{
   direction = DIR_NONE;

   if (!ks->up && old_ks.up)
      direction = DIR_UP;
   else if (!ks->right && old_ks.right)
      direction = DIR_RIGHT;
   else if (!ks->down && old_ks.down)
      direction = DIR_DOWN;
   else if (!ks->left && old_ks.left)
      direction = DIR_LEFT;

   old_ks = *ks;
}

void game_calculate_pitch(void)
{
   SCREEN_PITCH = cairo_format_stride_for_width(CAIRO_FORMAT_RGB16_565, SCREEN_WIDTH);
}

void game_init(uint16_t *frame_buf)
{
   surface = cairo_image_surface_create_for_data(
            (unsigned char*)frame_buf, CAIRO_FORMAT_RGB16_565, SCREEN_WIDTH, SCREEN_HEIGHT,
            SCREEN_PITCH);

   ctx = cairo_create(surface);

   color_lut[0] = cairo_pattern_create_rgba(238 / 255.0, 228 / 255.0, 218 / 255.0, 0.35);
   color_lut[1] = cairo_pattern_create_rgb(238 / 255.0, 228 / 255.0, 218 / 255.0);

   color_lut[2] = cairo_pattern_create_rgb(237 / 255.0, 224 / 255.0, 200 / 255.0);
   color_lut[3] = cairo_pattern_create_rgb(242 / 255.0, 177 / 255.0, 121 / 255.0);
   color_lut[4] = cairo_pattern_create_rgb(245 / 255.0, 149 / 255.0, 99 / 255.0);
   color_lut[5] = cairo_pattern_create_rgb(246 / 255.0, 124 / 255.0, 95 / 255.0);
   color_lut[6] = cairo_pattern_create_rgb(246 / 255.0, 94 / 255.0, 59 / 255.0);

   // TODO: shadow
   color_lut[7] = cairo_pattern_create_rgb(237 / 255.0, 207 / 255.0, 114 / 255.0);
   color_lut[8] = cairo_pattern_create_rgb(237 / 255.0, 204 / 255.0, 97 / 255.0);
   color_lut[9] = cairo_pattern_create_rgb(237 / 255.0, 200 / 255.0, 80 / 255.0);
   color_lut[10] = cairo_pattern_create_rgb(237 / 255.0, 197 / 255.0, 63 / 255.0);
   color_lut[11] = cairo_pattern_create_rgb(237 / 255.0, 194 / 255.0, 46 / 255.0);
   color_lut[12] = cairo_pattern_create_rgb(60 / 255.0, 58 / 255.0, 50 / 255.0);

   game_reset();
}

void game_deinit(void)
{
   for (int i = 0; i < 13; i++) {
      cairo_pattern_destroy(color_lut[i]);
      color_lut[i] = NULL;
   }

   cairo_destroy(ctx);
   cairo_surface_destroy(surface);

   ctx     = NULL;
   surface = NULL;
}

void game_reset(void)
{
   game_score = 0;

   for (int row = 0; row < 4; row++) {
      for (int col = 0; col < 4; col++) {
         grid[row * 4 + col] = (cell_t) {
            .grid_y = row,
            .grid_x = col,
            .value = 0,
            .origin = NULL
         };
      }
   }

   memset(&old_ks, 0, sizeof(old_ks));
}

void game_update(float delta, key_state_t *new_ks)
{
   frame_time = delta;

   handle_input(new_ks);

   if (direction != DIR_NONE) {
      move_tiles();
      add_tile();
   }
}

void game_render(void)
{
   // bg
   set_source_rgb(ctx, 250, 248, 239);
   filled_rectangle(ctx, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

   // grid bg
   set_source_rgb(ctx, 185, 172, 159);
   filled_rectangle(ctx, SPACING, BOARD_OFFSET_Y, BOARD_WIDTH, BOARD_WIDTH);

   // score bg
   set_source_rgb(ctx, 185, 172, 159);
   filled_rectangle(ctx, SPACING, SPACING, TILE_SIZE*2+SPACING*2, TILE_SIZE);

   // best bg
   set_source_rgb(ctx, 185, 172, 159);
   filled_rectangle(ctx, TILE_SIZE*2+SPACING*4, SPACING, TILE_SIZE*2+SPACING*2, TILE_SIZE);

   cairo_text_extents_t extents;
   cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
   cairo_set_font_size(ctx, FONT_SIZE);

   // score title
   cairo_set_source(ctx, color_lut[1]);
   cairo_text_extents(ctx, "SCORE", &extents);
   cairo_move_to(ctx,  SPACING*2, SPACING * 2 + extents.height);
   cairo_show_text(ctx, "SCORE");

   // best title
   cairo_set_source(ctx, color_lut[1]);
   cairo_text_extents(ctx, "BEST", &extents);
   cairo_move_to(ctx,  TILE_SIZE*2+SPACING*5, SPACING * 2 + extents.height);
   cairo_show_text(ctx, "BEST");

   char tmp[10] = {0};
   cairo_set_font_size(ctx, FONT_SIZE * 2);

   // score value
   sprintf(tmp, "%7i", game_score);
   cairo_set_source(ctx, color_lut[1]);
   cairo_text_extents(ctx, tmp, &extents);
   cairo_move_to(ctx,  SPACING*2, SPACING * 5 + extents.height);
   cairo_show_text(ctx, tmp);

   // best value
   sprintf(tmp, "%7i", 0);
   cairo_set_source(ctx, color_lut[1]);
   cairo_text_extents(ctx, tmp, &extents);
   cairo_move_to(ctx,  TILE_SIZE*2+SPACING*5, SPACING * 5 + extents.height);
   cairo_show_text(ctx, tmp);

   cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

   int ty = BOARD_OFFSET_Y + SPACING;

   for (int row = 0; row < 4; row++) {
      int tx = SPACING * 2;

      for (int col = 0; col < 4; col++) {
         cell_t index = grid[row * 4 + col];
         cairo_set_source(ctx, color_lut[index.value]);
         filled_rectangle(ctx, tx, ty, TILE_SIZE, TILE_SIZE);

         if (index.value) {

            if (index.value < 6) // one or two digits
               cairo_set_font_size(ctx, FONT_SIZE * 2.0);
            else if (index.value < 10) // three digits
               cairo_set_font_size(ctx, FONT_SIZE * 1.5);
            else // four digits
               cairo_set_font_size(ctx, FONT_SIZE);

            set_source_rgb(ctx, 119, 110, 101);

            cairo_text_extents(ctx, label_lut[index.value], &extents);

            int font_off_y = extents.height/2.0 + TILE_SIZE/2.0;
            int font_off_x = TILE_SIZE/2.0 - extents.width/2.0;

            cairo_move_to(ctx, tx + font_off_x, ty + font_off_y);

            cairo_show_text(ctx, label_lut[index.value]);
         }

         tx += TILE_SIZE + SPACING;
      }

      ty += TILE_SIZE + SPACING;
   }

   cairo_surface_flush(surface);
}

