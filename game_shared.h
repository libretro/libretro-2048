#ifndef _GAME_SHARED_H
#define _GAME_SHARED_H

#include "game.h"

float bump_out(float v0, float v1, float t);
float lerp(float v0, float v1, float t);
float cos_interp(float v0,float v1, float t);

void *game_data(void);
void *game_save_data(void);
unsigned game_data_size(void);
void render_game(void);
void init_game(void);
void start_game(void);
void change_state(game_state_t state);
game_state_t game_get_state(void);
void handle_input(key_state_t *ks);
int game_get_score(void);
int game_get_best_score(void);
cell_t * game_get_grid(void);
int *game_get_delta_score(void);
float *game_get_delta_score_time(void);
float *game_get_frame_time(void);

#endif
