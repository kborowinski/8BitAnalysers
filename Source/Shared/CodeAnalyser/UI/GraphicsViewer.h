#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <CodeAnalyser/CodeAnalyserTypes.h>

class FGraphicsView;
class FCodeAnalysisState;
struct FCodeAnalysisPage;

enum class GraphicsViewMode : int
{
	CharacterBitmap,		// 8x8 bitmap graphics
	CharacterBitmapWinding,	// winding bitmap (0,0) (1,0) (1,1) (0,1)

	Count
};

struct FGraphicsSet
{
	std::string	Name;
	FAddressRef	Address;	// start address of images
	int			XSizePixels;	// width in pixels
	int			YSizePixels;	// height in pixels
	int			Count;	// number of images
};

// Graphics Viewer
class FGraphicsViewer
{
public:
	bool			Init(FCodeAnalysisState* pCodeAnalysis);
	void			Shutdown(void);

	void			GoToAddress(FAddressRef address);

	void			Draw();

	bool			SaveGraphicsSets(const char* pFName);
	bool			LoadGraphicsSets(const char* pFName);

	// protected methods
protected:
	FCodeAnalysisState& GetCodeAnalysis() { return *pCodeAnalysis; }
	const FCodeAnalysisState& GetCodeAnalysis() const { return *pCodeAnalysis; }
	void			DrawCharacterGraphicsViewer(void);
	virtual void	DrawScreenViewer(void) = 0;

	uint16_t		GetAddressOffsetFromPositionInView(int x, int y) const;

	void			DrawMemoryBankAsGraphicsColumn(int16_t bankId, uint16_t memAddr, int xPos, int columnWidth);
	void			UpdateCharacterGraphicsViewerImage(void); // make virtual for other platforms?

	// protected Members
protected:
	int				ScreenWidth = 0;
	int				ScreenHeight = 0;
	bool			bShowPhysicalMemory = true;
	int32_t			Bank = -1;
	uint16_t		AddressOffset = 0;	// offset to view from the start of the region (bank or physical address space)
	uint32_t		MemorySize = 0x10000;	// size of area being viewed
	FAddressRef		ClickedAddress;
	GraphicsViewMode	ViewMode = GraphicsViewMode::CharacterBitmap;
	int				ViewScale = 1;
	int				HeatmapThreshold = 4;

	int				XSizePixels = 8;			// Image X Size in pixels
	int				YSizePixels = 8;			// Image Y Size in pixels
	int				ImageCount = 0;	// how many images?
	bool			YSizePixelsFineCtrl = false;

	std::string		ImageSetName;

	std::map<FAddressRef, FGraphicsSet>		GraphicsSets;

	// housekeeping
	FCodeAnalysisState* pCodeAnalysis = nullptr;
	FGraphicsView* pGraphicsView = nullptr;
	FGraphicsView* pScreenView = nullptr;
};

uint32_t GetHeatmapColourForMemoryAddress(const FCodeAnalysisPage& page, uint16_t addr, int currentFrameNo, int frameThreshold);