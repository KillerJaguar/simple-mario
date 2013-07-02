#include "SDL/SDL.h"
#include "SDL/SDL_mixer.h"
#include "SDL/SDL_ttf.h"

#ifndef MAIN_H_
#define MAIN_H_

/* global constants are defined in main.c */

extern const int SCREEN_WIDTH;
extern const int SCREEN_HEIGHT;
extern const int SCREEN_BPP;

extern int g_Running;

/* SDL resource functions */

SDL_Surface *loadImage( char *filename );
TTF_Font *loadFont( char *filename, int ptsize );
Mix_Chunk *loadSound( char *filename );
Mix_Music *loadMusic( char *filename );

#define FreeSurface(s) SDL_FreeSurface(s);s=NULL
#define FreeFont(s) TTF_CloseFont(s);s=NULL
#define FreeChunk(s) Mix_FreeChunk(s);s=NULL
#define FreeMusic(s) Mix_FreeMusic(s);s=NULL

void drawRect( SDL_Rect rect, char r, char g, char b, char a );
void drawImage( SDL_Surface *source, SDL_Rect *subrect, int x, int y );
void playSound( Mix_Chunk* sfx );

/* SDL_Rect utility functions */
SDL_Rect rect( int x, int y, unsigned w, unsigned h );
int rect_contains( SDL_Rect a, int x, int y );
int rect_intersect( SDL_Rect a, SDL_Rect b );

/* functions that are called every cycle -- to change state, change the function pointer */
extern void ( *g_handleEventsFn )( SDL_Event* );
extern void ( *g_updateFn )( unsigned );
extern void ( *g_drawFn )( void );

/* sprite utility struct and functions */
typedef struct s_Sprite
{
	SDL_Surface *image;
	SDL_Rect rect;
} Sprite;

void sprite_draw( Sprite *sprite, int x, int y );

/* timer utility struct and functions */
typedef struct s_Timer
{
	int tick;			/* time to change frame */
	int interval;		/* interval to change frame */
} Timer;

int timer_getElapsedTime( Timer *timer );
int timer_update( Timer *timer );
void timer_reset( Timer *timer );

/* game state functions */
int game_init( void );
void game_cleanup( void );
int game_setState( void );

#endif
