/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_DOM_H
#define GAME_SERVER_GAMEMODES_DOM_H
#include <game/server/gamecontroller.h>
#include <game/server/entity.h>

enum
{
	DOM_NEUTRAL = -1,
	DOM_RED = 0,
	DOM_BLUE = 1,
	DOM_NUMOFTEAMS = 2,
	DOM_MAXDSPOTS = 5,
	DOM_FLAG_WAY = 64
};

class CGameControllerDOM : public IGameController
{
protected:
	class CDominationSpot *m_apDominationSpots[DOM_MAXDSPOTS];	//	domination spots
	float m_aTeamscoreTick[DOM_NUMOFTEAMS];						//	number of ticks a team captured the dspots (updated)
	int m_aTeamDominationSpots[DOM_NUMOFTEAMS];					//	number of owned dspots per team
	float m_DompointsCounter;									//	points a domination point generates in a second
	int m_aDominationSpotsEnabled[DOM_MAXDSPOTS];				//	enable/disables the usage for every domination spot
	bool m_UpdateBroadcast;										//	reports if the capturing braoadcast message should be changed
	int m_LastBroadcastTick;									//	tick of last capturing broadcast
	int m_NumOfDominationSpots;									//	number of domination spots
	
	virtual bool EvaluateSpawnPos2(CSpawnEval *pEval, vec2 Pos) const;
	virtual void EvaluateSpawnType(CSpawnEval *pEval, int Type) const override;
	virtual void Init();
	virtual void UpdateCaptureProcess();
	virtual void UpdateBroadcast();
	virtual void UpdateScoring();

public:
	CGameControllerDOM(class CGameContext *pGameServer);
	virtual void Tick() override;
	virtual bool CanSpawn(int Team, vec2 *pOutPos) override;

	virtual float GetRespawnDelay(bool Self) override;

	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	virtual bool OnEntity(int Index, vec2 Pos) override;
	virtual void OnReset() override;

	void SendChat(int ClientID, const char *pText) const;
	virtual void SendChatCommand(int ClientID, const char *pCommand);
	virtual void SendChatInfo(int ClientID);
	virtual void SendChatStats(int ClientID);
};


class CDominationSpot : public CEntity
{
	// TODO move to ../entities
public:
	vec2 m_Pos;                  // Flag position
	int m_Team;                  // Owner Team
	int m_CapTeam;               // Capturing Team
	int m_Timer;                 // Timer for capturing process
	int m_IsGettingCaptured;     // Reports if a capturing process is running
	CCharacter* m_pCapCharacter; // This character gets the capturing points

protected:
	float m_FlagY;               // Flag distance to normal stand position (vertically)
	float m_FlagCounter;         // Moves the flag during capturing process
	int m_Id;                    // Identification for this domination spot
	int m_aCapTimes[MAX_CLIENTS / DOM_NUMOFTEAMS + 1]; // Dynamic capture timer, considering the team sizes (dyn_captimes[get_team_size])

public:
	CDominationSpot(CGameWorld *pGameWorld, vec2 Pos, int Id);
	virtual void Reset() override;
	virtual void Snap(int SnappingClient) override;

	void StartCapturing(const int CaptureTeam, const int CaptureTeamSize, const int DefTeamSize);
	const bool UpdateCapturing(const int NumCapturePlayers);
	void StopCapturing();
	const char *GetSpotName() const;
	const char *GetTeamName(const int Team) const;
	const char *GetTeamName() const { return GetTeamName(m_Team); }
};
#endif
