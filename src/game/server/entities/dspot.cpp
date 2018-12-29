#include <stdio.h>

#include <engine/shared/config.h>

#include <game/server/gamemodes/dom.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/entities/character.h>

#include "dspot.h"

// DSPOT
CDominationSpot::CDominationSpot(CGameWorld *pGameWorld, vec2 Pos, int Id) : CEntity(pGameWorld, CGameWorld::ENTTYPE_FLAG, Pos),
	m_Pos(Pos),
	m_Id(Id)
{
	Reset();
};

void CDominationSpot::Reset()
{
	m_Team = DOM_NEUTRAL;
	m_CapTeam = DOM_NEUTRAL;
	m_IsGettingCaptured = false;
	m_FlagY = 0.0f;
	sscanf(g_Config.m_SvDomCapTimes, "%d %d %d %d %d %d %d %d", m_aCapTimes + 1, m_aCapTimes + 2, m_aCapTimes + 3, m_aCapTimes + 4, m_aCapTimes + 5, m_aCapTimes + 6, m_aCapTimes + 7, m_aCapTimes +8);
	m_aCapTimes[0] = 5;
	for (int i = 1; i < MAX_PLAYERS / DOM_NUMOFTEAMS + 1; ++i)
		if (m_aCapTimes[i] <= 0)
			m_aCapTimes[i] = m_aCapTimes[i - 1];
		else if (m_aCapTimes[i] > 60)
			m_aCapTimes[i] = 60;
	m_aCapTimes[0] = m_aCapTimes[1];
}

void CDominationSpot::Snap(int SnappingClient)
{
	if (m_Team != DOM_NEUTRAL || m_IsGettingCaptured)
	{
		CNetObj_Flag *pFlag = static_cast<CNetObj_Flag*> (Server()->SnapNewItem(NETOBJTYPE_FLAG, DOM_NUMOFTEAMS * (m_Id + 1) + (m_IsGettingCaptured && m_Team == DOM_NEUTRAL ? m_CapTeam : m_Team), sizeof(CNetObj_Flag)));
		pFlag->m_Team = m_IsGettingCaptured && m_Team == DOM_NEUTRAL ? m_CapTeam : m_Team;
		pFlag->m_X = static_cast<int>(m_Pos.x);
		pFlag->m_Y = static_cast<int>(m_Pos.y + m_FlagY);
	}
}

void CDominationSpot::StartCapturing(const int CaptureTeam, const int CaptureTeamSize, const int DefTeamSize)
{
	m_Timer = m_aCapTimes[min(CaptureTeamSize, MAX_PLAYERS / DOM_NUMOFTEAMS + 1)] * Server()->TickSpeed();
	if (DefTeamSize > CaptureTeamSize)	//	handicap (faster capturing) for smaller team
		m_Timer = m_Timer * CaptureTeamSize / DefTeamSize;
	m_CapTeam = CaptureTeam;
	m_pCapCharacter = 0;
	m_IsGettingCaptured = true;
	m_FlagCounter = (m_Team == DOM_NEUTRAL ? -1.0f : 1.0f) * (static_cast<float>(DOM_FLAG_WAY) / static_cast<float>(m_Timer));
	m_FlagY = m_Team == DOM_NEUTRAL ? DOM_FLAG_WAY : 0.0f;
}

const bool CDominationSpot::UpdateCapturing(const int NumCapturePlayers)
{
	m_FlagY += m_FlagCounter * NumCapturePlayers;
	m_Timer -= NumCapturePlayers;
	if (m_Timer < 1)
	{
		//	capturing successful
		if (m_Team == DOM_RED || m_Team == DOM_BLUE)
		{
			m_Team = DOM_NEUTRAL;
			GameServer()->CreateGlobalSound(SOUND_CTF_DROP);
		}
		else
		{
			m_Team = m_CapTeam;
			GameServer()->CreateGlobalSound(SOUND_CTF_CAPTURE);
		}
		m_pCapCharacter->GetPlayer()->m_Score += g_Config.m_SvDomCapPoints;
		StopCapturing();
		return true;
	}
	return false;
}

void CDominationSpot::StopCapturing()
{
	m_IsGettingCaptured = false;
	m_FlagY = 0.0f;
}

const char* CDominationSpot::GetTeamName(const int Team) const
{
	switch(Team)
	{
	case 0: return "Red Team"; break;
	case 1: return "Blue Team"; break;
	default: return "Neutral"; break;
	}
}

const char* CDominationSpot::GetSpotName() const
{
	switch (m_Id)
	{
	case 0: return "A";
	case 1: return "B";
	case 2: return "C";
	case 3: return "D";
	case 4: return "E";
	default: return "Area 51";	//	should never occur
	}
}

const char* CDominationSpot::GetTeamBroadcastColor() const
{
	switch(m_Team)
	{
	case DOM_RED:  return "^900";
	case DOM_BLUE: return "^009";
	default:       return "^999";
	}
}