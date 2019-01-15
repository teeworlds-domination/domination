/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <float.h>
#include <stdio.h>

#include <engine/shared/config.h>

#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/entities/character.h>
#include <game/server/entities/dspot.h>
#include <game/server/entities/pickup.h>

#include "conq.h"

CGameControllerCONQ::CGameControllerCONQ(CGameContext *pGameServer)
: CGameControllerDOM(pGameServer)
{
	m_pGameType = "CONQ";

	m_WinTick = -1;

	mem_zero(m_aaaSpotSpawnPoints, DOM_MAXDSPOTS * DOM_NUMOFTEAMS * 64);
	mem_zero(m_aaNumSpotSpawnPoints, DOM_MAXDSPOTS * DOM_NUMOFTEAMS);

	SetCapTimes(g_Config.m_SvConqCapTimes);

	m_GameInfo.m_ScoreLimit = 5;
}

void CGameControllerCONQ::Init()
{
	CGameControllerDOM::Init();

	m_WinTick = -1;

	if (m_NumOfDominationSpots > 1)
	{
		int Spot = -1;
		while ((Spot = GetNextSpot(Spot)) > -1)
		{
			LockSpot(Spot, DOM_RED);
			LockSpot(Spot, DOM_BLUE);
		}

		m_aNumOfTeamDominationSpots[DOM_RED] = m_aNumOfTeamDominationSpots[DOM_BLUE] = 1;

		if ((Spot = GetNextSpot(-1)) > -1)
		{
			UnlockSpot(Spot, DOM_RED);
			m_apDominationSpots[Spot]->SetTeam(DOM_RED);
			OnCapture(Spot, DOM_RED, 0, 0);
		}
		if ((Spot = GetPreviousSpot(DOM_MAXDSPOTS)) > -1)
		{
			UnlockSpot(Spot, DOM_BLUE);
			m_apDominationSpots[Spot]->SetTeam(DOM_BLUE);
			OnCapture(Spot, DOM_BLUE, 0, 0);
		}

		CalculateSpawns();
	}
	else
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "CONQ", "Initialization failed, not enough spots.");
}

void CGameControllerCONQ::OnReset()
{
	CGameControllerDOM::OnReset();

	m_GameInfo.m_ScoreLimit = m_NumOfDominationSpots;
}

void CGameControllerCONQ::Tick()
{
	CGameControllerDOM::Tick();

	if(m_GameState == IGS_GAME_RUNNING && !GameServer()->m_World.m_ResetRequested)
	{
		DoWincheckMatch();
	}
	else if (m_GameState != IGS_GAME_RUNNING)
	{
		if (m_WinTick != -1)
			++m_WinTick;
	}
}

void CGameControllerCONQ::EndMatch()
{
	m_WinTick = -1;
	CGameControllerDOM::SendBroadcast(-1, "");
	IGameController::EndMatch();
}

void CGameControllerCONQ::DoWincheckMatch()
{
	if (m_NumOfDominationSpots <= 1)
		return;

	//	check if teams have players alive
	for (int Team = 0; Team < DOM_NUMOFTEAMS; ++Team)
	{
		if (!m_aNumOfTeamDominationSpots[Team])
		{
			bool PlayerAlive = false;
			for (int cid = 0; cid < MAX_CLIENTS; ++cid)
				if (GameServer()->m_apPlayers[cid] && GameServer()->m_apPlayers[cid]->GetTeam() == Team
						&& GameServer()->m_apPlayers[cid]->GetCharacter() && GameServer()->m_apPlayers[cid]->GetCharacter()->IsAlive())
					PlayerAlive = true;
			if (!PlayerAlive)
			{
				EndMatch();
				break;
			}

			if (g_Config.m_SvConqWintime && m_WinTick == -1)
				m_WinTick = Server()->Tick();
		}
		if (m_NumOfDominationSpots == m_aNumOfTeamDominationSpots[Team ^ 1])
		{
			if (m_WinTick != -1)
			{
				bool NoCapturing = true;
				int Spot = -1;
				while((Spot = GetNextSpot(Spot)) > -1)
				{
					if (m_apDominationSpots[Spot]->IsGettingCaptured())
					{
						NoCapturing = false;
						break;
					}
				}
				if (NoCapturing)
				{
					EndMatch(); // Opponent owns all spots -> game over
					break;
				}
			}
			else
			{
				EndMatch();
				break;
			}
		}
	}

	// Check timelimit
	if ( (m_GameInfo.m_TimeLimit > 0 && (Server()->Tick()-m_GameStartTick) >= m_GameInfo.m_TimeLimit*Server()->TickSpeed()*60)
			|| (m_WinTick != -1 && (Server()->Tick()-m_WinTick >= g_Config.m_SvConqWintime*Server()->TickSpeed())) )
	 	EndMatch();
}

float CGameControllerCONQ::GetRespawnDelay(bool Self) const
{
	return max(IGameController::GetRespawnDelay(Self), Self ? g_Config.m_SvConqRespawnDelay + 3.0f : g_Config.m_SvConqRespawnDelay);
}

void CGameControllerCONQ::EvaluateSpawnTypeConq(CSpawnEval *pEval, int Team) const
{
	if (!m_aNumSpawnPoints[Team + 1])
		return;
	
	int SpawnSpot = -1;

	int Spot = -1;
	while ((Spot = GetNextSpot(Spot, pEval->m_FriendlyTeam)) > -1)
	{
		if (m_apDominationSpots[Spot]->GetTeam() == pEval->m_FriendlyTeam) // own dspot
			SpawnSpot = Spot;
		else if (SpawnSpot != -1) // first non-own spot
			break;
	}

	if (SpawnSpot == -1 || !m_aaNumSpotSpawnPoints[SpawnSpot][Team])
	{
		pEval->m_Got = false;
		return;
	}

	int NumStartpoints = 0;
	int *pStartpoint = new int[m_aaNumSpotSpawnPoints[SpawnSpot][Team]];
	for(int i = 0; i < m_aaNumSpotSpawnPoints[SpawnSpot][Team]; i++)
	{
		// check if the position is occupado
		CCharacter *aEnts[MAX_CLIENTS];
		int Num = GameServer()->m_World.FindEntities(m_aaaSpotSpawnPoints[SpawnSpot][Team][i], 64, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		vec2 Positions[5] = { vec2(0.0f, 0.0f), vec2(-32.0f, 0.0f), vec2(0.0f, -32.0f), vec2(32.0f, 0.0f), vec2(0.0f, 32.0f) };	// start, left, up, right, down
		int Result = -1;
		for(int Index = 0; Index < 5 && Result == -1; ++Index)
		{
			Result = Index;
			for(int c = 0; c < Num; ++c)
				if(GameServer()->Collision()->CheckPoint(m_aaaSpotSpawnPoints[SpawnSpot][Team][i]+Positions[Index]) ||
					distance(aEnts[c]->GetPos(), m_aaaSpotSpawnPoints[SpawnSpot][Team][i]+Positions[Index]) <= aEnts[c]->GetProximityRadius())
				{
					Result = -1;
					break;
				}
		}
		if(Result == -1)
			continue;	// try next spawn point

		// check distance to other player
		vec2 P = m_aaaSpotSpawnPoints[SpawnSpot][Team][i]+Positions[Result];
		float S = IGameController::EvaluateSpawnPos(pEval, P);
		if(!pEval->m_Got || pEval->m_Score > S)
		{
			pEval->m_Got = true;
			pEval->m_Score = S;
			pEval->m_Pos = P;
			pStartpoint[NumStartpoints++] = i;
		}
	}

	if (NumStartpoints)
	{
		pEval->m_Got = true;
		pEval->m_Pos = m_aaaSpotSpawnPoints[SpawnSpot][Team][pStartpoint[Server()->Tick() % NumStartpoints]];
	}
	delete[] pStartpoint;
	pStartpoint = 0;
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

	// only try own team spawn
	EvaluateSpawnTypeConq(&Eval, Team);

	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}

void CGameControllerCONQ::CalculateSpawns()
{
	for (int Team = 0; Team < DOM_NUMOFTEAMS; ++Team)
		for (int Spot = 0; Spot < DOM_MAXDSPOTS; ++Spot)
			m_aaNumSpotSpawnPoints[Spot][Team] = 0;

	int Spot;
	for (int Team = 0; Team < DOM_NUMOFTEAMS; ++Team)
	{
		Spot = -1;
		while ((Spot = GetNextSpot(Spot, Team)) > -1)
			CalculateSpotSpawns(Spot, Team);
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
	int PreviousSpot = GetPreviousSpot(Spot, Team);
	int NextSpot     = GetNextSpot(Spot, Team);

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
			if (PreviousSpot > -1)
			{
				// pick all of previous spot
				for (int i = 0; i < m_aaNumSpotSpawnPoints[PreviousSpot][Team]; ++i)
				{
					m_aaaSpotSpawnPoints[Spot][Team][m_aaNumSpotSpawnPoints[Spot][Team]++] = m_aaaSpotSpawnPoints[PreviousSpot][Team][i];
				}
			}
			if (!m_aaNumSpotSpawnPoints[Spot][Team])
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

float CGameControllerCONQ::EvaluateSpawnPosConq(vec2 Pos, int SpawnSpot, int NextSpot, int PreviousSpot, bool &IsStartpointAfterPreviousSpot) const
{
	float DistanceSpawnSpot = min(FLT_MAX, distance(Pos, m_apDominationSpots[SpawnSpot]->GetPos()));
	float DistanceNextSpot = FLT_MAX;

	if (NextSpot > -1)
	{
		DistanceNextSpot = min(FLT_MAX, distance(Pos, m_apDominationSpots[NextSpot]->GetPos()));
		float DistanceSpawnToNextSpot = min(FLT_MAX, distance(m_apDominationSpots[SpawnSpot]->GetPos(), m_apDominationSpots[NextSpot]->GetPos()));

		if (DistanceSpawnSpot > DistanceNextSpot || DistanceNextSpot < (DistanceSpawnToNextSpot - (DistanceSpawnSpot * 0.3f)))
			return FLT_MAX;
	}

	if (PreviousSpot == -1)
		IsStartpointAfterPreviousSpot = true;
	else
	{
		float DistancePreviousToSpawnSpot = min(FLT_MAX, distance(m_apDominationSpots[PreviousSpot]->GetPos(), m_apDominationSpots[SpawnSpot]->GetPos()));
			if (DistanceSpawnSpot < DistancePreviousToSpawnSpot || (NextSpot >= 0 && DistanceNextSpot < min(FLT_MAX, distance(Pos, m_apDominationSpots[PreviousSpot]->GetPos()))))
			IsStartpointAfterPreviousSpot = true;
		else
			IsStartpointAfterPreviousSpot = false;
	}

	return DistanceSpawnSpot;
}

bool CGameControllerCONQ::SendPersonalizedBroadcast(int ClientID)
{
	if (m_WinTick != -1)
	{
		char aBuf[128];
		str_format(aBuf, sizeof(aBuf), "%s will win in %2i seconds.", GetTeamName(!m_aTeamscore[TEAM_RED] ? DOM_BLUE : DOM_RED)
				, g_Config.m_SvConqWintime - (Server()->Tick() - m_WinTick) / Server()->TickSpeed());
		CGameControllerDOM::SendBroadcast(-1, aBuf);
		return true;
	}
	return CGameControllerDOM::SendPersonalizedBroadcast(ClientID);
}

void CGameControllerCONQ::OnCapture(int Spot, int Team, int NumOfCapCharacters, CCharacter* apCapCharacters[MAX_PLAYERS])
{
	CGameControllerDOM::OnCapture(Spot, Team, NumOfCapCharacters, apCapCharacters);

	m_WinTick = -1;
	m_aTeamscore[Team] = m_aNumOfTeamDominationSpots[Team];
	m_aTeamscore[Team ^ 1] = m_aNumOfTeamDominationSpots[Team ^ 1];

	ShiftLocks(Spot, Team);
}

void CGameControllerCONQ::OnAbortCapturing(int Spot)
{
	ShiftLocks(Spot, m_apDominationSpots[Spot]->GetTeam());
}

void CGameControllerCONQ::ShiftLocks(int Spot, int Team)
{
	int PreviousSpot = GetPreviousSpot(Spot, Team);
	if (PreviousSpot == -1
			|| (m_apDominationSpots[PreviousSpot]->GetTeam() == Team && !m_apDominationSpots[PreviousSpot]->IsGettingCaptured()))
	{
		int NextSpot = GetNextSpot(Spot, Team);
		if (NextSpot > -1)
		{
			if (m_apDominationSpots[NextSpot]->GetTeam() != Team)
			{
				// shift this
				if (PreviousSpot > -1)
					LockSpot(PreviousSpot, Team ^ 1);
				UnlockSpot(NextSpot, Team);
			}
			else if (!m_apDominationSpots[NextSpot]->IsGettingCaptured())
			{
				// shift next
				int NextAfterNextSpot = GetNextSpot(NextSpot, Team);
				if (NextAfterNextSpot > -1)
				{
					LockSpot(Spot, Team ^ 1);
					UnlockSpot(NextAfterNextSpot, Team);
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

int CGameControllerCONQ::GetNextSpot(int Spot, int Team) const
{
	return Team == DOM_BLUE? GetPreviousSpot(Spot == -1? DOM_MAXDSPOTS : Spot) : GetNextSpot(Spot);
}

int CGameControllerCONQ::GetPreviousSpot(int Spot, int Team) const
{
	return Team == DOM_BLUE? GetNextSpot(Spot == DOM_MAXDSPOTS? -1 : Spot) : GetPreviousSpot(Spot);
}

void CGameControllerCONQ::AddColorizedOpenParenthesis(int Spot, char *pBuf, int &rCurrPos, int MarkerPos) const
{
	if (MarkerPos == 0)
	{
		AddColorizedMarker(Spot, pBuf, rCurrPos);
		return;
	}

	CGameControllerDOM::AddColorizedOpenParenthesis(Spot, pBuf, rCurrPos, MarkerPos);
	if (m_GameState != IGS_END_MATCH && !m_apDominationSpots[Spot]->IsGettingCaptured() && m_apDominationSpots[Spot]->GetTeam() != DOM_RED && !m_apDominationSpots[Spot]->IsLocked(DOM_RED))
		pBuf[rCurrPos - 1] = ':';
}

void CGameControllerCONQ::AddColorizedCloseParenthesis(int Spot, char *pBuf, int &rCurrPos, int MarkerPos) const
{
	if (MarkerPos == 5)
	{
		AddColorizedMarker(Spot, pBuf, rCurrPos);
		return;
	}

	CGameControllerDOM::AddColorizedCloseParenthesis(Spot, pBuf, rCurrPos, MarkerPos);
	if (m_GameState != IGS_END_MATCH && !m_apDominationSpots[Spot]->IsGettingCaptured() && m_apDominationSpots[Spot]->GetTeam() != DOM_BLUE && !m_apDominationSpots[Spot]->IsLocked(DOM_BLUE))
		pBuf[rCurrPos - 1] = ':';
}

void CGameControllerCONQ::SendChatInfo(int ClientID) const
{
	CGameControllerDOM::SendChat(ClientID, "GAMETYPE: CONQUEST");
	CGameControllerDOM::SendChat(ClientID, "——————————————————————————");
	CGameControllerDOM::SendChat(ClientID, "Capture domination spots.");
	CGameControllerDOM::SendChat(ClientID, "Tip: Capturing together speeds it up.");
	CGameControllerDOM::SendChat(ClientID, "Own all spots to win the match.");
	CGameControllerDOM::SendChat(ClientID, "(This mod is enjoyed best with enabled broadcast color.)");
}

void CGameControllerCONQ::ShowSpawns(int Spot) const
{
	if((m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_GAME_PAUSED) && !GameServer()->m_World.m_ResetRequested)
	{
		for (int Team = 0; Team  < DOM_NUMOFTEAMS; ++Team)
			for (int Spawn = 0; Spawn < m_aaNumSpotSpawnPoints[Spot][Team]; ++Spawn)
				new CPickup(&GameServer()->m_World, Team, m_aaaSpotSpawnPoints[Spot][Team][Spawn]);
	}
	else
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "conq", "This command can only be used in running games.");
}