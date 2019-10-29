#include "game.h"
#include "game_shared.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>

int SCREEN_PITCH = 0;

static unsigned int color_lut[13];
static const char* label_lut[13] =
{
   "",
   "2", "4", "8", "16",
   "32", "64", "128", "256",
   "512", "1024", "2048",
   "XXX"
};

/* LAME DRAW TEXT and FILLRECT */

static unsigned *frame_buf;

typedef struct ctx_t
{
   unsigned int color;
   int fontsize_x;
   int fontsize_y;
} ctx_t;

ctx_t nullctx={0,0,0};

#define PITCH 4
#define RGB32(r, g, b,a)  ( (a)<<24 |((r) << (16)) | ((g) << 8) | ((b) << 0))
#define nullctx_fontsize(a) nullctx.fontsize_x=nullctx.fontsize_y=a

int VIRTUAL_WIDTH;

void initgraph(void)
{
	VIRTUAL_WIDTH=SCREEN_WIDTH;
	printf("GW:%d GH:%d  GSZ:%d\n",GRID_WIDTH,GRID_HEIGHT,GRID_SIZE);
	printf("SP:%d TSZ:%d\n",SPACING,TILE_SIZE);
	printf("BW:%d BH:%d BOY:%d \n",BOARD_WIDTH,BOARD_HEIGHT,BOARD_OFFSET_Y);
	printf("size:%dx%dx%d \n",SCREEN_WIDTH,SCREEN_HEIGHT,SCREEN_PITCH);
}

void DrawFBoxBmp(char  *buffer,int x,int y,int dx,int dy,unsigned color)
{
   int i,j,idx;

#if defined PITCH && PITCH == 4
   unsigned *mbuffer=(unsigned*)buffer;
#else
   unsigned short *mbuffer=(unsigned short *)buffer;
#endif

   for(i = x; i < x + dx; i++)
   {
      for(j = y; j < y + dy; j++)
      {
         idx= i + j * VIRTUAL_WIDTH;
         mbuffer[idx] = color;
      }
   }

}

#include "noncairo/font2.c"

void Draw_string(char *surf, signed short int x, signed short int y, const unsigned char *string,unsigned short maxstrlen,unsigned short xscale, unsigned short yscale, unsigned  fg, unsigned  bg)
{
   int strlen, surfw, surfh;
   unsigned char *linesurf;
   signed  int ypixel;
   unsigned  *yptr;
   int col, bit;
   unsigned char b;

   int xrepeat, yrepeat;

#if defined PITCH && PITCH == 4
   unsigned *mbuffer=(unsigned*)surf;
#else
   unsigned short *mbuffer=(unsigned short *)surf;
#endif

   if(string == NULL)
      return;
   for(strlen = 0; strlen<maxstrlen && string[strlen]; strlen++)
   {}

   surfw=strlen * 7 * xscale;
   surfh=8 * yscale;

#if defined PITCH && PITCH == 4

   linesurf = malloc(sizeof(unsigned ) * surfw * surfh);
   yptr = (unsigned *)&linesurf[0];
#else
   linesurf = malloc(sizeof(unsigned short)* surfw * surfh);
   yptr = (unsigned short *)&linesurf[0];
#endif

   for(ypixel = 0; ypixel < 8; ypixel++)
   {
      for(col=0; col<strlen; col++)
      {
         b = font_array[(string[col]^0x80)*8 + ypixel];

         for(bit=0; bit<7; bit++, yptr++)
         {
            *yptr = (b & (1<<(7-bit))) ? fg : bg;
            for(xrepeat = 1; xrepeat < xscale; xrepeat++, yptr++)
               yptr[1] = *yptr;
         }
      }

      for(yrepeat = 1; yrepeat < yscale; yrepeat++)
         for(xrepeat = 0; xrepeat<surfw; xrepeat++, yptr++)
            *yptr = yptr[-surfw];

   }

#if defined PITCH && PITCH == 4
   yptr = (unsigned *)&linesurf[0];
#else
   yptr = (unsigned short*)&linesurf[0];
#endif

   for(yrepeat = y; yrepeat < y+ surfh; yrepeat++)
      for(xrepeat = x; xrepeat< x+surfw; xrepeat++,yptr++)
         if(*yptr!=0)
            mbuffer[xrepeat+yrepeat*VIRTUAL_WIDTH] = *yptr;

   free(linesurf);
}

void Draw_text(char *buffer,int x,int y,unsigned    fgcol,unsigned   int bgcol ,int scalex,int scaley , int max,const char *string,...)
{
   char text[256];
   va_list ap;

   if (string == NULL)
      return;

   va_start(ap, string);
   vsprintf(text, string, ap);
   va_end(ap);

   Draw_string(buffer, x,y,(unsigned char*) text,max, scalex, scaley,fgcol,bgcol);
}


static void set_rgb(int ctx, int r, int g, int b)
{
   nullctx.color=RGB32(r,g,b,255);
}

static void set_rgba(int ctx, int r, int g, int b, float a)
{
   nullctx.color=RGB32(r,g,b,(int)a*255);
}

static void fill_rectangle(int ctx, int x, int y, int w, int h)
{
   char *ptr=(char*)frame_buf;
   DrawFBoxBmp(ptr, x, y, w, h, nullctx.color);
}

static void draw_text_centered(int ctx, const char *utf8, int x, int y, int w, int h)
{

   char *ptr=(char*)frame_buf;
   int size=strlen(utf8);
   int foy = h ? (h - 8 * nullctx.fontsize_y) / 2 : 0;
   int fox = w ? (w - size * 7 * nullctx.fontsize_x) / 2 : 0;

   Draw_text(ptr,x+fox,y+foy,nullctx.color,0 ,nullctx.fontsize_x,nullctx.fontsize_y ,size, utf8);

}

static void draw_tile(int ctx, cell_t *cell)
{
   int x, y;
   int w = TILE_SIZE, h = TILE_SIZE;
   int font_size = FONT_SIZE;
   float *frame_time = game_get_frame_time();

   (void)font_size;

   if (cell->value && cell->move_time < 1)
   {
      int x1, y1, x2, y2;

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
#if 0
      w = lerp(0, TILE_SIZE, cell->appear_time);
      h = lerp(0, TILE_SIZE, cell->appear_time);
      font_size = lerp(0, FONT_SIZE, cell->appear_time);
#endif

      x += TILE_SIZE/2 - w/2;
      y += TILE_SIZE/2 - h/2;

      cell->appear_time += *frame_time * TILE_ANIM_SPEED;
   } else {
      grid_to_screen(cell->pos, &x, &y);
   }

   if (cell->value)
      nullctx.color=color_lut[cell->value];
   else
      nullctx.color=RGB32(205,192,180,255);

   fill_rectangle(ctx, x, y, w, h);

   if (cell->value)
   {
      if (cell->value < 10) /* one to three digits */
         nullctx_fontsize(3);
      else /* four digits */
         nullctx_fontsize(2);
      if (cell->value == 6) // Background of 64 is too dark
         set_rgb(ctx, 255, 255, 255);
      else
         set_rgb(ctx, 119, 110, 101);
      draw_text_centered(ctx, label_lut[cell->value], x, y, w, h);
   }
}

void game_calculate_pitch(void)
{
   SCREEN_PITCH = (SCREEN_WIDTH) * (PITCH);
}

static void init_luts(void)
{
   color_lut[0] = RGB32(238,228,218,90);
   color_lut[1] = RGB32(238,228,218,255);

   color_lut[2] = RGB32(237,224,200,255);
   color_lut[3] = RGB32(242,177,121,255);
   color_lut[4] = RGB32(245,149,99,255);
   color_lut[5] = RGB32(246,124,95,255);
   color_lut[6] = RGB32(246,94,59,255);

   /* TODO: shadow */
   color_lut[7] = RGB32(237,207,114,255);
   color_lut[8] = RGB32(237,204,97,255);
   color_lut[9] = RGB32(237,200,80,255);
   color_lut[10] = RGB32(237,197,63,255);
   color_lut[11] = RGB32(237,194,46,255);
   color_lut[12] = RGB32(60,58,50,255);
}

static void init_static_surface(void)
{
   int row, col;
   int static_ctx;
   cell_t dummy;
   static_ctx = 0;

   /* bg */
   set_rgb(static_ctx, 250, 248, 239);
   fill_rectangle(static_ctx, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

   /* grid bg */
   set_rgb(static_ctx, 185, 172, 159);
   fill_rectangle(static_ctx, SPACING, BOARD_OFFSET_Y, BOARD_WIDTH, BOARD_WIDTH);

   /* score bg */
   set_rgb(static_ctx, 185, 172, 159);
   fill_rectangle(static_ctx, SPACING, SPACING, TILE_SIZE*2+SPACING*2, TILE_SIZE);

   /* best bg */
   set_rgb(static_ctx, 185, 172, 159);
   fill_rectangle(static_ctx, TILE_SIZE*2+SPACING*4, SPACING, TILE_SIZE*2+SPACING*2, TILE_SIZE);

   nullctx.color=color_lut[1];
   nullctx_fontsize(3) ;

   /* score title */
   draw_text_centered(static_ctx, "SCORE", SPACING*2, SPACING * 2, 0, 0);

   /* best title */
   draw_text_centered(static_ctx, "BEST", TILE_SIZE*2+SPACING*5, SPACING*2, 0, 0);

   /* draw background cells */
   dummy.move_time   = 1;
   dummy.appear_time = 1;
   dummy.source      = NULL;
   dummy.value       = 0;

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

}

void game_init(void)
{
   unsigned int t = (unsigned int)time(NULL);
   frame_buf = calloc(SCREEN_HEIGHT, SCREEN_PITCH);

   srand(t);

	initgraph();

   init_luts();
   init_static_surface();

   init_game();
   start_game();
}

void game_deinit(void)
{
   if (frame_buf)
      free(frame_buf);
   frame_buf = NULL;
}

void render_playing(void)
{
   int *delta_score;
   float *delta_score_time;
   int row, col, ctx=0;
   char tmp[10] = {0};
   float *frame_time = game_get_frame_time();

   /* paint static background */

   nullctx_fontsize(3) ;

   /* score and best score value */
   set_rgb(ctx, 255, 255, 255);
   sprintf(tmp, "%i", game_get_score() % 1000000);
   draw_text_centered(ctx, tmp, SPACING * 2,  SPACING * 2 + FONT_SIZE, TILE_SIZE * 2, TILE_SIZE - FONT_SIZE);

   sprintf(tmp, "%i", game_get_best_score() % 1000000);
   nullctx.color=color_lut[1];
   draw_text_centered(ctx, tmp, TILE_SIZE * 2 + SPACING * 5, SPACING * 2 + FONT_SIZE, TILE_SIZE * 2, TILE_SIZE - FONT_SIZE);

   for (row = 0; row < 4; row++)
   {
      for (col = 0; col < 4; col++)
      {
         cell_t *grid = game_get_grid();
         cell_t *cell = &grid[row * 4 + col];

         if (cell->value)
            draw_tile(ctx, cell);
      }
   }

   delta_score_time = game_get_delta_score_time();
   delta_score = game_get_delta_score();

   /* draw +score animation */
   if (*delta_score_time < 1)
   {
      int x, y;

      nullctx_fontsize(3);
      x = SPACING * 2;
      y = SPACING * 5;
      y = lerp(y, y - TILE_SIZE, *delta_score_time);

      set_rgba(ctx, 119, 110, 101, lerp(1, 0, *delta_score_time));

      sprintf(tmp, "+%i", *delta_score);
      draw_text_centered(ctx, tmp, x, y, TILE_SIZE * 2, TILE_SIZE);

      *delta_score_time += *frame_time;
   }
}

void render_button(int ctx, const char *text, int y)
{
   int x = SPACING;
   int w = SCREEN_WIDTH - SPACING * 2;
   int h = TILE_SIZE;

   set_rgb(ctx, 185, 172, 159);
   fill_rectangle(ctx, x, y, w, h);

   nullctx_fontsize(3);
   nullctx.color= color_lut[1];
   draw_text_centered(ctx, text, x, y, w, h);
}

void render_heading(int ctx, const char *text)
{
   nullctx_fontsize(5) ;
   set_rgb(ctx, 185, 172, 159);
   draw_text_centered(ctx, text, 0, 0, SCREEN_WIDTH, TILE_SIZE * 3);
}

void render_score(int ctx)
{
   char tmp[100];
   nullctx_fontsize(3);

   set_rgb(ctx, 185, 172, 159);

   sprintf(tmp, "SCORE %i", game_get_score());
   draw_text_centered(ctx, tmp, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void render_title(void)
{
   int ctx=0;

   /* bg */
   set_rgb(ctx, 250, 248, 239);
   fill_rectangle(ctx, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

   render_heading(ctx, "2048");

   render_button(ctx, "PRESS START", TILE_SIZE * 4);
}

void render_win_or_game_over(void)
{
   game_state_t state = game_get_state();
   int ctx=0;

   if (state == STATE_GAME_OVER)
      render_playing();

   /* bg */
   set_rgba(ctx, 250, 248, 239, 0.85);
   fill_rectangle(ctx, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

   render_heading(ctx, state == STATE_GAME_OVER ? "GAME OVER" : "YOU WIN");

   render_score(ctx);

   render_button(ctx, "PRESS START", TILE_SIZE * 4);
}

void render_paused(void)
{
   int ctx=0;

   render_playing();

   /* bg */
   set_rgba(ctx, 250, 248, 239, 0.85);
   fill_rectangle(ctx, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

   render_heading(ctx, "PAUSED");

   render_score(ctx);

   render_button(ctx, "SELECT: NEW GAME", TILE_SIZE * 3.5);
   render_button(ctx, "START: CONTINUE", TILE_SIZE * 4.5 + SPACING);
}

int game_init_pixelformat(void)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
   {
      if (log_cb)
         log_cb(RETRO_LOG_INFO, "XRGB8888 is not supported.\n");
      return 0;
   }

   return 1;
}

void game_render(void)
{
   init_static_surface();

   render_game();
   video_cb(frame_buf, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_PITCH);
}
