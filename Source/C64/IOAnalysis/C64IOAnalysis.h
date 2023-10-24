#pragma once
#include <cstdint>
#include <string>
#include <map>
#include <set>

#include "VICAnalysis.h"
#include "SIDAnalysis.h"
#include "CIAAnalysis.h"

class FCodeAnalysisState;

class FC64IOAnalysis
{
public:
	void	Init(FCodeAnalysisState *pAnalysis);
	void	Reset();
	void	RegisterIORead(uint16_t addr, FAddressRef pc);
	void	RegisterIOWrite(uint16_t addr, uint8_t val, FAddressRef pc);

	void	DrawIOAnalysisUI(void);
private:

	FVICAnalysis	VICAnalysis;
	FSIDAnalysis	SIDAnalysis;
	FCIA1Analysis	CIA1Analysis;
	FCIA2Analysis	CIA2Analysis;
};