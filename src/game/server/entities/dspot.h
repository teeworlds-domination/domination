#ifndef GAME_SERVER_ENTITIES_DSPOT_H
#define GAME_SERVER_ENTITIES_DSPOT_H

#include <game/server/entity.h>

class CDominationSpot : public CEntity
{
public:
	vec2 m_Pos;                  // Flag position
	int m_Team;                  // Owner Team
	int m_CapTeam;               // Capturing Team
	int m_CapTime;               // Time needed for capturing process
	int m_Timer;                 // Timer for capturing process
	int m_IsGettingCaptured;     // Reports if a capturing process is running
	CCharacter* m_pCapCharacter; // This character gets the capturing points

protected:
	float m_FlagY;               // Flag distance to normal stand position (vertically)
	float m_FlagCounter;         // Moves the flag during capturing process
	int m_Id;                    // Identification for this domination spot
	int m_aCapTimes[MAX_CLIENTS / 2 + 1]; // Dynamic capture timer, considering the team sizes (dyn_captimes[get_team_size])

public:
	CDominationSpot(CGameWorld *pGameWorld, vec2 Pos, int Id);
	virtual void Reset() override;
	virtual void Snap(int SnappingClient) override;

	void StartCapturing(const int CaptureTeam, const int CaptureTeamSize, const int DefTeamSize);
	const bool UpdateCapturing(const int NumCapturePlayers, const int NumDefPlayers);
	void StopCapturing();
	const char *GetSpotName() const;
	const char *GetTeamBroadcastColor() const;
	const char *GetTeamName(const int Team) const;
	const char *GetTeamName() const { return GetTeamName(m_Team); }
};

#endif