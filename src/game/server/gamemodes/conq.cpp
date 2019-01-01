/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <float.h>
#include <stdio.h>

#include <engine/shared/config.h>

#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/entities/character.h>
#include <game/server/entities/dspot.h>

#include "conq.h"

CGameControllerCONQ::CGameControllerCONQ(CGameContext *pGameServer)
: CGameControllerDOM(pGameServer)
{
	m_pGameType = "CONQ";
}

void CGameControllerCONQ::Init()
{
	CGameControllerDOM::Init();

	m_WinTick = -1;

	if (m_NumOfDominationSpots > 1)
	{
		int SpotRed, SpotBlue;
		for (int i = 0; i < DOM_MAXDSPOTS; ++i)
		{
			if (!m_aDominationSpotsEnabled[i])
				continue;	
			m_apDominationSpots[i]->Lock(DOM_RED);
			m_apDominationSpots[i]->Lock(DOM_BLUE);
		}
		for (int i = 0; i < DOM_MAXDSPOTS; ++i)
		{
			if (!m_aDominationSpotsEnabled[i])
				continue;
			SpotRed = i;
			break;
		}
		for (int i = DOM_MAXDSPOTS - 1; i >= 0; --i)
		{
			if (!m_aDominationSpotsEnabled[i])
				continue;
			SpotBlue = i;
			break;
		}

		m_apDominationSpots[SpotRed]->m_Team = DOM_RED;
		m_apDominationSpots[SpotBlue]->m_Team = DOM_BLUE;

		OnCapture(SpotRed, DOM_RED);
		OnCapture(SpotBlue, DOM_BLUE);

		m_aTeamDominationSpots[DOM_RED] = m_aTeamDominationSpots[DOM_BLUE] = 1;

		CalculateSpawns();
	}
	else
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "CONQ", "Initialization failed, not enough spots.");
}

void CGameControllerCONQ::Tick()
{
	CGameControllerDOM::Tick();

	if((m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_GAME_PAUSED) && !GameServer()->m_World.m_ResetRequested)
	{
		DoWincheckMatch();
	}
}

void CGameControllerCONQ::EndMatch()
{
	CGameControllerDOM::SendBroadcast(-1, "");
	IGameController::EndMatch();
}

void CGameControllerCONQ::DoWincheckMatch()
{
	if (m_NumOfDominationSpots <= 1)
		return;

	//	check if teams have players alive
	for (int i = 0; i < DOM_NUMOFTEAMS; ++i)
	{
		if (m_NumOfDominationSpots == m_aTeamDominationSpots[i ^ 1])
			EndMatch(); // Opponent owns all spots -> game over
		else if (!m_aTeamDominationSpots[i])
		{
			bool PlayerAlive = false;
			for (int j = 0; j < MAX_CLIENTS; ++j)
				if (GameServer()->m_apPlayers[j] && GameServer()->m_apPlayers[j]->GetTeam() == i
						&& GameServer()->m_apPlayers[j]->GetCharacter() && GameServer()->m_apPlayers[j]->GetCharacter()->IsAlive())
					PlayerAlive = true;
			if (!PlayerAlive)
			{
				EndMatch();
				break;
			}

			if (g_Config.m_SvConqWintime && m_WinTick == -1)
				m_WinTick = Server()->Tick();
		}
	}

	// Check timelimit
	if ( (m_GameInfo.m_TimeLimit > 0 && (Server()->Tick()-m_GameStartTick) >= m_GameInfo.m_TimeLimit*Server()->TickSpeed()*60)
			|| (m_WinTick != -1 && (Server()->Tick()-m_WinTick >= g_Config.m_SvConqWintime*Server()->TickSpeed())) )
	{
	 	GameServer()->SendBroadcast("", -1);
	 	EndMatch();
	}
}

float CGameControllerCONQ::GetRespawnDelay(bool Self)
{
	return max(IGameController::GetRespawnDelay(Self), Self ? g_Config.m_SvConqRespawnDelay + 3.0f : g_Config.m_SvConqRespawnDelay);
}

void CGameControllerCONQ::EvaluateSpawnType(CSpawnEval *pEval, int Team) const
{
	if (!m_aNumSpawnPoints[Team + 1])
		return;
	if (!m_NumOfDominationSpots)
		return CGameControllerDOM::EvaluateSpawnType(pEval, Team + 1);
	
	int SpawnSpot = -1;

	if (pEval->m_FriendlyTeam == DOM_RED)
	{
		for (int i = 0; i < DOM_MAXDSPOTS; ++i)
		{
			if (!m_aDominationSpotsEnabled[i])
				continue;
			if (m_apDominationSpots[i]->m_Team == pEval->m_FriendlyTeam) // own dspot
				SpawnSpot = i;
			else if (SpawnSpot != -1)
				break;
		}
	}
	else if (pEval->m_FriendlyTeam == DOM_BLUE)
	{
		for (int i = DOM_MAXDSPOTS - 1; i >= 0; --i)
		{
			if (!m_aDominationSpotsEnabled[i])
				continue;
			if (m_apDominationSpots[i]->m_Team == pEval->m_FriendlyTeam) // own dspot
				SpawnSpot = i;
			else if (SpawnSpot != -1)
				break;
		}
	}

	if (SpawnSpot == -1 || !m_aNumSpotSpawnPoints[SpawnSpot][Team])
	{
		pEval->m_Got = false;
		return;
	}

	// get spawn point
	pEval->m_Got = true;
	pEval->m_Pos = m_aaSpotSpawnPoints[SpawnSpot][Team][Server()->Tick() % m_aNumSpotSpawnPoints[SpawnSpot][Team]];
}

bool CGameControllerCONQ::CanSpawn(int Team, vec2 *pOutPos)
{
	// spectators can't spawn
	if(Team == TEAM_SPECTATORS || GameServer()->m_World.m_Paused || GameServer()->m_World.m_ResetRequested)
		return false;

	CSpawnEval Eval;
	Eval.m_FriendlyTeam = Team;

	if (m_NumOfDominationSpots <= 1)
		return CGameControllerDOM::CanSpawn(Team, pOutPos);

	// try first try own team spawn
	EvaluateSpawnType(&Eval, Team);

	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}

void CGameControllerCONQ::CalculateSpawns()
{
	for (int Team = 0; Team < DOM_NUMOFTEAMS; ++Team)
	{
		for (int Spot = 0; Spot < DOM_MAXDSPOTS; ++Spot)
		{
			if (m_aDominationSpotsEnabled[Spot])
				CalculateSpotSpawns(Spot, Team);
		}
	}
}

void CGameControllerCONQ::CalculateSpotSpawns(int Spot, int Team)
{
	if (Spot < 0 || Spot >= DOM_MAXDSPOTS || !m_aDominationSpotsEnabled[Spot] || !m_aNumSpawnPoints[Team + 1])
		return;

	if (m_NumOfDominationSpots <= 1)
	{
		for (int i = 0; i < m_aNumSpawnPoints[Team + 1]; ++i)
			m_aaSpotSpawnPoints[Spot][Team][m_aNumSpotSpawnPoints[Spot][Team]++] = m_aaSpawnPoints[Team + 1][i];
		return;
	}

	// get last spots for distance calculation
	int PreviousSpot = -1;
	int NextSpot = -1;

	if (Team == DOM_RED)
	{
		if (Spot > 0)
		{
			for (int i = Spot - 1; i >= 0; --i)
			{
				if (!m_aDominationSpotsEnabled[i])
					continue;
				PreviousSpot = i;
				break;
			}
		}
		for (int i = Spot + 1; i < DOM_MAXDSPOTS; ++i)
		{
			if (!m_aDominationSpotsEnabled[i])
				continue;
			NextSpot = i;
			break;
		}
	}
	else if (Team == DOM_BLUE)
	{
		if (Spot < DOM_MAXDSPOTS - 1)
		{
			for (int i = Spot + 1; i < DOM_MAXDSPOTS; ++i)
			{
				if (!m_aDominationSpotsEnabled[i])
					continue;
				PreviousSpot = i;
				break;
			}
		}
		for (int i = Spot - 1; i >= 0; --i)
		{
			if (!m_aDominationSpotsEnabled[i])
				continue;
			NextSpot = i;
			break;
		}
	}

	// get spawn point
	int NumStartpoints = 0;
	int *pStartpoint = new int[m_aNumSpawnPoints[Team + 1]];
	float *pStartpointDistance = new float[m_aNumSpawnPoints[Team + 1]];
	bool *pIsStartpointAfterPreviousSpot = new bool[m_aNumSpawnPoints[Team + 1]];

	float CurrentDistance;
	bool IsStartpointAfterPreviousSpot = true;

	for (int i = 0; i < m_aNumSpawnPoints[Team + 1]; ++i)
	{
		CurrentDistance = EvaluateSpawnPos3(m_aaSpawnPoints[Team + 1][i], Spot, NextSpot, PreviousSpot, IsStartpointAfterPreviousSpot);
		if (CurrentDistance < FLT_MAX)
		{
			pStartpoint[NumStartpoints] = i;
			pStartpointDistance[NumStartpoints] = CurrentDistance;
			pIsStartpointAfterPreviousSpot[NumStartpoints] = IsStartpointAfterPreviousSpot;
			++NumStartpoints;
		}
	}
	if (NumStartpoints > 0)
	{
		bool HasStartpointAfterPreviousSpot = false;
		for (int i = 0; i < NumStartpoints; ++i)
		{
			if (pIsStartpointAfterPreviousSpot[i])
			{
				HasStartpointAfterPreviousSpot = true;
				break;
			}
		}

		if (HasStartpointAfterPreviousSpot)
		{
			for (int i = 0; i < NumStartpoints; ++i)
			{
				if (pIsStartpointAfterPreviousSpot[i])
					m_aaSpotSpawnPoints[Spot][Team][m_aNumSpotSpawnPoints[Spot][Team]++] = m_aaSpawnPoints[Team + 1][pStartpoint[i]];
			}
		}
		else
		{
			// choose nearest spawn behind last own spot
			int ClosestStartpoint = 0;
			CurrentDistance = pStartpointDistance[0];
			for (int i = 1; i < NumStartpoints; ++i)
			{
				if (pStartpointDistance[i] < CurrentDistance)
				{
					ClosestStartpoint = i;
					CurrentDistance = pStartpointDistance[i];
				}
			}
			m_aaSpotSpawnPoints[Spot][Team][m_aNumSpotSpawnPoints[Spot][Team]++] = m_aaSpawnPoints[Team + 1][pStartpoint[ClosestStartpoint]];
		}
	}
	else
	{
		// mapping failure: pick any of the team
		for (int i = 0; i < m_aNumSpawnPoints[Team + 1]; ++i)
			m_aaSpotSpawnPoints[Spot][Team][m_aNumSpotSpawnPoints[Spot][Team]++] = m_aaSpawnPoints[Team + 1][i];
		return;
	}

	delete[] pStartpoint;
	pStartpoint = 0;
	delete[] pStartpointDistance;
	pStartpointDistance = 0;
	delete[] pIsStartpointAfterPreviousSpot;
	pIsStartpointAfterPreviousSpot = 0;
}

float CGameControllerCONQ::EvaluateSpawnPos3(vec2 Pos, int LastOwnSpot, int LastEnemySpot, int PreviousOwnSpot, bool &IsStartpointAfterPreviousSpot) const
{
	float DistanceOwnSpot = min(FLT_MAX, distance(Pos, m_apDominationSpots[LastOwnSpot]->m_Pos));
	float DistanceEnemySpot = FLT_MAX;

	if (LastEnemySpot >= 0)
	{
		DistanceEnemySpot = min(FLT_MAX, distance(Pos, m_apDominationSpots[LastEnemySpot]->m_Pos));
		float DistanceOwnToEnemySpot = min(FLT_MAX, distance(m_apDominationSpots[LastOwnSpot]->m_Pos, m_apDominationSpots[LastEnemySpot]->m_Pos));

		if (DistanceOwnSpot > DistanceEnemySpot || DistanceEnemySpot < (DistanceOwnToEnemySpot - (DistanceOwnSpot*0.3f)))
			return FLT_MAX;
	}

	if (PreviousOwnSpot == -1)
		IsStartpointAfterPreviousSpot = true;
	else
	{
		float DistancePreviousToOwnSpot = min(FLT_MAX, distance(m_apDominationSpots[PreviousOwnSpot]->m_Pos, m_apDominationSpots[LastOwnSpot]->m_Pos));
		if (DistanceOwnSpot < DistancePreviousToOwnSpot || (LastEnemySpot >= 0 && DistanceEnemySpot < min(FLT_MAX, distance(m_apDominationSpots[PreviousOwnSpot]->m_Pos, m_apDominationSpots[LastEnemySpot]->m_Pos))))
			IsStartpointAfterPreviousSpot = true;
		else
			IsStartpointAfterPreviousSpot = false;
	}

	return DistanceOwnSpot;
}

void CGameControllerCONQ::UpdateBroadcast()
{
	if (m_WinTick == -1)
		CGameControllerDOM::UpdateBroadcast();
	else if (!((Server()->Tick() - m_WinTick) % Server()->TickSpeed()))
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "%s will win in %2i seconds.", m_apDominationSpots[0]->GetTeamName(!m_aTeamscore[TEAM_RED] ? DOM_BLUE : DOM_RED)
				, g_Config.m_SvConqWintime - (Server()->Tick() - m_WinTick) / Server()->TickSpeed());
		CGameControllerDOM::SendBroadcast(-1, aBuf);
	}
}

void CGameControllerCONQ::OnCapture(int SpotNumber, int Team)
{
	CGameControllerDOM::OnCapture(SpotNumber, Team);

	++m_aTeamscore[Team];
	m_apDominationSpots[SpotNumber]->Unlock(Team);

	bool HasAllPreviousSpots = true;

	// do un-/locking
	if (Team == DOM_RED)
	{
		if (SpotNumber > 0)
		{
			for (int i = SpotNumber - 1; i >= 0; --i)
			{
				if (!m_aDominationSpotsEnabled[i])
					continue;
				if (m_apDominationSpots[i]->m_Team != DOM_RED)
				{
					HasAllPreviousSpots = false;
					break;
				}
			}
		}
		if (HasAllPreviousSpots && SpotNumber < DOM_MAXDSPOTS - 1)
		{
			for (int i = SpotNumber + 1; i < DOM_MAXDSPOTS; ++i)
			{
				if (!m_aDominationSpotsEnabled[i])
					continue;
				if (m_apDominationSpots[i]->m_Team != DOM_RED)
				{
					m_apDominationSpots[i]->Unlock(DOM_RED);
					break;
				}
			}
		}
	}
	else
	{
		if (SpotNumber < DOM_MAXDSPOTS - 1)
		{
			for (int i = SpotNumber + 1; i < DOM_MAXDSPOTS; ++i)
			{
				if (!m_aDominationSpotsEnabled[i])
					continue;
				if (m_apDominationSpots[i]->m_Team != DOM_BLUE)
				{
					HasAllPreviousSpots = false;
					break;
				}
			}
		}
		if (HasAllPreviousSpots && SpotNumber > 0)
		{
			for (int i = SpotNumber - 1; i >= 0; --i)
			{
				if (!m_aDominationSpotsEnabled[i])
					continue;
				if (m_apDominationSpots[i]->m_Team != DOM_BLUE)
				{
					m_apDominationSpots[i]->Unlock(DOM_BLUE);
					break;
				}
			}
		}
	}
}

void CGameControllerCONQ::OnNeutralize(int SpotNumber, int Team)
{
	CGameControllerDOM::OnNeutralize(SpotNumber, Team);

	m_WinTick = -1;

	--m_aTeamscore[Team ^ 1];

	// do un-/locking
	if (Team == DOM_RED)
	{
		if (SpotNumber > 0)
		{
			for (int i = SpotNumber - 1; i >= 0; --i)
			{
				if (!m_aDominationSpotsEnabled[i] || m_apDominationSpots[i]->m_Team == DOM_BLUE)
					continue;
				m_apDominationSpots[i]->Lock(DOM_BLUE);
				break;
			}
		}
		if (SpotNumber < DOM_MAXDSPOTS - 1)
		{
			for (int i = SpotNumber + 1; i < DOM_MAXDSPOTS; ++i)
			{
				if (!m_aDominationSpotsEnabled[i])
					continue;
				if (m_apDominationSpots[i]->m_Team != DOM_BLUE)
				{
					m_apDominationSpots[SpotNumber]->Lock(DOM_BLUE);
					break;
				}
			}
		}
	}
	else
	{
		if (SpotNumber < DOM_MAXDSPOTS - 1)
		{
			for (int i = SpotNumber + 1; i < DOM_MAXDSPOTS; ++i)
			{
				if (!m_aDominationSpotsEnabled[i] || m_apDominationSpots[i]->m_Team == DOM_RED)
					continue;
				m_apDominationSpots[i]->Lock(DOM_RED);
				break;
			}
		}
		if (SpotNumber > 0)
		{
			for (int i = SpotNumber - 1; i >= 0; --i)
			{
				if (!m_aDominationSpotsEnabled[i])
					continue;
				if (m_apDominationSpots[i]->m_Team != DOM_RED)
				{
					m_apDominationSpots[SpotNumber]->Lock(DOM_RED);
					break;
				}
			}
		}
	}
}

int CGameControllerCONQ::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	if (!m_aTeamDominationSpots[pVictim->GetPlayer()->GetTeam()] && Weapon != WEAPON_GAME && m_NumOfDominationSpots > 1)
		CGameControllerDOM::SendChat(pVictim->GetPlayer()->GetCID(), "You can't respawn while your team does not control any spot.");
	return CGameControllerDOM::OnCharacterDeath(pVictim, pKiller, Weapon);
}

const char* CGameControllerCONQ::GetBroadcastPre(int SpotNumber) const
{
	if (SpotNumber < 0 || SpotNumber >= DOM_MAXDSPOTS)
		return "";

	if (m_apDominationSpots[SpotNumber]->m_Team == DOM_NEUTRAL)
	{
		if (m_apDominationSpots[SpotNumber]->m_IsGettingCaptured && m_apDominationSpots[SpotNumber]->m_CapTeam == DOM_BLUE)
			return "[--";
		else if (!m_apDominationSpots[SpotNumber]->IsLocked(DOM_RED))
			return ":--";
		else
			return "(--";
	}
	else
	{
		if (m_apDominationSpots[SpotNumber]->m_Team == DOM_RED)
			return "{--";
		else
		{
			if (m_apDominationSpots[SpotNumber]->IsLocked(DOM_RED))
				return "[--";
			else
				return ":--";
		}
	}
}

const char* CGameControllerCONQ::GetBroadcastPost(int SpotNumber) const
{
	if (SpotNumber < 0 || SpotNumber >= DOM_MAXDSPOTS)
		return "";

	if (m_apDominationSpots[SpotNumber]->m_Team == DOM_NEUTRAL)
	{
		if (m_apDominationSpots[SpotNumber]->m_IsGettingCaptured && m_apDominationSpots[SpotNumber]->m_CapTeam == DOM_RED)
			return "--}";
		else if (!m_apDominationSpots[SpotNumber]->IsLocked(DOM_BLUE))
			return "--:";
		else
			return "--)";
	}
	else
	{
		if (m_apDominationSpots[SpotNumber]->m_Team == DOM_RED)
		{
			if (m_apDominationSpots[SpotNumber]->IsLocked(DOM_BLUE))
				return "--}";
			else
				return "--:";
		}
		else
			return "--]";
	}
}

void CGameControllerCONQ::SendChatInfo(int ClientID)
{
	CGameControllerDOM::SendChat(ClientID, "GAMETYPE: CONQUEST");
	CGameControllerDOM::SendChat(ClientID, "Capture domination spots.");
	CGameControllerDOM::SendChat(ClientID, "Tip: Capture together to reduce the required time.");
	CGameControllerDOM::SendChat(ClientID, "Win the match by capturing all spots.");
	CGameControllerDOM::SendChat(ClientID, "(This mod is enjoyed best with enabled broadcast color.)");
}

void CGameControllerCONQ::SendChatCommand(int ClientID, const char *pCommand)
{
	if (ClientID >= 0 && ClientID < MAX_CLIENTS)
	{
		if (str_comp_nocase(pCommand, "/locks") == 0)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "Locks:", GameServer()->ModVersion());
			CGameControllerDOM::SendChat(ClientID, aBuf);

			SendChat(ClientID, "——————————————————————————————");
			for (int i = 0; i < DOM_MAXDSPOTS; ++i)
			{
				if (!m_aDominationSpotsEnabled[i])
					continue;
				str_format(aBuf, sizeof(aBuf), "%c: %i | %i", m_apDominationSpots[i]->GetSpotName()[0], m_apDominationSpots[i]->IsLocked(DOM_RED), m_apDominationSpots[i]->IsLocked(DOM_BLUE));
				CGameControllerDOM::SendChat(ClientID, aBuf);
			}
		}
		else
			CGameControllerDOM::SendChatCommand(ClientID, pCommand);
	}
}