#include "libretro.h"
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <cairo/cairo.h>


enum {
   SPACE = 5,
   FONT_SIZE = 12,
   TILE_SIZE = FONT_SIZE * 4,
   NUM_TILES = 16,
   BOARD_SIZE = SPACE + TILE_SIZE * 4 + SPACE * 3 + SPACE,
   BOARD_OFFSET_Y = SPACE + TILE_SIZE + SPACE,
   WIDTH  = SPACE + BOARD_SIZE + SPACE,
   HEIGHT = SPACE + TILE_SIZE + SPACE + BOARD_SIZE + SPACE,
};
static const char *FONT = "cairo:monospace";


static int grid[NUM_TILES] = {
   1, 0, 0, 1,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 1
};

static uint16_t *frame_buf;
static cairo_surface_t *surface;

static struct retro_log_callback logging;

static cairo_pattern_t *colors[13];
static const char* labels[13] = {
   "", "2", "4", "8",
   "16", "32", "64", "128",
   "256", "512", "1024", "2048",
   "XXX"
};

static int PITCH = 0;

static enum {
   NODIR, UP, RIGHT, DOWN, LEFT
} direction;

static int score = 0;

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
   (void)level;
   va_list va;
   va_start(va, fmt);
   vfprintf(stderr, fmt, va);
   va_end(va);
}


static void render_grid(void)
{
   cairo_t *ctx;

   ctx = cairo_create(surface);

   // bg
   cairo_set_source_rgb(ctx, 250 / 255.0, 248 / 255.0, 239 / 255.0);
   cairo_rectangle(ctx, 0, 0, WIDTH, HEIGHT);
   cairo_fill(ctx);

   // grid bg
   cairo_set_source_rgb(ctx, 185 / 255.0, 172 / 255.0, 159 / 255.0);
   cairo_rectangle(ctx, SPACE, BOARD_OFFSET_Y, BOARD_SIZE, BOARD_SIZE);
   cairo_fill(ctx);



   // score bg
   cairo_set_source_rgb(ctx, 185 / 255.0, 172 / 255.0, 159 / 255.0);
   cairo_rectangle(ctx, SPACE, SPACE, TILE_SIZE*2+SPACE*2, TILE_SIZE);
   cairo_fill(ctx);

   // best bg
   cairo_set_source_rgb(ctx, 185 / 255.0, 172 / 255.0, 159 / 255.0);
   cairo_rectangle(ctx, TILE_SIZE*2+SPACE*4, SPACE, TILE_SIZE*2+SPACE*2, TILE_SIZE);
   cairo_fill(ctx);

   cairo_text_extents_t extents;
   cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

   // score title
   cairo_set_source(ctx, colors[1]);
   cairo_text_extents(ctx, "SCORE", &extents);
   cairo_move_to(ctx,  SPACE*2, SPACE * 2 + extents.height);
   cairo_show_text(ctx, "SCORE");

   // best title
   cairo_set_source(ctx, colors[1]);
   cairo_text_extents(ctx, "BEST", &extents);
   cairo_move_to(ctx,  TILE_SIZE*2+SPACE*5, SPACE * 2 + extents.height);
   cairo_show_text(ctx, "BEST");

   char tmp[10] = {0};
   cairo_set_font_size(ctx, FONT_SIZE * 2);

   // score value
   sprintf(tmp, "%7i", score);
   cairo_set_source(ctx, colors[1]);
   cairo_text_extents(ctx, tmp, &extents);
   cairo_move_to(ctx,  SPACE*2, SPACE * 5 + extents.height);
   cairo_show_text(ctx, tmp);

   // best value
   sprintf(tmp, "%7i", 0);
   cairo_set_source(ctx, colors[1]);
   cairo_text_extents(ctx, tmp, &extents);
   cairo_move_to(ctx,  TILE_SIZE*2+SPACE*5, SPACE * 5 + extents.height);
   cairo_show_text(ctx, tmp);


   cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
   cairo_set_font_size(ctx, FONT_SIZE);

   int ty = BOARD_OFFSET_Y + SPACE;

   for (int row = 0; row < 4; row++) {
      int tx = SPACE * 2;

      for (int col = 0; col < 4; col++) {
         int index = grid[row * 4 + col];
         cairo_set_source(ctx, colors[index]);
         cairo_rectangle(ctx, tx, ty, TILE_SIZE, TILE_SIZE);
         cairo_fill(ctx);

         if (index) {
            cairo_set_source_rgb(ctx, 119 / 255.0, 110 / 255.0, 101 / 255.0);


            cairo_text_extents(ctx, labels[index], &extents);

            int font_off_y = extents.height/2.0 + TILE_SIZE/2.0;
            int font_off_x = TILE_SIZE/2.0 - extents.width/2.0;

            cairo_move_to(ctx, tx + font_off_x, ty + font_off_y);

            cairo_show_text(ctx, labels[index]);
         }

         tx += TILE_SIZE + SPACE;
      }
      ty += TILE_SIZE + SPACE;
   }

   cairo_surface_flush(surface);

   cairo_destroy(ctx);

   video_cb(frame_buf, WIDTH, HEIGHT, PITCH);
}

static void update_input(void)
{
   static int up, right, down, left;
   static int last_up = 0, last_right = 0, last_down = 0, last_left = 0;

   input_poll_cb();

   direction = NODIR;

   up = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
   right = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
   down = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
   left = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);

   if (!up && last_up)
      direction = UP;
   else if (!right && last_right)
      direction = RIGHT;
   else if (!down && last_down)
      direction = DOWN;
   else if (!left && last_left)
      direction = LEFT;

   last_up = up;
   last_right = right;
   last_down = down;
   last_left = left;
}

static void move_tiles(void)
{
   int vec_x, vec_y;

   switch (direction) {
      case UP:
         vec_x = 0; vec_y = -1;
         break;
      case DOWN:
         vec_x = 0; vec_y = 1;
         break;
      case RIGHT:
         vec_x = 1; vec_y = 0;
         break;
      case LEFT:
         vec_x = -1; vec_y = 0;
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

         int *cell = &grid[row * 4 + col];
         if (!*cell)
            continue;

         int *farthest;
         int *next = cell;

         int new_row = row , new_col = col;
         int farthest_row, farthest_col;

         do {
            farthest = next;
            farthest_row = new_row;
            farthest_col = new_col;

            new_row += vec_y;
            new_col += vec_x;

            if (new_row < 0 || new_col < 0 || new_row > 3 || new_col > 3)
               break;

            next = &grid[new_row * 4 + new_col];
         } while (!*next);

         // TODO: check for multiple merges (only one allowed)
         if (*next && *next == *cell && next != cell) {
            *next = *cell + 1;
            *cell = 0;
            score += 2 << *next;
         } else if (farthest != cell) {
            *farthest = *cell;
            *cell = 0;
         }
      }
   }
}

static void add_tile(void) {
   int *empty[NUM_TILES];

   int j = 0;
   for (int i = 0; i < NUM_TILES; i++) {
      empty[j] = NULL;
      if (!grid[i])
         empty[j++] = &grid[i];
   }

   if (j)
      *empty[random() % j] = 1;
   else
      logging.log(RETRO_LOG_INFO, "Game Over!");
}

void retro_init(void)
{
   PITCH = cairo_format_stride_for_width(CAIRO_FORMAT_RGB16_565, WIDTH);

   frame_buf = calloc(HEIGHT, PITCH);

   // XXX: last parameter might cause trouble in some systems
   surface = cairo_image_surface_create_for_data(
            (unsigned char*)frame_buf, CAIRO_FORMAT_RGB16_565, WIDTH, HEIGHT,
            PITCH);

   colors[0] = cairo_pattern_create_rgba(238 / 255.0, 228 / 255.0, 218 / 255.0, 0.35);
   colors[1] = cairo_pattern_create_rgb(238 / 255.0, 228 / 255.0, 218 / 255.0);

   colors[2] = cairo_pattern_create_rgb(237 / 255.0, 224 / 255.0, 200 / 255.0);
   colors[3] = cairo_pattern_create_rgb(242 / 255.0, 177 / 255.0, 121 / 255.0);
   colors[4] = cairo_pattern_create_rgb(245 / 255.0, 149 / 255.0, 99 / 255.0);
   colors[5] = cairo_pattern_create_rgb(246 / 255.0, 124 / 255.0, 95 / 255.0);
   colors[6] = cairo_pattern_create_rgb(246 / 255.0, 94 / 255.0, 59 / 255.0);

   // TODO: shadow
   colors[7] = cairo_pattern_create_rgb(237 / 255.0, 207 / 255.0, 114 / 255.0);
   colors[8] = cairo_pattern_create_rgb(237 / 255.0, 204 / 255.0, 97 / 255.0);
   colors[9] = cairo_pattern_create_rgb(237 / 255.0, 200 / 255.0, 80 / 255.0);
   colors[10] = cairo_pattern_create_rgb(237 / 255.0, 197 / 255.0, 63 / 255.0);
   colors[11] = cairo_pattern_create_rgb(237 / 255.0, 194 / 255.0, 46 / 255.0);
   colors[12] = cairo_pattern_create_rgb(60 / 255.0, 58 / 255.0, 50 / 255.0);

//   add_tile();  add_tile();
}

void retro_deinit(void)
{
   free(frame_buf);
   frame_buf = NULL;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "2048";
   info->library_version  = "v1.0";
   info->need_fullpath    = false;
   info->valid_extensions = NULL; // Anything is fine, we don't care.
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->timing.fps = 24.0;
   info->timing.sample_rate = 30000.0;

   info->geometry.base_width   = WIDTH;
   info->geometry.base_height  = HEIGHT;
   info->geometry.max_width    = WIDTH;
   info->geometry.max_height   = HEIGHT;
   info->geometry.aspect_ratio = 1;
}

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   bool no_rom = true;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);

   if (!cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
      logging.log = fallback_log;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_reset(void)
{
}


void retro_run(void)
{
   update_input();

   if (direction != NODIR) {
      move_tiles();
      add_tile();
   }

   render_grid();
}

bool retro_load_game(const struct retro_game_info *info)
{
   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      logging.log(RETRO_LOG_INFO, "RGB565 is not supported.\n");
      return false;
   }

   (void)info;
   return true;
}

void retro_unload_game(void)
{
}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   return 2;
}

bool retro_serialize(void *data_, size_t size)
{
   return false;
}

bool retro_unserialize(const void *data_, size_t size)
{
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

