/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_CONQ_H
#define GAME_SERVER_GAMEMODES_CONQ_H
#include "dom.h"

class CGameControllerCONQ : public CGameControllerDOM
{
private:
	int m_WinTick;

	void CalculateSpawns();
	void CalculateSpotSpawns(int Spot, int Team);

	void UnlockSpot(int Spot, int Team);
	void LockSpot(int Spot, int Team);

protected:
	vec2 m_aaaSpotSpawnPoints[DOM_MAXDSPOTS][DOM_NUMOFTEAMS][64];
	int  m_aaNumSpotSpawnPoints[DOM_MAXDSPOTS][DOM_NUMOFTEAMS];

	virtual float EvaluateSpawnPosConq(vec2 Pos, int LastOwnSpot, int LastEnemySpot, int PreviousOwnSpot, bool &IsStartpointAfterPreviousSpot) const;
	virtual void  EvaluateSpawnTypeConq(CSpawnEval *pEval, int Team) const;

	virtual void Init() override;

	void EndMatch();
	virtual void UpdateScoring() override {};
	virtual void UpdateBroadcast() override;

	void DoWincheckMatch();

protected:
	virtual void AddColorizedOpenParenthesis(int SpotNumber, char *pBuf, int &rCurrPos, int MarkerPos) const;
	virtual void AddColorizedCloseParenthesis(int SpotNumber, char *pBuf, int &rCurrPos, int MarkerPos) const;

public:
	CGameControllerCONQ(class CGameContext *pGameServer);
	virtual void Tick() override;
	virtual bool CanSpawn(int Team, vec2 *pOutPos) override;

	virtual float GetRespawnDelay(bool Self) override;

	virtual void OnCapture(int SpotNumber, int Team) override;
	virtual void OnNeutralize(int SpotNumber, int Team) override;
	virtual int  OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;

	virtual void SendChatInfo(int ClientID) override;
	virtual void SendChatCommand(int ClientID, const char *pCommand); // TODO DEBUG REMOVE THIS
};
#endif
