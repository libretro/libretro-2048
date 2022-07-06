#include "game.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include <file/file_path.h>
#include <streams/file_stream.h>

retro_log_printf_t log_cb;
retro_video_refresh_t video_cb;

#if 0
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
#endif

retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

#define SAVE_FILE_NAME "2048.srm"

static float frame_time        = 0;

static bool first_run          = true;
static bool sram_accessed      = false;
static bool use_sram_file      = false;

static bool block_sram_write   = false;
static void *game_data_scratch = NULL;

static bool libretro_supports_bitmasks = false;

bool dark_theme = false;

void log_2048(enum retro_log_level level, const char *format, ...)
{
   char msg[512];
   va_list ap;

   msg[0] = '\0';

   if (!format || (format[0] == '\0'))
      return;

   va_start(ap, format);
   vsprintf(msg, format, ap);
   va_end(ap);

   if (log_cb)
      log_cb(level, "[2048] %s", msg);
   else
      fprintf((level == RETRO_LOG_ERROR) ? stderr : stdout,
            "[2048] %s", msg);
}

static void read_save_file(void)
{
   char *save_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) &&
       save_dir)
   {
      RFILE *save_file = NULL;
      int64_t save_size;
      char save_path[1024];

      save_path[0] = '\0';

      /* Get save file path */
      fill_pathname_join(save_path, save_dir,
            SAVE_FILE_NAME, sizeof(save_path));

      if (!path_is_valid(save_path))
      {
         log_2048(RETRO_LOG_INFO, "No game data found: %s\n", save_path);
         return;
      }

      /* Open save file */
      save_file = filestream_open(save_path,
            RETRO_VFS_FILE_ACCESS_READ,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);

      if (!save_file)
      {
         log_2048(RETRO_LOG_ERROR, "Failed to open save file: %s\n", save_path);
         return;
      }

      /* Check save file size */
      filestream_seek(save_file, 0, RETRO_VFS_SEEK_POSITION_END);
      save_size = filestream_tell(save_file);
      filestream_seek(save_file, 0, RETRO_VFS_SEEK_POSITION_START);

      if (save_size != (int64_t)game_data_size())
      {
         log_2048(RETRO_LOG_ERROR, "Failed to load save file: incorrect size.\n");
         filestream_close(save_file);
         return;
      }

      /* Read save file */
      filestream_read(save_file, game_data(), game_data_size());
      filestream_close(save_file);

      log_2048(RETRO_LOG_INFO, "Loaded save file: %s\n", save_path);
   }
   else
      log_2048(RETRO_LOG_WARN, "Unable to load game data - save directory not set.\n");
}

static void write_save_file(void)
{
   char *save_dir = NULL;

   if (environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir) &&
       save_dir)
   {
      RFILE *save_file = NULL;
      char save_path[1024];

      save_path[0] = '\0';

      /* Get save file path */
      fill_pathname_join(save_path, save_dir,
            SAVE_FILE_NAME, sizeof(save_path));

      /* Open save file */
      save_file = filestream_open(save_path,
            RETRO_VFS_FILE_ACCESS_WRITE,
            RETRO_VFS_FILE_ACCESS_HINT_NONE);

      if (!save_file)
      {
         log_2048(RETRO_LOG_ERROR, "Failed to open save file: %s\n", save_path);
         return;
      }

      /* Write save file */
      filestream_write(save_file, game_data(), game_data_size());
      filestream_close(save_file);

      log_2048(RETRO_LOG_INFO, "Wrote save file: %s\n", save_path);
   }
   else
      log_2048(RETRO_LOG_WARN, "Unable to save game data - save directory not set.\n");
}

void retro_init(void)
{
   struct retro_log_callback logging;

   frame_time        = 0;
   first_run         = true;
   sram_accessed     = false;
   use_sram_file     = false;
   block_sram_write  = false;
   game_data_scratch = malloc(game_data_size());

   libretro_supports_bitmasks = false;
   if (environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, NULL))
      libretro_supports_bitmasks = true;

   log_cb = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
      log_cb = logging.log;

   game_calculate_pitch();

   game_init();
}

void retro_deinit(void)
{
   if (use_sram_file)
      write_save_file();

   game_deinit();

   frame_time        = 0;
   first_run         = true;
   sram_accessed     = false;
   use_sram_file     = false;

   block_sram_write = false;
   if (game_data_scratch)
      free(game_data_scratch);
   game_data_scratch = NULL;

   libretro_supports_bitmasks = false;
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
#ifndef GIT_VERSION
#define GIT_VERSION ""
#endif
   info->library_version  = "v1.0" GIT_VERSION;
   info->need_fullpath    = false;
   info->valid_extensions = NULL; /* Anything is fine, we don't care. */
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->timing.fps = 60.0;
   info->timing.sample_rate = 0.0;

   info->geometry.base_width   = SCREEN_WIDTH;
   info->geometry.base_height  = SCREEN_HEIGHT;
   info->geometry.max_width    = SCREEN_WIDTH;
   info->geometry.max_height   = SCREEN_HEIGHT;
   info->geometry.aspect_ratio = 0.0;
}

static void check_variables(void)
{
   struct retro_variable var        = {0};

   var.key = "2048_theme";
   if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
   {
      if (!strncmp(var.value, "Light", 4))
         dark_theme = false;
      else if (!strncmp(var.value, "Dark", 4))
         dark_theme = true;
   }
}

void retro_set_environment(retro_environment_t cb)
{
   struct retro_vfs_interface_info vfs_iface_info;
   bool no_rom = true;
   
   static const struct retro_variable vars[] = {
      { "2048_theme", "Theme (restart); Light|Dark" },
      { NULL, NULL },
   };

   environ_cb = cb;
   environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);
   cb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)vars);

   vfs_iface_info.required_interface_version = 1;
   vfs_iface_info.iface                      = NULL;
   if (environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info))
      filestream_vfs_init(&vfs_iface_info);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
#if 0
   audio_cb = cb;
#endif
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
#if 0
   audio_batch_cb = cb;
#endif
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
   game_reset();
}

static void frame_time_cb(retro_usec_t usec)
{
   frame_time = usec / 1000000.0;
}

void retro_run(void)
{
   int16_t ret = 0;
   key_state_t ks;

   block_sram_write = false;

   /* If this is the first call of retro_run(),
    * check if the SRAM memory data has already
    * been accessed. If not, this means the
    * frontend SRAM interface is disabled, and
    * the core must handle save file reading
    * and writing internally */
   if (first_run)
   {
      if (!sram_accessed)
      {
         read_save_file();
         use_sram_file = true;
      }
      
      check_variables();

      first_run = false;
   }

   input_poll_cb();
   
   if (libretro_supports_bitmasks)
      ret = input_state_cb(0, RETRO_DEVICE_JOYPAD,
            0, RETRO_DEVICE_ID_JOYPAD_MASK);
   else
   {
      unsigned i;
      for (i = 0; i < RETRO_DEVICE_ID_JOYPAD_RIGHT+1; i++)
      {
         if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i))
            ret |= (1 << i);
      }
   }

   ks.up     = (ret & (1 << RETRO_DEVICE_ID_JOYPAD_UP));
   ks.right  = (ret & (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT));
   ks.down   = (ret & (1 << RETRO_DEVICE_ID_JOYPAD_DOWN));
   ks.left   = (ret & (1 << RETRO_DEVICE_ID_JOYPAD_LEFT));
   ks.start  = (ret & (1 << RETRO_DEVICE_ID_JOYPAD_START));
   ks.select = (ret & (1 << RETRO_DEVICE_ID_JOYPAD_SELECT));

   game_update(frame_time, &ks);
   game_render();
}

bool retro_load_game(const struct retro_game_info *info)
{
   struct retro_frame_time_callback frame_cb;
   struct retro_input_descriptor desc[] = {
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT,  "Left" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP,    "Up" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN,  "Down" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Start" },
      { 0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Pause" },
      { 0 },
   };

   environ_cb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

   if (!game_init_pixelformat())
      return false;

   frame_cb.callback  = frame_time_cb;
   frame_cb.reference = 1000000 / 60;
   frame_cb.callback(frame_cb.reference);
   environ_cb(RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK, &frame_cb);

   return true;
}

void retro_unload_game(void)
{
   block_sram_write = false;
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
   return game_data_size();
}

bool retro_serialize(void *data_, size_t size)
{
   block_sram_write = false;

   if (size < game_data_size())
      return false;

   memcpy(data_, game_data(), game_data_size());
   return true;
}

bool retro_unserialize(const void *data_, size_t size)
{
   block_sram_write = true;

   if (size < game_data_size())
      return false;

   memcpy(game_data(), data_, game_data_size());
   return true;
}

void *retro_get_memory_data(unsigned id)
{
   if (id != RETRO_MEMORY_SAVE_RAM)
      return NULL;

   sram_accessed = true;

   /* An ugly workaround for a RetroArch frontend
    * feature...
    * If a save state is restored with the RetroArch
    * [Don't Overwrite SaveRAM on Loading Save State]
    * option enabled, then the following sequence
    * occurs:
    * 1. Current SRAM data is read from the core
    * 2. retro_unserialize() is called
    * 3. The SRAM data from (1) is written back
    *    to the core
    * The issue here is that the SRAM data and the
    * serialised data for this core are identical
    * (they both access the same buffer). Thus
    * step (3) reverts the de-serialisation operation
    * by overwriting the result with the previous
    * game state...
    * The only way around this is to temporarily
    * disable any SRAM write operations immediately
    * following retro_unserialize() by passing a
    * dummy buffer to the frontend (any changes to
    * which are ignored by the core). */
   if (block_sram_write)
   {
      memcpy(game_data_scratch, game_data(), game_data_size());
      return game_data_scratch;
   }

   return game_data();
}

size_t retro_get_memory_size(unsigned id)
{
   if (id != RETRO_MEMORY_SAVE_RAM)
      return 0;

   return game_data_size();
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

