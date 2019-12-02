
#include "SpeccyUI.h"
#include <windows.h>


#include "imgui_impl_lucidextra.h"
#include "GameViewers/StarquakeViewer.h"
#include "GameViewers/MiscGameViewers.h"
#include "GraphicsView.h"
#include "Util/FileUtil.h"

#include "ui/ui_dbg.h"

/* reboot callback */
static void boot_cb(zx_t* sys, zx_type_t type)
{
	zx_desc_t desc = {}; // TODO
	zx_init(sys, &desc);
}

void* gfx_create_texture(int w, int h)
{
	return ImGui_ImplDX11_CreateTextureRGBA(nullptr, w, h);
}

void gfx_update_texture(void* h, void* data, int data_byte_size)
{
	ImGui_ImplDX11_UpdateTextureRGBA(h, (unsigned char *)data);
}

void gfx_destroy_texture(void* h)
{
	
}

static uint8_t DasmCB(void* user_data)
{
	FSpeccyUI *pUI = (FSpeccyUI *)user_data;
	uint8_t val = ReadySpeccyByte(*pUI->pSpeccy, pUI->dasmCurr++);
	return val;
}

static inline uint16_t DisasmLen(FSpeccyUI *pUI, uint16_t pc)
{
	pUI->dasmCurr = pc;
	uint16_t next_pc = z80dasm_op(pc, DasmCB, 0, pUI);
	return next_pc - pc;
}

int UITrapCallback(uint16_t pc, int ticks, uint64_t pins, void* user_data)
{
	FSpeccyUI *pUI = (FSpeccyUI *)user_data;
	const uint16_t addr = Z80_GET_ADDR(pins);
	const bool bRead = (pins & Z80_CTRL_MASK) == (Z80_MREQ | Z80_RD);
	const bool bWrite = (pins & Z80_CTRL_MASK) == (Z80_MREQ | Z80_WR);

	// increment counters
	pUI->MemStats.ExecCount[pc]++;
	const int op_len = DisasmLen(pUI, pc);
	for (int i = 1; i < op_len; i++) {
		pUI->MemStats.ExecCount[(pc + i) & 0xFFFF]++;
	}

	if (bRead)
		pUI->MemStats.ReadCount[addr]++;
	if (bWrite)
		pUI->MemStats.WriteCount[addr]++;

	assert(!(bRead == true && bWrite == true));

	// See if we can find a handler
	for(auto& handler : pUI->MemoryAccessHandlers)
	{
		if (handler.bEnabled == false)
			continue;

		bool bCallHandler = false;
		
		if(handler.Type == MemoryAccessType::Execute)	// Execution
		{
			if (pc >= handler.MemStart && pc <= handler.MemEnd)
				bCallHandler = true;
		}
		else // Memory access
		{
			if (addr >= handler.MemStart && addr <= handler.MemEnd)
			{
				bool bExecute = false;
				
				if (handler.Type == MemoryAccessType::Read && bRead)
					bCallHandler = true;
				else if (handler.Type == MemoryAccessType::Write && bWrite)
					bCallHandler = true;
			}
		}

		if(bCallHandler)
		{
			// update handler stats
			handler.TotalCount++;
			handler.CallerCounts[pc]++;
			handler.AddressCounts[addr]++;
			if(handler.pHandlerFunction!=nullptr)
				handler.pHandlerFunction(handler, pUI->pActiveGame, pc, pins);

			if (handler.bBreak)
				return UI_DBG_STEP_TRAPID;
		}
	}

	return 0;
}

void AddMemoryHandler(FSpeccyUI *pUI, const FMemoryAccessHandler &handler)
{
	pUI->MemoryAccessHandlers.push_back(handler);
}

// TODO: go through memory and identify which areas are code & data


MemoryUse DetermineAddressMemoryUse(const FMemoryStats &memStats, uint16_t addr, bool &smc)
{
	const bool bCode = memStats.ExecCount[addr] > 0;
	const bool bData = memStats.ReadCount[addr] > 0 || memStats.WriteCount[addr] > 0;

	if (bCode && memStats.WriteCount[addr] > 0)
	{
		smc = true;
	}
	
	if (bCode)
		return MemoryUse::Code;
	if(bData)
		return MemoryUse::Data;

	return MemoryUse::Unknown;
}

void AnalyseMemory(FMemoryStats &memStats)
{
	FMemoryBlock currentBlock;
	bool bSelfModifiedCode = false;

	memStats.MemoryBlockInfo.clear();	// Clear old list
	memStats.CodeAndDataList.clear();
	
	currentBlock.StartAddress = 0;
	currentBlock.Use = DetermineAddressMemoryUse(memStats, 0, bSelfModifiedCode);
	if (bSelfModifiedCode)
		memStats.CodeAndDataList.push_back(0);
	
	for(int addr = 1; addr < 65536;addr++)
	{
		bSelfModifiedCode = false;
		const MemoryUse addrUse = DetermineAddressMemoryUse(memStats, addr, bSelfModifiedCode);
		if (bSelfModifiedCode)
			memStats.CodeAndDataList.push_back(addr);
		if(addrUse != currentBlock.Use)
		{
			currentBlock.EndAddress = addr - 1;
			memStats.MemoryBlockInfo.push_back(currentBlock);

			// start new block
			currentBlock.StartAddress = addr;
			currentBlock.Use = addrUse;
		}
	}

	// finish off last block
	currentBlock.EndAddress = 0xffff;
	memStats.MemoryBlockInfo.push_back(currentBlock);
}

void ResetMemoryStats(FMemoryStats &memStats)
{
	memStats.MemoryBlockInfo.clear();	// Clear list
	// 
	// reset counters
	memset(memStats.ExecCount, 0, sizeof(memStats.ExecCount));
	memset(memStats.ReadCount, 0, sizeof(memStats.ReadCount));
	memset(memStats.WriteCount, 0, sizeof(memStats.WriteCount));
}

FSpeccyUI* InitSpeccyUI(FSpeccy *pSpeccy)
{
	FSpeccyUI *pUI = new FSpeccyUI;
	memset(&pUI->UIZX, 0, sizeof(ui_zx_t));

	// Trap callback needs to be set before we create the UI
	z80_trap_cb(&pSpeccy->CurrentState.cpu, UITrapCallback, pUI);

	pUI->pSpeccy = pSpeccy;
	//ui_init(zxui_draw);
	ui_zx_desc_t desc = { 0 };
	desc.zx = &pSpeccy->CurrentState;
	desc.boot_cb = boot_cb;
	desc.create_texture_cb = gfx_create_texture;
	desc.update_texture_cb = gfx_update_texture;
	desc.destroy_texture_cb = gfx_destroy_texture;
	desc.dbg_keys.break_keycode = ImGui::GetKeyIndex(ImGuiKey_Space);
	desc.dbg_keys.break_name = "F5";
	desc.dbg_keys.continue_keycode = VK_F5;
	desc.dbg_keys.continue_name = "F5";
	desc.dbg_keys.step_over_keycode = VK_F6;
	desc.dbg_keys.step_over_name = "F6";
	desc.dbg_keys.step_into_keycode = VK_F7;
	desc.dbg_keys.step_into_name = "F7";
	desc.dbg_keys.toggle_breakpoint_keycode = VK_F9;
	desc.dbg_keys.toggle_breakpoint_name = "F9";
	ui_zx_init(&pUI->UIZX, &desc);

	// setup pixel buffer
	pUI->pGraphicsViewerView = CreateGraphicsView(64, 64);
	/*const int graphicsViewSize = 64;
	const size_t pixelBufferSize = graphicsViewSize * graphicsViewSize * 4;
	pUI->GraphicsViewPixelBuffer = new unsigned char[pixelBufferSize];

	pUI->GraphicsViewTexture = ImGui_ImplDX11_CreateTextureRGBA(pUI->GraphicsViewPixelBuffer, graphicsViewSize, graphicsViewSize);
	*/
	// register Viewers
	RegisterStarquakeViewer(pUI);
	RegisterGames(pUI);
	
	return pUI;
}

void ShutdownSpeccyUI(FSpeccyUI* pUI)
{

}

void StartGame(FSpeccyUI* pUI, FGameConfig *pGameConfig)
{
	pUI->MemoryAccessHandlers.clear();	// remove old memory handlers
	pUI->pActiveGame = new FGame;
	pUI->pActiveGame->pConfig = pGameConfig;
	pUI->pActiveGame->pViewerData = pGameConfig->pInitFunction(pUI, pGameConfig);

	ResetMemoryStats(pUI->MemStats);
	
}

static void DrawMainMenu(FSpeccyUI* pUI, double timeMS)
{
	ui_zx_t* pZXUI = &pUI->UIZX;
	FSpeccy *pSpeccy = pUI->pSpeccy;
	assert(pZXUI && pZXUI->zx && pZXUI->boot_cb);
	
	if (ImGui::BeginMainMenuBar()) 
	{
		if (ImGui::BeginMenu("File"))
		{
			
			if (ImGui::BeginMenu( "Open Game"))
			{
				for (const auto& pGameConfig : pUI->GameConfigs)
				{
					if (ImGui::MenuItem(pGameConfig->Name.c_str()))
					{
						if(LoadZ80File(*pSpeccy, pGameConfig->Z80file.c_str()))
						{
							StartGame(pUI,pGameConfig);
						}
					}
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Open Z80 File"))
			{
				for (const auto& game : GetGameList())
				{
					if (ImGui::MenuItem(game.c_str()))
					{
						if (LoadZ80File(*pSpeccy, game.c_str()))
						{
							pUI->pActiveGame = nullptr;
						}
					}
				}

				ImGui::EndMenu();
			}

			if (ImGui::MenuItem("Export Binary File"))
			{
				if (pUI->pActiveGame != nullptr)
				{
					EnsureDirectoryExists("OutputBin/");
					std::string outBinFname = "OutputBin/" + pUI->pActiveGame->pConfig->Name + ".bin";
					uint8_t *pSpecMem = new uint8_t[65536];
					for (int i = 0; i < 65536; i++)
						pSpecMem[i] = ReadySpeccyByte(*pSpeccy, i);
					SaveBinaryFile(outBinFname.c_str(), pSpecMem, 65536);
					delete pSpecMem;
				}
			}

			if (ImGui::MenuItem("Export Region Info File"))
			{
			}
			ImGui::EndMenu();
		}
		
		if (ImGui::BeginMenu("System")) 
		{
			if (ImGui::MenuItem("Reset")) 
			{
				zx_reset(pZXUI->zx);
				ui_dbg_reset(&pZXUI->dbg);
			}
			if (ImGui::MenuItem("ZX Spectrum 48K", 0, (pZXUI->zx->type == ZX_TYPE_48K)))
			{
				pZXUI->boot_cb(pZXUI->zx, ZX_TYPE_48K);
				ui_dbg_reboot(&pZXUI->dbg);
			}
			if (ImGui::MenuItem("ZX Spectrum 128", 0, (pZXUI->zx->type == ZX_TYPE_128)))
			{
				pZXUI->boot_cb(pZXUI->zx, ZX_TYPE_128);
				ui_dbg_reboot(&pZXUI->dbg);
			}
			if (ImGui::BeginMenu("Joystick")) 
			{
				if (ImGui::MenuItem("None", 0, (pZXUI->zx->joystick_type == ZX_JOYSTICKTYPE_NONE)))
				{
					pZXUI->zx->joystick_type = ZX_JOYSTICKTYPE_NONE;
				}
				if (ImGui::MenuItem("Kempston", 0, (pZXUI->zx->joystick_type == ZX_JOYSTICKTYPE_KEMPSTON)))
				{
					pZXUI->zx->joystick_type = ZX_JOYSTICKTYPE_KEMPSTON;
				}
				if (ImGui::MenuItem("Sinclair #1", 0, (pZXUI->zx->joystick_type == ZX_JOYSTICKTYPE_SINCLAIR_1)))
				{
					pZXUI->zx->joystick_type = ZX_JOYSTICKTYPE_SINCLAIR_1;
				}
				if (ImGui::MenuItem("Sinclair #2", 0, (pZXUI->zx->joystick_type == ZX_JOYSTICKTYPE_SINCLAIR_2)))
				{
					pZXUI->zx->joystick_type = ZX_JOYSTICKTYPE_SINCLAIR_2;
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Hardware")) 
		{
			ImGui::MenuItem("Memory Map", 0, &pZXUI->memmap.open);
			ImGui::MenuItem("Keyboard Matrix", 0, &pZXUI->kbd.open);
			ImGui::MenuItem("Audio Output", 0, &pZXUI->audio.open);
			ImGui::MenuItem("Z80 CPU", 0, &pZXUI->cpu.open);
			if (pZXUI->zx->type == ZX_TYPE_128)
			{
				ImGui::MenuItem("AY-3-8912", 0, &pZXUI->ay.open);
			}
			else 
			{
				pZXUI->ay.open = false;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Debug")) 
		{
			ImGui::MenuItem("CPU Debugger", 0, &pZXUI->dbg.ui.open);
			ImGui::MenuItem("Breakpoints", 0, &pZXUI->dbg.ui.show_breakpoints);
			ImGui::MenuItem("Memory Heatmap", 0, &pZXUI->dbg.ui.show_heatmap);
			if (ImGui::BeginMenu("Memory Editor")) 
			{
				ImGui::MenuItem("Window #1", 0, &pZXUI->memedit[0].open);
				ImGui::MenuItem("Window #2", 0, &pZXUI->memedit[1].open);
				ImGui::MenuItem("Window #3", 0, &pZXUI->memedit[2].open);
				ImGui::MenuItem("Window #4", 0, &pZXUI->memedit[3].open);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Disassembler")) 
			{
				ImGui::MenuItem("Window #1", 0, &pZXUI->dasm[0].open);
				ImGui::MenuItem("Window #2", 0, &pZXUI->dasm[1].open);
				ImGui::MenuItem("Window #3", 0, &pZXUI->dasm[2].open);
				ImGui::MenuItem("Window #4", 0, &pZXUI->dasm[3].open);
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		/*if (ImGui::BeginMenu("Game Viewers"))
		{
			for (auto &viewerIt : pUI->GameViewers)
			{
				FGameViewer &viewer = viewerIt.second;
				ImGui::MenuItem(viewerIt.first.c_str(), 0, &viewer.bOpen);
			}
			ImGui::EndMenu();
		}*/
		
		ui_util_options_menu(timeMS, pZXUI->dbg.dbg.stopped);

		ImGui::EndMainMenuBar();
	}

}

static void UpdateMemmap(ui_zx_t* ui)
{
	assert(ui && ui->zx);
	ui_memmap_reset(&ui->memmap);
	if (ZX_TYPE_48K == ui->zx->type) 
	{
		ui_memmap_layer(&ui->memmap, "System");
		ui_memmap_region(&ui->memmap, "ROM", 0x0000, 0x4000, true);
		ui_memmap_region(&ui->memmap, "RAM", 0x4000, 0xC000, true);
	}
	else 
	{
		const uint8_t m = ui->zx->last_mem_config;
		ui_memmap_layer(&ui->memmap, "Layer 0");
		ui_memmap_region(&ui->memmap, "ZX128 ROM", 0x0000, 0x4000, !(m & (1 << 4)));
		ui_memmap_region(&ui->memmap, "RAM 5", 0x4000, 0x4000, true);
		ui_memmap_region(&ui->memmap, "RAM 2", 0x8000, 0x4000, true);
		ui_memmap_region(&ui->memmap, "RAM 0", 0xC000, 0x4000, 0 == (m & 7));
		ui_memmap_layer(&ui->memmap, "Layer 1");
		ui_memmap_region(&ui->memmap, "ZX48K ROM", 0x0000, 0x4000, 0 != (m & (1 << 4)));
		ui_memmap_region(&ui->memmap, "RAM 1", 0xC000, 0x4000, 1 == (m & 7));
		ui_memmap_layer(&ui->memmap, "Layer 2");
		ui_memmap_region(&ui->memmap, "RAM 2", 0xC000, 0x4000, 2 == (m & 7));
		ui_memmap_layer(&ui->memmap, "Layer 3");
		ui_memmap_region(&ui->memmap, "RAM 3", 0xC000, 0x4000, 3 == (m & 7));
		ui_memmap_layer(&ui->memmap, "Layer 4");
		ui_memmap_region(&ui->memmap, "RAM 4", 0xC000, 0x4000, 4 == (m & 7));
		ui_memmap_layer(&ui->memmap, "Layer 5");
		ui_memmap_region(&ui->memmap, "RAM 5", 0xC000, 0x4000, 5 == (m & 7));
		ui_memmap_layer(&ui->memmap, "Layer 6");
		ui_memmap_region(&ui->memmap, "RAM 6", 0xC000, 0x4000, 6 == (m & 7));
		ui_memmap_layer(&ui->memmap, "Layer 7");
		ui_memmap_region(&ui->memmap, "RAM 7", 0xC000, 0x4000, 7 == (m & 7));
	}
}

void DrawDebuggerUI(ui_dbg_t *pDebugger)
{
	ui_dbg_draw(pDebugger);
	/*
	if (!(pDebugger->ui.open || pDebugger->ui.show_heatmap || pDebugger->ui.show_breakpoints)) {
		return;
	}
	_ui_dbg_dbgwin_draw(pDebugger);
	_ui_dbg_heatmap_draw(pDebugger);
	_ui_dbg_bp_draw(pDebugger);*/
}

void UpdatePreTickSpeccyUI(FSpeccyUI* pUI)
{
	pUI->pSpeccy->ExecThisFrame = ui_zx_before_exec(&pUI->UIZX);
}

static const uint32_t g_kColourLUT[8]=
{
	0xFF000000,     // black
	0xFFFF0000,     // blue
	0xFF0000FF,     // red
	0xFFFF00FF,     // magenta
	0xFF00FF00,     // green
	0xFFFFFF00,     // cyan
	0xFF00FFFF,     // yellow
	0xFFFFFFFF,     // white
};

// coords are in pixel units
// w & h in characters
void PlotImageAt(const uint8_t *pSrc, int xp,int yp,int w,int h,uint32_t *pDest, int destWidth, uint8_t colAttr)
{
	uint32_t* pBase = pDest + (xp + (yp * destWidth));
	uint32_t inkCol = g_kColourLUT[colAttr & 7];
	uint32_t paperCol = g_kColourLUT[(colAttr>>3) & 7];

	if (0 == (colAttr & (1 << 6))) 
	{
		// standard brightness
		inkCol &= 0xFFD7D7D7;
		paperCol &= 0xFFD7D7D7;
	}
	
	*pBase = 0;
	for(int y=0;y<h*8;y++)
	{
		for (int x = 0; x < w; x++)
		{
			const uint8_t charLine = *pSrc++;

			for (int xpix = 0; xpix < 8; xpix++)
			{
				const bool bSet = (charLine & (1 << (7 - xpix))) != 0;
				const uint32_t col = bSet ? inkCol : paperCol;
				*(pBase + xpix + (x * 8)) = col;
			}
		}

		pBase += destWidth;
	}
}

/*void PlotCharacterBlockAt(const FSpeccy *pSpeccy,uint16_t addr, int xp, int yp,int w,int h, uint32_t *pDest, int destWidth)
{
	uint16_t currAddr = addr;

	for (int y = yp; y < yp + h; y++)
	{
		for (int x = xp; x < xp + w; x++)
		{
			const uint8_t *pChar = GetSpeccyMemPtr(*pSpeccy, currAddr);
			PlotImageAt(pChar, x, y,1,1 pDest, destWidth);
			currAddr += 8;
		}
	}
}*/

void DrawGraphicsView(FSpeccyUI* pUI)
{
	FGraphicsView *pGraphicsView = pUI->pGraphicsViewerView;
	static int memOffset = 0;
	static int xs = 1;
	static int ys = 1;

	int byteOff = 0;
	int offsetMax = 0xffff - (64 * 8);
	
	ImGui::Begin("Graphics View");
	ImGui::Text("Memory Map Address: 0x%x", memOffset);
	DrawGraphicsView(*pGraphicsView);
	ImGui::SameLine();
	ImGui::VSliderInt("##int", ImVec2(64, 256), &memOffset, 0, offsetMax);//,"0x%x");
	ImGui::InputInt("Address", &memOffset,1,8, ImGuiInputTextFlags_CharsHexadecimal);
	if (ImGui::Button("<<"))
		memOffset -= xs * ys * 8;
	ImGui::SameLine();
	if (ImGui::Button(">>"))
		memOffset += xs * ys * 8;

	ClearGraphicsView(*pGraphicsView, 0xff000000);

	// view 1 - straight character
	// draw 64 * 8 bytes
	ImGui::InputInt("XSize", &xs, 1, 4);
	ImGui::InputInt("YSize", &ys, 1, 4);

	static char configName[64];
	ImGui::Separator();
	ImGui::InputText("Config Name", configName, 64);
	ImGui::SameLine();
	if (ImGui::Button("Store"))
	{
		// TODO: store this in the config map
	}

	xs = min(max(1, xs), 8);
	ys = min(max(1, ys), 8);
	
	const int xcount = 8 / xs;
	const int ycount = 8 / ys;

	uint16_t speccyAddr = memOffset;
	int y = 0;
	for (int y = 0; y < ycount; y++)
	{
		for (int x = 0; x < xcount; x++)
		{
			const uint8_t *pImage = GetSpeccyMemPtr(*pUI->pSpeccy, speccyAddr);
			PlotImageAt(pImage, x * xs * 8, y * ys * 8, xs, ys, (uint32_t*)pGraphicsView->PixelBuffer, 64);
			speccyAddr += xs * ys * 8;
		}
	}

	ImGui::End();
}

void DrawMemoryHandlers(FSpeccyUI* pUI)
{
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
	ImGui::BeginChild("DrawMemoryHandlersGUIChild1", ImVec2(ImGui::GetWindowContentRegionWidth() * 0.25f, 0), false, window_flags);
	FMemoryAccessHandler *pSelectedHandler = nullptr;

	for (auto &handler : pUI->MemoryAccessHandlers)
	{
		const bool bSelected = pUI->SelectedMemoryHandler == handler.Name;
		if (bSelected)
		{
			pSelectedHandler = &handler;
		}

		if (ImGui::Selectable(handler.Name.c_str(), bSelected))
		{
			pUI->SelectedMemoryHandler = handler.Name;
		}

	}
	ImGui::EndChild();
	
	ImGui::SameLine();

	// Handler details
	ImGui::BeginChild("DrawMemoryHandlersGUIChild2", ImVec2(0, 0), false, window_flags);
	if (pSelectedHandler != nullptr)
	{
		ImGui::Checkbox("Enabled", &pSelectedHandler->bEnabled);
		ImGui::Checkbox("Break", &pSelectedHandler->bBreak);
		ImGui::Text(pSelectedHandler->Name.c_str());
		ImGui::Text("0x%x - 0x%x", pSelectedHandler->MemStart, pSelectedHandler->MemEnd);
		ImGui::Text("Total Accesses %d", pSelectedHandler->TotalCount);

		ImGui::Text("Callers");
		for (const auto &accessPC : pSelectedHandler->CallerCounts)
		{
			ImGui::Text("0x%x - %d accesses", accessPC.first, accessPC.second);
		}
	}

	ImGui::EndChild();
}

void DrawMemoryAnalysis(FSpeccyUI* pUI)
{
	
	ImGui::Text("Memory Analysis");
	if(ImGui::Button("Analyse"))
	{
		AnalyseMemory(pUI->MemStats);	// non-const on purpose
	}
	const FMemoryStats& memStats = pUI->MemStats;
	ImGui::Text("%d self modified code points", (int)memStats.CodeAndDataList.size());
	ImGui::Text("%d blocks", (int)memStats.MemoryBlockInfo.size());
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_HorizontalScrollbar;
	ImGui::BeginChild("DrawMemoryAnalysisChild1", ImVec2(0, 0), false, window_flags);
	for(const auto &memblock : memStats.MemoryBlockInfo)
	{
		const char *pTypeStr = "Unknown";
		if (memblock.Use == MemoryUse::Code)
			pTypeStr = "Code";
		else if (memblock.Use == MemoryUse::Data)
			pTypeStr = "Data";
		ImGui::Text("%s", pTypeStr);
		ImGui::SameLine(100);
		ImGui::Text("0x%x - 0x%x", memblock.StartAddress, memblock.EndAddress);
	}
	ImGui::EndChild();

}

void DrawMemoryTools(FSpeccyUI* pUI)
{
	ImGui::Begin("Memory Tools");
	
	ImGui::BeginTabBar("MemoryToolsTabBar");

	if (ImGui::BeginTabItem("MemoryHandlers"))
	{
		DrawMemoryHandlers(pUI);
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("MemoryAnalysis"))
	{
		DrawMemoryAnalysis(pUI);
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();

	ImGui::End();
}

void ReadSpeccyKeys(FSpeccy *pSpeccy)
{
	ImGuiIO &io = ImGui::GetIO();
	for(int i=0;i<10;i++)
	{
		if (io.KeysDown[0x30+i] == 1)
		{
			zx_key_down(&pSpeccy->CurrentState, '0' + i);
			zx_key_up(&pSpeccy->CurrentState, '0' + i);
		}
	}

	for (int i = 0; i < 26; i++)
	{
		if (io.KeysDown[0x41 + i] == 1)
		{
			zx_key_down(&pSpeccy->CurrentState, 'a' + i);
			zx_key_up(&pSpeccy->CurrentState, 'a' + i);
		}
	}
}


void DrawSpeccyUI(FSpeccyUI* pUI)
{
	ui_zx_t* pZXUI = &pUI->UIZX;
	FSpeccy *pSpeccy = pUI->pSpeccy;
	const double timeMS = 1000.0f / ImGui::GetIO().Framerate;
	
	if(pSpeccy->ExecThisFrame)
		ui_zx_after_exec(pZXUI);
	
	DrawMainMenu(pUI, timeMS);

	if (pZXUI->memmap.open)
	{
		UpdateMemmap(pZXUI);
	}

	// call the Chips UI functions
	ui_audio_draw(&pZXUI->audio, pZXUI->zx->sample_pos);
	ui_z80_draw(&pZXUI->cpu);
	ui_ay38910_draw(&pZXUI->ay);
	ui_kbd_draw(&pZXUI->kbd);
	ui_memmap_draw(&pZXUI->memmap);

	for (int i = 0; i < 4; i++)
	{
		ui_memedit_draw(&pZXUI->memedit[i]);
		ui_dasm_draw(&pZXUI->dasm[i]);
	}

	DrawDebuggerUI(&pZXUI->dbg);

	

	// show spectrum window
	ImGui::Begin("Spectrum View");
	ImGui::Image(pSpeccy->Texture, ImVec2(320, 256));
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

	// read keys
	if (ImGui::IsWindowFocused())
	{
		ReadSpeccyKeys(pUI->pSpeccy);
	}
	ImGui::End();
	
	if (ImGui::Begin("Game Viewer"))
	{
		if (pUI->pActiveGame != nullptr)
		{
			ImGui::Text(pUI->pActiveGame->pConfig->Name.c_str());
			pUI->pActiveGame->pConfig->pDrawFunction(pUI, pUI->pActiveGame);
		}
		
		ImGui::End();
	}
	
	DrawGraphicsView(pUI);
	DrawMemoryTools(pUI);
}

bool DrawDockingView(FSpeccyUI *pUI)
{
	//SCOPE_PROFILE_CPU("UI", "DrawUI", ProfCols::UI);

	static bool opt_fullscreen_persistant = true;
	bool opt_fullscreen = opt_fullscreen_persistant;
	//static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
	bool bOpen = false;
	ImGuiDockNodeFlags dockFlags = 0;

	// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
	// because it would be confusing to have two docking targets within each others.
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	if (opt_fullscreen)
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	}

	// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and handle the pass-thru hole, so we ask Begin() to not render a background.
	if (dockFlags & ImGuiDockNodeFlags_PassthruCentralNode)
		window_flags |= ImGuiWindowFlags_NoBackground;

	// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
	// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive, 
	// all active windows docked into it will lose their parent and become undocked.
	// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise 
	// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
	bool bQuit = false;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	if (ImGui::Begin("DockSpace Demo", &bOpen, window_flags))
	{
		ImGui::PopStyleVar();

		if (opt_fullscreen)
			ImGui::PopStyleVar(2);

		// DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			const ImGuiID dockspaceId = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), dockFlags);
		}

		//bQuit = MainMenu();
		//DrawDebugWindows(uiState);
		DrawSpeccyUI(pUI);
		ImGui::End();
	}
	else
	{
		ImGui::PopStyleVar();
		bQuit = true;
	}

	return bQuit;
}

void UpdatePostTickSpeccyUI(FSpeccyUI* pUI)
{
	DrawDockingView(pUI);
}
/*
FGameViewer &AddGameViewer(FSpeccyUI *pUI,const char *pName)
{
	FGameViewer &gameViewer = pUI->GameViewers[pName];
	gameViewer.Name = pName;
	return gameViewer;
}*/

