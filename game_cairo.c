#include "game.h"
#include "game_shared.h"

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>

#include <cairo/cairo.h>

int SCREEN_PITCH = 0;

static cairo_surface_t *surface = NULL;
static cairo_surface_t *static_surface = NULL;
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

static uint16_t *frame_buf;

static void set_rgb(cairo_t *ctx, int r, int g, int b)
{
   cairo_set_source_rgb(ctx, r / 255.0, g / 255.0, b / 255.0);
}

static void set_rgba(cairo_t *ctx, int r, int g, int b, float a)
{
   cairo_set_source_rgba(ctx, r / 255.0, g / 255.0, b / 255.0, a);
}

static void fill_rectangle(cairo_t *ctx, int x, int y, int w, int h)
{
   cairo_rectangle(ctx, x, y, w, h);
   cairo_fill(ctx);
}

static void draw_text(cairo_t *ctx, const char *utf8, int x, int y)
{
   cairo_move_to(ctx, x, y);
   cairo_show_text(ctx, utf8);
}

static void draw_text_centered(cairo_t *ctx, const char *utf8, int x, int y, int w, int h)
{
   cairo_text_extents_t extents;
   cairo_text_extents(ctx, utf8, &extents);

   double font_off_y = h ? extents.height / 2.0 + h / 2.0 : extents.height;
   double font_off_x = w ? w / 2.0 - extents.width / 2.0 : 0.0;

   draw_text(ctx, utf8, x + font_off_x, y + font_off_y);
}

static void draw_tile(cairo_t *ctx, cell_t *cell)
{
   int x, y;
   int w = TILE_SIZE, h = TILE_SIZE;
   int font_size = FONT_SIZE;
   float *frame_time = game_get_frame_time();

   if (cell->value && cell->move_time < 1)
   {
      int x1, y1;
      int x2, y2;

      grid_to_screen(cell->old_pos, &x1, &y1);
      grid_to_screen(cell->pos, &x2, &y2);

      x = lerp(x1, x2, cell->move_time);
      y = lerp(y1, y2, cell->move_time);

      if (cell->move_time < 0.5 && cell->source)
         draw_tile(ctx, cell->source);

      cell->move_time += *frame_time * TILE_ANIM_SPEED;
   }
   else if (cell->appear_time < 1)
   {
      grid_to_screen(cell->pos, &x, &y);

      w = h = bump_out(0, TILE_SIZE, cell->appear_time);
      font_size = bump_out(0, FONT_SIZE, cell->appear_time);
//      w = lerp(0, TILE_SIZE, cell->appear_time);
//      h = lerp(0, TILE_SIZE, cell->appear_time);
//      font_size = lerp(0, FONT_SIZE, cell->appear_time);

      x += TILE_SIZE/2 - w/2;
      y += TILE_SIZE/2 - h/2;

      cell->appear_time += *frame_time * TILE_ANIM_SPEED;
   } else {
      grid_to_screen(cell->pos, &x, &y);
   }

   cairo_set_source(ctx, color_lut[cell->value]);
   fill_rectangle(ctx, x, y, w, h);

   if (cell->value) {
      cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

      if (cell->value < 6) // one or two digits
         cairo_set_font_size(ctx, font_size * 2.0);
      else if (cell->value < 10) // three digits
         cairo_set_font_size(ctx, font_size * 1.5);
      else // four digits
         cairo_set_font_size(ctx, font_size);

      set_rgb(ctx, 119, 110, 101);
      draw_text_centered(ctx, label_lut[cell->value], x, y, w, h);
   }
}

void game_calculate_pitch(void)
{
   SCREEN_PITCH = cairo_format_stride_for_width(CAIRO_FORMAT_RGB16_565, SCREEN_WIDTH);
}

static void init_luts(void)
{
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
}

static void init_static_surface(void)
{
   int row, col;
   cell_t dummy;
   cairo_t *static_ctx;

   static_surface = cairo_image_surface_create(CAIRO_FORMAT_RGB16_565, SCREEN_WIDTH, SCREEN_HEIGHT);
   static_ctx = cairo_create(static_surface);

   // bg
   set_rgb(static_ctx, 250, 248, 239);
   fill_rectangle(static_ctx, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

   // grid bg
   set_rgb(static_ctx, 185, 172, 159);
   fill_rectangle(static_ctx, SPACING, BOARD_OFFSET_Y, BOARD_WIDTH, BOARD_WIDTH);

   // score bg
   set_rgb(static_ctx, 185, 172, 159);
   fill_rectangle(static_ctx, SPACING, SPACING, TILE_SIZE*2+SPACING*2, TILE_SIZE);

   // best bg
   set_rgb(static_ctx, 185, 172, 159);
   fill_rectangle(static_ctx, TILE_SIZE*2+SPACING*4, SPACING, TILE_SIZE*2+SPACING*2, TILE_SIZE);

   cairo_select_font_face(static_ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
   cairo_set_font_size(static_ctx, FONT_SIZE);

   // score title
   cairo_set_source(static_ctx, color_lut[1]);
   draw_text_centered(static_ctx, "SCORE", SPACING*2, SPACING * 2, 0, 0);

   // best title
   cairo_set_source(static_ctx, color_lut[1]);
   draw_text_centered(static_ctx, "BEST", TILE_SIZE*2+SPACING*5, SPACING*2, 0, 0);

   // draw background cells
   dummy.move_time = 1;
   dummy.appear_time = 1;
   dummy.source = NULL;
   dummy.value = 0;

   for (row = 0; row < 4; row++)
   {
      for (col = 0; col < 4; col++)
      {
         dummy.pos.x = col;
         dummy.pos.y = row;
         dummy.old_pos = dummy.pos;
         draw_tile(static_ctx, &dummy);
      }
   }

   cairo_destroy(static_ctx);
}

void game_init(void)
{
   frame_buf = calloc(SCREEN_HEIGHT, SCREEN_PITCH);
   srand(time(NULL));

   surface = cairo_image_surface_create_for_data(
            (unsigned char*)frame_buf, CAIRO_FORMAT_RGB16_565, SCREEN_WIDTH, SCREEN_HEIGHT,
            SCREEN_PITCH);

   ctx = cairo_create(surface);

   init_luts();
   init_static_surface();

   init_game();
   start_game();
}

void game_deinit(void)
{
   int i;

   for (i = 0; i < 13; i++)
   {
      cairo_pattern_destroy(color_lut[i]);
      color_lut[i] = NULL;
   }

   cairo_destroy(ctx);
   cairo_surface_destroy(surface);
   cairo_surface_destroy(static_surface);

   ctx     = NULL;
   surface = NULL;
   static_surface = NULL;

   if (frame_buf)
      free(frame_buf);
   frame_buf = NULL;
}

void render_playing(void)
{
   int *delta_score;
   float *delta_score_time;
   char tmp[10] = {0};
   float *frame_time = game_get_frame_time();

   // paint static background
   cairo_set_source_surface(ctx, static_surface, 0, 0);
   cairo_paint(ctx);

   cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
   cairo_set_font_size(ctx, FONT_SIZE * 2);

   // score and best score value
   set_rgb(ctx, 255, 255, 255);
   sprintf(tmp, "%i", game_get_score() % 1000000);
   draw_text_centered(ctx, tmp, SPACING*2, SPACING * 5, TILE_SIZE*2, 0);

   sprintf(tmp, "%i", game_get_best_score() % 1000000);
   cairo_set_source(ctx, color_lut[1]);
   draw_text_centered(ctx, tmp, TILE_SIZE*2+SPACING*5, SPACING * 5, TILE_SIZE*2, 0);

   for (int row = 0; row < 4; row++)
   {
      for (int col = 0; col < 4; col++)
      {
         cell_t *grid = game_get_grid();
         cell_t *cell = &grid[row * 4 + col];

         if (cell->value)
            draw_tile(ctx, cell);
      }
   }

   delta_score_time = game_get_delta_score_time();
   delta_score = game_get_delta_score();

   // draw +score animation
   if (*delta_score_time < 1)
   {
      cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size(ctx, FONT_SIZE * 1.2);

      int x = SPACING * 2;
      int y = SPACING * 5;

      y = lerp(y, y - TILE_SIZE, *delta_score_time);

      set_rgba(ctx, 119, 110, 101, lerp(1, 0, *delta_score_time));

      sprintf(tmp, "+%i", *delta_score);
      draw_text_centered(ctx, tmp, x, y, TILE_SIZE * 2, TILE_SIZE);

      *delta_score_time += *frame_time;
   }

   cairo_surface_flush(surface);
}

void render_title(void)
{
   // bg
   set_rgb(ctx, 250, 248, 239);
   fill_rectangle(ctx, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

   cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
   cairo_set_font_size(ctx, FONT_SIZE * 5);

   set_rgb(ctx, 185, 172, 159);
   draw_text_centered(ctx, "2048", 0, 0, SCREEN_WIDTH, TILE_SIZE*3);


   set_rgb(ctx, 185, 172, 159);
   fill_rectangle(ctx, TILE_SIZE / 2, TILE_SIZE * 4, SCREEN_HEIGHT - TILE_SIZE * 2, FONT_SIZE * 3);

   cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
   cairo_set_font_size(ctx, FONT_SIZE);

   cairo_set_source(ctx, color_lut[1]);
   draw_text_centered(ctx, "PRESS START", TILE_SIZE / 2 + SPACING, TILE_SIZE * 4 + SPACING,
                      SCREEN_HEIGHT - TILE_SIZE * 2 - SPACING * 2, FONT_SIZE * 3 - SPACING * 2);

}

void render_win_or_game_over(void)
{
   game_state_t state = game_get_state();

   if (state == STATE_GAME_OVER)
      render_playing();

   // bg
   set_rgba(ctx, 250, 248, 239, 0.85);
   fill_rectangle(ctx, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

   cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
   cairo_set_font_size(ctx, FONT_SIZE * 2);

   set_rgb(ctx, 185, 172, 159);
   draw_text_centered(ctx, (state == STATE_GAME_OVER ? "Game Over" : "You Win"), 0, 0, SCREEN_WIDTH, TILE_SIZE*3);

   cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
   cairo_set_font_size(ctx, FONT_SIZE);

   set_rgb(ctx, 185, 172, 159);
   char tmp[100];

   sprintf(tmp, "Score: %i", game_get_score());
   draw_text_centered(ctx, tmp, 0, 0, SCREEN_WIDTH, TILE_SIZE*5);

   set_rgb(ctx, 185, 172, 159);
   fill_rectangle(ctx, TILE_SIZE / 2, TILE_SIZE * 4, SCREEN_HEIGHT - TILE_SIZE * 2, FONT_SIZE * 3);
   cairo_set_source(ctx, color_lut[1]);
   draw_text_centered(ctx, "PRESS START", TILE_SIZE / 2 + SPACING, TILE_SIZE * 4 + SPACING,
                      SCREEN_HEIGHT - TILE_SIZE * 2 - SPACING * 2, FONT_SIZE * 3 - SPACING * 2);
}

void render_paused(void)
{
   render_playing();

   // bg
   set_rgba(ctx, 250, 248, 239, 0.85);
   fill_rectangle(ctx, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

   cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
   cairo_set_font_size(ctx, FONT_SIZE * 2);

   set_rgb(ctx, 185, 172, 159);
   draw_text_centered(ctx, "Paused", 0, 0, SCREEN_WIDTH, TILE_SIZE*3);

   cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
   cairo_set_font_size(ctx, FONT_SIZE);

   set_rgb(ctx, 185, 172, 159);
   char tmp[100];

   sprintf(tmp, "Score: %i", game_get_score());
   draw_text_centered(ctx, tmp, 0, 0, SCREEN_WIDTH, TILE_SIZE*5);

   set_rgb(ctx, 185, 172, 159);
   fill_rectangle(ctx, TILE_SIZE / 2, TILE_SIZE * 4, SCREEN_HEIGHT - TILE_SIZE * 2, FONT_SIZE * 5);
   cairo_set_source(ctx, color_lut[1]);
   draw_text_centered(ctx, "SELECT: New Game", TILE_SIZE / 2 + SPACING, TILE_SIZE * 4 + SPACING,
                      SCREEN_HEIGHT - TILE_SIZE * 2 - SPACING * 2, FONT_SIZE * 3 - SPACING * 2);
   draw_text_centered(ctx, "START: Continue", TILE_SIZE / 2 + SPACING, TILE_SIZE * 4 + SPACING + FONT_SIZE * 2,
                      SCREEN_HEIGHT - TILE_SIZE * 2 - SPACING * 2, FONT_SIZE * 3 - SPACING * 2);
}

int game_init_pixelformat(void)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (log_cb)
         log_cb(RETRO_LOG_INFO, "RGB565 is not supported.\n");
      return 0;
   }

   return 1;
}

void game_render(void)
{
   render_game();
   video_cb(frame_buf, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_PITCH);
}
