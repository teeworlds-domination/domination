/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <stdio.h>

#include <engine/shared/config.h>

#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/entities/character.h>

#include "kdom.h"

CGameControllerKDOM::CGameControllerKDOM(CGameContext *pGameServer)
: CGameControllerDOM(pGameServer)
{
	m_pGameType = "KDOM";
}

int CGameControllerKDOM::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	if(pKiller && Weapon != WEAPON_GAME)
	{
		// do team scoring
		if(pKiller == pVictim->GetPlayer() || pKiller->GetTeam() == pVictim->GetPlayer()->GetTeam())
			m_aTeamscore[pKiller->GetTeam()] -= m_aTeamDominationSpots[pKiller->GetTeam()];
		else
			m_aTeamscore[pKiller->GetTeam()] += m_aTeamDominationSpots[pKiller->GetTeam()];
	}
	
	return CGameControllerDOM::OnCharacterDeath(pVictim, pKiller, Weapon);
}

void CGameControllerKDOM::OnCapture(int Team)
{
	CGameControllerDOM::OnCapture(Team);

	m_aTeamscore[Team] += g_Config.m_SvKdomCapTeamscoreMultiplier * (m_aTeamDominationSpots[Team] - 1);
}

void CGameControllerKDOM::SendChatInfo(int ClientID)
{
	CGameControllerDOM::SendChat(ClientID, "GAMETYPE: KILL DOMINATION");
	CGameControllerDOM::SendChat(ClientID, "Capture domination spots.");
	CGameControllerDOM::SendChat(ClientID, "Tip: Capture together to reduce the required time.");
	CGameControllerDOM::SendChat(ClientID, "Gain team scores by killing your enemies.");
	CGameControllerDOM::SendChat(ClientID, "Each captured spot increases the amount of score you receive.");
}