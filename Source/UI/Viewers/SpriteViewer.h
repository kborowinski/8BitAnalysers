#pragma once

#include "imgui.h"
#include <vector>

struct FGraphicsView;
struct FGraphicsViewerState;
class FSpectrumEmu;
struct FGameConfig;

// Sprite Stuff

// sprite definition
struct FSpriteDef
{
	uint16_t	Address;
	int			Width;	// width in chars
	int			Height;	// height in chars
};

struct FSpriteDefList
{
	uint16_t	BaseAddress;
	int			Width;
	int			Height;

	std::vector< FSpriteDef>	Sprites;
};



struct FUISpriteList
{
	int				Selection = 0;
	FSpriteDefList	SpriteList;
};

void GenerateSpriteListsFromConfig(FGraphicsViewerState &state, FGameConfig *pGameConfig);
//void GenerateSpriteList(FSpriteDefList &spriteList, uint16_t startAddress, int count, int width, int height);
void DrawSpriteListGUI(FGraphicsViewerState &state, FGraphicsView *pGraphicsView);