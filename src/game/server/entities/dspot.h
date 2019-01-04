#ifndef GAME_SERVER_ENTITIES_DSPOT_H
#define GAME_SERVER_ENTITIES_DSPOT_H

#include <game/server/entity.h>

class CDominationSpot : public CEntity
{
private:
	vec2 m_Pos;                  // Flag position
	int m_Id;                    // Identification for this domination spot
	float m_FlagY;               // Flag distance to normal stand position (vertically)
	float m_FlagCounter;         // Moves the flag during capturing process
	int m_aCapTimes[MAX_PLAYERS / 2 + 1]; // Dynamic capture timer, considering the team sizes (dyn_captimes[get_team_size])

	int m_CapTeam;               // Capturing Team
	int m_Team;                  // Owner Team
	bool m_IsGettingCaptured;    // Reports if a capturing process is running
	bool m_IsLocked[2];          // Capturing this spot is locked for this team

	int m_CapTime;               // Time needed for capturing process
	int m_Timer;                 // Timer for capturing process

public:
	CDominationSpot(CGameWorld *pGameWorld, vec2 Pos, int Id, int CapTimes[MAX_PLAYERS / 2 + 1]);
	virtual void Reset() override;
	virtual void Snap(int SnappingClient) override;

	void StartCapturing(const int CaptureTeam, const int CaptureTeamSize, const int DefTeamSize, const bool Consecutive);
	const bool UpdateCapturing(const int NumCapturePlayers, const int NumDefPlayers);
	void StopCapturing();

	bool IsGettingCaptured() const { return m_IsGettingCaptured; }

	bool IsLocked(int Team) const { return m_IsLocked[Team]; }
	void Lock(int Team)    { m_IsLocked[Team] = true; }
	void Unlock(int Team)  { m_IsLocked[Team] = false; }

	int GetCapTeam() const { return m_CapTeam; }
	int GetTeam() const    { return m_Team; }
	vec2 GetPos() const    { return m_Pos; }
	int GetCapTime() const { return m_CapTime; }
	int GetTimer() const   { return m_Timer; }

	void SetTeam(int Team) { m_Team = Team; }
};

#endif