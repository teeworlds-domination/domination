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

enum
{
	ADD_CAPSTRENGTH = 30,
	BASE_CAPSTRENGTH = 100
};

class CDominationSpot;

class CGameControllerDOM : public IGameController
{
private:
	int  m_ConstructorTick;
	int  m_aPlayerIntroTick[MAX_CLIENTS];

	char  m_aBufBroadcastOverview[256];
	char  m_aaBufBroadcastSpotOverview[DOM_MAXDSPOTS][48];
	int   m_LastBroadcastTick;									//	tick of last capturing broadcast
	int   m_LastBroadcastCalcTick;
	int   m_aLastBroadcastState[DOM_MAXDSPOTS];
	int   m_aLastSpotCapStrength[DOM_MAXDSPOTS];

	void UpdateBroadcastOverview();
	const char* GetDominationSpotBroadcastOverview(int Spot, char *pBuf);

private:
	int   m_aCapTimes[MAX_PLAYERS / 2 + 1];						// Dynamic capture timer, considering the team sizes (dyn_captimes[get_team_size])
	float m_DompointsCounter;									//	points a domination point generates in a second
	float m_aTeamscoreTick[DOM_NUMOFTEAMS];						//	number of ticks a team captured the dspots (updated)

protected:
	int  m_aaNumSpotSpawnPoints[DOM_MAXDSPOTS][3];
	vec2 m_aaaSpotSpawnPoints[DOM_MAXDSPOTS][3][64];

	CDominationSpot *m_apDominationSpots[DOM_MAXDSPOTS];		//	domination spots
	int   m_aNumOfTeamDominationSpots[DOM_NUMOFTEAMS];			//	number of owned dspots per team
	int   m_aDominationSpotsEnabled[DOM_MAXDSPOTS];				//	enable/disables the usage for every domination spot
	int   m_NumOfDominationSpots;								//	number of domination spots

protected:
	virtual void Init();
	virtual void CalculateSpawns();
	virtual void CalculateSpotSpawns(int Spot, int Type);
	
	bool IsSpawnAtSpot(vec2 Pos, int Spot) const;
	void EvaluateSpawnTypeDom(CSpawnEval *pEval, int SpotTeam) const;
	void EvaluateSpawnTypeDom(CSpawnEval *pEval, int SpotTeam, int SpawnType) const;

	virtual void StartCapturing(int Spot, int RedCapStrength, int BlueCapStrength);
	virtual void Capture(int Spot, int NumOfCapCharacters, CCharacter* apCapCharacters[MAX_PLAYERS]);
	virtual void Neutralize(int Spot, int NumOfCapCharacters, CCharacter* apCapCharacters[MAX_PLAYERS]);

	virtual void UpdateCaptureProcess();
	virtual void UpdateBroadcast();
	virtual void UpdateChat();
	virtual void UpdateScoring();

	virtual int CalcCaptureStrength(int Spot, CCharacter *pChr, bool IsFirst) const;

	virtual int GetNextSpot(int Spot) const;
	virtual int GetPreviousSpot(int Spot) const;

	virtual void SetCapTime(int CapTime);

	virtual bool WithAdditiveCapStrength() const { return true; }
	virtual bool WithHandicap() const { return true; }
	virtual bool WithHardCaptureAbort() const { return false; }
	virtual bool WithNeutralState() const { return true; }

protected:
	virtual void AddColorizedOpenParenthesis(int Spot, char *pBuf, int &rCurrPos, int MarkerPos) const;
	virtual void AddColorizedCloseParenthesis(int Spot, char *pBuf, int &rCurrPos, int MarkerPos) const;
	virtual void AddColorizedLine(int Spot, char *pBuf, int &rCurrPos, int MarkerPos, int LineNum) const;
	virtual void AddColorizedSpot(int Spot, char *pBuf, int &rCurrPos) const;
	virtual void AddColorizedMarker(int Spot, char *pBuf, int &rCurrPos) const;

	void AddColorizedSymbol(char *pBuf, int &rCurrPos, int ColorCode, const char Symbol) const;

	virtual bool SendPersonalizedBroadcast(int ClientID);
	void SendBroadcast(int ClientID, const char *pText) const;

	void SendChat(int ClientID, const char *pText) const;
	void SendChatIntro(int ClientID) const;
	virtual void SendChatInfo(int ClientID) const;
	virtual void SendChatStats(int ClientID) const;

	void ForceSpotBroadcastUpdate(int Spot);
	const char GetSpotName(int Spot) const;
	const char *GetTeamBroadcastColor(int Team) const;
	const char GetTeamBroadcastOpenParenthesis(int Team) const;
	const char GetTeamBroadcastCloseParenthesis(int Team) const;
	const char GetTeamBroadcastMarker(int Team, int CapStrength) const;
	const char *GetTeamName(int Team) const;

	int GetMaxCapStrengthForTeamSize(int TeamSize) const;
	
public:
	CGameControllerDOM(class CGameContext *pGameServer);
	virtual void Tick() override;
	virtual bool CanSpawn(int Team, vec2 *pOutPos) override;

	virtual float GetRespawnDelay(bool Self) const override;

	virtual bool OnEntity(int Index, vec2 Pos) override;
	virtual void OnInit();
	virtual void OnPlayerConnect(class CPlayer *pPlayer) override;
	virtual void OnReset() override;

	virtual void OnStartCapturing(int Spot, int Team) {};
	virtual void OnAbortCapturing(int Spot) {};
	virtual void OnCapture(int Spot, int Team, int NumOfCapCharacters, CCharacter* apCapCharacters[MAX_PLAYERS]);
	virtual void OnNeutralize(int Spot, int Team, int NumOfCapCharacters, CCharacter* apCapCharacters[MAX_PLAYERS]) {};

	virtual void SendChatCommand(int ClientID, const char *pCommand) const;

	virtual void ShowSpawns(int Spot) const;
};

#endif