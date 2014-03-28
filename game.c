
#include "game.h"
#include "libretro.h"

#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>

#include <cairo/cairo.h>

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
   STATE_WON
} game_state_t;

typedef struct vector {
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

static game_t game;

// +score animations
static int delta_score;
static float delta_score_time;

int SCREEN_PITCH = 0;

static cairo_surface_t *surface = NULL;
static cairo_surface_t *static_surface = NULL;
static cairo_t *ctx = NULL;
static float frame_time = 0.016;
static cairo_pattern_t* color_lut[13];
static const char* label_lut[13] =
{
   "",
   "2", "4", "8", "16",
   "32", "64", "128", "256",
   "512", "1024", "2048",
   "XXX"
};

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

// interpolation functions {{{
static float lerp(float v0, float v1, float t)
{
   return v0 * (1 - t) + v1 * t;
}

static float cos_interp(float v0,float v1, float t)
{
   float t2;

   t2 = (1-cos(t*PI))/2;
   return(v0*(1-t2)+v1*t2);
}

// out back bicubic
// from http://www.timotheegroleau.com/Flash/experiments/easing_function_generator.htm
static float bump_out(float v0, float v1, float t)
{
   t /= 1;// intensity (d)
   float ts = t  * t;
   float tc = ts * t;
   return v0 + v1 * (4*tc + -9*ts + 6*t);
}
// }}}

static void grid_to_screen(vector_t pos, int *x, int *y)
{
   *x = SPACING * 2 + ((TILE_SIZE + SPACING) * pos.x);
   *y = BOARD_OFFSET_Y + SPACING + ((TILE_SIZE + SPACING) * pos.y);
}

static void draw_tile(cairo_t *ctx, cell_t *cell)
{
   int x, y;
   int w = TILE_SIZE, h = TILE_SIZE;
   int font_size = FONT_SIZE;

   if (cell->value && cell->move_time < 1) {
      int x1, y1;
      int x2, y2;

      grid_to_screen(cell->old_pos, &x1, &y1);
      grid_to_screen(cell->pos, &x2, &y2);

      x = lerp(x1, x2, cell->move_time);
      y = lerp(y1, y2, cell->move_time);

      if (cell->move_time < 0.5 && cell->source)
         draw_tile(ctx, cell->source);

      cell->move_time += frame_time * TILE_ANIM_SPEED;
   } else if (cell->appear_time < 1) {

      grid_to_screen(cell->pos, &x, &y);

      w = h = bump_out(0, TILE_SIZE, cell->appear_time);
      font_size = bump_out(0, FONT_SIZE, cell->appear_time);
//      w = lerp(0, TILE_SIZE, cell->appear_time);
//      h = lerp(0, TILE_SIZE, cell->appear_time);
//      font_size = lerp(0, FONT_SIZE, cell->appear_time);

      x += TILE_SIZE/2 - w/2;
      y += TILE_SIZE/2 - h/2;

      cell->appear_time += frame_time * TILE_ANIM_SPEED;
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

static void change_state(game_state_t state);
static void add_tile(void)
{
   cell_t *empty[GRID_SIZE];

   if (game.state != STATE_PLAYING)
      return;

   int j = 0;
   for (int i = 0; i < GRID_SIZE; i++) {
      empty[j] = NULL;
      if (!game.grid[i].value)
         empty[j++] = &game.grid[i];
   }

   if (j) {
      j = rand() % j;
      empty[j]->old_pos = empty[j]->pos;
      empty[j]->source = NULL;
      empty[j]->move_time = 1;
      empty[j]->appear_time = 0;
      empty[j]->value = (rand() / RAND_MAX) < 0.9 ? 1 : 2;
   } else
      change_state(STATE_GAME_OVER);
}

static void start_game(void)
{
   game.score = 0;

   for (int row = 0; row < 4; row++) {
      for (int col = 0; col < 4; col++) {
         cell_t *cell = &game.grid[row * 4 + col];

         cell->pos.x = col;
         cell->pos.y = row;
         cell->old_pos = cell->pos;
         cell->move_time = 1;
         cell->appear_time = 0;
         cell->value = 0;
         cell->source = NULL;
      }
   }

   // reset +score animation
   delta_score    = 0;
   delta_score_time = 1;

   add_tile();
   add_tile();
}

static void end_game(void)
{
   game.best_score = game.score > game.best_score ? game.score : game.best_score;
}

static void change_state(game_state_t state)
{
   switch (game.state) {
   case STATE_TITLE:
   case STATE_GAME_OVER:
      assert(state == STATE_PLAYING);
      game.state = state;
      start_game();
      break;
   case STATE_PLAYING:
      assert(state == STATE_GAME_OVER || state == STATE_WON);
      end_game();
      break;
   case STATE_WON:
      assert(state == STATE_TITLE);
      break;
   }

   game.state = state;
}

static bool move_tiles(void)
{
   int vec_x, vec_y;

   switch (game.direction) {
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
         return false;
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

   if (vec_y > 0)
   {
      row_begin = 3;
      row_end   = -1;
      row_inc   = -1;
   }

   bool moved = false;

   delta_score = game.score;

   // clear source cell and save current position in the grid
   for (int row = row_begin; row != row_end; row += row_inc)
   {
      for (int col = col_begin; col != col_end; col += col_inc)
      {
         cell_t *cell = &game.grid[row * 4 + col];
         cell->old_pos = cell->pos;
         cell->source = NULL;
         cell->move_time = 1;
         cell->appear_time = 1;
      }
   }

   for (int row = row_begin; row != row_end; row += row_inc)
   {
      for (int col = col_begin; col != col_end; col += col_inc)
      {
         cell_t *cell = &game.grid[row * 4 + col];
         if (!cell->value)
            continue;

         cell_t *farthest;
         cell_t *next = cell;

         int new_row = row , new_col = col;

         do
         {
            farthest = next;

            new_row += vec_y;
            new_col += vec_x;

            if (new_row < 0 || new_col < 0 || new_row > 3 || new_col > 3)
               break;

            next = &game.grid[new_row * 4 + new_col];
         } while (!next->value);

         // only tiles that have not been merged
         if (next->value && next->value == cell->value && next != cell && !next->source)
         {
            next->value = cell->value + 1;
            next->source = cell;
            next->old_pos = cell->pos;
            next->move_time = 0;
            cell->value = 0;

            game.score += 2 << next->value;
            moved = true;

            if (next->value == 11)
               game.state = STATE_WON;
         }
         else if (farthest != cell)
         {
            farthest->value = cell->value;
            farthest->old_pos = cell->pos;
            farthest->move_time = 0;
            cell->value = 0;
            moved = true;
         }
      }
   }

   delta_score      = game.score - delta_score;
   delta_score_time = delta_score == 0 ? 1 : 0;

   return moved;
}

static bool cells_available(void)
{
   for (int row = 0; row < GRID_HEIGHT; row++)
   {
      for (int col = 0; col < GRID_WIDTH; col++)
      {
         if (!game.grid[row * GRID_WIDTH + col].value)
            return true;
      }
   }

   return false;
}

static bool matches_available(void)
{
   for (int row = 0; row < GRID_HEIGHT; row++)
   {
      for (int col = 0; col < GRID_WIDTH; col++)
      {
         cell_t *cell = &game.grid[row * GRID_WIDTH + col];

         if (!cell->value)
            continue;

         if ((col > 0 && game.grid[row * GRID_WIDTH + col - 1].value == cell->value) ||
             (col < GRID_WIDTH - 1 && game.grid[row * GRID_WIDTH + col + 1].value == cell->value) ||
             (row > 0 && game.grid[(row - 1) * GRID_WIDTH + col].value == cell->value) ||
             (row < GRID_HEIGHT - 1 && game.grid[(row + 1) * GRID_WIDTH + col].value == cell->value))
            return true;
      }
   }

   return false;
}

static void handle_input(key_state_t *ks)
{
   game.direction = DIR_NONE;

   if (game.state == STATE_TITLE || game.state == STATE_GAME_OVER || game.state == STATE_WON)
   {
      if (!ks->start && game.old_ks.start)
         change_state(game.state == STATE_WON ? STATE_TITLE : STATE_PLAYING);
   }
   else if (game.state == STATE_PLAYING)
   {
      if (!ks->up && game.old_ks.up)
         game.direction = DIR_UP;
      else if (!ks->right && game.old_ks.right)
         game.direction = DIR_RIGHT;
      else if (!ks->down && game.old_ks.down)
         game.direction = DIR_DOWN;
      else if (!ks->left && game.old_ks.left)
         game.direction = DIR_LEFT;
   }

   game.old_ks = *ks;
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

static void init_static_surface()
{
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
   cell_t dummy;
   dummy.move_time = 1;
   dummy.appear_time = 1;
   dummy.source = NULL;
   dummy.value = 0;

   for (int row = 0; row < 4; row++)
   {
      for (int col = 0; col < 4; col++)
      {
         dummy.pos.x = col;
         dummy.pos.y = row;
         dummy.old_pos = dummy.pos;
         draw_tile(static_ctx, &dummy);
      }
   }

   cairo_destroy(static_ctx);
}

void game_init(uint16_t *frame_buf)
{
   srand(time(NULL));

   surface = cairo_image_surface_create_for_data(
            (unsigned char*)frame_buf, CAIRO_FORMAT_RGB16_565, SCREEN_WIDTH, SCREEN_HEIGHT,
            SCREEN_PITCH);

   ctx = cairo_create(surface);

   init_luts();
   init_static_surface();

   memset(&game, 0, sizeof(game));

   game.state = STATE_TITLE;
   start_game();
}

void game_deinit(void)
{
   for (int i = 0; i < 13; i++)
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
}

void game_reset(void)
{
   start_game();
}

void game_update(float delta, key_state_t *new_ks)
{
   frame_time = delta;

   handle_input(new_ks);

   if (game.state == STATE_PLAYING)
   {
      if (game.direction != DIR_NONE && move_tiles())
         add_tile();

      if (!matches_available() && !cells_available())
         change_state(STATE_GAME_OVER);
   }
}

void *game_data()
{
   return &game;
}

unsigned game_data_size()
{
   return sizeof(game);
}

static void render_playing(void)
{
   char tmp[10] = {0};

   // paint static background
   cairo_set_source_surface(ctx, static_surface, 0, 0);
   cairo_paint(ctx);

   cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
   cairo_set_font_size(ctx, FONT_SIZE * 2);

   // score and best score value
   set_rgb(ctx, 255, 255, 255);
   sprintf(tmp, "%i", game.score % 1000000);
   draw_text_centered(ctx, tmp, SPACING*2, SPACING * 5, TILE_SIZE*2, 0);

   sprintf(tmp, "%i", game.best_score % 1000000);
   cairo_set_source(ctx, color_lut[1]);
   draw_text_centered(ctx, tmp, TILE_SIZE*2+SPACING*5, SPACING * 5, TILE_SIZE*2, 0);

   for (int row = 0; row < 4; row++)
   {
      for (int col = 0; col < 4; col++)
      {
         cell_t *cell = &game.grid[row * 4 + col];

         if (cell->value)
            draw_tile(ctx, cell);
      }
   }

   // draw +score animation
   if (delta_score_time < 1)
   {
      cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
      cairo_set_font_size(ctx, FONT_SIZE * 1.2);

      int x = SPACING * 2;
      int y = SPACING * 5;

      y = lerp(y, y - TILE_SIZE, delta_score_time);

      set_rgba(ctx, 119, 110, 101, lerp(1, 0, delta_score_time));

      sprintf(tmp, "+%i", delta_score);
      draw_text_centered(ctx, tmp, x, y, TILE_SIZE * 2, TILE_SIZE);

      delta_score_time += frame_time;
   }

   cairo_surface_flush(surface);
}

static void render_title(void)
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

static void render_win_or_game_over(void)
{
   if (game.state == STATE_GAME_OVER)
      render_playing();

   // bg
   set_rgba(ctx, 250, 248, 239, 0.85);
   fill_rectangle(ctx, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

   cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
   cairo_set_font_size(ctx, FONT_SIZE * 2);

   set_rgb(ctx, 185, 172, 159);
   draw_text_centered(ctx, (game.state == STATE_GAME_OVER ? "Game Over" : "You Win"), 0, 0, SCREEN_WIDTH, TILE_SIZE*3);

   cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
   cairo_set_font_size(ctx, FONT_SIZE);

   set_rgb(ctx, 185, 172, 159);
   char tmp[100];

   sprintf(tmp, "Score: %i", game.score);
   draw_text_centered(ctx, tmp, 0, 0, SCREEN_WIDTH, TILE_SIZE*5);

   set_rgb(ctx, 185, 172, 159);
   fill_rectangle(ctx, TILE_SIZE / 2, TILE_SIZE * 4, SCREEN_HEIGHT - TILE_SIZE * 2, FONT_SIZE * 3);
   cairo_set_source(ctx, color_lut[1]);
   draw_text_centered(ctx, "PRESS START", TILE_SIZE / 2 + SPACING, TILE_SIZE * 4 + SPACING,
                      SCREEN_HEIGHT - TILE_SIZE * 2 - SPACING * 2, FONT_SIZE * 3 - SPACING * 2);
}

void game_render(void)
{
   if (game.state == STATE_PLAYING)
      render_playing();
   else if (game.state == STATE_TITLE)
      render_title();
   else if (game.state == STATE_GAME_OVER || game.state == STATE_WON)
      render_win_or_game_over();
}
