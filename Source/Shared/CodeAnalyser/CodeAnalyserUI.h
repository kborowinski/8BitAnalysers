#pragma once
#include <cstdint>

struct FCodeAnalysisState;
struct FDataInfo;

// UI
void DrawCodeAddress(FCodeAnalysisState &state, uint16_t addr, bool bFunctionRel = false);
void DrawAddressLabel(FCodeAnalysisState &state, uint16_t addr, bool bFunctionRel = false);
int GetItemIndexForAddress(const FCodeAnalysisState& state, uint16_t addr);
void DrawCodeAnalysisItemAtIndex(FCodeAnalysisState& state, int i);


void DrawCodeAnalysisData(FCodeAnalysisState &state);
void DrawGlobals(FCodeAnalysisState &state);
void DrawCallStack(FCodeAnalysisState& state);
void DrawTrace(FCodeAnalysisState& state);

void DrawDataDetails(FCodeAnalysisState &state, FDataInfo *pDataInfo);

void CodeAnalyserGoToAddress(FCodeAnalysisState& state, uint16_t newAddress, bool bLabel = false);