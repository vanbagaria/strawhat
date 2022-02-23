#include "SGE_Graphics.h"
#include "SGE.h"
#include "SGE_Logger.h"

#include <SDL2/SDL_image.h>

#include <stdlib.h>
#include <stdio.h>

/*******************
 * Color constants
 *******************/

const SDL_Color SGE_COLOR_WHITE        = {255, 255, 255, 255};
const SDL_Color SGE_COLOR_BLACK        = {  0,   0,   0, 255};
const SDL_Color SGE_COLOR_GRAY         = { 50,  50,  50, 255};
const SDL_Color SGE_COLOR_RED          = {255,   0,   0, 255};
const SDL_Color SGE_COLOR_GREEN        = {  0, 255,   0, 255};
const SDL_Color SGE_COLOR_BLUE         = {  0,   0, 255, 255};
const SDL_Color SGE_COLOR_YELLOW       = {255, 255,   0, 255};
const SDL_Color SGE_COLOR_PINK         = {255,   0, 255, 255};
const SDL_Color SGE_COLOR_AQUA         = {  0, 255, 255, 255};
const SDL_Color SGE_COLOR_LIGHT_GRAY   = {195, 195, 195, 255};
const SDL_Color SGE_COLOR_LIGHT_PURPLE = {200, 191, 231, 255};
const SDL_Color SGE_COLOR_DARK_RED     = {136,   0,  21, 255};
const SDL_Color SGE_COLOR_CERISE       = {222,  49,  99, 255};
const SDL_Color SGE_COLOR_ORANGE       = {255, 127,   0, 255};
const SDL_Color SGE_COLOR_INDIGO       = { 63,  72, 204, 255};
const SDL_Color SGE_COLOR_PURPLE       = {163,  73, 164, 255};

/***********************
 * Rendering Functions
 ***********************/

void SGE_SetBackgroundColor(SDL_Color color)
{
	SGE_GetEngineData()->defaultScreenClearColor = color;
}

void SGE_ClearScreenRGBA(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	SDL_SetRenderDrawColor(SGE_GetEngineData()->renderer, r, g, b, a);
	SDL_RenderClear(SGE_GetEngineData()->renderer);
}

void SGE_ClearScreen(SDL_Color color)
{
	SDL_SetRenderDrawColor(SGE_GetEngineData()->renderer, color.r, color.g, color.b, color.a);
	SDL_RenderClear(SGE_GetEngineData()->renderer);
}

void SGE_SetDrawColorRGBA(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
	SDL_SetRenderDrawColor(SGE_GetEngineData()->renderer, r, g, b, a);
}

void SGE_SetDrawColor(SDL_Color color)
{
	SDL_SetRenderDrawColor(SGE_GetEngineData()->renderer, color.r, color.g, color.b, color.a);
}

void SGE_DrawRect(SDL_Rect *rect)
{
	SDL_RenderDrawRect(SGE_GetEngineData()->renderer, rect);
}

void SGE_DrawFillRect(SDL_Rect *rect)
{
	SDL_RenderFillRect(SGE_GetEngineData()->renderer, rect);
}

void SGE_DrawLine(int x1, int y1, int x2, int y2)
{
	SDL_RenderDrawLine(SGE_GetEngineData()->renderer, x1, y1, x2, y2);
}

/****************
 * SGE_Texture
 ****************/

/* Background color for Shaded text mode. Default is transparent white. */
static SDL_Color fontBGColor = {255, 255, 255, 1};

/* Word wrap in pixels for Blended text mode. */
static int wordWrap = 500;

void SGE_SetTextureFontBGColor(SDL_Color bgColor)
{
	fontBGColor = bgColor;
}

void SGE_SetTextureWordWrap(int wrap)
{
	wordWrap = wrap;
}

void SGE_EmptyTextureData(SGE_Texture *gTexture)
{
	gTexture->x = 0;
	gTexture->y = 0;
	gTexture->w = 0;
	gTexture->h = 0;
	gTexture->original_w = 0;
	gTexture->original_h = 0;
	
	gTexture->rotation = 0;
	gTexture->flip = SDL_FLIP_NONE;
	gTexture->texture = NULL;
	
	gTexture->destRect.x = 0;
	gTexture->destRect.y = 0;
	gTexture->destRect.w = 0;
	gTexture->destRect.h = 0;
	
	gTexture->clipRect.x = 0;
	gTexture->clipRect.y = 0;
	gTexture->clipRect.w = 0;
	gTexture->clipRect.h = 0;
}

SGE_Texture* SGE_LoadTexture(const char *path)
{
	SGE_Texture *gTexture = (SGE_Texture*)malloc(sizeof(SGE_Texture));
	
	SGE_EmptyTextureData(gTexture);
	
	SDL_Surface *tempSurface = NULL;
	tempSurface = IMG_Load(path);
	if(tempSurface == NULL)
	{
		SGE_LogPrintLine(SGE_LOG_ERROR, "Failed to load image: %s!", path, IMG_GetError());
		SGE_LogPrintLine(SGE_LOG_ERROR, "IMG_Error: %s", IMG_GetError());
		free(gTexture);
		return NULL;
	}
	
	gTexture->w = tempSurface->w;
	gTexture->h = tempSurface->h;
	gTexture->original_w = gTexture->w;
	gTexture->original_h = gTexture->h;
	gTexture->destRect.w = gTexture->w;
	gTexture->destRect.h = gTexture->h;
	gTexture->clipRect.w = gTexture->w;
	gTexture->clipRect.h = gTexture->h;
	
	gTexture->texture = SDL_CreateTextureFromSurface(SGE_GetEngineData()->renderer, tempSurface);
	SDL_FreeSurface(tempSurface);
	if(gTexture->texture == NULL)
	{
		SGE_LogPrintLine(SGE_LOG_ERROR, "Failed to create texture from image!");
		SGE_LogPrintLine(SGE_LOG_ERROR, "SDL_Error: %s", SDL_GetError());
		free(gTexture);
		return NULL;
	}
	
	return gTexture;
}

SGE_Texture* SGE_CreateTextureFromText(const char *text, TTF_Font *font, SDL_Color fg, SGE_TextRenderMode textMode)
{
	SGE_Texture *gTexture = (SGE_Texture*)malloc(sizeof(SGE_Texture));
	
	SGE_EmptyTextureData(gTexture);
	
	SDL_Surface *tempSurface = NULL;
	
	if(textMode == SGE_TEXT_MODE_SOLID)
	{
		tempSurface = TTF_RenderText_Solid(font, text, fg);
	}
	else if(textMode == SGE_TEXT_MODE_SHADED)
	{
		tempSurface = TTF_RenderText_Shaded(font, text, fg, fontBGColor);
	}
	else
	{
		tempSurface = TTF_RenderText_Blended_Wrapped(font, text, fg, wordWrap);
	}
	
	if(tempSurface == NULL)
	{
		SGE_LogPrintLine(SGE_LOG_ERROR, "Failed to render text surface!");
		SGE_LogPrintLine(SGE_LOG_ERROR, "TTF_Error: %s", TTF_GetError());
		free(gTexture);
		return NULL;
	}
	
	gTexture->w = tempSurface->w;
	gTexture->h = tempSurface->h;
	gTexture->original_w = gTexture->w;
	gTexture->original_h = gTexture->h;
	gTexture->destRect.w = gTexture->w;
	gTexture->destRect.h = gTexture->h;
	gTexture->clipRect.w = gTexture->w;
	gTexture->clipRect.h = gTexture->h;
	
	gTexture->texture = SDL_CreateTextureFromSurface(SGE_GetEngineData()->renderer, tempSurface);
	SDL_FreeSurface(tempSurface);
	if(gTexture->texture == NULL)
	{
		SGE_LogPrintLine(SGE_LOG_ERROR, "Failed to create texture from image!");
		SGE_LogPrintLine(SGE_LOG_ERROR, "SDL_Error: %s", SDL_GetError());
		free(gTexture);
		return NULL;
	}
	
	return gTexture;
}

void SGE_UpdateTextureFromText(SGE_Texture *gTexture, const char *text, TTF_Font *font, SDL_Color fg, SGE_TextRenderMode textMode)
{
	SDL_DestroyTexture(gTexture->texture);
	gTexture->texture = NULL;
	
	SDL_Surface *tempSurface = NULL;
	
	if(textMode == SGE_TEXT_MODE_SOLID)
	{
		tempSurface = TTF_RenderText_Solid(font, text, fg);
	}
	else if(textMode == SGE_TEXT_MODE_SHADED)
	{
		tempSurface = TTF_RenderText_Shaded(font, text, fg, fontBGColor);
	}
	else
	{
		tempSurface = TTF_RenderText_Blended_Wrapped(font, text, fg, wordWrap);
	}
	
	if(tempSurface == NULL)
	{
		SGE_LogPrintLine(SGE_LOG_ERROR, "Failed to render text surface!");
		SGE_LogPrintLine(SGE_LOG_ERROR, "TTF_Error: %s", TTF_GetError());
	}
	
	gTexture->w = tempSurface->w;
	gTexture->h = tempSurface->h;
	gTexture->original_w = gTexture->w;
	gTexture->original_h = gTexture->h;
	gTexture->destRect.w = gTexture->w;
	gTexture->destRect.h = gTexture->h;
	gTexture->clipRect.w = gTexture->w;
	gTexture->clipRect.h = gTexture->h;
	
	gTexture->texture = SDL_CreateTextureFromSurface(SGE_GetEngineData()->renderer, tempSurface);
	SDL_FreeSurface(tempSurface);
	if(gTexture->texture == NULL)
	{
		SGE_LogPrintLine(SGE_LOG_ERROR, "Failed to create texture from image!");
		SGE_LogPrintLine(SGE_LOG_ERROR, "SDL_Error: %s", SDL_GetError());
	}
}

void SGE_FreeTexture(SGE_Texture *gTexture)
{
	if(gTexture != NULL)
	{
		SDL_DestroyTexture(gTexture->texture);
		free(gTexture);
	}
	else
	{
		SGE_LogPrintLine(SGE_LOG_WARNING, "Attempt to free NULL texture!");
	}
}

void SGE_RenderTexture(SGE_Texture *gTexture)
{
	gTexture->destRect.x = gTexture->x;
	gTexture->destRect.y = gTexture->y;
	gTexture->destRect.w = gTexture->w;
	gTexture->destRect.h = gTexture->h;
	SDL_RenderCopyEx(SGE_GetEngineData()->renderer, gTexture->texture, &gTexture->clipRect, &gTexture->destRect, gTexture->rotation, NULL, gTexture->flip);
}

void SGE_SetTextureColor(SGE_Texture *gTexture, Uint8 red, Uint8 green, Uint8 blue)
{
	SDL_SetTextureColorMod(gTexture->texture, red, green, blue);
}

void SGE_SetTextureBlendMode(SGE_Texture *gTexture, SDL_BlendMode blending)
{
	SDL_SetTextureBlendMode(gTexture->texture, blending);
}

void SGE_SetTextureAlpha(SGE_Texture *gTexture, Uint8 alpha)
{
	SDL_SetTextureAlphaMod(gTexture->texture, alpha);
}

/**********************
 * SGE_AnimatedSprite
 **********************/

SGE_AnimatedSprite *SGE_CreateAnimatedSprite(const char *path, int nFrames, int fps)
{
	SGE_AnimatedSprite *sprite = (SGE_AnimatedSprite *)malloc(sizeof(SGE_AnimatedSprite));
	sprite->frameCount = nFrames;
	sprite->currentFrame = 0;
	sprite->fps = fps;
	sprite->increment = 1;
	sprite->lastDrawTime = 0;
	
	sprite->texture = SGE_LoadTexture(path);
	if(sprite->texture == NULL)
	{
		SGE_LogPrintLine(SGE_LOG_ERROR, "Failed to load texture for AnimatedSprite!");
		free(sprite);
		return NULL;
	}
	
	sprite->texture->clipRect.w = sprite->texture->original_w / sprite->frameCount;
	sprite->texture->w = sprite->texture->clipRect.w;
	
	sprite->x = sprite->texture->x;
	sprite->y = sprite->texture->y;
	sprite->w = sprite->texture->w;
	sprite->h = sprite->texture->h;
	sprite->rotation = 0;
	sprite->flip = SDL_FLIP_NONE;
	
	return sprite;
}

void SGE_FreeAnimatedSprite(SGE_AnimatedSprite *sprite)
{
	if(sprite != NULL)
	{
		SGE_FreeTexture(sprite->texture);
		free(sprite);
	}
	else
	{
		SGE_LogPrintLine(SGE_LOG_WARNING, "Attempt to free NULL AnimatedSprite!");
	}
}

void SGE_RenderAnimatedSprite(SGE_AnimatedSprite *sprite)
{
	if(!sprite->paused)
	{
		if(SDL_GetTicks() > sprite->lastDrawTime + 1000/sprite->fps)
		{
			sprite->currentFrame += sprite->increment;
			sprite->lastDrawTime = SDL_GetTicks();
		}
	
		sprite->texture->clipRect.x = sprite->currentFrame * sprite->texture->clipRect.w;
		if(sprite->currentFrame < 0)
		{
			sprite->texture->clipRect.x = sprite->texture->original_w - sprite->texture->clipRect.w;
			sprite->currentFrame = sprite->frameCount - 1;
		}
		if(sprite->currentFrame > sprite->frameCount - 1)
		{
			sprite->texture->clipRect.x = 0;
			sprite->currentFrame = 0;
		}
	}
	
	sprite->texture->x = sprite->x;
	sprite->texture->y = sprite->y;
	sprite->texture->w = sprite->w;
	sprite->texture->h = sprite->h;
	sprite->texture->rotation = sprite->rotation;
	sprite->texture->flip = sprite->flip;
	SGE_RenderTexture(sprite->texture);
}

void SGE_RestartAnimatedSprite(SGE_AnimatedSprite *sprite, int frame)
{
	sprite->currentFrame = frame - 1;
	sprite->lastDrawTime = SDL_GetTicks();
}

void SGE_SetAnimatedSpriteFPS(SGE_AnimatedSprite *sprite, int fps)
{
	if(fps == 0)
	{
		sprite->paused = true;
		return;
	}

	if(fps < 0)
	{
		if(sprite->increment > 0)
		{
			sprite->increment = -sprite->increment;
		}
	}
	else
	{
		sprite->increment = SDL_abs(sprite->increment);
	}	
	sprite->fps = SDL_abs(fps);
}

/*
 * Give this function the no of frames and the path to the folder containing seperate images
 * with names starting from 1.png, 2.png, ... [nFrames].png
 * It will save atlas.png in the same folder.
*/
void SGE_CreateSpriteSheet(const char *folderPath, int nFrames)
{
	char path[100];
	sprintf(path, folderPath);
	strcat(path, "/1.png");
	
	SDL_Surface *frame = IMG_Load(path);
	SDL_Surface *atlasSurface = SDL_CreateRGBSurface(0, nFrames * frame->w, frame->h, 32, 0, 0, 0, 0);
	SDL_Rect dest = {0, 0, frame->w, frame->h};
	
	int i = 0;
	for(i = 0; i < nFrames; i++)
	{
		sprintf(path, "%s/%d.png", folderPath, i + 1);
		frame = IMG_Load(path);
		dest.x = i * 51;
		SDL_BlitSurface(frame, NULL, atlasSurface, &dest);
		SDL_FreeSurface(frame);
	}
	
	sprintf(path, folderPath);
	strcat(path, "/atlas.png");
	IMG_SavePNG(atlasSurface, path);
	SDL_FreeSurface(atlasSurface);
}
