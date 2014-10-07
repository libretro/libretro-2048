#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "game_shared.h"

static game_t game;

// score animations
static int delta_score;
static float delta_score_time;
static float frame_time = 0.016;

#define PI 3.14159

// out back bicubic
// from http://www.timotheegroleau.com/Flash/experiments/easing_function_generator.htm
float bump_out(float v0, float v1, float t)
{
   t /= 1;// intensity (d)

   float ts = t  * t;
   float tc = ts * t;
   return v0 + v1 * (4*tc + -9*ts + 6*t);
}

// interpolation functions
float lerp(float v0, float v1, float t)
{
   return v0 * (1 - t) + v1 * t;
}

float cos_interp(float v0,float v1, float t)
{
   float t2;

   t2 = (1-cos(t*PI))/2;
   return(v0*(1-t2)+v1*t2);
}

void *game_data(void)
{
   return &game;
}

void *game_save_data(void)
{
   int row, col;

   // stop animations
   for (row = 0; row < 4; row++)
   {
      for (col = 0; col < 4; col++)
      {
         game.grid[row * 4 + col].appear_time = 1;
         game.grid[row * 4 + col].move_time   = 1;
      }
   }

   delta_score_time = 1;

   // show title screen when the game gets loaded again.
   if (game.state != STATE_PLAYING && game.state != STATE_PAUSED)
   {
      game.score = 0;
      game.state = STATE_TITLE;
   }

   return &game;
}

unsigned game_data_size(void)
{
   return sizeof(game);
}

void render_game(void)
{
   if (game.state == STATE_PLAYING)
      render_playing();
   else if (game.state == STATE_TITLE)
      render_title();
   else if (game.state == STATE_GAME_OVER || game.state == STATE_WON)
      render_win_or_game_over();
   else if (game.state == STATE_PAUSED)
      render_paused();
}

static void add_tile(void)
{
   int i, j;
   cell_t *empty[GRID_SIZE];

   if (game.state != STATE_PLAYING)
      return;

   j = 0;
   for (i = 0; i < GRID_SIZE; i++)
   {
      empty[j] = NULL;
      if (!game.grid[i].value)
         empty[j++] = &game.grid[i];
   }

   if (j)
   {
      j = rand() % j;
      empty[j]->old_pos = empty[j]->pos;
      empty[j]->source = NULL;
      empty[j]->move_time = 1;
      empty[j]->appear_time = 0;
      empty[j]->value = (rand() / RAND_MAX) < 0.9 ? 1 : 2;
   }
   else
      change_state(STATE_GAME_OVER);
}

void init_game(void)
{
   memset(&game, 0, sizeof(game));

   game.state = STATE_TITLE;
}

void start_game(void)
{
   int row, col;
   game.score = 0;

   for (row = 0; row < 4; row++)
   {
      for (col = 0; col < 4; col++)
      {
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

static bool cells_available(void)
{
   int row, col;
   for (row = 0; row < GRID_HEIGHT; row++)
   {
      for (col = 0; col < GRID_WIDTH; col++)
      {
         if (!game.grid[row * GRID_WIDTH + col].value)
            return true;
      }
   }

   return false;
}

static bool matches_available(void)
{
   int row, col;
   for (row = 0; row < GRID_HEIGHT; row++)
   {
      for (col = 0; col < GRID_WIDTH; col++)
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

static bool move_tiles(void)
{
   int vec_x, vec_y;

   switch (game.direction)
   {
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

float *game_get_frame_time(void)
{
   return &frame_time;
}

int *game_get_delta_score(void)
{
   return &delta_score;
}

float *game_get_delta_score_time(void)
{
   return &delta_score_time;
}

int game_get_score(void)
{
   return game.score;
}

int game_get_best_score(void)
{
   return game.best_score;
}
cell_t * game_get_grid(void)
{
   return game.grid;
}

game_state_t game_get_state(void)
{
   return game.state;
}

static void end_game(void)
{
   game.best_score = game.score > game.best_score ? game.score : game.best_score;
}

void change_state(game_state_t state)
{
   switch (game.state)
   {
      case STATE_TITLE:
      case STATE_GAME_OVER:
         assert(state == STATE_PLAYING);
         game.state = state;
         start_game();
         break;
      case STATE_PLAYING:
         assert(state == STATE_GAME_OVER || state == STATE_WON || state == STATE_PAUSED);
         if (state != STATE_PAUSED)
            end_game();
         break;
      case STATE_WON:
         assert(state == STATE_TITLE);
         break;
      case STATE_PAUSED:
         assert(state == STATE_PLAYING || state == STATE_TITLE);
   }

   game.state = state;
}

void handle_input(key_state_t *ks)
{
   game.direction = DIR_NONE;

   if (game.state == STATE_TITLE || game.state == STATE_GAME_OVER || game.state == STATE_WON)
   {
      if (!ks->start && game.old_ks.start)
         change_state(game.state == STATE_WON ? STATE_TITLE : STATE_PLAYING);
   }
   else if (game.state == STATE_PLAYING)
   {
      if (ks->up && !game.old_ks.up)
         game.direction = DIR_UP;
      else if (ks->right && !game.old_ks.right)
         game.direction = DIR_RIGHT;
      else if (ks->down && !game.old_ks.down)
         game.direction = DIR_DOWN;
      else if (ks->left && !game.old_ks.left)
         game.direction = DIR_LEFT;
      else if (ks->start && !game.old_ks.start)
         change_state(STATE_PAUSED);
   }
   else if (game.state == STATE_PAUSED)
   {
      if (ks->start && !game.old_ks.start)
         change_state(STATE_PLAYING);
      else if (ks->select && !game.old_ks.select)
      {
         game.state = STATE_PLAYING;
         start_game();
      }
   }

   game.old_ks = *ks;
}

void game_reset(void)
{
   start_game();
}

void grid_to_screen(vector_t pos, int *x, int *y)
{
   *x = SPACING * 2 + ((TILE_SIZE + SPACING) * pos.x);
   *y = BOARD_OFFSET_Y + SPACING + ((TILE_SIZE + SPACING) * pos.y);
}
