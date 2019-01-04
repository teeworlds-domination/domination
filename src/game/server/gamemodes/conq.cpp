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

	mem_zero(m_aaaSpotSpawnPoints, DOM_MAXDSPOTS * DOM_NUMOFTEAMS * 64);
	mem_zero(m_aaNumSpotSpawnPoints, DOM_MAXDSPOTS * DOM_NUMOFTEAMS);

	SetCapTimes(g_Config.m_SvConqCapTimes);
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
			LockSpot(i, DOM_RED);
			LockSpot(i, DOM_BLUE);
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

		m_apDominationSpots[SpotRed]->SetTeam(DOM_RED);
		m_apDominationSpots[SpotBlue]->SetTeam(DOM_BLUE);

		OnCapture(SpotRed, DOM_RED);
		OnCapture(SpotBlue, DOM_BLUE);

		m_aNumOfTeamDominationSpots[DOM_RED] = m_aNumOfTeamDominationSpots[DOM_BLUE] = 1;

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
		if (m_NumOfDominationSpots == m_aNumOfTeamDominationSpots[i ^ 1])
			EndMatch(); // Opponent owns all spots -> game over
		else if (!m_aNumOfTeamDominationSpots[i])
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
		else if (m_aNumOfTeamDominationSpots[i ^ 1] && m_WinTick != -1)
			m_WinTick = -1; // both teams have spots -> reset WinTick
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

void CGameControllerCONQ::EvaluateSpawnTypeConq(CSpawnEval *pEval, int Team) const
{
	if (!m_aNumSpawnPoints[Team + 1])
		return;
	
	int SpawnSpot = -1;

	if (pEval->m_FriendlyTeam == DOM_RED)
	{
		for (int i = 0; i < DOM_MAXDSPOTS; ++i)
		{
			if (!m_aDominationSpotsEnabled[i])
				continue;
			if (m_apDominationSpots[i]->GetTeam() == pEval->m_FriendlyTeam) // own dspot
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
			if (m_apDominationSpots[i]->GetTeam() == pEval->m_FriendlyTeam) // own dspot
				SpawnSpot = i;
			else if (SpawnSpot != -1)
				break;
		}
	}

	if (SpawnSpot == -1 || !m_aaNumSpotSpawnPoints[SpawnSpot][Team])
	{
		pEval->m_Got = false;
		return;
	}

	// get spawn point
	pEval->m_Got = true;
	pEval->m_Pos = m_aaaSpotSpawnPoints[SpawnSpot][Team][Server()->Tick() % m_aaNumSpotSpawnPoints[SpawnSpot][Team]];
}

bool CGameControllerCONQ::CanSpawn(int Team, vec2 *pOutPos)
{
	// spectators can't spawn
	if(Team == TEAM_SPECTATORS || GameServer()->m_World.m_Paused || GameServer()->m_World.m_ResetRequested)
		return false;

	if (m_NumOfDominationSpots <= 1)
		return CGameControllerDOM::CanSpawn(Team, pOutPos);

	CSpawnEval Eval;
	Eval.m_FriendlyTeam = Team;

	// try first try own team spawn
	EvaluateSpawnTypeConq(&Eval, Team);

	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}

void CGameControllerCONQ::CalculateSpawns()
{
	for (int Spot = 0; Spot < DOM_MAXDSPOTS; ++Spot)
		for (int Team = 0; Team < DOM_NUMOFTEAMS; ++Team)
			m_aaNumSpotSpawnPoints[Spot][Team] = 0;

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
			m_aaaSpotSpawnPoints[Spot][Team][m_aaNumSpotSpawnPoints[Spot][Team]++] = m_aaSpawnPoints[Team + 1][i];
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
		CurrentDistance = EvaluateSpawnPosConq(m_aaSpawnPoints[Team + 1][i], Spot, NextSpot, PreviousSpot, IsStartpointAfterPreviousSpot);
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
					m_aaaSpotSpawnPoints[Spot][Team][m_aaNumSpotSpawnPoints[Spot][Team]++] = m_aaSpawnPoints[Team + 1][pStartpoint[i]];
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
			m_aaaSpotSpawnPoints[Spot][Team][m_aaNumSpotSpawnPoints[Spot][Team]++] = m_aaSpawnPoints[Team + 1][pStartpoint[ClosestStartpoint]];
		}
	}
	else
	{
		// mapping failure: pick any of the team
		for (int i = 0; i < m_aNumSpawnPoints[Team + 1]; ++i)
			m_aaaSpotSpawnPoints[Spot][Team][m_aaNumSpotSpawnPoints[Spot][Team]++] = m_aaSpawnPoints[Team + 1][i];
		return;
	}

	delete[] pStartpoint;
	pStartpoint = 0;
	delete[] pStartpointDistance;
	pStartpointDistance = 0;
	delete[] pIsStartpointAfterPreviousSpot;
	pIsStartpointAfterPreviousSpot = 0;
}

float CGameControllerCONQ::EvaluateSpawnPosConq(vec2 Pos, int LastOwnSpot, int LastEnemySpot, int PreviousOwnSpot, bool &IsStartpointAfterPreviousSpot) const
{
	float DistanceOwnSpot = min(FLT_MAX, distance(Pos, m_apDominationSpots[LastOwnSpot]->GetPos()));
	float DistanceEnemySpot = FLT_MAX;

	if (LastEnemySpot >= 0)
	{
		DistanceEnemySpot = min(FLT_MAX, distance(Pos, m_apDominationSpots[LastEnemySpot]->GetPos()));
		float DistanceOwnToEnemySpot = min(FLT_MAX, distance(m_apDominationSpots[LastOwnSpot]->GetPos(), m_apDominationSpots[LastEnemySpot]->GetPos()));

		if (DistanceOwnSpot > DistanceEnemySpot || DistanceEnemySpot < (DistanceOwnToEnemySpot - (DistanceOwnSpot*0.3f)))
			return FLT_MAX;
	}

	if (PreviousOwnSpot == -1)
		IsStartpointAfterPreviousSpot = true;
	else
	{
		float DistancePreviousToOwnSpot = min(FLT_MAX, distance(m_apDominationSpots[PreviousOwnSpot]->GetPos(), m_apDominationSpots[LastOwnSpot]->GetPos()));
		if (DistanceOwnSpot < DistancePreviousToOwnSpot || (LastEnemySpot >= 0 && DistanceEnemySpot < min(FLT_MAX, distance(m_apDominationSpots[PreviousOwnSpot]->GetPos(), m_apDominationSpots[LastEnemySpot]->GetPos()))))
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
		str_format(aBuf, sizeof(aBuf), "%s will win in %2i seconds.", GetTeamName(!m_aTeamscore[TEAM_RED] ? DOM_BLUE : DOM_RED)
				, g_Config.m_SvConqWintime - (Server()->Tick() - m_WinTick) / Server()->TickSpeed());
		CGameControllerDOM::SendBroadcast(-1, aBuf);
	}
}

void CGameControllerCONQ::OnCapture(int SpotNumber, int Team)
{
	CGameControllerDOM::OnCapture(SpotNumber, Team);

	++m_aTeamscore[Team];
	UnlockSpot(SpotNumber, Team);

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
				if (m_apDominationSpots[i]->GetTeam() != DOM_RED)
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
				if (m_apDominationSpots[i]->GetTeam() != DOM_RED)
				{
					UnlockSpot(i, DOM_RED);
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
				if (m_apDominationSpots[i]->GetTeam() != DOM_BLUE)
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
				if (m_apDominationSpots[i]->GetTeam() != DOM_BLUE)
				{
					UnlockSpot(i, DOM_BLUE);
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
				if (!m_aDominationSpotsEnabled[i] || m_apDominationSpots[i]->GetTeam() == DOM_BLUE)
					continue;
				LockSpot(i, DOM_BLUE);
				break;
			}
		}
		if (SpotNumber < DOM_MAXDSPOTS - 1)
		{
			for (int i = SpotNumber + 1; i < DOM_MAXDSPOTS; ++i)
			{
				if (!m_aDominationSpotsEnabled[i])
					continue;
				if (m_apDominationSpots[i]->GetTeam() != DOM_BLUE)
				{
					LockSpot(SpotNumber, DOM_BLUE);
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
				if (!m_aDominationSpotsEnabled[i] || m_apDominationSpots[i]->GetTeam() == DOM_RED)
					continue;
				LockSpot(i, DOM_RED);
				break;
			}
		}
		if (SpotNumber > 0)
		{
			for (int i = SpotNumber - 1; i >= 0; --i)
			{
				if (!m_aDominationSpotsEnabled[i])
					continue;
				if (m_apDominationSpots[i]->GetTeam() != DOM_RED)
				{
					LockSpot(SpotNumber, DOM_RED);
					break;
				}
			}
		}
	}
}

int CGameControllerCONQ::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	if (!m_aNumOfTeamDominationSpots[pVictim->GetPlayer()->GetTeam()] && Weapon != WEAPON_GAME && m_NumOfDominationSpots > 1)
		CGameControllerDOM::SendChat(pVictim->GetPlayer()->GetCID(), "You can't respawn while your team does not control any spot.");
	return CGameControllerDOM::OnCharacterDeath(pVictim, pKiller, Weapon);
}

void CGameControllerCONQ::UnlockSpot(int Spot, int Team)
{
	m_apDominationSpots[Spot]->Unlock(Team);
	m_aLastBroadcastState[Spot] = -2; // force update
}

void CGameControllerCONQ::LockSpot(int Spot, int Team)
{
	m_apDominationSpots[Spot]->Lock(Team);
	m_aLastBroadcastState[Spot] = -2; // force update
}

void CGameControllerCONQ::AddColorizedOpenParenthesis(int SpotNumber, char *pBuf, int &rCurrPos, int MarkerPos) const
{
	if (MarkerPos == 0)
	{
		AddColorizedMarker(SpotNumber, pBuf, rCurrPos);
		return;
	}

	if (m_apDominationSpots[SpotNumber]->GetTeam() == DOM_NEUTRAL)
	{
		if (m_apDominationSpots[SpotNumber]->IsGettingCaptured() && m_apDominationSpots[SpotNumber]->GetCapTeam() == DOM_RED)
			AddColorizedSymbol(pBuf, rCurrPos, DOM_RED, '{');
		else
			AddColorizedSymbol(pBuf, rCurrPos, DOM_NEUTRAL, m_apDominationSpots[SpotNumber]->IsLocked(DOM_RED) || m_apDominationSpots[SpotNumber]->IsGettingCaptured()? '(' : ':');
	}
	else if (m_apDominationSpots[SpotNumber]->GetTeam() == DOM_RED)
		AddColorizedSymbol(pBuf, rCurrPos, DOM_RED, '{');
	else
	{
		if (m_apDominationSpots[SpotNumber]->IsGettingCaptured())
			AddColorizedSymbol(pBuf, rCurrPos, DOM_NEUTRAL, '(');
		else
			AddColorizedSymbol(pBuf, rCurrPos, DOM_BLUE, m_apDominationSpots[SpotNumber]->IsLocked(DOM_RED)? '[' : ':');
	}
}

void CGameControllerCONQ::AddColorizedCloseParenthesis(int SpotNumber, char *pBuf, int &rCurrPos, int MarkerPos) const
{
	if (MarkerPos == 5)
	{
		AddColorizedMarker(SpotNumber, pBuf, rCurrPos);
		return;
	}

	if (m_apDominationSpots[SpotNumber]->GetTeam() == DOM_NEUTRAL)
	{
		if (m_apDominationSpots[SpotNumber]->IsGettingCaptured() && m_apDominationSpots[SpotNumber]->GetCapTeam() == DOM_BLUE)
			AddColorizedSymbol(pBuf, rCurrPos, DOM_BLUE, ']');
		else
			AddColorizedSymbol(pBuf, rCurrPos, DOM_NEUTRAL, m_apDominationSpots[SpotNumber]->IsLocked(DOM_BLUE) || m_apDominationSpots[SpotNumber]->IsGettingCaptured()? ')' : ':');
	}
	else if (m_apDominationSpots[SpotNumber]->GetTeam() == DOM_RED)
	{
		if (m_apDominationSpots[SpotNumber]->IsGettingCaptured())
			AddColorizedSymbol(pBuf, rCurrPos, DOM_NEUTRAL, ')');
		else
			AddColorizedSymbol(pBuf, rCurrPos, DOM_RED, m_apDominationSpots[SpotNumber]->IsLocked(DOM_BLUE)? '}' : ':');
	}
	else // DOM_BLUE
		AddColorizedSymbol(pBuf, rCurrPos, DOM_BLUE, ']');
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
				str_format(aBuf, sizeof(aBuf), "%c: %i | %i", GetSpotName(i)[0], m_apDominationSpots[i]->IsLocked(DOM_RED), m_apDominationSpots[i]->IsLocked(DOM_BLUE));
				CGameControllerDOM::SendChat(ClientID, aBuf);
			}
		}
		else
			CGameControllerDOM::SendChatCommand(ClientID, pCommand);
	}
}