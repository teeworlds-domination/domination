#include <float.h>
#include <stdio.h>
#include <string.h>

#include <engine/shared/config.h>

#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/entities/character.h>
#include <game/server/entities/dspot.h>
#include <game/server/entities/csdom_pickup.h>

#include "dom.h"

CGameControllerDOM::CGameControllerDOM(CGameContext *pGameServer)
: IGameController(pGameServer)
		, m_ConstructorTick(Server()->Tick())
		, m_LastBroadcastTick(-1)
		, m_LastBroadcastCalcTick(-1)
		, m_IsInit(false)
		, m_DompointsCounter(0.0f)
		, m_NumOfDominationSpots(0)

{
	m_pGameType = "DOM";
	m_GameFlags = GAMEFLAG_TEAMS|GAMEFLAG_FLAGS;

	m_apDominationSpots[0] = m_apDominationSpots[1] = m_apDominationSpots[2] = m_apDominationSpots[3] = m_apDominationSpots[4] = 0;

	m_aNumOfTeamDominationSpots[0] = 0;
	m_aNumOfTeamDominationSpots[1] = 0;

	mem_zero(m_aaNumSpotSpawnPoints, DOM_MAXDSPOTS * 3);
	for (int Type = 0; Type < 3; ++Type)
		for (int Spot = 0; Spot < DOM_MAXDSPOTS; ++Spot)
			m_aaNumSpotSpawnPoints[Spot][Type] = 0;

	for (int cid = 0; cid < MAX_CLIENTS; ++cid)
		m_aPlayerIntroTick[cid] = -1;

	mem_zero(m_aaaSpotSpawnPoints, DOM_MAXDSPOTS * 3 * 64);

	m_aTeamscoreTick[0] = 0;
	m_aTeamscoreTick[1] = 0;

	SetCapTime(g_Config.m_SvDomCapTime);
}

void CGameControllerDOM::Init()
{
	m_IsInit = true;

	sscanf(g_Config.m_SvDomUseSpots, "%d %d %d %d %d", m_aDominationSpotsEnabled , m_aDominationSpotsEnabled + 1, m_aDominationSpotsEnabled + 2, m_aDominationSpotsEnabled + 3, m_aDominationSpotsEnabled + 4);
	float Temp[6] = {0};
	m_NumOfDominationSpots = 0;
	sscanf(g_Config.m_SvDomScorings, "%f %f %f %f %f", Temp + 1 , Temp + 2, Temp + 3, Temp + 4, Temp + 5);
	for (int Spot = 0; Spot < DOM_MAXDSPOTS; ++Spot)
	{
		if (!m_apDominationSpots[Spot])
			m_aDominationSpotsEnabled[Spot] = false;
		if (m_aDominationSpotsEnabled[Spot])
			++m_NumOfDominationSpots;
	}
	if (Temp[m_NumOfDominationSpots] <= 0.0f)
		Temp[m_NumOfDominationSpots] = 0.1;
	else if (Temp[m_NumOfDominationSpots] > 10.0f)
		Temp[m_NumOfDominationSpots] = 10.0f;
	m_DompointsCounter = Temp[m_NumOfDominationSpots] / Server()->TickSpeed();

	if (m_NumOfDominationSpots)
		CalculateSpawns();
}

void CGameControllerDOM::OnReset()
{
	IGameController::OnReset();

	if (!m_IsInit)
		Init();

	m_LastBroadcastTick = -1;
	m_LastBroadcastCalcTick = -1;

	mem_zero(m_aaBufBroadcastSpotOverview, DOM_MAXDSPOTS * 48 * sizeof(char));
	for (int Spot = 0; Spot < DOM_MAXDSPOTS; ++Spot)
	{
		ForceSpotBroadcastUpdate(Spot);
		m_aLastSpotCapStrength[Spot] = 0;
	}

	m_aNumOfTeamDominationSpots[0] = 0;
	m_aNumOfTeamDominationSpots[1] = 0;

	m_aTeamscoreTick[0] = 0;
	m_aTeamscoreTick[1] = 0;
}

void CGameControllerDOM::Tick()
{
	IGameController::Tick();

	if(m_GameState == IGS_GAME_RUNNING && !GameServer()->m_World.m_ResetRequested)
	{
		UpdateCaptureProcess();
		UpdateBroadcast();
		UpdateScoring();
	}
	else if(m_LastBroadcastTick != -1)
		UpdateBroadcast();

	UpdateChat();
}

bool CGameControllerDOM::OnEntity(int Index, vec2 Pos)
{
	if(IGameController::OnEntity(Index, Pos))
		return true;

	if (Index + ENTITY_OFFSET >= TILE_DOM_FLAG_A && Index + ENTITY_OFFSET <= TILE_DOM_CAPAREA_E)
	{
		//	save domination spot flag positions
		switch (Index + ENTITY_OFFSET)
		{
		case TILE_DOM_FLAG_A: if (!m_apDominationSpots[0]) GameServer()->m_World.InsertEntity(m_apDominationSpots[0] = new CDominationSpot(&GameServer()->m_World, Pos, 0, m_aCapTimes, WithHardCaptureAbort(), WithNeutralState())); break;
		case TILE_DOM_FLAG_B: if (!m_apDominationSpots[1]) GameServer()->m_World.InsertEntity(m_apDominationSpots[1] = new CDominationSpot(&GameServer()->m_World, Pos, 1, m_aCapTimes, WithHardCaptureAbort(), WithNeutralState())); break;
		case TILE_DOM_FLAG_C: if (!m_apDominationSpots[2]) GameServer()->m_World.InsertEntity(m_apDominationSpots[2] = new CDominationSpot(&GameServer()->m_World, Pos, 2, m_aCapTimes, WithHardCaptureAbort(), WithNeutralState())); break;
		case TILE_DOM_FLAG_D: if (!m_apDominationSpots[3]) GameServer()->m_World.InsertEntity(m_apDominationSpots[3] = new CDominationSpot(&GameServer()->m_World, Pos, 3, m_aCapTimes, WithHardCaptureAbort(), WithNeutralState())); break;
		case TILE_DOM_FLAG_E: if (!m_apDominationSpots[4]) GameServer()->m_World.InsertEntity(m_apDominationSpots[4] = new CDominationSpot(&GameServer()->m_World, Pos, 4, m_aCapTimes, WithHardCaptureAbort(), WithNeutralState())); break;
		default: return false;
		}
	}
	else
	{
		// save spawn positions
		switch (Index)
		{
		case ENTITY_SPAWN_SPOT_A_RANDOM: m_aaaSpotSpawnPoints[0][0][m_aaNumSpotSpawnPoints[0][0]++] = Pos; break;
		case ENTITY_SPAWN_SPOT_A_RED:    m_aaaSpotSpawnPoints[0][1][m_aaNumSpotSpawnPoints[0][1]++] = Pos; break;
		case ENTITY_SPAWN_SPOT_A_BLUE:   m_aaaSpotSpawnPoints[0][2][m_aaNumSpotSpawnPoints[0][2]++] = Pos; break;
		case ENTITY_SPAWN_SPOT_B_RANDOM: m_aaaSpotSpawnPoints[1][0][m_aaNumSpotSpawnPoints[1][0]++] = Pos; break;
		case ENTITY_SPAWN_SPOT_B_RED:    m_aaaSpotSpawnPoints[1][1][m_aaNumSpotSpawnPoints[1][1]++] = Pos; break;
		case ENTITY_SPAWN_SPOT_B_BLUE:   m_aaaSpotSpawnPoints[1][2][m_aaNumSpotSpawnPoints[1][2]++] = Pos; break;
		case ENTITY_SPAWN_SPOT_C_RANDOM: m_aaaSpotSpawnPoints[2][0][m_aaNumSpotSpawnPoints[2][0]++] = Pos; break;
		case ENTITY_SPAWN_SPOT_C_RED:    m_aaaSpotSpawnPoints[2][1][m_aaNumSpotSpawnPoints[2][1]++] = Pos; break;
		case ENTITY_SPAWN_SPOT_C_BLUE:   m_aaaSpotSpawnPoints[2][2][m_aaNumSpotSpawnPoints[2][2]++] = Pos; break;
		case ENTITY_SPAWN_SPOT_D_RANDOM: m_aaaSpotSpawnPoints[3][0][m_aaNumSpotSpawnPoints[3][0]++] = Pos; break;
		case ENTITY_SPAWN_SPOT_D_RED:    m_aaaSpotSpawnPoints[3][1][m_aaNumSpotSpawnPoints[3][1]++] = Pos; break;
		case ENTITY_SPAWN_SPOT_D_BLUE:   m_aaaSpotSpawnPoints[3][2][m_aaNumSpotSpawnPoints[3][2]++] = Pos; break;
		case ENTITY_SPAWN_SPOT_E_RANDOM: m_aaaSpotSpawnPoints[4][0][m_aaNumSpotSpawnPoints[4][0]++] = Pos; break;
		case ENTITY_SPAWN_SPOT_E_RED:    m_aaaSpotSpawnPoints[4][1][m_aaNumSpotSpawnPoints[4][1]++] = Pos; break;
		case ENTITY_SPAWN_SPOT_E_BLUE:   m_aaaSpotSpawnPoints[4][2][m_aaNumSpotSpawnPoints[4][2]++] = Pos; break;
		default: return false;
		}
	}
	
	return true;
}

void CGameControllerDOM::OnPlayerConnect(CPlayer *pPlayer)
{
	IGameController::OnPlayerConnect(pPlayer);
	
	if (m_ConstructorTick+Server()->TickSpeed()*3 < Server()->Tick())
		m_aPlayerIntroTick[pPlayer->GetCID()] = Server()->Tick()+Server()->TickSpeed()*4;
	pPlayer->m_RespawnTick = IsGameRunning()? Server()->Tick()+Server()->TickSpeed()*GetRespawnDelay(false) : Server()->Tick();
}

float CGameControllerDOM::GetRespawnDelay(bool Self) const
{
	return max(IGameController::GetRespawnDelay(Self), Self ? g_Config.m_SvDomRespawnDelay + 3.0f : g_Config.m_SvDomRespawnDelay);
}

void CGameControllerDOM::EvaluateSpawnTypeDom(CSpawnEval *pEval, int SpotTeam, int SpawnType) const
{
	int Type = SpotTeam == DOM_NEUTRAL? 0 : 1+(SpotTeam&1);
	int NumSpawns = 0;

	int Spot = -1;
	while ((Spot = GetNextSpot(Spot)) > -1)
	{
		if (m_apDominationSpots[Spot]->GetTeam() == SpotTeam)
			NumSpawns += m_aaNumSpotSpawnPoints[Spot][SpawnType];
	}

	if (!NumSpawns)
		return;

	vec2 *pSpawnpoint = new vec2[NumSpawns];
	NumSpawns = 0;

	Spot = -1;
	while ((Spot = GetNextSpot(Spot)) > -1)
	{
		if (m_apDominationSpots[Spot]->GetTeam() != SpotTeam)
			continue;

		for (int Spawn = 0; Spawn < m_aaNumSpotSpawnPoints[Spot][SpawnType]; ++Spawn)
			pSpawnpoint[NumSpawns++] = m_aaaSpotSpawnPoints[Spot][SpawnType][Spawn];
	}

	vec2 *pStartpoint = new vec2[NumSpawns];
	int NumStartpoints = 0;

	for(int i = 0; i < NumSpawns; i++)
	{
		// check if the position is occupado
		CCharacter *aEnts[MAX_CLIENTS];
		int Num = GameServer()->m_World.FindEntities(pSpawnpoint[i], 64, (CEntity**)aEnts, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		vec2 Positions[5] = { vec2(0.0f, 0.0f), vec2(-32.0f, 0.0f), vec2(0.0f, -32.0f), vec2(32.0f, 0.0f), vec2(0.0f, 32.0f) };	// start, left, up, right, down
		int Result = -1;
		for(int Index = 0; Index < 5 && Result == -1; ++Index)
		{
			Result = Index;
			for(int c = 0; c < Num; ++c)
				if(GameServer()->Collision()->CheckPoint(pSpawnpoint[i]+Positions[Index]) ||
					distance(aEnts[c]->GetPos(), pSpawnpoint[i]+Positions[Index]) <= aEnts[c]->GetProximityRadius())
				{
					Result = -1;
					break;
				}
		}
		if(Result == -1)
			continue;	// try next spawn point

		vec2 P = pSpawnpoint[i]+Positions[Result];
		if (!Type) // for neutral spawns, check distance to other player
		{
			float S = IGameController::EvaluateSpawnPos(pEval, P);
			if(!pEval->m_Got || pEval->m_Score >= S - (S * 0.3f))
			{
				if (NumStartpoints && pEval->m_Score > S + (S * 0.3f))
				{
					NumStartpoints = 0; // got a way better spawn point, drop the other
					pEval->m_Score = S;
				}

				if (!pEval->m_Got)
					pEval->m_Score = S;
				pEval->m_Got = true;
			}
			else
				continue;
		}
		pStartpoint[NumStartpoints++] = P;
	}

	if (NumStartpoints)
	{
		pEval->m_Got = true;
		pEval->m_Pos = pStartpoint[Server()->Tick() % NumStartpoints];
	}
	delete[] pSpawnpoint;
	pSpawnpoint = 0;
	delete[] pStartpoint;
	pStartpoint = 0;
}

void CGameControllerDOM::EvaluateSpawnTypeDom(CSpawnEval *pEval, int SpotTeam) const
{
	// first try own spawns, then random spawns
	EvaluateSpawnTypeDom(pEval, SpotTeam, 1+(pEval->m_FriendlyTeam&1));
	if (!pEval->m_Got)
		EvaluateSpawnTypeDom(pEval, SpotTeam, 0);
}

bool CGameControllerDOM::CanSpawn(int Team, vec2 *pOutPos)
{
	// spectators can't spawn
	if(Team == TEAM_SPECTATORS || GameServer()->m_World.m_Paused || GameServer()->m_World.m_ResetRequested)
		return false;

	if (!m_NumOfDominationSpots)
		return IGameController::CanSpawn(Team, pOutPos);

	CSpawnEval Eval;
	Eval.m_FriendlyTeam = Team;

	EvaluateSpawnTypeDom(&Eval, Team);         // own spawn or random spawn at own spot
	if (!Eval.m_Got)
	{
		EvaluateSpawnTypeDom(&Eval, DOM_NEUTRAL);  // own spawn or random spawn at neutral spot
		if (!Eval.m_Got)
			EvaluateSpawnTypeDom(&Eval, Team ^ 1);     // own spawn or random spawn at enemy spot
	}

	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}

void CGameControllerDOM::CalculateSpawns()
{
	int Spot;
	// calc implicit spot spawns if no explicit are set
	Spot = -1;
	while ((Spot = GetNextSpot(Spot)) > -1)
	{
		CalculateSpotSpawns(Spot, 0); // random
		CalculateSpotSpawns(Spot, 1); // red
		CalculateSpotSpawns(Spot, 2); // blue
	}
}

void CGameControllerDOM::CalculateSpotSpawns(int Spot, int Type)
{
	if (m_aaNumSpotSpawnPoints[Spot][Type] || !m_aNumSpawnPoints[Type])
		return;

	if (!m_NumOfDominationSpots)
	{
		for (int i = 0; i < m_aNumSpawnPoints[Type]; ++i)
			m_aaaSpotSpawnPoints[Spot][Type][m_aaNumSpotSpawnPoints[Spot][Type]++] = m_aaSpawnPoints[Type][i];
		return;
	}

	for (int i = 0; i < m_aNumSpawnPoints[Type]; ++i)
		if (IsSpawnAtSpot(m_aaSpawnPoints[Type][i], Spot))
			m_aaaSpotSpawnPoints[Spot][Type][m_aaNumSpotSpawnPoints[Spot][Type]++] = m_aaSpawnPoints[Type][i];

	return;
}

bool CGameControllerDOM::IsSpawnAtSpot(vec2 Pos, int Spot) const
{
	float DistanceSpot = min(FLT_MAX, distance(Pos, m_apDominationSpots[Spot]->GetPos()));
	float DistanceNextSpot = FLT_MAX;

	int NextSpot = -1;
	while ((NextSpot = GetNextSpot(NextSpot)) > -1)
	{
		if (NextSpot == Spot)
			continue;
		DistanceNextSpot = min(DistanceNextSpot, distance(Pos, m_apDominationSpots[NextSpot]->GetPos()));
	}

	return DistanceSpot <= DistanceNextSpot;
}

void CGameControllerDOM::UpdateCaptureProcess()
{
	int aaPlayerStats[DOM_MAXDSPOTS][DOM_NUMOFTEAMS];	//	number of players per team per capturing area
	int aaPlayerStrength[DOM_MAXDSPOTS][DOM_NUMOFTEAMS];	//	capture strength of players per team per capturing area
	CCharacter* aaapCapPlayers[DOM_MAXDSPOTS][DOM_NUMOFTEAMS][MAX_PLAYERS];	//	capturing players in a capturing area
	mem_zero(aaPlayerStats, DOM_MAXDSPOTS * DOM_NUMOFTEAMS * sizeof(int));
	mem_zero(aaPlayerStrength, DOM_MAXDSPOTS * DOM_NUMOFTEAMS * sizeof(int));
	mem_zero(aaapCapPlayers, DOM_MAXDSPOTS * MAX_PLAYERS * sizeof(CCharacter*));
	CPlayer* pPlayer;
	int DomCaparea;

	// check all players for being in a capture area
	for (int cid = 0; cid < MAX_CLIENTS; ++cid)
	{
		pPlayer = GameServer()->m_apPlayers[cid];
		if (!pPlayer || pPlayer->GetTeam() == TEAM_SPECTATORS || !pPlayer->GetCharacter() || !pPlayer->GetCharacter()->IsAlive())
			continue;
		DomCaparea = min(GameServer()->Collision()->GetCollisionAt(static_cast<int>(pPlayer->GetCharacter()->GetPos().x),
			static_cast<int>(pPlayer->GetCharacter()->GetPos().y)) >> 3, 5) - 1;
		if (DomCaparea != -1 && m_aDominationSpotsEnabled[DomCaparea])
		{
			aaapCapPlayers[DomCaparea][pPlayer->GetTeam()][aaPlayerStats[DomCaparea][pPlayer->GetTeam()]++] = pPlayer->GetCharacter();
			if (!aaPlayerStrength[DomCaparea][pPlayer->GetTeam()] || WithAdditiveCapStrength())
				aaPlayerStrength[DomCaparea][pPlayer->GetTeam()] += CalcCaptureStrength(DomCaparea, pPlayer->GetCharacter(), !aaPlayerStrength[DomCaparea][pPlayer->GetTeam()]);
		}
	}

	if (WithHandicap())
		for (DomCaparea = 0; DomCaparea < m_NumOfDominationSpots; ++DomCaparea)
		{
			if (!m_aDominationSpotsEnabled[DomCaparea])
				continue;

			for (int Team = 0; Team < DOM_NUMOFTEAMS; ++Team)
			{
				//	handicap (faster capturing) for smaller team
				if (aaPlayerStrength[DomCaparea][Team] && GetTeamSize(Team) < GetTeamSize(Team ^ 1))
					aaPlayerStrength[DomCaparea][Team] += ADD_CAPSTRENGTH;
			}
		}

	//	capturing process for all dspots
	int Spot = -1;
	while ((Spot = GetNextSpot(Spot)) > -1)
	{
		if (m_apDominationSpots[Spot]->IsGettingCaptured())
		{
			if (m_apDominationSpots[Spot]->UpdateCapturing(aaPlayerStrength[Spot][m_apDominationSpots[Spot]->GetCapTeam()], aaPlayerStrength[Spot][m_apDominationSpots[Spot]->GetCapTeam() ^ 1]))
			{
				if (m_apDominationSpots[Spot]->GetTeam() == DOM_NEUTRAL)
				{
					Neutralize(Spot, aaPlayerStats[Spot][m_apDominationSpots[Spot]->GetCapTeam()], aaapCapPlayers[Spot][m_apDominationSpots[Spot]->GetCapTeam()]);
					StartCapturing(Spot, aaPlayerStrength[Spot][DOM_RED], aaPlayerStrength[Spot][DOM_BLUE]);
				}
				else
					Capture(Spot, aaPlayerStats[Spot][m_apDominationSpots[Spot]->GetCapTeam()], aaapCapPlayers[Spot][m_apDominationSpots[Spot]->GetCapTeam()]);
			}
		}
		else
			StartCapturing(Spot, aaPlayerStrength[Spot][DOM_RED], aaPlayerStrength[Spot][DOM_BLUE]);
	}
}

void CGameControllerDOM::StartCapturing(int Spot, int RedCapStrength, int BlueCapStrength)
{
	if (Spot < 0 || Spot > DOM_MAXDSPOTS - 1)
		return;

	int Team = -1;
	if (m_apDominationSpots[Spot]->GetTeam() != DOM_RED && RedCapStrength > BlueCapStrength)
		Team = TEAM_RED;
	else if (m_apDominationSpots[Spot]->GetTeam() != DOM_BLUE && RedCapStrength < BlueCapStrength)
		Team = TEAM_BLUE;
	else
		return;

	m_apDominationSpots[Spot]->StartCapturing(Team, GetTeamSize(Team), Team == TEAM_RED? RedCapStrength : BlueCapStrength, Team == TEAM_RED? BlueCapStrength : RedCapStrength);
}

void CGameControllerDOM::Capture(int Spot, int NumOfCapCharacters, CCharacter* apCapCharacters[MAX_PLAYERS])
{
	if (Spot < 0 || Spot > DOM_MAXDSPOTS - 1)
		return;

	if (!WithNeutralState())
	{
		// TODO ugly
		int NumSpots = 0;
		int s = -1;
		while ((s = GetNextSpot(s)) > -1)
			if (m_apDominationSpots[s]->GetTeam() == (m_apDominationSpots[Spot]->GetTeam() ^ 1))
				++NumSpots;
		m_aNumOfTeamDominationSpots[m_apDominationSpots[Spot]->GetCapTeam() ^ 1] = NumSpots;
	}

	++m_aNumOfTeamDominationSpots[m_apDominationSpots[Spot]->GetTeam()];
	ForceSpotBroadcastUpdate(Spot);
	m_aLastSpotCapStrength[Spot] = 0;

	OnCapture(Spot, m_apDominationSpots[Spot]->GetTeam(), NumOfCapCharacters, apCapCharacters);
}

void CGameControllerDOM::OnCapture(int Spot, int Team, int NumOfCapCharacters, CCharacter* apCapCharacters[MAX_PLAYERS])
{
	for (int i = 0; i < NumOfCapCharacters; ++i)
	{
		if (apCapCharacters[i] && apCapCharacters[i]->GetPlayer())
			apCapCharacters[i]->GetPlayer()->m_Score += g_Config.m_SvDomCapPoints;
	}
}

void CGameControllerDOM::Neutralize(int Spot, int NumOfCapCharacters, CCharacter* apCapCharacters[MAX_PLAYERS])
{
	if (Spot < 0 || Spot > DOM_MAXDSPOTS - 1)
		return;

	--m_aNumOfTeamDominationSpots[m_apDominationSpots[Spot]->GetCapTeam() ^ 1];
	ForceSpotBroadcastUpdate(Spot);
	m_aLastSpotCapStrength[Spot] = 0;

	OnNeutralize(Spot, m_apDominationSpots[Spot]->GetCapTeam(), NumOfCapCharacters, apCapCharacters);
}

void CGameControllerDOM::UpdateBroadcast()
{
	if (m_LastBroadcastCalcTick + Server()->TickSpeed()*0.5f > Server()->Tick())
		return;

	m_LastBroadcastCalcTick = Server()->Tick();
	UpdateBroadcastOverview();

	for (int cid = 0; cid < MAX_CLIENTS; ++cid)
	{
		if (GameServer()->m_apPlayers[cid] && !SendPersonalizedBroadcast(cid))
			SendBroadcast(cid, ""); // clean up previous personalized broadcast
	}
}

void CGameControllerDOM::UpdateBroadcastOverview()
{
	str_format(m_aBufBroadcastOverview, sizeof(m_aBufBroadcastOverview), "%s%s%s%s%s"
			, GetDominationSpotBroadcastOverview(0, m_aaBufBroadcastSpotOverview[0])
			, GetDominationSpotBroadcastOverview(1, m_aaBufBroadcastSpotOverview[1])
			, GetDominationSpotBroadcastOverview(2, m_aaBufBroadcastSpotOverview[2])
			, GetDominationSpotBroadcastOverview(3, m_aaBufBroadcastSpotOverview[3])
			, GetDominationSpotBroadcastOverview(4, m_aaBufBroadcastSpotOverview[4]) );
}


void CGameControllerDOM::UpdateScoring()
{
	// add captured score to the teamscores
	for (int Team = 0; Team < DOM_NUMOFTEAMS; ++Team)
	{
		if (!m_aNumOfTeamDominationSpots[Team])
			continue;
		double AddScore;
		m_aTeamscoreTick[Team] = modf(m_aTeamscoreTick[Team] + static_cast<float>(m_aNumOfTeamDominationSpots[Team]) * m_DompointsCounter, &AddScore);
		m_aTeamscore[Team] += static_cast<int>(AddScore);
	}
}

void CGameControllerDOM::UpdateChat()
{
	for (int cid = 0; cid < MAX_CLIENTS; ++cid)
	{
		if (m_aPlayerIntroTick[cid] != -1)
		{
			if (!GameServer()->m_apPlayers[cid])
			{
				m_aPlayerIntroTick[cid] = -1;
				continue;
			}
			if (m_aPlayerIntroTick[cid] <= Server()->Tick())
			{
				m_aPlayerIntroTick[cid] = -1;
				SendChatIntro(cid);
			}
		}
	}
}

bool CGameControllerDOM::SendPersonalizedBroadcast(int ClientID)
{
	if (GetRespawnDelay(false) > 3.0f && GameServer()->m_apPlayers[ClientID]->GetTeam() != TEAM_SPECTATORS
			&& !GameServer()->m_apPlayers[ClientID]->GetCharacter() && GameServer()->m_apPlayers[ClientID]->m_RespawnTick > Server()->Tick()+Server()->TickSpeed())
	{
		char aBuf[32] = {0};
		str_format(aBuf, sizeof(aBuf), "Respawn in %i seconds", (GameServer()->m_apPlayers[ClientID]->m_RespawnTick - Server()->Tick()) / Server()->TickSpeed());
		SendBroadcast(ClientID, aBuf);
		return true;
	}
	return false;
}

void CGameControllerDOM::SendBroadcast(int ClientID, const char *pText) const
{
	if (strlen(pText) > 0)
	{
		char aBufResult[1024];
		str_format(aBufResult, sizeof(aBufResult), "%s\\n%s", pText, m_aBufBroadcastOverview);
		GameServer()->SendBroadcast(aBufResult, ClientID);
	}
	else
		GameServer()->SendBroadcast(m_aBufBroadcastOverview, ClientID);
}

const char* CGameControllerDOM::GetDominationSpotBroadcastOverview(int Spot, char *pBuf)
{
	if (!m_aDominationSpotsEnabled[Spot])
		return "";

	int MarkerPos;

	if (m_apDominationSpots[Spot]->IsGettingCaptured())
	{
		float MarkerStepWidth = m_apDominationSpots[Spot]->GetCapTime() / 5.0f; // 5 marker pos
		MarkerPos = ceil(m_apDominationSpots[Spot]->GetTimer() / Server()->TickSpeed() / MarkerStepWidth) ;
		if (MarkerPos >= 0 && m_apDominationSpots[Spot]->GetCapTeam() == DOM_RED)
			MarkerPos = 5 - MarkerPos; // the other way around
		if (m_apDominationSpots[Spot]->GetCapTeam() == DOM_BLUE && MarkerPos == 0)
			++MarkerPos;
		else if (m_apDominationSpots[Spot]->GetCapTeam() == DOM_RED && MarkerPos == 5)
			--MarkerPos;
	}
	else
		MarkerPos = -1;

	if (MarkerPos != m_aLastBroadcastState[Spot] || m_apDominationSpots[Spot]->GetCapStrength() != m_aLastSpotCapStrength[Spot])
	{
		m_aLastBroadcastState[Spot] = MarkerPos;
		m_aLastSpotCapStrength[Spot] = m_apDominationSpots[Spot]->GetCapStrength();

		int CurrPos = 0;
		AddColorizedOpenParenthesis (Spot, pBuf, CurrPos, MarkerPos);
		AddColorizedLine            (Spot, pBuf, CurrPos, MarkerPos, 1);
		AddColorizedLine            (Spot, pBuf, CurrPos, MarkerPos, 2);
		AddColorizedSpot            (Spot, pBuf, CurrPos);
		AddColorizedLine            (Spot, pBuf, CurrPos, MarkerPos, 3);
		AddColorizedLine            (Spot, pBuf, CurrPos, MarkerPos, 4);
		AddColorizedCloseParenthesis(Spot, pBuf, CurrPos, MarkerPos);
		pBuf[CurrPos++] = ' ';
		pBuf[CurrPos] = 0;
	}

	return pBuf;
}

void CGameControllerDOM::AddColorizedOpenParenthesis(int Spot, char *pBuf, int &rCurrPos, int MarkerPos) const
{
	if (MarkerPos == 0)
	{
		AddColorizedMarker(Spot, pBuf, rCurrPos);
		return;
	}

	int Team;

	if (!m_apDominationSpots[Spot]->IsGettingCaptured())
		Team = m_apDominationSpots[Spot]->GetTeam();
	else
	{
		if (m_apDominationSpots[Spot]->GetCapTeam() == DOM_RED)
			Team = m_apDominationSpots[Spot]->GetNextTeam();
		else
			Team = m_apDominationSpots[Spot]->GetTeam();
	}

	AddColorizedSymbol(pBuf, rCurrPos, Team, GetTeamBroadcastOpenParenthesis(Team));
}

void CGameControllerDOM::AddColorizedCloseParenthesis(int Spot, char *pBuf, int &rCurrPos, int MarkerPos) const
{
	if (MarkerPos == 5)
	{
		AddColorizedMarker(Spot, pBuf, rCurrPos);
		return;
	}

	int Team;

	if (!m_apDominationSpots[Spot]->IsGettingCaptured())
		Team = m_apDominationSpots[Spot]->GetTeam();
	else
	{
		if (m_apDominationSpots[Spot]->GetCapTeam() == DOM_BLUE)
			Team = m_apDominationSpots[Spot]->GetNextTeam();
		else
			Team = m_apDominationSpots[Spot]->GetTeam();
	}

	AddColorizedSymbol(pBuf, rCurrPos, Team, GetTeamBroadcastCloseParenthesis(Team));
}

void CGameControllerDOM::AddColorizedLine(int Spot, char *pBuf, int &rCurrPos, int MarkerPos, int LineNum) const
{
	if (MarkerPos == LineNum)
	{
		AddColorizedMarker(Spot, pBuf, rCurrPos);
		return;
	}

	// static const char* CHAR_LINE = "—"; // TODO hackish
	// static const char* CHAR_LINEx = "–";
	static const char CHAR_LINE = '\342';

	int Team;

	if (!m_apDominationSpots[Spot]->IsGettingCaptured())
		Team = m_apDominationSpots[Spot]->GetTeam();
	else
	{
		if (m_apDominationSpots[Spot]->GetCapTeam() == DOM_RED)
			Team = LineNum < MarkerPos? m_apDominationSpots[Spot]->GetNextTeam() : m_apDominationSpots[Spot]->GetTeam();
		else
			Team = MarkerPos < LineNum? m_apDominationSpots[Spot]->GetNextTeam() : m_apDominationSpots[Spot]->GetTeam();
	}

	AddColorizedSymbol(pBuf, rCurrPos, Team, CHAR_LINE);

	pBuf[rCurrPos++] = '\200';
	pBuf[rCurrPos++] = '\223'; // Halbgeviertstrich
	// pBuf[rCurrPos++] = '\224'; // Geviertstrich
}

void CGameControllerDOM::AddColorizedSpot(int Spot, char *pBuf, int &rCurrPos) const
{
	AddColorizedSymbol(pBuf, rCurrPos, m_apDominationSpots[Spot]->GetTeam(), GetSpotName(Spot));
}

void CGameControllerDOM::AddColorizedMarker(int Spot, char *pBuf, int &rCurrPos) const
{
	if (!m_apDominationSpots[Spot]->IsGettingCaptured())
		return; // should not occur

	AddColorizedSymbol(pBuf, rCurrPos
			, m_apDominationSpots[Spot]->GetNextTeam()
			, GetTeamBroadcastMarker(m_apDominationSpots[Spot]->GetCapTeam(), m_apDominationSpots[Spot]->GetCapStrength()));

	// TODO hackish
	// static const char* CHAR_DOUBLE_ARROW_RIGHT = "»";
	// static const char* CHAR_DOUBLE_ARROW_LEFT = "«";
	if (abs(m_apDominationSpots[Spot]->GetCapStrength()) > (GetMaxCapStrengthForTeamSize(GetTeamSize(m_apDominationSpots[Spot]->GetCapTeam())) / 2) + (BASE_CAPSTRENGTH / 2) )
	{
		if (m_apDominationSpots[Spot]->GetCapStrength() > 0)
			pBuf[rCurrPos++] = (m_apDominationSpots[Spot]->GetCapTeam() == DOM_RED? '\273' : '\253');
		else
			pBuf[rCurrPos++] = (m_apDominationSpots[Spot]->GetCapTeam() == DOM_BLUE? '\273' : '\253');
	}
}

void CGameControllerDOM::AddColorizedSymbol(char *pBuf, int &rCurrPos, int ColorCode, const char Symbol) const
{
	const char* TeamColor = GetTeamBroadcastColor(ColorCode);
	pBuf[rCurrPos++] = TeamColor[0];
	pBuf[rCurrPos++] = TeamColor[1];
	pBuf[rCurrPos++] = TeamColor[2];
	pBuf[rCurrPos++] = TeamColor[3];
	pBuf[rCurrPos++] = Symbol;
}

int CGameControllerDOM::GetNextSpot(int Spot) const
{
	for (int NextSpot = Spot + 1; NextSpot < DOM_MAXDSPOTS; ++NextSpot)
	{
		if (!m_aDominationSpotsEnabled[NextSpot])
			continue;
		return NextSpot;
	}
	return -1;
}

int CGameControllerDOM::GetPreviousSpot(int Spot) const
{
	for (int PreviousSpot = Spot - 1; PreviousSpot >= 0; --PreviousSpot)
	{
		if (!m_aDominationSpotsEnabled[PreviousSpot])
			continue;
		return PreviousSpot;
	}
	return -1;
}

int CGameControllerDOM::CalcCaptureStrength(int Spot, CCharacter* pChr, bool IsFirst) const
{
	return (IsFirst? BASE_CAPSTRENGTH : (GetTeamSize(pChr->GetPlayer()->GetTeam()) == 2? ADD_CAPSTRENGTH * 2 : ADD_CAPSTRENGTH))
			+ (pChr->IsNinja()? BASE_CAPSTRENGTH : 0);  // ninja doubles the players strength
}

int CGameControllerDOM::GetMaxCapStrengthForTeamSize(int TeamSize) const {
	if (!TeamSize)
		return 0;

	if (TeamSize == 2)
		return GetMaxCapStrengthForTeamSize(3); // for 2, the second is double the strength

	return BASE_CAPSTRENGTH + (ADD_CAPSTRENGTH * (TeamSize - 1));
}


const char* CGameControllerDOM::GetTeamName(int Team) const
{
	switch(Team)
	{
	case 0: return "Red Team"; break;
	case 1: return "Blue Team"; break;
	default: return "Neutral"; break;
	}
}

const char CGameControllerDOM::GetSpotName(int Spot) const
{
	switch (Spot)
	{
	case 0: return 'A';
	case 1: return 'B';
	case 2: return 'C';
	case 3: return 'D';
	default: return 'E';
	}
}

void CGameControllerDOM::ForceSpotBroadcastUpdate(int Spot) {
	if (Spot < 0 || Spot > DOM_MAXDSPOTS || !m_aDominationSpotsEnabled[Spot])
		return;

	m_aLastBroadcastState[Spot] = -2;
	m_LastBroadcastCalcTick = -1;
}


const char* CGameControllerDOM::GetTeamBroadcastColor(int Team) const
{
	switch(Team)
	{
	case DOM_RED:  return "^900";
	case DOM_BLUE: return "^009";
	default:       return "^999";
	}
}

const char CGameControllerDOM::GetTeamBroadcastOpenParenthesis(int Team) const
{
	switch(Team)
	{
	case DOM_RED:  return '{';
	case DOM_BLUE: return '[';
	default:       return '(';
	}
}

const char CGameControllerDOM::GetTeamBroadcastCloseParenthesis(int Team) const
{
	switch(Team)
	{
	case DOM_RED:  return '}';
	case DOM_BLUE: return ']';
	default:       return ')';
	}
}

const char CGameControllerDOM::GetTeamBroadcastMarker(int Team, int CapStrength) const
{
	if (!CapStrength)
		return 'x';

	if (abs(CapStrength) > (GetMaxCapStrengthForTeamSize(GetTeamSize(Team)) / 2) + (BASE_CAPSTRENGTH / 2) )
		return '\302';

	return (Team == DOM_RED && CapStrength > 0) || (Team == TEAM_BLUE && CapStrength < 0)? '>' : '<';

	/*
	switch(Team)
	{
	case DOM_RED:  
		return '>';
	default:
		return '<';
	}
	*/
}

void CGameControllerDOM::SetCapTime(int CapTime)
{
	m_aCapTimes[1] = CapTime * BASE_CAPSTRENGTH;
	for (int i = 1; i < MAX_PLAYERS / DOM_NUMOFTEAMS + 1; ++i)
		m_aCapTimes[i] = CapTime * (BASE_CAPSTRENGTH + (WithAdditiveCapStrength()? ((i - 1) * ADD_CAPSTRENGTH) : 0) );
	m_aCapTimes[0] = m_aCapTimes[1];

	// special case: for 2 player teams the second player is two times as strong (60)
	if (WithAdditiveCapStrength())
		m_aCapTimes[2] += CapTime * ADD_CAPSTRENGTH;
}

void CGameControllerDOM::SendChat(int ClientID, const char *pText) const
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Mode = CHAT_WHISPER;
	Msg.m_ClientID = -1;
	Msg.m_pMessage = pText;
	Msg.m_TargetID = ClientID;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameControllerDOM::SendChatIntro(int ClientID) const
{
	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "Hey, %s! New here?", Server()->ClientName(ClientID));
	SendChat(ClientID, aBuf);

	SendChat(ClientID, "Type '/info' and I will give you a quick introduction.");
}

void CGameControllerDOM::SendChatCommand(int ClientID, const char *pCommand) const
{
	if (ClientID >= 0 && ClientID < MAX_CLIENTS)
	{
		if (str_comp_nocase(pCommand, "/info") == 0 || str_comp_nocase(pCommand, "/help") == 0)
		{
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "Domination Mod (%s) by Slayer and Fisico.", GameServer()->ModVersion());
			SendChat(ClientID, aBuf);
			SendChatInfo(ClientID);
		}
		else if (str_comp_nocase(pCommand, "/spots") == 0 || str_comp_nocase(pCommand, "/domspots") == 0)
			SendChatStats(ClientID);
		else
			SendChat(ClientID, "Unknown command. Type '/help' for more information about this mod.");
	}
}

void CGameControllerDOM::SendChatInfo(int ClientID) const
{
	SendChat(ClientID, "GAMETYPE: DOMINATION");
	SendChat(ClientID, "——————————————————————————");
	SendChat(ClientID, "Capture domination spots.");
	SendChat(ClientID, "Tip: Capturing together speeds it up.");
	SendChat(ClientID, "Each team spot increases your teams score over time.");
	SendChat(ClientID, "(Please enable broadcast color in your settings.)");
}

void CGameControllerDOM::SendChatStats(int ClientID) const
{
	SendChat(ClientID, "——— Domination Spots ———");

	if (m_NumOfDominationSpots == 0)
		SendChat(ClientID, "No domination spots available on this map.");
	else
	{
		char aBuf[32];
		int Spot = -1;
		while ((Spot = GetNextSpot(Spot)) > -1)
		{
			str_format(aBuf, sizeof(aBuf), "%c: %s", GetSpotName(Spot), GetTeamName(m_apDominationSpots[Spot]->GetTeam()));
			SendChat(ClientID, aBuf);
		}
	}
}

void CGameControllerDOM::ShowSpawns(int Spot) const
{
	if((m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_GAME_PAUSED) && !GameServer()->m_World.m_ResetRequested)
	{
		int PickupType;
		for (int Type = 0; Type < 3; ++Type)
		{
			switch (Type)
			{
			case 0:  PickupType = PICKUP_ARMOR; break;
			case 1:  PickupType = PICKUP_HEALTH; break;
			default: PickupType = PICKUP_AMMO; break;
			}
			for (int Spawn = 0; Spawn < m_aaNumSpotSpawnPoints[Spot][Type]; ++Spawn)
				new CCSDOMPickup(&GameServer()->m_World, PickupType, m_aaaSpotSpawnPoints[Spot][Type][Spawn], true);
		}
	}
	else
		GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "DOM", "This command can only be used in running games.");
}