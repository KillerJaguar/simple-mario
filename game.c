#include "main.h"

#include <ctype.h>
#include <stdio.h>

static const int TILE_WIDTH			= 16;
static const int TILE_HEIGHT			= 16;

#define NUM_TILES ((SCREEN_WIDTH/TILE_WIDTH)*(SCREEN_HEIGHT/TILE_HEIGHT))

static const int PLAYER_WIDTH 		= 16;
static const int PLAYER_HEIGHT		= 28;

#define HALF_PLAYER_WIDTH 			(PLAYER_WIDTH/2)
#define HALF_PLAYER_HEIGHT 			(PLAYER_HEIGHT/2)

/* note: move speeds are in pixel-per-second */

static const int PLAYER_MOVE_SPEED 	= 32;
static const int PLAYER_JUMP_SPEED		= 64;
static const int PLAYER_FALL_SPEED		= 64;

static const int PLAYER_MAX_JUMP_SPEED 	= 640;
static const int PLAYER_MAX_MOVE_SPEED  = 386;

static const int PLATFORM_MOVE_SPEED 	= 64;

static const int COINS_PER_LIFE		= 25;
static const int INIT_PLAYER_LIVES		= 2;
static const int MAX_PLAYER_LIVES		= 5;

/* resources */

static SDL_Surface *g_imgTileset 		= NULL;
static SDL_Surface *g_imgPlayer 		= NULL;
static SDL_Surface *g_imgEnemy 		= NULL;
static SDL_Surface *g_imgBG 			= NULL;
static SDL_Surface *g_imgLives 		= NULL;
static SDL_Surface *g_imgGameOver		= NULL;

static TTF_Font *g_fontSmall			= NULL;
static TTF_Font *g_fontLarge			= NULL;

static SDL_Surface *g_textLives		= NULL;
static SDL_Surface *g_textScore		= NULL;
static SDL_Surface *g_textCoins		= NULL;
static SDL_Surface *g_textLevel		= NULL;
static SDL_Surface *g_textPressAnyKey	= NULL;

static Mix_Music *g_musBGM			= NULL;
static Mix_Music *g_musDeath			= NULL;
static Mix_Music *g_musGameOver		= NULL;
static Mix_Chunk *g_sfxCoin			= NULL;
static Mix_Chunk *g_sfxJump			= NULL;
static Mix_Chunk *g_sfxStomp			= NULL;
static Mix_Chunk *g_sfx1Up			= NULL;

/* global variables */

static int g_curLevel				= 1;
static int g_displayLevelText			= 1;
static Timer g_utilTimer;

int reset( void );

/************************************************************/

typedef enum e_Direction
{
	UP = 0,
	DOWN,
	LEFT,
	RIGHT
} Direction;

Direction dir_getOpposite( Direction d )
{
	switch ( d )
	{
		case UP:		return DOWN;
		case DOWN:	return UP;
		case LEFT:	return RIGHT;
		case RIGHT:	return LEFT;
	}
}

/************************************************************/

typedef struct s_CoinController
{
	int count;		/* number of coins */
	int size;			/* size of array */
	int *array;		/* coin pos array */
} CoinController;

void cc_init( CoinController *cc )
{
	cc->count = 0;
	cc->size 	= 10; /* arbitrary number */
	cc->array = (int*) calloc( cc->size, sizeof( int ) );
	
	int i;
	for ( i = 0; i < cc->size; i++ )
		cc->array[i] = -1;
}

void cc_cleanup( CoinController *cc )
{	
	free( cc->array );
}

void cc_addCoin( CoinController *cc, int index )
{
	/* check for available space */
	if ( cc->count == cc->size )
	{
		/* realloc the array */
		cc->size *= 2;
		cc->array = (int*) realloc( cc->array, sizeof( int ) * cc->size );
		
		int i;
		for ( i = cc->count; i < cc->size; i++ )
			cc->array[i] = -1;
	}
	
	cc->array[ cc->count++ ] = index;
}

/************************************************************/

typedef struct s_MovingPlatform
{
	int startPos;			/* starting position */
	float x, y;			/* position of platform */
	Direction dir;			/* direction to move in */
} MovingPlatform;

typedef struct s_MovingPlatformController
{
	int count;			/* number of active platforms */
	int size;				/* size of platform array */
	MovingPlatform **array;	/* moving platform array */
} MovingPlatformController;

void mpc_init( MovingPlatformController *mpc )
{
	mpc->count = 0;
	mpc->size = 10; /* arbitrary number */
	mpc->array = (MovingPlatform**) realloc( NULL, sizeof( MovingPlatform* ) * mpc->size );
	
	int i;
	for ( i = 0; i < mpc->size; i++ )
		mpc->array[i] = NULL;
}

void mpc_cleanup( MovingPlatformController *mpc )
{
	int i;
	for ( i = 0; i < mpc->size; i++ )
		free( mpc->array[i] );
	free( mpc->array );
}

void mpc_addPlatform( MovingPlatformController *mpc, int i, Direction d )
{
	/* check for available space */
	if ( mpc->count == mpc->size )
	{
		/* realloc the array */
		mpc->size *= 2;
		mpc->array = (MovingPlatform**) realloc( mpc->array, sizeof( MovingPlatform* ) * mpc->size );
		
		int i;
		for ( i = mpc->count; i < mpc->size; i++ )
			mpc->array[i] = NULL;
	}
	
	/* create a new platform */
	MovingPlatform *mp = (MovingPlatform*) malloc( sizeof( MovingPlatform ) );
	mp->x 	= i % ( SCREEN_WIDTH / TILE_WIDTH ) * TILE_WIDTH;
	mp->y 	= i / ( SCREEN_WIDTH / TILE_WIDTH ) * TILE_HEIGHT;
	mp->dir 	= d;
	mp->startPos = i;
	
	/* add the platform the array */
	mpc->array[ mpc->count++ ] = mp;
}

void mpc_reset( MovingPlatformController *mpc )
{
	int i;
	for ( i = 0; i < mpc->count; i++ )
	{
		MovingPlatform *mp = mpc->array[i];
		if ( mp == NULL ) continue;
		
		mp->x = mp->startPos % ( SCREEN_WIDTH / TILE_WIDTH ) * TILE_WIDTH;
		mp->y = mp->startPos / ( SCREEN_WIDTH / TILE_WIDTH ) * TILE_HEIGHT;
		
		switch ( mp->dir )
		{
			case LEFT:
			case RIGHT:
				mp->dir = RIGHT;
			break;
			case UP:
			case DOWN:
				mp->dir = UP;
			break;
		}
	}
}

/************************************************************/

typedef enum e_JumpState
{
	CAN_JUMP,
	JUMPING,
	JUMPED
} JumpState;

static struct s_Player
{
	float x, y;			/* position of player */
	float xVel, yVel;		/* velocity of player */
	Direction lastDir;		/* last direction facing */
	JumpState jump;		/* jump state to determine if can jump */
	int lives; 			/* number of remaining lives */
	int time; 			/* amount of time left */
	int score; 			/* score counter */
	int coins; 			/* number of collected coins */
	int frame;			/* frame to animate */
	int dead;				/* boolean if player is dead */
	int keyPressed[4];       /* array of which key is pressed */
	int onPlatform;          /* boolean if player is on platform */
	Timer frameTimer;		/* timer to update frame */
	Sprite sprite;			/* sprite information */
} g_Player;

/************************************************************/

typedef struct s_Map
{
	char *data;					/* map data */
	int startPos;					/* starting position */
	CoinController cc;				/* coins */
	MovingPlatformController mpc;		/* moving platforms */
} Map;

static Map *g_Map = NULL;

void map_cleanup( Map *map )
{
	if ( map == NULL ) return;

	mpc_cleanup( &map->mpc );
	cc_cleanup( &map->cc );
	free( map->data );
}

void map_change( void )
{
	if ( ++g_curLevel > 9 ) 
		g_curLevel = 1;
	
	char str[20];
	
	/* format the string for display */
	sprintf( str, "Level %d", g_curLevel );
	FreeSurface( g_textLevel );
	g_textLevel = TTF_RenderText_Solid( g_fontLarge, str, (SDL_Color) { 0xFF, 0xFF, 0xFF } );
	
	/* format the string for loading */
	sprintf( str, "levels/level%d", g_curLevel );
	map_load( str );
	
	g_displayLevelText = 1;
	timer_reset( &g_utilTimer );
}

int map_load( char *filename )
{
	FILE *fp = NULL;
	char *data = NULL, next;
	int i, num_tiles = NUM_TILES;
	
	fp = fopen( filename, "r" );
	if ( fp == NULL )
	{
		fprintf( stderr, "Failed to open map \"%s\": file not found\n", filename );
		return 1;
	}
	
	Map *map = (Map*) malloc( sizeof( Map ) );
	mpc_init( &map->mpc );
	cc_init( &map->cc );
	map->data = (char*) calloc( num_tiles, sizeof( char ) );
	
	int startPos = -1, endPos = -1;
	
	for ( i = 0; i < num_tiles; i++ )
	{
		do 
		{
			if ( feof( fp ) )
			{
				fprintf( stderr, "Unexpected end of file: %s\n", filename );
				goto error_cleanup;
			}
			next = fgetc( fp );
		} while ( isspace( next ) );
		
		switch ( next )
		{
			case 'H':		mpc_addPlatform( &map->mpc, i, RIGHT ); break;
			case 'V':		mpc_addPlatform( &map->mpc, i, UP ); break;
			case 'C':		cc_addCoin( &map->cc, i ); break;
		}		
		
		map->data[i] = next;
		
		if ( startPos == -1 && next == 'S' )
			startPos = i;
		if ( endPos == -1 && next == 'E' )
			endPos = i;
	}
	
	if ( startPos == -1 )
	{
		fprintf( stderr, "Failed to load map \"%s\": missing starting position\n", filename );
		goto error_cleanup;
	}
	
	if ( endPos == -1 )
	{
		fprintf( stderr, "Failed to load map \"%s\": missing end position\n", filename );
		goto error_cleanup;
	}
	
	fclose( fp );
	
	map->startPos = startPos;
	
	/* clear the old map data */
	map_cleanup( g_Map );
	free( g_Map );
	
	/* set the new map to the new one */
	g_Map = map;
	
	/* reposition the player to the start */
	g_Player.x = ( startPos % ( SCREEN_WIDTH / TILE_WIDTH ) ) * TILE_WIDTH;
	g_Player.y = ( startPos / ( SCREEN_WIDTH / TILE_WIDTH ) ) * TILE_HEIGHT + ( TILE_HEIGHT * 2 - PLAYER_HEIGHT );
	g_Player.xVel = 0;
	g_Player.yVel = 0;
	g_Player.lastDir = RIGHT;
	
	fprintf( stdout, "Loaded map: %s\n", filename );
	
	return 0;
	
	error_cleanup:
	
		map_cleanup( map );
		free( map );
		fclose( fp );

	return 1;
}

char map_getTile( int x, int y )
{
	return g_Map->data[ y * ( SCREEN_WIDTH / TILE_WIDTH ) + x ];
}

SDL_Rect map_getTileRect( int x, int y )
{
	SDL_Rect rect;
	
	rect.x = TILE_WIDTH * x;
	rect.y = TILE_HEIGHT * y;
	rect.w = TILE_WIDTH;
	rect.h = TILE_HEIGHT;
	
	return rect;
}

int map_checkCollision( int x, int y )
{
	switch ( map_getTile( x / TILE_WIDTH, y / TILE_HEIGHT ) )
	{
		case '#':
		case '/':
		case '\\':
		case '-':
		case '[':
		case ']':
		case 'X':
		case '<':
		case '>':
		case 'E':
			return 1;
		default: 	
			return 0;
	}
}

void map_draw( void )
{
	int i = 0, max = NUM_TILES, x, y;
	SDL_Rect rect;
	for ( i = 0; i < max; i++ )
	{
		switch ( g_Map->data[ i ] )
		{
			case '#':  rect = map_getTileRect( 8, 0 );  break;
			case '/':  rect = map_getTileRect( 8, 9 );  break;
			case '\\': rect = map_getTileRect( 9, 10 ); break;
			case '-':  rect = map_getTileRect( 3, 9 );  break;
			case '[':  rect = map_getTileRect( 3, 10 ); break;
			case ']':  rect = map_getTileRect( 4, 10 ); break;
			case 'X':  rect = map_getTileRect( 6, 9 );  break;
			case '<':  rect = map_getTileRect( 2, 11 ); break;
			case '>':  rect = map_getTileRect( 4, 11 ); break;
			case 'E':  rect = map_getTileRect( 6, 6 );  break;
			default:   continue;
		}
		x = ( i % ( SCREEN_WIDTH / TILE_WIDTH ) ) * TILE_WIDTH;
		y = ( i / ( SCREEN_WIDTH / TILE_WIDTH ) ) * TILE_HEIGHT;
		drawImage( g_imgTileset, &rect, x, y );
	}
}

/************************************************************/

typedef enum e_Animation
{
	PLAYER_IDLE_LEFT,
	PLAYER_IDLE_RIGHT,
	PLAYER_MOVE_LEFT,
	PLAYER_MOVE_RIGHT,
	PLAYER_JUMP_LEFT,
	PLAYER_JUMP_RIGHT,
	PLAYER_DUCK_LEFT,
	PLAYER_DUCK_RIGHT,
	PLAYER_TURN_LEFT,
	PLAYER_TURN_RIGHT,
	ENEMY_MOVE_LEFT,
	ENEMY_MOVE_RIGHT
} Animation;

SDL_Rect anim_getRect( Animation anim, int frame )
{
	SDL_Rect rect;
	switch ( anim )
	{
		case PLAYER_IDLE_LEFT: 
			rect.x = 0; rect.y = PLAYER_HEIGHT; 
			rect.w = PLAYER_WIDTH; rect.h = PLAYER_HEIGHT; 
		break;
		case PLAYER_IDLE_RIGHT:
			rect.x = 0; rect.y = 0;
			rect.w = PLAYER_WIDTH; rect.h = PLAYER_HEIGHT;
		break;
		case PLAYER_MOVE_LEFT: 
			rect.x = PLAYER_WIDTH * frame; rect.y = PLAYER_HEIGHT; 
			rect.w = PLAYER_WIDTH; rect.h = PLAYER_HEIGHT; 
		break;
		case PLAYER_MOVE_RIGHT:
			rect.x = PLAYER_WIDTH * frame; rect.y = 0;
			rect.w = PLAYER_WIDTH; rect.h = PLAYER_HEIGHT;
		break;
		case PLAYER_JUMP_LEFT:
			rect.x = PLAYER_WIDTH * 5; rect.y = PLAYER_HEIGHT;
			rect.w = PLAYER_WIDTH; rect.h = PLAYER_HEIGHT;
		break;
		case PLAYER_JUMP_RIGHT:
			rect.x = PLAYER_WIDTH * 5; rect.y = 0;
			rect.w = PLAYER_WIDTH; rect.h = PLAYER_HEIGHT;
		break;
		case PLAYER_DUCK_LEFT:
			rect.x = PLAYER_WIDTH * 6; rect.y = PLAYER_HEIGHT;
			rect.w = PLAYER_WIDTH; rect.h = PLAYER_HEIGHT;
		break;
		case PLAYER_DUCK_RIGHT:
			rect.x = PLAYER_WIDTH * 6; rect.y = 0;
			rect.w = PLAYER_WIDTH; rect.h = PLAYER_HEIGHT;
		break;
		case PLAYER_TURN_LEFT:
		     rect.x = PLAYER_WIDTH * 7; rect.y = 0;
		     rect.w = PLAYER_WIDTH; rect.h = PLAYER_HEIGHT;
		break;
		case PLAYER_TURN_RIGHT:
		     rect.x = PLAYER_WIDTH * 7; rect.y = PLAYER_HEIGHT;
		     rect.w = PLAYER_WIDTH; rect.h = PLAYER_HEIGHT;
		break;
	}
	return rect;
}

/************************************************************/

void cc_update( void )
{
	CoinController *cc = &g_Map->cc;
	SDL_Rect player = rect( g_Player.x, g_Player.y, PLAYER_WIDTH, PLAYER_HEIGHT );

	int i;	
	for ( i = 0; i < cc->size; i++ )
		if ( cc->array[i] != -1 )
		{
			int x, y;
			x = cc->array[i] % ( SCREEN_WIDTH / TILE_WIDTH ) * TILE_WIDTH;
			y = cc->array[i] / ( SCREEN_WIDTH / TILE_WIDTH ) * TILE_HEIGHT;
			
			if ( rect_intersect( player, rect( x, y, TILE_WIDTH, TILE_HEIGHT ) ) )
			{
				cc->array[i] = -1;
				if ( ++g_Player.coins % COINS_PER_LIFE == 0 )
				{
					if ( ++g_Player.lives <= MAX_PLAYER_LIVES )
						playSound( g_sfx1Up );
					else
						g_Player.lives = MAX_PLAYER_LIVES;
				}
				else
					playSound( g_sfxCoin );
			}
		}
}

void cc_draw( void )
{
	CoinController *cc = &g_Map->cc;

	int i;
	for ( i = 0; i < cc->count; i++ )
		if ( cc->array[i] != -1 )
		{
			int x, y;
			x = cc->array[i] % ( SCREEN_WIDTH / TILE_WIDTH ) * TILE_WIDTH;
			y = cc->array[i] / ( SCREEN_WIDTH / TILE_WIDTH ) * TILE_HEIGHT;
			
			SDL_Rect COIN_RECT = map_getTileRect( 7, 1 );
			drawImage( g_imgTileset, &COIN_RECT, x, y );
		}
}

/************************************************************/

int mp_update( MovingPlatform *mp, unsigned deltaTicks )
{	
     SDL_Rect r = rect( mp->x, mp->y, TILE_WIDTH, TILE_HEIGHT );
     int precheck = ( g_Player.onPlatform && ( 
                      rect_contains( r, g_Player.x, g_Player.y + PLAYER_HEIGHT ) ||
	                 rect_contains( r, g_Player.x + HALF_PLAYER_WIDTH, g_Player.y + PLAYER_HEIGHT ) ||
	                 rect_contains( r, g_Player.x + PLAYER_WIDTH, g_Player.y + PLAYER_HEIGHT ) ) );

	int calcX, calcY;
	float inc = PLATFORM_MOVE_SPEED * ( deltaTicks / 1000.0f );
	switch ( mp->dir )
	{
		case UP:
			mp->y += -inc;
			calcX = ( mp->x + ( TILE_WIDTH / 2 ) );
			calcY = mp->y ;
		break;
		case DOWN:
			mp->y += inc;
			calcX = ( mp->x + ( TILE_WIDTH / 2 ) );
			calcY = mp->y + TILE_HEIGHT;
		break;
		case LEFT:
			mp->x += -inc;
			calcX = mp->x;
			calcY = ( mp->y + ( TILE_HEIGHT / 2 ) );
		break;
		case RIGHT:
			mp->x += inc;
			calcX = mp->x + TILE_WIDTH;
			calcY = ( mp->y + ( TILE_HEIGHT / 2 ) );
		break;
	}
	
	if ( map_getTile( calcX / TILE_WIDTH, calcY / TILE_HEIGHT ) == 'd' || map_checkCollision( calcX, calcY ) )
		mp->dir = dir_getOpposite( mp->dir );
		
	if ( g_Player.jump != JUMPING && ( precheck || (
	     rect_contains( r, g_Player.x, g_Player.y + PLAYER_HEIGHT ) ||
	     rect_contains( r, g_Player.x + HALF_PLAYER_WIDTH, g_Player.y + PLAYER_HEIGHT ) ||
	     rect_contains( r, g_Player.x + PLAYER_WIDTH, g_Player.y + PLAYER_HEIGHT ) ) ) )
     {
          if ( mp->dir == LEFT )
               g_Player.x -= inc;
          else if ( mp->dir == RIGHT )
               g_Player.x += inc;
               
          g_Player.y = mp->y - PLAYER_HEIGHT;       
          return 1;
     }
     
     /* 
          there are two bugs with moving block platforms: 
               (1) the player doesn't move horizontally with the platform and 
                    - because of int to float conversion and back, the player doesn't move along with the platform
               (2) when the platform is going down, the player switches between falling and not falling 
     */
     
     return 0;
}

void mp_draw( MovingPlatform *mp )
{
	SDL_Rect PLATFORM_RECT = map_getTileRect( 5, 9 );
	drawImage( g_imgTileset, &PLATFORM_RECT, mp->x, mp->y );
}

void mpc_update( unsigned deltaTicks )
{
	MovingPlatformController *mpc = &g_Map->mpc;

	int i, onPlatform = 0;
	for ( i = 0; i < mpc->count; i++ )
		if ( mpc->array[i] != NULL )
			onPlatform = mp_update( mpc->array[i], deltaTicks ) || onPlatform;
	if ( g_Player.onPlatform = onPlatform )
	{
	     g_Player.jump = CAN_JUMP;
	     g_Player.yVel = 0;
	}
}

void mpc_draw( void )
{
	MovingPlatformController *mpc = &g_Map->mpc;

	int i;
	for ( i = 0; i < mpc->count; i++ )
		if ( mpc->array[i] != NULL )
			mp_draw( mpc->array[i] );
}

/************************************************************/

void player_init( void )
{
	g_Player.x = 0;
	g_Player.y = 0;
	g_Player.xVel = 0;
	g_Player.yVel = 0;
	g_Player.lastDir = RIGHT;
	g_Player.lives = INIT_PLAYER_LIVES;
	g_Player.time = 0;
	g_Player.score = 0;
	g_Player.coins = 0;
	g_Player.dead = 0;
	g_Player.frame = 0;
	g_Player.frameTimer.tick = 0;
	g_Player.frameTimer.interval = 80;
	g_Player.sprite.image = g_imgPlayer;
	g_Player.jump = CAN_JUMP;
	g_Player.onPlatform = 0;
	g_Player.keyPressed[0] = g_Player.keyPressed[1] = g_Player.keyPressed[2] = g_Player.keyPressed[3] = 0;
}

void player_kill( void )
{
#ifndef DISABLE_DEATH
	g_Player.dead = 1;
	Mix_PlayMusic( g_musDeath, 1 );
	timer_reset( &g_utilTimer );
#else
	g_Player.x = ( g_Map->startPos % ( SCREEN_WIDTH / TILE_WIDTH ) ) * TILE_WIDTH;
	g_Player.y = ( g_Map->startPos / ( SCREEN_WIDTH / TILE_WIDTH ) ) * TILE_HEIGHT + ( TILE_HEIGHT * 2 - PLAYER_HEIGHT );
	g_Player.xVel = 0;
	g_Player.yVel = 0;
	g_Player.lastDir = RIGHT;
	
	g_Player.onPlatform = 0;

	mpc_reset( &g_Map->mpc );
#endif
}

void player_reset( void )
{
     g_Player.dead = 0;

     g_Player.x = ( g_Map->startPos % ( SCREEN_WIDTH / TILE_WIDTH ) ) * TILE_WIDTH;
     g_Player.y = ( g_Map->startPos / ( SCREEN_WIDTH / TILE_WIDTH ) ) * TILE_HEIGHT + ( TILE_HEIGHT * 2 - PLAYER_HEIGHT );
     g_Player.xVel = 0;
     g_Player.yVel = 0;
     g_Player.lastDir = RIGHT;
     
     g_Player.onPlatform = 0;
     
     int i;
     for ( i = 0; i < 4; i++ )
          g_Player.keyPressed[i] = 0;
}

void player_update( unsigned deltaTicks )
{
	/* reset the player's position after death music finished */
	if ( g_Player.dead && g_Player.lives >= 0 && !Mix_PlayingMusic() )
	{
		if ( --g_Player.lives >= 0 )
		{
               player_reset();
		
			mpc_reset( &g_Map->mpc );
			
			g_displayLevelText = 1;
			timer_reset( &g_utilTimer );
		}
		else /* no more lives, play game over music */
			Mix_PlayMusic( g_musGameOver, 1 );
	}
	
	/* if player is dead, go no further */
	if ( g_Player.dead || g_Player.lives < 0 ) return;

	/* check if player died */
	if ( SCREEN_HEIGHT < (int) g_Player.y )
	     player_kill();
	
	if ( !g_Player.onPlatform )
	{
	     if ( g_Player.jump == JUMPING )
	     {
		     g_Player.yVel += -PLAYER_JUMP_SPEED;
		     if ( !g_Player.keyPressed[UP] || g_Player.yVel <= -PLAYER_MAX_JUMP_SPEED )
			     g_Player.jump = JUMPED;
	     }
	     else if ( g_Player.jump == JUMPED )
		     g_Player.yVel += PLAYER_FALL_SPEED;
	}
		
	/* set the player's move speed if left or right (but not both) are pressed */
	if ( ( g_Player.keyPressed[LEFT] || g_Player.keyPressed[RIGHT] ) && 
	     !( g_Player.keyPressed[LEFT] && g_Player.keyPressed[RIGHT] ) )
	{
	     g_Player.lastDir = g_Player.keyPressed[RIGHT] ? RIGHT : LEFT;
	     if ( g_Player.lastDir == RIGHT )
	     {
               g_Player.xVel += PLAYER_MOVE_SPEED;
               if ( g_Player.xVel >= PLAYER_MAX_MOVE_SPEED )
                    g_Player.xVel = PLAYER_MAX_MOVE_SPEED;
          }
          else
          {
               g_Player.xVel += -PLAYER_MOVE_SPEED;
               if ( g_Player.xVel <= -PLAYER_MAX_MOVE_SPEED )
                    g_Player.xVel = -PLAYER_MAX_MOVE_SPEED;
          }
     }
	else /* not pressing movement key, revert back to zero */
	{
	     if ( g_Player.lastDir == RIGHT )
	     {
	          g_Player.xVel += -PLAYER_MOVE_SPEED;
	          if ( g_Player.xVel <= 0 )
	               g_Player.xVel = 0;
	     }
	     else
	     {
	          g_Player.xVel += PLAYER_MOVE_SPEED;
	          if ( g_Player.xVel >= 0 )
	               g_Player.xVel = 0;
	     }
	}
	
	/* 
		notes about collision:
		- when player is EXACTLY one tile inbetween two blocks, and tries to navigate, weird things happen
		- left/right could only check if yVel != 0 and then check top and bot
		- if a player is moving fast enough, they can clip through tiles
		- moving block collision does NOT work
		
		for all intents and purposes, it works but it could be improved
		
		suggestions for redoing collision:
		http://www.wildbunny.co.uk/blog/2011/12/14/how-to-make-a-2d-platform-game-part-2-collision-detection/
		http://games.greggman.com/game/programming_m_c__kids/
	*/
	
	if ( !g_Player.onPlatform )
	{
	     if ( g_Player.yVel != 0 )
	     {	
          	int yNew = g_Player.y + g_Player.yVel * ( deltaTicks / 1000.f );
	
	          /* downward tile collision */
	          if ( map_checkCollision( g_Player.x + HALF_PLAYER_WIDTH, yNew + PLAYER_HEIGHT ) || /* check bot-mid, or */
		          map_checkCollision( g_Player.x, yNew + PLAYER_HEIGHT ) || /* bot-left if facing right, or */
		          map_checkCollision( g_Player.x + PLAYER_WIDTH - 1, yNew + PLAYER_HEIGHT ) ) /* bot-right if facing left */
	          {
		          yNew = ( yNew / TILE_HEIGHT ) * TILE_HEIGHT + ( TILE_HEIGHT * 2 - PLAYER_HEIGHT );
		          while ( map_checkCollision( g_Player.x + HALF_PLAYER_WIDTH, yNew ) || map_checkCollision( g_Player.x + HALF_PLAYER_WIDTH, yNew + PLAYER_HEIGHT - 1 ) )
			          yNew -= TILE_HEIGHT;
		          g_Player.yVel = 0;
		          g_Player.jump = CAN_JUMP;
	          }
	
	          /* upward tile collision */
	          else	if ( map_checkCollision( g_Player.x + HALF_PLAYER_WIDTH, yNew ) || /* top-mid */ 
		               map_checkCollision( g_Player.x, yNew ) || /* top-left */ 
		               map_checkCollision( g_Player.x + PLAYER_WIDTH - 1, yNew ) )
	          {
		          yNew = ( ( yNew / TILE_HEIGHT ) + 1 ) * TILE_HEIGHT;
		          g_Player.yVel = 0;
		          g_Player.jump = JUMPED;
	          }
	          
	          g_Player.y = yNew;
	     }
	     /* check if the player fell off the platform */
	     else if ( g_Player.jump == CAN_JUMP &&
	               !map_checkCollision( g_Player.x, g_Player.y + PLAYER_HEIGHT ) &&
	               !map_checkCollision( g_Player.x + PLAYER_WIDTH, g_Player.y + PLAYER_HEIGHT ) )
          {
               g_Player.jump = JUMPED;
          }
     }
	
	if ( g_Player.xVel != 0 )
	{	
	     int xNew = g_Player.x + g_Player.xVel * ( deltaTicks / 1000.f );
	
	     /* leftward tile collision */
	     if ( map_checkCollision( xNew, g_Player.y + HALF_PLAYER_HEIGHT ) || /* check mid-left, or */
		     ( g_Player.yVel < 0 && map_checkCollision( xNew, g_Player.y + PLAYER_HEIGHT ) ) || /* if falling, check bot-left; or */
		     ( g_Player.yVel > 0 && map_checkCollision( xNew, g_Player.y ) ) ) /* if rising, check top-left */
	     {
		     xNew = ( ( xNew / TILE_WIDTH ) + 1 ) * TILE_WIDTH;
		     g_Player.xVel = 0;
	     }
	
	     /* rightward tile collision */
	     else if ( map_checkCollision( xNew + PLAYER_WIDTH, g_Player.y + HALF_PLAYER_HEIGHT ) || /* check mid-right, or */
		     ( g_Player.yVel < 0 && map_checkCollision( xNew + PLAYER_WIDTH, g_Player.y + PLAYER_HEIGHT ) || /* if falling, check bot-right; or */
		     ( g_Player.yVel > 0 && map_checkCollision( xNew + PLAYER_WIDTH, g_Player.y ) ) ) ) /* if rising, check top-right */
	     {
		     xNew = ( xNew / TILE_WIDTH ) * TILE_WIDTH;
		     g_Player.xVel = 0;
	     }
	     
	     g_Player.x = xNew;
	}
	/* if player standing above end and pressed down, change map -- TODO: add a neat effect */
	else if ( g_Player.keyPressed[DOWN] && map_getTile( ( g_Player.x + HALF_PLAYER_WIDTH ) / TILE_WIDTH, ( g_Player.y + PLAYER_HEIGHT ) / TILE_HEIGHT ) == 'E' )
	{
	     map_change();
	     player_reset();
	     return;
     }
	
	/* show the proper animation */
	
	if ( g_Player.yVel != 0 || g_Player.jump != CAN_JUMP ) /* jump / falling animation */
		g_Player.sprite.rect = anim_getRect( g_Player.lastDir == RIGHT ? PLAYER_JUMP_RIGHT : PLAYER_JUMP_LEFT, 0 );
	else if ( g_Player.keyPressed[DOWN] ) /* crouch animation */
          g_Player.sprite.rect = anim_getRect( g_Player.lastDir == RIGHT ? PLAYER_DUCK_RIGHT : PLAYER_DUCK_LEFT, 0 );
	else if ( g_Player.keyPressed[LEFT] || g_Player.keyPressed[RIGHT] ) /* walking animation */
	{
		if ( g_Player.keyPressed[LEFT] && g_Player.xVel > 0 )
		     g_Player.sprite.rect = anim_getRect( PLAYER_TURN_LEFT, 0 );
		else if ( g_Player.keyPressed[RIGHT] && g_Player.xVel < 0 )
		     g_Player.sprite.rect = anim_getRect( PLAYER_TURN_RIGHT, 0 );
		else
     		g_Player.sprite.rect = anim_getRect( g_Player.lastDir == RIGHT ? PLAYER_MOVE_RIGHT : PLAYER_MOVE_LEFT, g_Player.frame );
		
		if ( timer_update( &g_Player.frameTimer ) )
			g_Player.frame = ( g_Player.frame + 1 ) % 5;
	}
	else
	     g_Player.sprite.rect = anim_getRect( g_Player.lastDir == RIGHT ? PLAYER_IDLE_RIGHT : PLAYER_IDLE_LEFT, 0 );
	
	
	/* reposition the player within the bounds of the screen */
	if ( g_Player.x < 0 )
		g_Player.x = 0;
	else if ( g_Player.x + PLAYER_WIDTH > SCREEN_WIDTH )
		g_Player.x = SCREEN_WIDTH - PLAYER_WIDTH;
}

void player_draw( void )
{
	sprite_draw( &g_Player.sprite, (int) g_Player.x, (int) g_Player.y );
}

/************************************************************/

void handleEvent( SDL_Event* event )
{
	/* press any key is visible */
	if ( g_Player.lives < 0 && !Mix_PlayingMusic() && event->type == SDL_KEYDOWN )
	{
		reset();
		return;
	}
		
	/* do not continue if player is dead or displaying level text */
	if ( g_Player.dead || g_displayLevelText ) return;

	Uint8 *keystate = SDL_GetKeyState( NULL );

	if ( event->type == SDL_KEYDOWN )
	{
		switch ( event->key.keysym.sym )
		{
		     case SDLK_UP:
				if ( g_Player.jump == CAN_JUMP )
				{
					g_Player.jump = JUMPING;
					playSound( g_sfxJump );
				}
				g_Player.keyPressed[UP] = 1;
			break;
			case SDLK_DOWN:
				g_Player.keyPressed[DOWN] = 1;
			break;
			case SDLK_RIGHT:
                    g_Player.frame = 1;
                    timer_reset( &g_Player.frameTimer );
                    g_Player.keyPressed[RIGHT] = 1;
			break;
			case SDLK_LEFT:
				g_Player.frame = 1;
				timer_reset( &g_Player.frameTimer );
				g_Player.keyPressed[LEFT] = 1;
			break;
			case SDLK_z:
				map_change(); 
			break;
		}
	}
	else if ( event->type == SDL_KEYUP )
	{
		switch ( event->key.keysym.sym )
		{
			case SDLK_UP:
				g_Player.keyPressed[UP] = 0;
			break;
			case SDLK_DOWN:
			     g_Player.keyPressed[DOWN] = 0;
			break;
			case SDLK_LEFT:
			     g_Player.keyPressed[LEFT] = 0;
			break;
			case SDLK_RIGHT:
			     g_Player.keyPressed[RIGHT] = 0;
			break;
		}
	}
}

void update( unsigned deltaTick )
{
	/* play the music */
	if ( !g_Player.dead && g_Player.lives >= 0 )
	{
		if ( !Mix_PlayingMusic() && Mix_PlayMusic( g_musBGM, -1 ) == -1 ) 
			fprintf( stderr, "Error playing BGM: %s\n", Mix_GetError() );
	}
	
	if ( g_displayLevelText && timer_getElapsedTime( &g_utilTimer ) >= 1000 )
		g_displayLevelText = 0;
	
	if ( !g_displayLevelText )
	{
		int lastScore = g_Player.score;
		int lastCoins = g_Player.coins;
		char str[20];
	
	     mpc_update( deltaTick );
		player_update( deltaTick );
		cc_update();
		
		/* update the dynamic text: coins */
		if ( g_textCoins == NULL || lastCoins != g_Player.coins )
		{
			FreeSurface( g_textCoins );
			sprintf( str, "Coins: %d", g_Player.coins );
			g_textCoins = TTF_RenderText_Solid( g_fontSmall, str, (SDL_Color) { 0xFF, 0xFF, 0xFF } );
		}

		/* update the dynamic text: score */
		if ( g_textScore == NULL || lastScore != g_Player.score )
		{
			FreeSurface( g_textScore );
			sprintf( str, "Score: %d", g_Player.score );
			g_textScore = TTF_RenderText_Solid( g_fontSmall, str, (SDL_Color) { 0xFF, 0xFF, 0xFF } );
		}
	}
}

void draw( void )
{
	int i;
	if ( g_displayLevelText ) /* display the name of the current level */
	{
		drawRect( rect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT ), 0, 0, 0, 255 );
		drawImage( g_textLevel, NULL, ( SCREEN_WIDTH / 2 ) - ( g_textLevel->w / 2 ), ( SCREEN_HEIGHT / 2 ) - ( g_textLevel->h / 2 ) );
	}
	else if ( g_Player.lives >= 0 ) /* draw the game as normal */
	{
		drawImage( g_imgBG, NULL, 0, 0 );	
	
		map_draw(); 		/* draw the map */
		mpc_draw();		/* draw the moving platforms */
		cc_draw();		/* draw the coins */
		player_draw();		/* draw the player */
			
		/* draw the number of lives */
		drawImage( g_textLives, NULL, 5, 5 );
		for ( i = 0; i < g_Player.lives + 1; i++ )
			drawImage( g_imgLives, NULL, i * ( g_imgLives->w + 2 ) + 5, 15 );

		/* draw the player's score count */
		drawImage( g_textScore, NULL, SCREEN_WIDTH - 95, 5 );
		
		/* draw the player's coin count */
		drawImage( g_textCoins, NULL, SCREEN_WIDTH - 95, 15 );
	}
	else /* player ran out of lives */
	{
		drawRect( rect( 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT ), 0, 0, 0, 255 );
		drawImage( g_imgGameOver, NULL, ( SCREEN_WIDTH / 2 ) - ( g_imgGameOver->w / 2 ), ( SCREEN_HEIGHT / 2 ) - ( g_imgGameOver->h / 2 ) );
		
		if ( !Mix_PlayingMusic() )
			drawImage( g_textPressAnyKey, NULL, ( SCREEN_WIDTH / 2 ) - ( g_textPressAnyKey->w / 2 ), ( SCREEN_HEIGHT / 2 ) - ( g_textPressAnyKey->h / 2 ) + 30 );
	}
}

/************************************************************/

int reset( void )
{
	player_init();
	
	/* load first level */
	if ( map_load( "levels/level1" ) != 0 )
		return -1;
		
	g_curLevel = 1;
	FreeSurface( g_textScore );
	FreeSurface( g_textCoins );
		
	return 0;
}

/************************************************************/

int game_init()
{
	/* load images */
	if ( ( g_imgTileset	= loadImage( "images/tiles.bmp" ) ) 	== NULL ||
		( g_imgPlayer 	= loadImage( "images/player.bmp" ) ) 	== NULL ||
		( g_imgEnemy 	= loadImage( "images/enemy.bmp" ) )	== NULL ||
		( g_imgBG 	= loadImage( "images/bg.bmp" ) ) 		== NULL ||
		( g_imgLives 	= loadImage( "images/lives.bmp" ) )	== NULL )
	{
		return -1;
	}
	
	/* load sound */
	if ( ( g_sfxCoin	= loadSound( "audio/sfx-coin.wav" ) )	== NULL ||
		( g_sfxJump	= loadSound( "audio/sfx-jump.wav" ) ) 	== NULL ||
		( g_sfxStomp	= loadSound( "audio/sfx-stomp.wav" ) )	== NULL ||
		( g_sfx1Up	= loadSound( "audio/sfx-1up.wav" ) )	== NULL ||
		( g_musBGM	= loadMusic( "audio/mus-bgm.ogg" ) )	== NULL ||
		( g_musDeath	= loadMusic( "audio/mus-death.wav" ) ) 	== NULL ||
		( g_musGameOver = loadMusic( "audio/mus-gameover.wav" ) ) == NULL )
	{
		return -1;
	}
	
	/* load font / static text */
	SDL_Color color = { 0xFF, 0xFF, 0xFF };
	if ( ( g_fontSmall		= loadFont( "images/font.ttf", 9 ) ) == NULL ||
		( g_fontLarge	 	= loadFont( "images/font.ttf", 16 ) ) == NULL ||
		( g_textLives		= TTF_RenderText_Solid( g_fontSmall, "Lives:", color ) ) == NULL ||
		( g_imgGameOver 	= TTF_RenderText_Solid( g_fontLarge, "GAME OVER", color ) ) == NULL ||
		( g_textPressAnyKey = TTF_RenderText_Solid( g_fontLarge, "Press ANY key to continue", color ) ) == NULL )
	{
		return -1;
	}	
	
	g_textLevel = TTF_RenderText_Solid( g_fontLarge, "Level 1", (SDL_Color) { 0xFF, 0xFF, 0xFF } );
	g_displayLevelText = 1;
	timer_reset( &g_utilTimer );
	
	return 0;
}

void game_cleanup( void )
{
	FreeSurface( g_imgTileset );
	FreeSurface( g_imgPlayer );
	FreeSurface( g_imgEnemy );
	FreeSurface( g_imgBG );
	FreeSurface( g_imgLives );
	
	FreeSurface( g_imgGameOver );
	
	FreeFont( g_fontSmall );
	FreeFont( g_fontLarge );
	
	FreeSurface( g_textLives );
	FreeSurface( g_textScore );
	FreeSurface( g_textCoins );
	FreeSurface( g_textLevel );
	FreeSurface( g_textPressAnyKey );
	
	FreeChunk( g_sfxCoin );
	FreeChunk( g_sfxJump );
	FreeChunk( g_sfxStomp );
	FreeChunk( g_sfx1Up );
	
	FreeMusic( g_musBGM );
	FreeMusic( g_musDeath );
	FreeMusic( g_musGameOver );
	
	map_cleanup( g_Map );
	free( g_Map );
	g_Map = NULL;
}

int game_setState( void )
{	
	if ( reset() == -1 )
		return -1;
	
	g_handleEventsFn 	= &handleEvent;
	g_updateFn 		= &update;
	g_drawFn 			= &draw;
	
	return 0;
}
