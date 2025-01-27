#pragma once

#include <string>
#include <vector>

class FSystem;

enum class ESnapshotType
{
	Z80,
	SNA,
	TAP,
	TZX,
	RZX,
	PRG,

	Unknown
};

struct FGameSnapshot
{
	ESnapshotType	Type;
	std::string		DisplayName;
	std::string		FileName;
};

class IGameLoader
{
public:
	virtual bool LoadSnapshot(const FGameSnapshot& snapshot) = 0;
	//virtual ESnapshotType GetSnapshotTypeFromFileName(const std::string& fn) = 0;
};

class FGamesList
{
public:
	void	SetLoader(IGameLoader* pLoader) 
	{ 
		pGameLoader = pLoader;
	}
	bool	EnumerateGames(const char* pRootDir);
	bool	LoadGame(int index) const;
	bool	LoadGame(const char* pFileName) const;

	int		GetNoGames() const { return (int)GamesList.size(); }
	const FGameSnapshot& GetGame(int index) const { return GamesList[index]; }
	const FGameSnapshot* GetGame(const char* pSnapshotName) const;
	//const std::string& GetGameName(int index) const { return GamesList[index].DisplayName; }

private:
	std::vector< FGameSnapshot>	GamesList;
	std::string RootDir;
	IGameLoader* pGameLoader = 0;
};
