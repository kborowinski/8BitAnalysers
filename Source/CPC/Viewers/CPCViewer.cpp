#include "CPCViewer.h"

#include <CodeAnalyser/CodeAnalyser.h>

#include <imgui.h>
#include "../CPCEmu.h"
#include <CodeAnalyser/UI/CodeAnalyserUI.h>

#include <Util/Misc.h>
#include <ImGuiSupport/ImGuiTexture.h>
#include <ImGuiSupport/ImGuiScaling.h>
#include <algorithm>

int CpcKeyFromImGuiKey(ImGuiKey key);

template<typename T> static inline T Clamp(T v, T mn, T mx)
{ 
	return (v < mn) ? mn : (v > mx) ? mx : v; 
}

void FCpcViewer::Init(FCpcEmu* pEmu)
{
	pCpcEmu = pEmu;

	// setup texture
	chips_display_info_t dispInfo = cpc_display_info(&pEmu->CpcEmuState);

	// setup pixel buffer
	int w = dispInfo.frame.dim.width; // 1024
	int h = dispInfo.frame.dim.height; // 312

	const size_t pixelBufferSize = w * h;
	FrameBuffer = new uint32_t[pixelBufferSize * 2];
	ScreenTexture = ImGui_CreateTextureRGBA(FrameBuffer, w, h);

	textureWidth = AM40010_DISPLAY_WIDTH / 2;
	textureHeight = AM40010_DISPLAY_HEIGHT;
}

void FCpcViewer::Draw()
{		
#ifndef NDEBUG
	// debug code to manually iterate through all snaps in a directory
	if (pCpcEmu->GetGamesList().GetNoGames())
	{
		static int gGameIndex = 0;
		bool bLoadSnap = false;
		if (ImGui::Button("Prev snap") || ImGui::IsKeyPressed(ImGuiKey_F1))
		{
			if (gGameIndex > 0)
				gGameIndex--;
			bLoadSnap = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Next snap") || ImGui::IsKeyPressed(ImGuiKey_F2))
		{
			if (gGameIndex < pCpcEmu->GetGamesList().GetNoGames()-1) 
				gGameIndex++;
			bLoadSnap = true;
		}
		ImGui::SameLine();
		const FGameSnapshot& game = pCpcEmu->GetGamesList().GetGame(gGameIndex);
		ImGui::Text("(%d/%d) %s", gGameIndex+1, pCpcEmu->GetGamesList().GetNoGames(), game.DisplayName.c_str());
		if (bLoadSnap)
			pCpcEmu->GetGamesList().LoadGame(gGameIndex);
	}
#endif

	static bool bDrawScreenExtents = true;
	ImGui::Checkbox("Draw screen extents", &bDrawScreenExtents);
	ImGui::SameLine();
	static uint8_t scrRectAlpha = 0xff;
	ImGui::PushItemWidth(40);
	ImGui::InputScalar("Alpha", ImGuiDataType_U8, &scrRectAlpha, NULL, NULL, "%u", ImGuiInputTextFlags_CharsDecimal);
	ImGui::PopItemWidth();
	static bool bShowScreenmodeChanges = false;
	ImGui::Checkbox("Show screenmode changes", &bShowScreenmodeChanges);
#ifndef NDEBUG
	ImGui::Checkbox("Write to screen on click", &bClickWritesToScreen);
#endif

	// see if mixed screen modes are used
	int scrMode = pCpcEmu->CpcEmuState.ga.video.mode;
	for (int s=0; s< AM40010_DISPLAY_HEIGHT; s++)
	{
		if (pCpcEmu->ScreenModePerScanline[s] != scrMode)
		{
			scrMode = -1;
			break;
		}
	}

	// display screen mode and resolution
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	const uint8_t charHeight = GetCharacterHeight();
	const int scrWidth = GetScreenWidth();
	const int scrHeight = GetScreenHeight();
	const int multiplier[4] = {4, 8, 16, 4};
	if(scrMode == -1)
	{
		ImGui::Text("Screen mode: mixed", scrWidth, scrHeight);
		ImGui::Text("Resolution: mixed (%d) x %d", scrWidth, scrHeight);
	}
	else
	{
		ImGui::Text("Screen mode: %d", scrMode);
		ImGui::Text("Resolution: %d x %d", crtc.h_displayed * multiplier[scrMode], scrHeight);
	}

	// draw the cpc display
	chips_display_info_t disp = cpc_display_info(&pCpcEmu->CpcEmuState);

	// convert texture to RGBA
	const uint8_t* pix = (const uint8_t*)disp.frame.buffer.ptr;
	const uint32_t* pal = (const uint32_t*)disp.palette.ptr;
	for (int i = 0; i < disp.frame.buffer.size; i++)
		FrameBuffer[i] = pal[pix[i]];

	ImGui_UpdateTextureRGBA(ScreenTexture, FrameBuffer);

	const static float uv0w = 0.0f;
	const static float uv0h = 0.0f;
	const static float uv1w = (float)AM40010_DISPLAY_WIDTH / (float)AM40010_FRAMEBUFFER_WIDTH;
	static float uv1h = (float)AM40010_DISPLAY_HEIGHT / (float)AM40010_FRAMEBUFFER_HEIGHT;
#if 0
	ImGui::InputScalar("width", ImGuiDataType_Float, &textureWidth, NULL, NULL, "%f", ImGuiInputTextFlags_CharsDecimal); ImGui::SameLine();
	ImGui::InputScalar("height", ImGuiDataType_Float, textureHeight, NULL, NULL, "%f", ImGuiInputTextFlags_CharsDecimal);// ImGui::SameLine();
	ImGui::InputScalar("uv1w", ImGuiDataType_Float, &uv1w, NULL, NULL, "%f", ImGuiInputTextFlags_CharsDecimal); ImGui::SameLine();
	ImGui::InputScalar("uv1h", ImGuiDataType_Float, &uv1h, NULL, NULL, "%f", ImGuiInputTextFlags_CharsDecimal); ImGui::SameLine();
	ImGui::InputScalar("uv0w", ImGuiDataType_Float, &uv0w, NULL, NULL, "%f", ImGuiInputTextFlags_CharsDecimal); ImGui::SameLine();
	ImGui::InputScalar("uv0h", ImGuiDataType_Float, &uv0h, NULL, NULL, "%f", ImGuiInputTextFlags_CharsDecimal);
	
	ImGui::SliderFloat("uv1w", &uv1w, 0.0f, 1.0f);
	ImGui::SliderFloat("uv1h", &uv1h, 0.0f, 1.0f);
	ImGui::SliderFloat("uv0w", &uv0w, 0.0f, 1.0f);
	ImGui::SliderFloat("uv0h", &uv0h, 0.0f, 1.0f);
#endif

	const ImVec2 pos = ImGui::GetCursorScreenPos(); // get the position of the texture
	ImVec2 uv0(uv0w, uv0h);
	ImVec2 uv1(uv1w, uv1h);
	ImGui::Image(ScreenTexture, ImVec2(textureWidth, textureHeight), uv0, uv1);

	// work out the position and size of the logical cpc screen based on the crtc registers.
	// note: these calculations will be wrong if the game sets crtc registers dynamically during the frame.
	// registers not hooked up: R3
	const int scanLinesPerCharOffset = 37 - (8 - (crtc.max_scanline_addr + 1)) * 9; 
	const int hTotOffset = (crtc.h_total - 63) * 8;					// offset based on the default horiz total size (63 chars)
	const int vTotalOffset = (crtc.v_total - 38) * charHeight;		// offset based on the default vertical total size (38 chars)
	const int hSyncOffset = (crtc.h_sync_pos - 46) * 8;				// offset based on the default horiz sync position (46 chars)
	const int vSyncOffset = (crtc.v_sync_pos - 30) * charHeight;	// offset based on the default vert sync position (30 chars)
	const int scrEdgeL = crtc.h_displayed * 8 - hSyncOffset + 32 - scrWidth + hTotOffset;
	const int scrTop = crtc.v_displayed * charHeight - vSyncOffset + scanLinesPerCharOffset - scrHeight + crtc.v_total_adjust + vTotalOffset;
	
	ImDrawList* dl = ImGui::GetWindowDrawList();

	// draw screen area.
	if (bDrawScreenExtents)
	{
		const float y_min = Clamp(pos.y + scrTop, pos.y, pos.y + textureHeight);
		const float y_max = Clamp(pos.y + scrTop + scrHeight, pos.y, pos.y + textureHeight);
		const float x_min = Clamp(pos.x + scrEdgeL, pos.x, pos.x + textureWidth);
		const float x_max = Clamp(pos.x + scrEdgeL + scrWidth, pos.x, pos.x + textureWidth);

		dl->AddRect(ImVec2(x_min, y_min), ImVec2(x_max, y_max), scrRectAlpha << 24 | 0xffffff);
	}

	// colourize scanlines depending on the screen mode
	if (bShowScreenmodeChanges)
	{
		for (int s=0; s< AM40010_DISPLAY_HEIGHT; s++)
		{
			uint8_t scrMode = pCpcEmu->ScreenModePerScanline[s];
			dl->AddLine(ImVec2(pos.x, pos.y + s), ImVec2(pos.x + textureWidth, pos.y + s), scrMode == 0 ? 0x40ffff00 : 0x4000ffff);
		}
	}

	// todo highlight hovered address in code analyser view

	bool bJustSelectedChar = false;
	if (ImGui::IsItemHovered())
	{
		bJustSelectedChar = OnHovered(pos, scrEdgeL, scrTop);
	}

	ImGui::SliderFloat("Speed Scale", &pCpcEmu->ExecSpeedScale, 0.0f, 2.0f);
	ImGui::SameLine();
	if (ImGui::Button("Reset"))
		pCpcEmu->ExecSpeedScale = 1.0f;
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

	bWindowFocused = ImGui::IsWindowFocused();
}

// this needs to be dynamic
static const int kBorderOffsetX = (320 - 256) / 2;
static const int kBorderOffsetY = (256 - 192) / 2;

bool FCpcViewer::OnHovered(const ImVec2& pos, int scrEdgeL, int scrTop)
{
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImGuiIO& io = ImGui::GetIO();
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	const uint8_t charHeight = GetCharacterHeight();
	const int scrWidth = GetScreenWidth();
	const int scrHeight = GetScreenHeight();

	// get mouse cursor coords in logical screen area space
	const int xp = Clamp((int)(io.MousePos.x - pos.x - scrEdgeL), 0, scrWidth - 1);
	const int yp = Clamp((int)(io.MousePos.y - pos.y - scrTop), 0, scrHeight - 1);

	// get the screen mode for the raster line the mouse is pointed at
	// note: the logic doesn't always work and we sometimes end up with a negative scanline.
	// for example, this can happen for demos that set the scanlines-per-character to 1.
	const int scanline = scrTop + yp;
	const int scrMode = scanline > 0 && scanline < AM40010_DISPLAY_HEIGHT ? pCpcEmu->ScreenModePerScanline[scanline] : -1;
	const int charWidth = scrMode == 0 ? 16 : 8; // todo: screen mode 2

	// note: for screen mode 0 this will be in coord space of 320 x 200.
	// not sure that is right?
	uint16_t scrAddress = 0;
	if (pCpcEmu->GetScreenMemoryAddress(xp, yp, scrAddress))
	{
		const int charX = xp & ~(charWidth - 1);
		const int charY = (yp / charHeight) * charHeight;
		const float rx = Clamp(pos.x + scrEdgeL + charX, pos.x, pos.x + textureWidth);
		const float ry = Clamp(pos.y + scrTop + charY, pos.y, pos.y + textureHeight);

		// highlight the current character "square" (could actually be a rectangle if the char height is not 8)
		dl->AddRect(ImVec2(rx, ry), ImVec2((float)rx + charWidth, (float)ry + charHeight), 0xffffffff);

		FCodeAnalysisState& codeAnalysis = pCpcEmu->CodeAnalysis;
		const FAddressRef lastPixWriter = codeAnalysis.GetLastWriterForAddress(scrAddress);

		if (ImGui::IsMouseClicked(0))
		{
#ifndef NDEBUG
			if (bClickWritesToScreen)
			{
				const uint8_t numBytes = GetBitsPerPixel(scrMode);
				uint16_t plotAddress = 0;
				for (int y = 0; y < charHeight; y++)
				{
					if (pCpcEmu->GetScreenMemoryAddress(charX, charY + y, plotAddress))
					{
						for (int b = 0; b < numBytes; b++)
						{
							pCpcEmu->WriteByte(plotAddress + b, 0xff);
						}
					}
				}
			}
#endif			
		}
		ImGui::BeginTooltip();

		// adjust the x position based on the screen mode for the scanline
		const int divisor[4] = { 4, 2, 1, 2 };
		const int x_adj = (xp * 2) / (scrMode == -1 ? 2 : divisor[scrMode]);

		ImGui::Text("Screen Pos (%d,%d)", x_adj, yp);
		ImGui::Text("Addr: %s", NumStr(scrAddress));

		FCodeAnalysisViewState& viewState = codeAnalysis.GetFocussedViewState();
		if (lastPixWriter.IsValid())
		{
			ImGui::Text("Pixel Writer: ");
			ImGui::SameLine();
			DrawCodeAddress(codeAnalysis, viewState, lastPixWriter);
		}

		if (scrMode == -1)
			ImGui::Text("Screen Mode: unknown", scrMode);
		else
			ImGui::Text("Screen Mode: %d", scrMode);
		
		

#if 0
		const float rectSize = 10;
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		const float startPos = pos.x;

		for (int byte = 0; byte < 8; byte++) // need to use char height
		{
			uint16_t pixLineAddress = 0;
			pCpcEmu->GetScreenMemoryAddress(xp & ~0x7, (yp & ~0x7) + byte, pixLineAddress); // // this wont work
			const uint8_t val = pCpcEmu->ReadByte(pixLineAddress);
			//const uint8_t val = pCpcEmu->ReadByte(GetScreenPixMemoryAddress(xp & ~0x7, (yp & ~0x7) + byte)); // this wont work

			for (int bit = 7; bit >= 0; bit--)
			{
				const ImVec2 rectMin(pos.x, pos.y);
				const ImVec2 rectMax(pos.x + rectSize, pos.y + rectSize);
				if (val & (1 << bit))
					dl->AddRectFilled(rectMin, rectMax, 0xffffffff);
				else
					dl->AddRect(rectMin, rectMax, 0xffffffff);

				pos.x += rectSize;
			}

			pos.x = startPos;
			pos.y += rectSize;
		}

		ImGui::Text("");
		ImGui::Text("");
		ImGui::Text("");
		ImGui::Text("");
		ImGui::Text("");
#endif	




		ImGui::EndTooltip();

		if (ImGui::IsMouseDoubleClicked(0))
			viewState.GoToAddress(lastPixWriter);
	}

#if SPECCY
	FCodeAnalysisState& codeAnalysis = pCpcEmu->CodeAnalysis;
	FCodeAnalysisViewState& viewState = codeAnalysis.GetFocussedViewState();

	const float scale = ImGui_GetScaling();
	bool bJustSelectedChar = false;

	ImGuiIO& io = ImGui::GetIO();
	const int xp = std::min(std::max((int)((io.MousePos.x - pos.x) / scale) - kBorderOffsetX, 0), 255); // this needs to look at the screen mode
	const int yp = std::min(std::max((int)((io.MousePos.y - pos.y) / scale) - kBorderOffsetY, 0), 191);

	//const uint16_t scrPixAddress = GetScreenPixMemoryAddress(xp, yp);
	//const uint16_t scrAttrAddress = GetScreenAttrMemoryAddress(xp, yp);
	uint16_t scrAddress = 0;
	pCpcEmu->GetScreenMemoryAddress(xp, yp, scrAddress);

	if (scrAddress != 0)
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const int rx = kBorderOffsetX + (xp & ~0x7);
		const int ry = kBorderOffsetY + (yp & ~0x7);
		dl->AddRect(ImVec2(pos.x + ((float)rx * scale), pos.y + ((float)ry * scale)), ImVec2(pos.x + (float)(rx + 8) * scale, pos.y + (float)(ry + 8) * scale), 0xffffffff);
		ImGui::BeginTooltip();
		ImGui::Text("Screen Pos (%d,%d)", xp, yp);
		ImGui::Text("Pixel: %s", NumStr(scrAddress));

		const FAddressRef lastPixWriter = codeAnalysis.GetLastWriterForAddress(scrAddress);
		//const FAddressRef lastAttrWriter = codeAnalysis.GetLastWriterForAddress(scrAttrAddress);
		if (lastPixWriter.IsValid())
		{
			ImGui::Text("Pixel Writer: ");
			ImGui::SameLine();
			DrawCodeAddress(codeAnalysis, viewState, lastPixWriter);
		}
		/*if (lastAttrWriter.IsValid())
		{
			ImGui::Text("Attribute Writer: ");
			ImGui::SameLine();
			DrawCodeAddress(codeAnalysis, viewState, lastAttrWriter);
		}*/
		{
			//ImGui::Text("Image: ");
			//const float line_height = ImGui::GetTextLineHeight();
			const float rectSize = 10;
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImVec2 pos = ImGui::GetCursorScreenPos();
			const float startPos = pos.x;
			//pos.y -= rectSize + 2;

			for (int byte = 0; byte < 8; byte++) // need to use char height
			{
				uint16_t pixLineAddress = 0;
				pCpcEmu->GetScreenMemoryAddress(xp & ~0x7, (yp & ~0x7) + byte, pixLineAddress); // // this wont work
				const uint8_t val = pCpcEmu->ReadByte(pixLineAddress); 
				//const uint8_t val = pCpcEmu->ReadByte(GetScreenPixMemoryAddress(xp & ~0x7, (yp & ~0x7) + byte)); // this wont work

				for (int bit = 7; bit >= 0; bit--)
				{
					const ImVec2 rectMin(pos.x, pos.y);
					const ImVec2 rectMax(pos.x + rectSize, pos.y + rectSize);
					if (val & (1 << bit))
						dl->AddRectFilled(rectMin, rectMax, 0xffffffff);
					else
						dl->AddRect(rectMin, rectMax, 0xffffffff);

					pos.x += rectSize;
				}

				pos.x = startPos;
				pos.y += rectSize;
			}

			ImGui::Text("");
			ImGui::Text("");
			ImGui::Text("");
			ImGui::Text("");
			ImGui::Text("");
		}
		ImGui::EndTooltip();

		/*if (ImGui::IsMouseClicked(0))
		{
			bScreenCharSelected = true;
			SelectedCharX = rx;
			SelectedCharY = ry;
			SelectPixAddr = scrPixAddress;
			SelectAttrAddr = scrAttrAddress;

			// store pixel data for selected character
			for (int charLine = 0; charLine < 8; charLine++)
				CharData[charLine] = pSpectrumEmu->ReadByte(GetScreenPixMemoryAddress(xp & ~0x7, (yp & ~0x7) + charLine));
			//CharDataFound = codeAnalysis.FindMemoryPatternInPhysicalMemory(CharData, 8, 0, FoundCharDataAddress);
			FoundCharAddresses = codeAnalysis.FindAllMemoryPatterns(CharData, 8, true, false);
			FoundCharIndex = 0;
			bJustSelectedChar = true;
		}

		if (ImGui::IsMouseClicked(1))
		{
			bScreenCharSelected = false;
			bJustSelectedChar = false;
		}*/

		if (ImGui::IsMouseDoubleClicked(0))
			viewState.GoToAddress(lastPixWriter);
	}

	return bJustSelectedChar;
#endif
	return false;
}

uint8_t FCpcViewer::GetCharacterHeight() const
{
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	return crtc.max_scanline_addr + 1;			// crtc register 9 defines how many scanlines in a character square
}

int FCpcViewer::GetScreenWidth() const
{
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	return crtc.h_displayed * 8;
}

int FCpcViewer::GetScreenHeight() const
{
	const mc6845_t& crtc = pCpcEmu->CpcEmuState.crtc;
	return crtc.v_displayed * GetCharacterHeight();
}

void FCpcViewer::Tick(void)
{
	// Check keys - not event driven, hopefully perf isn't too bad
	for (ImGuiKey key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_COUNT; key++)
	{
		if (ImGui::IsKeyPressed(key,false))
		{ 
			if (bWindowFocused)
			{
				int cpcKey = CpcKeyFromImGuiKey(key);
				if (cpcKey != 0)
					cpc_key_down(&pCpcEmu->CpcEmuState, cpcKey);
			}
		}
		else if (ImGui::IsKeyReleased(key))
		{
			const int cpcKey = CpcKeyFromImGuiKey(key);
			if (cpcKey != 0)
				cpc_key_up(&pCpcEmu->CpcEmuState, cpcKey);
		}
	}
}

int CpcKeyFromImGuiKey(ImGuiKey key)
{
	int cpcKey = 0;

	if (key >= ImGuiKey_0 && key <= ImGuiKey_9)
	{
		cpcKey = '0' + (key - ImGuiKey_0);
	}
	else if (key >= ImGuiKey_A && key <= ImGuiKey_Z)
	{
		cpcKey = 'A' + (key - ImGuiKey_A) + 0x20;
	}
	else if (key >= ImGuiKey_Keypad1 && key <= ImGuiKey_Keypad9)
	{
		cpcKey = 0xf1 + (key - ImGuiKey_Keypad1);
	}
	else if (key == ImGuiKey_Keypad0)
	{
		cpcKey = 0xfa;
	}
	else if (key == ImGuiKey_Space)
	{
		cpcKey = ' ';
	}
	else if (key == ImGuiKey_Enter)
	{
		cpcKey = 0xd;
	}
	else if (key == ImGuiKey_Backspace)
	{	
		cpcKey = 0x1;
	}
	else if (key == ImGuiKey_Comma)
	{
		cpcKey = 0x2c;
	}
	else if (key == ImGuiKey_Tab)
	{
		cpcKey = 0x6;
	}
	else if (key == ImGuiKey_Period)
	{
		cpcKey = 0x2e;
	}
	else if (key == ImGuiKey_Semicolon)
	{
		cpcKey = 0x3a;
	}
	else if (key == ImGuiKey_CapsLock)
	{
		cpcKey = 0x7;
	}
	else if (key == ImGuiKey_Escape)
	{
		cpcKey = 0x3;
	}
	else if (key == ImGuiKey_Minus)
	{
		cpcKey = 0x2d;
	}
	else if (key == ImGuiKey_Apostrophe)
	{
		// ; semicolon
		cpcKey = 0x3b;
	}
	else if (key == ImGuiKey_Equal)
	{
		// up arrow with pound sign
		cpcKey = 0x5e;
	}
	else if (key == ImGuiKey_Delete)
	{
		// CLR
		cpcKey = 0xc;
	}
	else if (key == ImGuiKey_Insert)
	{
		// Copy
		cpcKey = 0x5;
	}
	else if (key == ImGuiKey_Slash)
	{
		// forward slash /
		cpcKey = 0x2f;
	}
	else if (key == ImGuiKey_LeftBracket)
	{
		// [
		cpcKey = 0x5b;
	}
	else if (key == ImGuiKey_RightBracket)
	{
		// ]
		cpcKey = 0x5d;
	}
	else if (key == ImGuiKey_Backslash)
	{
		// backslash '\'
		cpcKey = 0x5c;
	}
	else if (key == ImGuiKey_GraveAccent) // `
	{
		// @
		cpcKey = 0x40;
	}
	else if (key == ImGuiKey_LeftArrow)
	{
		cpcKey = 0x8;
	}
	else if (key == ImGuiKey_RightArrow)
	{
		cpcKey = 0x9;
	}
	else if (key == ImGuiKey_UpArrow)
	{
		cpcKey = 0xb;
	}
	else if (key == ImGuiKey_DownArrow)
	{
		cpcKey = 0xa;
	}
	else if (key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift)
	{
		cpcKey = 0xe;
	}
	else if (key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl)
	{
		cpcKey = 0xf;
	}

	return cpcKey;
}