#include <stdio.h>

#include <engine/shared/config.h>

#include <game/server/gamemodes/dom.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/entities/character.h>

#include "dspot.h"

// DSPOT
CDominationSpot::CDominationSpot(CGameWorld *pGameWorld, vec2 Pos, int Id, int CapTimes[MAX_PLAYERS / 2 + 1]
		, bool Handicap, bool HardCaptureAbort, bool Neutral)
		: CEntity(pGameWorld, CGameWorld::ENTTYPE_FLAG, Pos)
	, m_Pos(Pos)
	, m_Id(Id)
	, m_WithHandicap(Handicap)
	, m_WithHardCaptureAbort(HardCaptureAbort)
	, m_WithNeutral(Neutral)
{
	for (int i = 0; i < MAX_PLAYERS / DOM_NUMOFTEAMS + 1; ++i)
		m_aCapTimes[i] = CapTimes[1];

	Reset();
};

void CDominationSpot::Reset()
{
	m_CapTime = 0;
	m_Timer = -1;
	m_Team = DOM_NEUTRAL;
	m_CapTeam = DOM_NEUTRAL;
	m_NextTeam = DOM_NEUTRAL;
	m_IsGettingCaptured = false;
	m_IsLocked[DOM_RED] = m_IsLocked[DOM_BLUE] = false;
	m_FlagRedY = 0.0f;
	m_FlagBlueY = 0.0f;
}

void CDominationSpot::Snap(int SnappingClient)
{
	if (m_Team == DOM_RED || (m_IsGettingCaptured && m_NextTeam == DOM_RED))
	{
		// snap capture flag
		CNetObj_Flag *pRedFlag = static_cast<CNetObj_Flag*> (Server()->SnapNewItem(NETOBJTYPE_FLAG, 10 + DOM_NUMOFTEAMS * (m_Id + 1), sizeof(CNetObj_Flag)));
		pRedFlag->m_Team = TEAM_RED;
		pRedFlag->m_X = static_cast<int>(m_Pos.x);
		pRedFlag->m_Y = static_cast<int>(m_Pos.y + m_FlagRedY);
	}

	if (m_Team == DOM_BLUE || (m_IsGettingCaptured && m_NextTeam == DOM_BLUE))
	{
		// snap capture flag
		CNetObj_Flag *pBlueFlag = static_cast<CNetObj_Flag*> (Server()->SnapNewItem(NETOBJTYPE_FLAG, 20 + DOM_NUMOFTEAMS * (m_Id + 1), sizeof(CNetObj_Flag)));
		pBlueFlag->m_Team = TEAM_BLUE;
		pBlueFlag->m_X = static_cast<int>(m_Pos.x);
		pBlueFlag->m_Y = static_cast<int>(m_Pos.y + m_FlagBlueY);
	}
}

void CDominationSpot::StartCapturing(int CaptureTeam, int CaptureTeamSize, int DefTeamSize)
{
	if (m_IsLocked[CaptureTeam])
		return;

	m_CapTime = m_aCapTimes[min(CaptureTeamSize, MAX_PLAYERS / DOM_NUMOFTEAMS + 1)];
	if (m_WithHandicap && DefTeamSize > CaptureTeamSize)	//	handicap (faster capturing) for smaller team
		m_CapTime = m_CapTime * CaptureTeamSize / DefTeamSize;
	m_Timer = m_CapTime * Server()->TickSpeed();
	m_CapTeam = CaptureTeam;
	m_NextTeam = m_WithNeutral && m_Team != DOM_NEUTRAL? DOM_NEUTRAL : m_CapTeam;
	m_IsGettingCaptured = true;
	m_FlagCounter = (m_NextTeam == DOM_NEUTRAL ? 1.0f : -1.0f) * (static_cast<float>(DOM_FLAG_WAY) / static_cast<float>(m_Timer));
	if (m_NextTeam == DOM_RED || (m_Team == DOM_RED && m_NextTeam == DOM_NEUTRAL))
		m_FlagRedY = m_NextTeam == DOM_NEUTRAL ? 0.0f : DOM_FLAG_WAY;
	if (m_NextTeam == DOM_BLUE || (m_Team == DOM_BLUE && m_NextTeam == DOM_NEUTRAL))
		m_FlagBlueY = m_NextTeam == DOM_NEUTRAL ? 0.0f : DOM_FLAG_WAY;

	static_cast<CGameControllerDOM*>(GameServer()->m_pController)->OnStartCapturing(m_Id, CaptureTeam);
}

bool CDominationSpot::UpdateCapturing(int NumCapturePlayers, int NumDefPlayers)
{
	if (m_WithHardCaptureAbort && !NumCapturePlayers)
		AbortCapturing();

	int DiffPlayers = NumCapturePlayers - NumDefPlayers;
	if (!DiffPlayers)
	{
		if (NumCapturePlayers) // even number on both sides
			return false;
		else
			DiffPlayers = -1; // no player at all
	}

	m_Timer -= DiffPlayers;

	if (m_Timer / Server()->TickSpeed() >= m_CapTime)
	{
		// capture defended
		AbortCapturing();
		return false;
	}

	if (m_NextTeam == DOM_RED || (m_Team == DOM_RED && m_NextTeam == DOM_NEUTRAL))
		m_FlagRedY += m_FlagCounter * DiffPlayers;
	if (m_NextTeam == DOM_BLUE || (m_Team == DOM_BLUE && m_NextTeam == DOM_NEUTRAL))
		m_FlagBlueY += m_FlagCounter * DiffPlayers;

	if (m_Timer <= 0)
	{
		if (m_NextTeam == DOM_NEUTRAL)
			Neutralize();
		else
			Capture();
		return true;
	}
	return false;
}

void CDominationSpot::Neutralize()
{
	m_Team = m_NextTeam;
	GameServer()->CreateGlobalSound(SOUND_CTF_DROP);
	StopCapturing();
	// static_cast<CGameControllerDOM*>(GameServer()->m_pController)->OnNeutralize(m_Id);
}

void CDominationSpot::Capture()
{
	m_Team = m_NextTeam;
	GameServer()->CreateGlobalSound(SOUND_CTF_CAPTURE);
	StopCapturing();
	// static_cast<CGameControllerDOM*>(GameServer()->m_pController)->OnCapture(m_Id);
}

void CDominationSpot::AbortCapturing()
{
	StopCapturing();
	static_cast<CGameControllerDOM*>(GameServer()->m_pController)->OnAbortCapturing(m_Id);
}

void CDominationSpot::StopCapturing()
{
	m_IsGettingCaptured = false;
	m_NextTeam = m_Team;
	m_FlagRedY = 0.0f;
	m_FlagBlueY = 0.0f;
}