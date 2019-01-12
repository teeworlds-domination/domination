/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_CONQ_H
#define GAME_SERVER_GAMEMODES_CONQ_H
#include "dom.h"

class CGameControllerCONQ : public CGameControllerDOM
{
private:
	int m_WinTick;

	void CalculateSpotSpawns(int Spot, int Team);

protected:
	vec2 m_aaaSpotSpawnPoints[DOM_MAXDSPOTS][DOM_NUMOFTEAMS][64];
	int  m_aaNumSpotSpawnPoints[DOM_MAXDSPOTS][DOM_NUMOFTEAMS];

	virtual void CalculateSpawns();

	virtual float EvaluateSpawnPosConq(vec2 Pos, int SpawnSpot, int NextSpot, int PreviousSpot, bool &IsStartpointAfterPreviousSpot) const;
	virtual void  EvaluateSpawnTypeConq(CSpawnEval *pEval, int Team) const;

	virtual void Init() override;

	void EndMatch();
	virtual void UpdateScoring() override {};

	void UnlockSpot(int Spot, int Team);
	void LockSpot(int Spot, int Team);

	virtual bool SendPersonalizedBroadcast(int ClientID) const;

	void DoWincheckMatch();

	int GetNextSpot(int Spot) const override    { return CGameControllerDOM::GetNextSpot(Spot); }
	int GetPreviousSpot(int Spot) const override { return CGameControllerDOM::GetPreviousSpot(Spot); }
	int GetNextSpot(int Spot, int Team) const;
	int GetPreviousSpot(int Spot, int Team) const;

protected:
	virtual void AddColorizedOpenParenthesis(int Spot, char *pBuf, int &rCurrPos, int MarkerPos) const;
	virtual void AddColorizedCloseParenthesis(int Spot, char *pBuf, int &rCurrPos, int MarkerPos) const;

public:
	CGameControllerCONQ(class CGameContext *pGameServer);
	virtual void Tick() override;
	virtual bool CanSpawn(int Team, vec2 *pOutPos) override;

	virtual float GetRespawnDelay(bool Self) const override;

	virtual void OnReset() override;

	virtual void OnCapture(int Spot, int Team, int NumOfCapCharacters, CCharacter* apCapCharacters[MAX_PLAYERS]) override;
	virtual void OnNeutralize(int Spot, int Team, int NumOfCapCharacters, CCharacter* apCapCharacters[MAX_PLAYERS]) override;
	virtual int  OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;

	virtual void SendChatInfo(int ClientID) const override;

	virtual void ShowSpawns(int Spot) const override;
};
#endif
