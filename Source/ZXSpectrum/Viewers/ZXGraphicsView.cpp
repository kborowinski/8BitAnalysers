#include "ZXGraphicsView.h"

// speccy colour CLUT
const uint32_t FZXGraphicsView::ColourLUT[8] =
{
	0xFF000000,     // 0 - black
	0xFFFF0000,     // 1 - blue
	0xFF0000FF,     // 2 - red
	0xFFFF00FF,     // 3 - magenta
	0xFF00FF00,     // 4 - green
	0xFFFFFF00,     // 5 - cyan
	0xFF00FFFF,     // 6 - yellow
	0xFFFFFFFF,     // 7 - white
};

uint32_t FZXGraphicsView::GetColFromAttr(uint8_t colBits, bool bBright)
{
	const uint32_t outCol = ColourLUT[colBits & 7];
	if (bBright == false)
		return outCol & 0xFFD7D7D7;
	else
		return outCol;
}

void FZXGraphicsView::DrawCharLine(uint8_t charLine, int xp, int yp, uint8_t colAttr)
{
	const bool bBright = !!(colAttr & (1 << 6));
	const uint32_t inkCol = GetColFromAttr(colAttr & 7, bBright);
	const uint32_t paperCol = GetColFromAttr(colAttr >> 3, bBright);
	FGraphicsView::DrawCharLine(charLine, xp, yp, inkCol, paperCol);
}

void FZXGraphicsView::DrawBitImage(const uint8_t* pSrc, int xp, int yp, int widthChars, int heightChars, uint8_t colAttr)
{
	const bool bBright = !!(colAttr & (1 << 6));
	const uint32_t cols[] = { GetColFromAttr(colAttr >> 3, bBright), GetColFromAttr(colAttr & 7, bBright) };
	FGraphicsView::Draw1BppImageAt(pSrc, xp, yp, widthChars * 8, heightChars * 8, cols);
}

void FZXGraphicsView::DrawBitImageChars(const uint8_t* pSrc, int xp, int yp, int widthChars, int heightChars, uint8_t colAttr)
{
	const bool bBright = !!(colAttr & (1 << 6));
	const uint32_t cols[] = { GetColFromAttr(colAttr >> 3, bBright), GetColFromAttr(colAttr & 7, bBright) };
	FGraphicsView::Draw1BppImageFromCharsAt(pSrc, xp, yp, widthChars, heightChars, cols);
}
