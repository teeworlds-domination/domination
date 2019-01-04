/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_DOM_H
#define GAME_SERVER_GAMEMODES_DOM_H

#include <engine/shared/protocol.h>

#include <game/server/gamecontroller.h>

enum
{
	DOM_NEUTRAL = -1,
	DOM_RED = 0,
	DOM_BLUE = 1,
	DOM_NUMOFTEAMS = 2,
	DOM_MAXDSPOTS = 5,
	DOM_FLAG_WAY = 64
};

class CDominationSpot;

class CGameControllerDOM : public IGameController
{
private:
	char m_aBufBroadcastOverview[256];

private:
	void UpdateBroadcastOverview();

	void AddColorizedOpenParenthesis(int SpotNumber, char *pBuf, int &rCurrPos, int MarkerPos) const;
	void AddColorizedCloseParenthesis(int SpotNumber, char *pBuf, int &rCurrPos, int MarkerPos) const;
	void AddColorizedLine(int SpotNumber, char *pBuf, int &rCurrPos, int MarkerPos, int LineNum) const;
	void AddColorizedSpot(int SpotNumber, char *pBuf, int &rCurrPos) const;
	void AddColorizedMarker(int SpotNumber, char *pBuf, int &rCurrPos) const;
	void AddColorizedSymbol(char *pBuf, int &rCurrPos, int ColorCode, const char Symbol) const;

	void SendChatInfoWithHeader(int ClientID);

protected:
	class CDominationSpot *m_apDominationSpots[DOM_MAXDSPOTS];	//	domination spots
	float m_aTeamscoreTick[DOM_NUMOFTEAMS];						//	number of ticks a team captured the dspots (updated)
	int m_aNumOfTeamDominationSpots[DOM_NUMOFTEAMS];					//	number of owned dspots per team
	float m_DompointsCounter;									//	points a domination point generates in a second
	int m_aDominationSpotsEnabled[DOM_MAXDSPOTS];				//	enable/disables the usage for every domination spot
	bool m_UpdateBroadcast;										//	reports if the capturing braoadcast message should be changed
	int m_LastBroadcastTick;									//	tick of last capturing broadcast
	int m_NumOfDominationSpots;									//	number of domination spots
	int m_aCapTimes[MAX_PLAYERS / 2 + 1]; // Dynamic capture timer, considering the team sizes (dyn_captimes[get_team_size])
	
	virtual bool EvaluateSpawnPosDom(CSpawnEval *pEval, vec2 Pos) const;
	virtual void EvaluateSpawnTypeDom(CSpawnEval *pEval, int Type) const;
	virtual void Init();

	virtual void StartCapturing(int SpotNumber, int RedTeamSize, int BlueTeamSize, bool Consecutive);
	virtual void Capture(int SpotNumber);
	virtual void Neutralize(int SpotNumber);

	virtual void UpdateCaptureProcess();
	virtual void UpdateBroadcast();
	virtual void UpdateScoring();

	virtual const char* GetDominationSpotBroadcastOverview(int SpotNumber, char *pBuf) const;

	virtual void SetCapTimes(const char* pCapTimes);

public:
	CGameControllerDOM(class CGameContext *pGameServer);
	virtual void Tick() override;
	virtual bool CanSpawn(int Team, vec2 *pOutPos) override;

	virtual float GetRespawnDelay(bool Self) override;

	virtual bool OnEntity(int Index, vec2 Pos) override;
	virtual void OnPlayerConnect(class CPlayer *pPlayer) override;
	virtual void OnReset() override;

	virtual void OnCapture(int SpotNumber, int Team) {};
	virtual void OnNeutralize(int SpotNumber, int Team) {};

	virtual void SendBroadcast(int ClientID, const char *pText) const;
	void SendChat(int ClientID, const char *pText) const;
	virtual void SendChatCommand(int ClientID, const char *pCommand);
	virtual void SendChatInfo(int ClientID);
	virtual void SendChatStats(int ClientID);

	const char *GetSpotName(const int SpotID) const;
	const char *GetTeamBroadcastColor(const int Team) const;
	const char *GetTeamName(const int Team) const;
};

#endif