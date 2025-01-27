#pragma once

#define UI_DBG_USE_Z80
#define UI_DASM_USE_Z80
#include "chips/chips_common.h"
#include "chips/z80.h"
#include "chips/m6502.h"
#include "chips/beeper.h"
#include "chips/ay38910.h"
#include "util/z80dasm.h"
#include "util/m6502dasm.h"
#include "chips/mem.h"
#include "chips/kbd.h"
#include "chips/clk.h"
#include "systems/zx.h"
#include "chips/mem.h"
#include "ui/ui_util.h"
#include "ui/ui_chip.h"
#include "ui/ui_z80.h"
#include "ui/ui_ay38910.h"
#include "ui/ui_audio.h"
#include "ui/ui_kbd.h"
#include "ui/ui_dasm.h"
#include "ui/ui_dbg.h"
#include "ui/ui_memedit.h"
#include "ui/ui_memmap.h"
#include "ui/ui_snapshot.h"
#include "ui/ui_zx.h"

//#include <map>
#include <string>
#include "Viewers/SpriteViewer.h"
#include "MemoryHandlers.h"
//#include "Disassembler.h"
//#include "FunctionHandlers.h"
#include "CodeAnalyser/CodeAnalyser.h"
#include "CodeAnalyser/IOAnalyser.h"
#include "Viewers/ViewerBase.h"
#include "Viewers/ZXGraphicsViewer.h"
#include "Viewers/SpectrumViewer.h"
#include "Viewers/FrameTraceViewer.h"
#include "Misc/GamesList.h"
#include "IOAnalysis.h"
#include "SnapshotLoaders/GameLoader.h"
#include "SnapshotLoaders/RZXLoader.h"
#include "Util/Misc.h"
#include "SpectrumDevices.h"
#include "Misc/EmuBase.h"

struct FGame;
struct FGameViewer;
struct FGameViewerData;
struct FGameConfig;
struct FViewerConfig;
struct FSkoolFileInfo;
struct FZXSpectrumConfig;
struct FZXSpectrumGameConfig;

enum class ESpectrumModel
{
	Spectrum48K,
	Spectrum128K
};

struct FSpectrumLaunchConfig : public FEmulatorLaunchConfig
{
	void ParseCommandline(int argc, char** argv) override;
	ESpectrumModel	Model = ESpectrumModel::Spectrum48K;
	std::string		SkoolkitImport;
};

struct FGame
{
	FGameConfig *		pConfig	= nullptr;
	FViewerConfig *		pViewerConfig = nullptr;
	FGameViewerData *	pViewerData = nullptr;
};


class FSpectrumEmu : public FEmuBase
{
public:
	FSpectrumEmu()
	{
		CPUType = ECPUType::Z80;
		for (int i = 0; i < kNoROMBanks; i++)
			ROMBanks[i] = -1;
		for (int i = 0; i < kNoRAMBanks; i++)
			RAMBanks[i] = -1;
	}

	bool	Init(const FEmulatorLaunchConfig& config) override;
	void	Shutdown() override;
	void	Tick() override;
	void	Reset() override;

	bool	LoadLua() override;

	bool	NewGameFromSnapshot(const FGameSnapshot& snapshot) override;
	bool	StartGame(FGameConfig* pGameConfig, bool bLoadGame) override;
	bool	SaveCurrentGameData() override;

	bool	IsInitialised() const { return bInitialised; }

	//void	DrawMainMenu(double timeMS);
	
	void	FileMenuAdditions(void) override;
	void	SystemMenuAdditions(void)  override;
	void	OptionsMenuAdditions(void) override;
	void	WindowsMenuAdditions(void)  override;


	//void	DrawExportAsmModalPopup();
	//void	DrawReplaceGameModalPopup();
	void	DrawCheatsUI();
	bool	ImportSkoolFile(const char* pFilename, const char* pOutSkoolInfoName = nullptr, FSkoolFileInfo* pSkoolInfo=nullptr);
	bool	ExportSkoolFile(bool bHexadecimal, const char* pName = nullptr);
	void	DoSkoolKitTest(const char* pGameName, const char* pInSkoolFileName, bool bHexadecimal, const char* pOutSkoolName = nullptr);
	void	AppFocusCallback(int focused) override;

	void	OnInstructionExecuted(int ticks, uint64_t pins);
	uint64_t Z80Tick(int num, uint64_t pins);

	void	DrawMemoryTools();
	void	DrawEmulatorUI() override;
	//bool	DrawDockingView();

	// disable copy & assign because this class is big!
	FSpectrumEmu(const FSpectrumEmu&) = delete;
	FSpectrumEmu& operator= (const FSpectrumEmu&) = delete;

	//ICPUInterface Begin
	uint8_t		ReadByte(uint16_t address) const override;
	uint16_t	ReadWord(uint16_t address) const override;
	const uint8_t*	GetMemPtr(uint16_t address) const override;
	void		WriteByte(uint16_t address, uint8_t value) override;

	FAddressRef	GetPC(void) override;
	uint16_t	GetSP(void) override;
	void*		GetCPUEmulator(void) const override;
	//ICPUInterface End

	void		FormatSpectrumMemory(FCodeAnalysisState& state);

	void SetROMBank(int bankNo);
	void SetRAMBank(int slot, int bankNo);

	void AddMemoryHandler(const FMemoryAccessHandler& handler)
	{
		MemoryAccessHandlers.push_back(handler);
	}

	const FZXSpectrumConfig* GetZXSpectrumGlobalConfig() { return (const FZXSpectrumConfig*)pGlobalConfig; }


	// TODO: Make private
//private:
	// Emulator 
	zx_t			ZXEmuState;	// Chips Spectrum State
	uint8_t*		MappedInMemory = nullptr;
	//FZXSpectrumConfig *	pGlobalConfig = nullptr;

	float			ExecSpeedScale = 1.0f;

	// Chips UI
	ui_zx_t			UIZX;

	FGame *			pActiveGame = nullptr;

	//FGamesList		GamesList;
	FGamesList		RZXGamesList;
	FZXGameLoader	GameLoader;

	//Viewers
	FSpectrumViewer			SpectrumViewer;
	FFrameTraceViewer		FrameTraceViewer;
	//FZXGraphicsViewer		GraphicsViewer;
	//FCodeAnalysisState		CodeAnalysis;

	// IO Devices
	FSpectrumKeyboard	Keyboard;
	FSpectrumBeeper		Beeper;
	FAYAudioDevice			AYSoundChip;
	FSpectrum128MemoryCtrl	MemoryControl;

	// Code analysis pages - to cover 48K & 128K Spectrums
	static const int	kNoBankPages = 16;	// no of pages per physical address slot (16k)
	static const int	kNoRAMPages = 128;
	static const int	kNoROMBanks = 2;
	static const int	kNoRAMBanks = 8;

	int16_t				ROMBanks[kNoROMBanks];
	int16_t				RAMBanks[kNoRAMBanks];
	int16_t				CurROMBank = -1;
	int16_t				CurRAMBank[4] = { -1,-1,-1,-1 };

	// Memory handling
	std::string							SelectedMemoryHandler;
	std::vector< FMemoryAccessHandler>	MemoryAccessHandlers;

	FMemoryStats	MemStats;

	// interrupt handling info
	bool			bHasInterruptHandler = false;
	uint16_t		InterruptHandlerAddress = 0;
	
	uint16_t		PreviousPC = 0;		// store previous pc
	int				InstructionsTicks = 0;

	FRZXManager		RZXManager;
	int				RZXFetchesRemaining = 0;

private:
	//std::vector<FViewerBase*>	Viewers;

	//bool	bReplaceGamePopup = false;
	//bool	bExportAsm = false;

	//int		ReplaceGameSnapshotIndex = 0;

	//bool	bShowDebugLog = false;
	bool	bInitialised = false;
};


uint16_t GetScreenPixMemoryAddress(int x, int y);
uint16_t GetScreenAttrMemoryAddress(int x, int y);
bool GetScreenAddressCoords(uint16_t addr, int& x, int& y);
bool GetAttribAddressCoords(uint16_t addr, int& x, int& y);
