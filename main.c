#include "main.h"
#include <stdio.h>

const int SCREEN_WIDTH 				= 640;
const int SCREEN_HEIGHT 				= 480;
const int SCREEN_BPP 				= 32;

static const int FRAMES_PER_SECOND 	= 60;

void ( *g_handleEventsFn )( SDL_Event* ) = NULL;
void ( *g_updateFn )( unsigned )		 = NULL;
void ( *g_drawFn )( void )			 = NULL;

int g_Running						= 1;

static SDL_Surface *g_Screen 			= NULL;
static char g_WinCaption[30];

/************************************************************/

SDL_Surface *loadImage( char *filename )
{
	SDL_Surface *image = SDL_LoadBMP( filename );
	SDL_Surface *optimized = NULL;
	
	if ( image != NULL )
	{
		optimized = SDL_DisplayFormat( image );
		SDL_FreeSurface( image );
		
		if ( optimized != NULL )
		{
			SDL_SetColorKey( optimized, SDL_SRCCOLORKEY, SDL_MapRGB( optimized->format, 0xFF, 0x00, 0xFF ) );
			fprintf( stdout, "Loaded image: %s\n", filename );
		}
	}
	
	if ( optimized == NULL )
		fprintf( stderr, "Failed to load image \"%s\": %s\n", filename, SDL_GetError() );
	
	return optimized;
}

TTF_Font *loadFont( char *filename, int ptsize )
{
	TTF_Font *font = TTF_OpenFont( filename, ptsize );
	
	int loaded = font != NULL;
	fprintf( loaded ? stdout : stderr, "%s: %s\n", loaded ? "Loaded font" : TTF_GetError(), filename );
	
	return font;
}

Mix_Chunk *loadSound( char *filename )
{
	Mix_Chunk *sfx = Mix_LoadWAV( filename );
	
	int loaded = sfx != NULL;
	fprintf( loaded ? stdout : stderr, "%s: %s\n", loaded ? "Loaded sound" : SDL_GetError(), filename );
	
	return sfx;
}

Mix_Music *loadMusic( char *filename )
{
	Mix_Music *mus = Mix_LoadMUS( filename );
	
	int loaded = mus != NULL;
	fprintf( loaded ? stdout : stderr, "%s: %s\n", loaded ? "Loaded music" : SDL_GetError(), filename );
	
	return mus;
}

/************************************************************/

void drawImage( SDL_Surface *source, SDL_Rect *subrect, int x, int y )
{
	SDL_Rect rect;
	rect.x = x;
	rect.y = y;
	
	SDL_BlitSurface( source, subrect, g_Screen, &rect );
}

void drawRect( SDL_Rect rect, char r, char g, char b, char a )
{
	SDL_FillRect( g_Screen, &rect, SDL_MapRGBA( g_Screen->format, r, g, b, a ) );
}

void playSound( Mix_Chunk* sfx )
{
	Mix_PlayChannel( -1, sfx, 0 );
}

/************************************************************/

SDL_Rect rect( int x, int y, unsigned w, unsigned h )
{
	SDL_Rect r;
	r.x = x; r.y = y; r.w = w; r.h = h;
	return r;
}

int rect_contains( SDL_Rect a, int x, int y )
{
	return a.x <= x && x <= a.x + a.w && a.y <= y && y <= a.y + a.h;
}

int rect_intersect( SDL_Rect a, SDL_Rect b )
{
     return !( b.x > a.x + a.w || 
               b.x + b.w < a.x || 
               b.y > a.y + a.h ||
               b.y + b.h < a.y );
}

/************************************************************/

void sprite_draw( Sprite *sprite, int x, int y )
{
	drawImage( sprite->image, &sprite->rect, x, y );
}

/************************************************************/

int timer_getElapsedTime( Timer *timer )
{
	return SDL_GetTicks() - timer->tick;
}

int timer_update( Timer *timer )
{
	if ( timer->tick + timer->interval < SDL_GetTicks() )
	{
		timer->tick = SDL_GetTicks();
		return 1;
	}
	return 0;
}

void timer_reset( Timer *timer )
{
	timer->tick = SDL_GetTicks();
}

/************************************************************/

int init( void )
{
	if ( SDL_Init( SDL_INIT_EVERYTHING ) == -1 )
	{
		fprintf( stderr, "Failed to initialize SDL: %s\n", SDL_GetError() );
		return 1;
	}

	if ( Mix_OpenAudio( 44100, AUDIO_S16SYS, 2, 1024 ) < 0 )
	{
		fprintf( stderr, "Error initializing SDL_mixer: %s\n", Mix_GetError());
		return 1;
	}
	
	if ( TTF_Init() == -1 )
	{
		fprintf( stderr, "Error initializing TTF_font: %s\n", TTF_GetError() );
		return -1;
	}
	
	/* create the screen */	
	g_Screen = SDL_SetVideoMode( SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP, SDL_SWSURFACE );
	
	if ( g_Screen == NULL )
	{
		fprintf( stderr, "Failed to create a window: %s\n", SDL_GetError() );
		return -1;
	}
	
	if ( game_init() != 0 || game_setState() != 0 )
		return 1;
		
	sprintf( g_WinCaption, "Mario Tangent -- %d FPS", FRAMES_PER_SECOND );
	SDL_WM_SetCaption( g_WinCaption, NULL );
	
	return 0;
}

void clean_up( void )
{
	game_cleanup();	
	SDL_Quit();
}

/************************************************************/

int main( int argc, char** argv )
{
	int errc = 0;
	if ( ( errc = init() ) != 0 )
		return errc;
		
	int nextTick = 0, interval = 1 * 1000 / FRAMES_PER_SECOND;
	
	/* fps counter */
	int fps = FRAMES_PER_SECOND;
	Timer FPStimer;
	FPStimer.tick = 0;
	FPStimer.interval = 1000;
	
	/* delta timer -- used for frame-independent movement */
	Timer delta;
	delta.tick = 0;
	
	/* cycle functions */
	void ( *handleEventsFn )( SDL_Event* ) 	= g_handleEventsFn;
	void ( *updateFn )( unsigned ) 		= g_updateFn;
	void ( *drawFn )( void ) 			= g_drawFn;
		
	SDL_Event event;
		
	while ( g_Running )
	{
		while ( SDL_PollEvent( &event ) )
		{
			if ( event.type == SDL_QUIT )
				g_Running = 0;
			else
				(*handleEventsFn)( &event );
		}
		(*updateFn)( timer_getElapsedTime( &delta ) );
		(*drawFn)();
		
		/* update the screen */
		SDL_Flip( g_Screen );
		
		delta.tick = SDL_GetTicks();
		
		/* frame rate control */
		if ( nextTick > SDL_GetTicks() )
			SDL_Delay( nextTick - SDL_GetTicks() );
		nextTick = SDL_GetTicks() + interval;
		fps++;
		
		/* if 1 second passed, update the frame rate counter */
		if ( timer_update( &FPStimer ) )
		{
			sprintf( g_WinCaption, "Mario Tangent -- %d FPS", fps );
			SDL_WM_SetCaption( g_WinCaption, NULL );
			fps = 0;
		}
		
		/* update cycle functions */
		handleEventsFn = g_handleEventsFn;
		updateFn 		= g_updateFn;
		drawFn 		= g_drawFn;
	}
	
	clean_up();
	
	return errc;
}
