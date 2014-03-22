#include "libretro.h"
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <cairo/cairo.h>


#define SPACING    5

#define FONT "cairo:monospace"
#define FONT_SIZE 12
#define TILE_SIZE (FONT_SIZE * 4)

#define GRID_WIDTH   4
#define GRID_HEIGHT  4
#define GRID_SIZE    (GRID_WIDTH * GRID_HEIGHT)

#define BOARD_WIDTH  (SPACING + TILE_SIZE * GRID_WIDTH  + SPACING * (GRID_WIDTH  - 1) + SPACING)
#define BOARD_HEIGHT (SPACING + TILE_SIZE * GRID_HEIGHT + SPACING * (GRID_HEIGHT - 1) + SPACING)

#define BOARD_OFFSET_Y  SPACING + TILE_SIZE + SPACING

#define SCREEN_WIDTH   SPACING + BOARD_WIDTH + SPACING
#define SCREEN_HEIGHT  BOARD_OFFSET_Y + BOARD_HEIGHT + SPACING

static int SCREEN_PITCH = 0;

static enum
{
   DIR_NONE, DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT
} direction;

static int grid[GRID_SIZE] =
{
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0,
   0, 0, 0, 0
};

static uint16_t *frame_buf;
static cairo_surface_t *surface = NULL;
static cairo_t *ctx = NULL;


static cairo_pattern_t* color_lut[13];

static const char* label_lut[13] =
{
   "", "2", "4", "8",
   "16", "32", "64", "128",
   "256", "512", "1024", "2048",
   "XXX"
};

static int game_score = 0;

static struct retro_log_callback logging;
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

static void set_source_rgb(cairo_t *ctx, int r, int g, int b)
{
   cairo_set_source_rgb(ctx, r / 255.0, g / 255.0, b / 255.0);
}

static void render_grid(void)
{
   // bg
   set_source_rgb(ctx, 250, 248, 239);
   cairo_rectangle(ctx, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
   cairo_fill(ctx);

   // grid bg
   set_source_rgb(ctx, 185, 172, 159);
   cairo_rectangle(ctx, SPACING, BOARD_OFFSET_Y, BOARD_WIDTH, BOARD_WIDTH);
   cairo_fill(ctx);



   // score bg
   set_source_rgb(ctx, 185, 172, 159);
   cairo_rectangle(ctx, SPACING, SPACING, TILE_SIZE*2+SPACING*2, TILE_SIZE);
   cairo_fill(ctx);

   // best bg
   set_source_rgb(ctx, 185, 172, 159);
   cairo_rectangle(ctx, TILE_SIZE*2+SPACING*4, SPACING, TILE_SIZE*2+SPACING*2, TILE_SIZE);
   cairo_fill(ctx);

   cairo_text_extents_t extents;
   cairo_select_font_face(ctx, FONT, CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

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
   cairo_set_font_size(ctx, FONT_SIZE);

   int ty = BOARD_OFFSET_Y + SPACING;

   for (int row = 0; row < 4; row++) {
      int tx = SPACING * 2;

      for (int col = 0; col < 4; col++) {
         int index = grid[row * 4 + col];
         cairo_set_source(ctx, color_lut[index]);
         cairo_rectangle(ctx, tx, ty, TILE_SIZE, TILE_SIZE);
         cairo_fill(ctx);

         if (index) {
            set_source_rgb(ctx, 119, 110, 101);

            cairo_text_extents(ctx, label_lut[index], &extents);

            int font_off_y = extents.height/2.0 + TILE_SIZE/2.0;
            int font_off_x = TILE_SIZE/2.0 - extents.width/2.0;

            cairo_move_to(ctx, tx + font_off_x, ty + font_off_y);

            cairo_show_text(ctx, label_lut[index]);
         }

         tx += TILE_SIZE + SPACING;
      }
      ty += TILE_SIZE + SPACING;
   }

   cairo_surface_flush(surface);


}

static void update_input(void)
{
   static int up, right, down, left;
   static int last_up = 0, last_right = 0, last_down = 0, last_left = 0;

   input_poll_cb();

   direction = DIR_NONE;

   up = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP);
   right = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT);
   down = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN);
   left = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT);

   if (!up && last_up)
      direction = DIR_UP;
   else if (!right && last_right)
      direction = DIR_RIGHT;
   else if (!down && last_down)
      direction = DIR_DOWN;
   else if (!left && last_left)
      direction = DIR_LEFT;

   last_up = up;
   last_right = right;
   last_down = down;
   last_left = left;
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

         int *cell = &grid[row * 4 + col];
         if (!*cell)
            continue;

         int *farthest;
         int *next = cell;

         int new_row = row , new_col = col;

         do {
            farthest = next;

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
            game_score += 2 << *next;
         } else if (farthest != cell) {
            *farthest = *cell;
            *cell = 0;
         }
      }
   }
}

static void add_tile(void) {
   int *empty[GRID_SIZE];

   int j = 0;
   for (int i = 0; i < GRID_SIZE; i++) {
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
   SCREEN_PITCH = cairo_format_stride_for_width(CAIRO_FORMAT_RGB16_565, SCREEN_WIDTH);

   frame_buf = calloc(SCREEN_HEIGHT, SCREEN_PITCH);

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

//   add_tile();  add_tile();
}

void retro_deinit(void)
{
   free(frame_buf);
   frame_buf = NULL;

   for (int i = 0; i < 13; i++) {
      cairo_pattern_destroy(color_lut[i]);
      color_lut[i] = NULL;
   }

   cairo_destroy(ctx);
   cairo_surface_destroy(surface);

   ctx     = NULL;
   surface = NULL;
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

   info->geometry.base_width   = SCREEN_WIDTH;
   info->geometry.base_height  = SCREEN_HEIGHT;
   info->geometry.max_width    = SCREEN_WIDTH;
   info->geometry.max_height   = SCREEN_HEIGHT;
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

   if (direction != DIR_NONE) {
      move_tiles();
      add_tile();
   }

   render_grid();

   video_cb(frame_buf, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_PITCH);
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

