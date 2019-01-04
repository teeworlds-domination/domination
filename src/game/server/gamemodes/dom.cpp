#include <float.h>
#include <stdio.h>
#include <string.h>

#include <engine/shared/config.h>

#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/entities/character.h>
#include <game/server/entities/dspot.h>

#include "dom.h"

// TODO
#define	BCOLOR_BLUE "^009"
#define	BCOLOR_RED "^900"
#define BCOLOR_NEUTRAL "^999"
#define BCHAR_LINE "—"

CGameControllerDOM::CGameControllerDOM(CGameContext *pGameServer)
: IGameController(pGameServer)
{
	m_apDominationSpots[0] = m_apDominationSpots[1] = m_apDominationSpots[2] = m_apDominationSpots[3] = m_apDominationSpots[4] = 0;
	m_pGameType = "DOM";

	m_GameFlags = GAMEFLAG_TEAMS|GAMEFLAG_FLAGS;

	m_aTeamscoreTick[0] = 0;
	m_aTeamscoreTick[1] = 0;

	m_aNumOfTeamDominationSpots[0] = 0;
	m_aNumOfTeamDominationSpots[1] = 0;

	m_LastBroadcastTick = -1;

	Init();
}

void CGameControllerDOM::Init()
{
	sscanf(g_Config.m_SvDomUseSpots, "%d %d %d %d %d", m_aDominationSpotsEnabled , m_aDominationSpotsEnabled + 1, m_aDominationSpotsEnabled + 2, m_aDominationSpotsEnabled + 3, m_aDominationSpotsEnabled + 4);
	float Temp[6] = {0};
	m_NumOfDominationSpots = 0;
	sscanf(g_Config.m_SvDomScorings, "%f %f %f %f %f", Temp + 1 , Temp + 2, Temp + 3, Temp + 4, Temp + 5);
	for (int i = 0; i < DOM_MAXDSPOTS; ++i)
	{
		if (!m_apDominationSpots[i])
			m_aDominationSpotsEnabled[i] = false;
		if (m_aDominationSpotsEnabled[i])
			++m_NumOfDominationSpots;
	}
	if (Temp[m_NumOfDominationSpots] <= 0.0f)
		Temp[m_NumOfDominationSpots] = 0.1;
	else if (Temp[m_NumOfDominationSpots] > 10.0f)
		Temp[m_NumOfDominationSpots] = 10.0f;
	m_DompointsCounter = Temp[m_NumOfDominationSpots] / Server()->TickSpeed();

	SetCapTimes(g_Config.m_SvDomCapTimes);
}

void CGameControllerDOM::OnReset()
{
	IGameController::OnReset();

	m_aTeamscoreTick[0] = 0;
	m_aTeamscoreTick[1] = 0;
	m_aNumOfTeamDominationSpots[0] = 0;
	m_aNumOfTeamDominationSpots[1] = 0;
	m_LastBroadcastTick = -1;

	Init();
}

void CGameControllerDOM::Tick()
{
	IGameController::Tick();

	if((m_GameState == IGS_GAME_RUNNING || m_GameState == IGS_GAME_PAUSED) && !GameServer()->m_World.m_ResetRequested)
	{
		UpdateCaptureProcess();
		UpdateBroadcast();
		UpdateScoring();
	}
}

bool CGameControllerDOM::OnEntity(int Index, vec2 Pos)
{
	if(IGameController::OnEntity(Index, Pos))
		return true;

	//	save domination spot flag positions
	switch (Index + ENTITY_OFFSET)
	{
	case TILE_DOM_FLAG_A: if (!m_apDominationSpots[0]) GameServer()->m_World.InsertEntity(m_apDominationSpots[0] = new CDominationSpot(&GameServer()->m_World, Pos, 0, m_aCapTimes)); break;
	case TILE_DOM_FLAG_B: if (!m_apDominationSpots[1]) GameServer()->m_World.InsertEntity(m_apDominationSpots[1] = new CDominationSpot(&GameServer()->m_World, Pos, 1, m_aCapTimes)); break;
	case TILE_DOM_FLAG_C: if (!m_apDominationSpots[2]) GameServer()->m_World.InsertEntity(m_apDominationSpots[2] = new CDominationSpot(&GameServer()->m_World, Pos, 2, m_aCapTimes)); break;
	case TILE_DOM_FLAG_D: if (!m_apDominationSpots[3]) GameServer()->m_World.InsertEntity(m_apDominationSpots[3] = new CDominationSpot(&GameServer()->m_World, Pos, 3, m_aCapTimes)); break;
 	case TILE_DOM_FLAG_E: if (!m_apDominationSpots[4]) GameServer()->m_World.InsertEntity(m_apDominationSpots[4] = new CDominationSpot(&GameServer()->m_World, Pos, 4, m_aCapTimes)); break;
	default: return false;
	}
	
	return true;
}

void CGameControllerDOM::OnPlayerConnect(CPlayer *pPlayer)
{
	IGameController::OnPlayerConnect(pPlayer);
	pPlayer->m_RespawnTick = IsGameRunning()? Server()->Tick()+Server()->TickSpeed()*GetRespawnDelay(false) : Server()->Tick();
	SendChatInfoWithHeader(pPlayer->GetCID());
}

float CGameControllerDOM::GetRespawnDelay(bool Self)
{
	return max(IGameController::GetRespawnDelay(Self), Self ? g_Config.m_SvDomRespawnDelay + 3.0f : g_Config.m_SvDomRespawnDelay);
}

bool CGameControllerDOM::EvaluateSpawnPosDom(CSpawnEval *pEval, vec2 Pos) const
{
	float BadDistance = FLT_MAX;
	float GoodDistance = FLT_MAX;
	for(int i = 0; i < DOM_MAXDSPOTS; i++)
	{
		if (!m_aDominationSpotsEnabled[i])
			continue;
		if (m_apDominationSpots[i]->GetTeam() == pEval->m_FriendlyTeam) // own dspot
			GoodDistance = min(GoodDistance, distance(Pos, m_apDominationSpots[i]->GetPos()));
		else	// neutral or enemy dspot
			BadDistance = min(BadDistance, distance(Pos, m_apDominationSpots[i]->GetPos()));
	}
	return GoodDistance <= BadDistance;
}

//	choose a random spawn point near an own domination spot, else a random one
void CGameControllerDOM::EvaluateSpawnTypeDom(CSpawnEval *pEval, int Type) const
{
	if (!m_aNumSpawnPoints[Type])
		return;
	// get spawn point
	int NumStartpoints = 0;
	int *pStartpoint = new int[m_aNumSpawnPoints[Type]];
	for(int i  = 0; i < m_aNumSpawnPoints[Type]; i++)
		if (EvaluateSpawnPosDom(pEval, m_aaSpawnPoints[Type][i]))
			pStartpoint[NumStartpoints++] = i;
	pEval->m_Got = true;
	pEval->m_Pos = NumStartpoints ? m_aaSpawnPoints[Type][pStartpoint[Server()->Tick() % NumStartpoints]] :
		m_aaSpawnPoints[Type][Server()->Tick() % m_aNumSpawnPoints[Type]];
	delete[] pStartpoint;
	pStartpoint = 0;
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

	// try first try own team spawn, then normal spawn
	EvaluateSpawnTypeDom(&Eval, 1+(Team&1));
	if(!Eval.m_Got)
	{
		EvaluateSpawnTypeDom(&Eval, 0);
		if(!Eval.m_Got)
			EvaluateSpawnTypeDom(&Eval, 1+((Team+1)&1));
	}

	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}

void CGameControllerDOM::UpdateCaptureProcess()
{
	int aaPlayerStats[DOM_MAXDSPOTS][DOM_NUMOFTEAMS];	//	number of players per team per capturing area
	int aaPlayerStrength[DOM_MAXDSPOTS][DOM_NUMOFTEAMS];	//	capture strength of players per team per capturing area
	CCharacter* aaapCapPlayers[DOM_NUMOFTEAMS][DOM_MAXDSPOTS][MAX_CLIENTS];	//	capturing players in a capturing area
	mem_zero(aaPlayerStats, DOM_MAXDSPOTS * DOM_NUMOFTEAMS * sizeof(int));
	mem_zero(aaPlayerStrength, DOM_MAXDSPOTS * DOM_NUMOFTEAMS * sizeof(int));
	mem_zero(aaapCapPlayers, DOM_MAXDSPOTS * MAX_CLIENTS * sizeof(CCharacter*));
	CPlayer* pPlayer;
	int DomCaparea;

	// check all players for being in a capture area
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		pPlayer = GameServer()->m_apPlayers[i];
		if (!pPlayer || pPlayer->GetTeam() == TEAM_SPECTATORS || !pPlayer->GetCharacter() || !pPlayer->GetCharacter()->IsAlive())
			continue;
		DomCaparea = min(GameServer()->Collision()->GetCollisionAt(static_cast<int>(pPlayer->GetCharacter()->GetPos().x),
			static_cast<int>(pPlayer->GetCharacter()->GetPos().y)) >> 3, 5) - 1;
		if (DomCaparea != -1 && m_aDominationSpotsEnabled[DomCaparea])
		{
			aaapCapPlayers[pPlayer->GetTeam()][DomCaparea][aaPlayerStats[DomCaparea][pPlayer->GetTeam()]++] = pPlayer->GetCharacter();
			aaPlayerStrength[DomCaparea][pPlayer->GetTeam()] += pPlayer->GetCharacter()->IsNinja()? 2 : 1; // ninja doubles the players strength
		}
	}

	//	capturing process for all dspots
	for (int i = 0; i < DOM_MAXDSPOTS; ++i)
	{
		if (!m_aDominationSpotsEnabled[i])
			continue;
		
		if (m_apDominationSpots[i]->IsGettingCaptured())
		{
			if (m_apDominationSpots[i]->UpdateCapturing(aaPlayerStrength[i][m_apDominationSpots[i]->GetCapTeam()], aaPlayerStrength[i][m_apDominationSpots[i]->GetCapTeam() ^ 1]))
			{
				if (m_apDominationSpots[i]->GetTeam() == DOM_NEUTRAL)
				{
					Neutralize(i);
					StartCapturing(i, aaPlayerStrength[i][DOM_RED], aaPlayerStrength[i][DOM_BLUE], true);
				}
				else
					Capture(i);
			}
		}
		else
			StartCapturing(i, aaPlayerStrength[i][DOM_RED], aaPlayerStrength[i][DOM_BLUE], false);
	}
}

void CGameControllerDOM::StartCapturing(int SpotNumber, int NumOfRedCapPlayers, int NumOfBlueCapPlayers, bool Consecutive)
{
	if (SpotNumber < 0 || SpotNumber > DOM_MAXDSPOTS - 1)
		return;

	if (m_apDominationSpots[SpotNumber]->GetTeam() == DOM_NEUTRAL)
	{
		if (NumOfRedCapPlayers < NumOfBlueCapPlayers)
			m_apDominationSpots[SpotNumber]->StartCapturing(DOM_BLUE, GetTeamSize(TEAM_BLUE), GetTeamSize(TEAM_RED), Consecutive);
		if (NumOfRedCapPlayers > NumOfBlueCapPlayers)
			m_apDominationSpots[SpotNumber]->StartCapturing(DOM_RED, GetTeamSize(TEAM_RED), GetTeamSize(TEAM_BLUE), Consecutive);
	}
	else
		if ((m_apDominationSpots[SpotNumber]->GetTeam() == DOM_RED && NumOfBlueCapPlayers) || (m_apDominationSpots[SpotNumber]->GetTeam() == DOM_BLUE && NumOfRedCapPlayers))
			m_apDominationSpots[SpotNumber]->StartCapturing(m_apDominationSpots[SpotNumber]->GetTeam() ^ 1, GetTeamSize(m_apDominationSpots[SpotNumber]->GetTeam() ^ 1), GetTeamSize(m_apDominationSpots[SpotNumber]->GetTeam()), Consecutive);
}

void CGameControllerDOM::Capture(int SpotNumber)
{
	if (SpotNumber < 0 || SpotNumber > DOM_MAXDSPOTS - 1)
		return;

	++m_aNumOfTeamDominationSpots[m_apDominationSpots[SpotNumber]->GetTeam()];
	OnCapture(SpotNumber, m_apDominationSpots[SpotNumber]->GetTeam());
}

void CGameControllerDOM::Neutralize(int SpotNumber)
{
	if (SpotNumber < 0 || SpotNumber > DOM_MAXDSPOTS - 1)
		return;

	--m_aNumOfTeamDominationSpots[m_apDominationSpots[SpotNumber]->GetCapTeam() ^ 1];

	OnNeutralize(SpotNumber, m_apDominationSpots[SpotNumber]->GetCapTeam());
}

void CGameControllerDOM::UpdateBroadcast()
{
	if (m_LastBroadcastTick + Server()->TickSpeed() <= Server()->Tick())
	{
		char aBuf[256] = {0};
		for (int cid = 0; cid < MAX_CLIENTS; ++cid)
		{
			if (GameServer()->m_apPlayers[cid])
			{
				if (GetRespawnDelay(false) > 3.0f && GameServer()->m_apPlayers[cid]->GetTeam() != TEAM_SPECTATORS
						&& !GameServer()->m_apPlayers[cid]->GetCharacter() && GameServer()->m_apPlayers[cid]->m_RespawnTick > Server()->Tick()+Server()->TickSpeed())
				{
					str_format(aBuf, sizeof(aBuf), "Respawn in %i seconds", (GameServer()->m_apPlayers[cid]->m_RespawnTick - Server()->Tick()) / Server()->TickSpeed());
					SendBroadcast(cid, aBuf);
				}
				else
					SendBroadcast(cid, "");
			}
		}
		m_LastBroadcastTick = Server()->Tick();
	}
}

void CGameControllerDOM::UpdateScoring()
{
	// add captured score to the teamscores
	for (int i = 0; i < DOM_NUMOFTEAMS; ++i)
	{
		if (!m_aNumOfTeamDominationSpots[i])
			continue;
		double AddScore;
		m_aTeamscoreTick[i] = modf(m_aTeamscoreTick[i] + static_cast<float>(m_aNumOfTeamDominationSpots[i]) * m_DompointsCounter, &AddScore);
		m_aTeamscore[i] += static_cast<int>(AddScore);
	}
}

void CGameControllerDOM::SendBroadcast(int ClientID, const char *pText) const
{
	float MarkerStepWidth;
	int MarkerPos;

	char aBuf[1024] = {0};
	int TextLen = 0;

	if(strlen(pText) > 0)
		TextLen += snprintf(aBuf + TextLen, sizeof(aBuf), "%s\\n", pText);
	for (int i = 0; i < DOM_MAXDSPOTS; ++i)
	{
		if (m_aDominationSpotsEnabled[i])
		{
			if (m_apDominationSpots[i]->IsGettingCaptured())
			{
				MarkerStepWidth = m_apDominationSpots[i]->GetCapTime() / 5.0f; // 5 marker pos
				MarkerPos = static_cast<int>(m_apDominationSpots[i]->GetTimer() / Server()->TickSpeed() / MarkerStepWidth);
				if (MarkerPos >= 0
						&& ((m_apDominationSpots[i]->GetTeam() == DOM_NEUTRAL && m_apDominationSpots[i]->GetCapTeam() == DOM_BLUE)
						|| (m_apDominationSpots[i]->GetTeam() == DOM_RED && m_apDominationSpots[i]->GetCapTeam() == DOM_BLUE)))
					MarkerPos = 4 - MarkerPos; // the other way around
				else
					++MarkerPos;
			}
			else
				MarkerPos = -1;

			// TODO Refactor: This is getting out of hand
			TextLen += snprintf(aBuf + TextLen, sizeof(aBuf), "%s%s%s%s%c%s%s%s "
					, GetBroadcastOpen(i, MarkerPos) // Opening Parenthesis
					, GetBroadcastLine(i, MarkerPos)
					, GetBroadcastLine(i, MarkerPos)
					, GetTeamBroadcastColor(m_apDominationSpots[i]->GetTeam()), GetSpotName(i)[0]
					, GetBroadcastLine(i, MarkerPos)
					, GetBroadcastLine(i, MarkerPos)
					, GetBroadcastClose(i, MarkerPos)); // Closing Parenthesis
		}
	}
	if (TextLen)
	{
		aBuf[TextLen - 1] = 0;
		GameServer()->SendBroadcast(aBuf, ClientID);
	}
}

const char* CGameControllerDOM::GetBroadcastOpen(int SpotNumber, int &rMarkerPos) const
{
	if (rMarkerPos-- == 0)
		return GetBroadcastMarker(SpotNumber);
	else
	{
		if (m_apDominationSpots[SpotNumber]->GetTeam() == DOM_NEUTRAL)
		{
			if (m_apDominationSpots[SpotNumber]->IsGettingCaptured() && m_apDominationSpots[SpotNumber]->GetCapTeam() == DOM_RED)
				return BCOLOR_RED "{";
			else
				return BCOLOR_NEUTRAL "(";
		}
		else if (m_apDominationSpots[SpotNumber]->GetTeam() == DOM_RED)
			return BCOLOR_RED "{";			
		else
		{
			if (m_apDominationSpots[SpotNumber]->IsGettingCaptured())
				return BCOLOR_NEUTRAL "(";
			else
				return BCOLOR_BLUE "[";
		}
	}
}

const char* CGameControllerDOM::GetBroadcastLine(int SpotNumber, int &rMarkerPos) const
{
	if (rMarkerPos-- == 0)
		return GetBroadcastMarker(SpotNumber);
	else
	{
		if (m_apDominationSpots[SpotNumber]->GetTeam() == DOM_NEUTRAL)
		{
			if (m_apDominationSpots[SpotNumber]->IsGettingCaptured())
			{
				if (m_apDominationSpots[SpotNumber]->GetCapTeam() == DOM_RED)
				{
					if (rMarkerPos < 0)
						return BCOLOR_RED BCHAR_LINE;
					else
						return BCOLOR_NEUTRAL BCHAR_LINE;
				}
				else
				{
					if (rMarkerPos < 0)
						return BCOLOR_NEUTRAL BCHAR_LINE;
					else
						return BCOLOR_BLUE BCHAR_LINE;
				}
			}
			else
				return BCOLOR_NEUTRAL BCHAR_LINE;
		}
		else if (m_apDominationSpots[SpotNumber]->GetTeam() == DOM_RED)
		{
			if (m_apDominationSpots[SpotNumber]->IsGettingCaptured() && rMarkerPos >= 0)
				return BCOLOR_NEUTRAL BCHAR_LINE;
			else
				return BCOLOR_RED BCHAR_LINE;
		}
		else
		{
			if (m_apDominationSpots[SpotNumber]->IsGettingCaptured() && rMarkerPos < 0)
				return BCOLOR_NEUTRAL BCHAR_LINE;
			else
				return BCOLOR_BLUE BCHAR_LINE;
		}
	}
}

const char* CGameControllerDOM::GetBroadcastClose(int SpotNumber, int &rMarkerPos) const
{
	if (rMarkerPos-- == 0)
		return GetBroadcastMarker(SpotNumber);
	else
	{
		if (m_apDominationSpots[SpotNumber]->GetTeam() == DOM_NEUTRAL)
		{
			if (m_apDominationSpots[SpotNumber]->IsGettingCaptured() && m_apDominationSpots[SpotNumber]->GetCapTeam() == DOM_BLUE)
				return BCOLOR_BLUE "]";
			else
				return BCOLOR_NEUTRAL ")";
		}
		else if (m_apDominationSpots[SpotNumber]->GetTeam() == DOM_RED)
		{
			if (m_apDominationSpots[SpotNumber]->IsGettingCaptured())
				return BCOLOR_NEUTRAL ")";
			else
				return BCOLOR_RED "}";
		}
		else // DOM_BLUE
			return BCOLOR_BLUE "]";
	}
}

const char* CGameControllerDOM::GetBroadcastMarker(int SpotNumber) const
{
	if (!m_apDominationSpots[SpotNumber]->IsGettingCaptured())
		return ""; // should not occur

	if (m_apDominationSpots[SpotNumber]->GetTeam() == DOM_NEUTRAL)
	{
		if (m_apDominationSpots[SpotNumber]->GetCapTeam() == DOM_RED)
			return BCOLOR_RED ">";
		else // DOM_BLUE
			return BCOLOR_BLUE "<";
	}
	else if (m_apDominationSpots[SpotNumber]->GetCapTeam() == DOM_RED)
		return BCOLOR_NEUTRAL ">";
	else // DOM_BLUE
		return BCOLOR_NEUTRAL "<";
}

void CGameControllerDOM::SetCapTimes(const char *pCapTimes)
{
	sscanf(pCapTimes, "%d %d %d %d %d %d %d %d", m_aCapTimes + 1, m_aCapTimes + 2, m_aCapTimes + 3, m_aCapTimes + 4, m_aCapTimes + 5, m_aCapTimes + 6, m_aCapTimes + 7, m_aCapTimes +8);
	m_aCapTimes[0] = 5;
	for (int i = 1; i < MAX_PLAYERS / DOM_NUMOFTEAMS + 1; ++i)
		if (m_aCapTimes[i] <= 0)
			m_aCapTimes[i] = m_aCapTimes[i - 1];
		else if (m_aCapTimes[i] > 60)
			m_aCapTimes[i] = 60;
	m_aCapTimes[0] = m_aCapTimes[1];
}

const char* CGameControllerDOM::GetTeamName(const int Team) const
{
	switch(Team)
	{
	case 0: return "Red Team"; break;
	case 1: return "Blue Team"; break;
	default: return "Neutral"; break;
	}
}

const char* CGameControllerDOM::GetSpotName(const int SpotID) const
{
	switch (SpotID)
	{
	case 0: return "A";
	case 1: return "B";
	case 2: return "C";
	case 3: return "D";
	case 4: return "E";
	default: return "Area 51";	//	should never occur
	}
}

const char* CGameControllerDOM::GetTeamBroadcastColor(const int Team) const
{
	switch(Team)
	{
	case DOM_RED:  return "^900";
	case DOM_BLUE: return "^009";
	default:       return "^999";
	}
}

void CGameControllerDOM::SendChat(int ClientID, const char *pText) const
{
	CNetMsg_Sv_Chat Msg;
	Msg.m_Mode = CHAT_ALL;
	Msg.m_ClientID = -1;
	Msg.m_pMessage = pText;
	Msg.m_TargetID = -1;
	Server()->SendPackMsg(&Msg, MSGFLAG_VITAL, ClientID);
}

void CGameControllerDOM::SendChatCommand(int ClientID, const char *pCommand)
{
	if (ClientID >= 0 && ClientID < MAX_CLIENTS)
	{
		if (str_comp_nocase(pCommand, "/info") == 0 || str_comp_nocase(pCommand, "/help") == 0)
		{
			SendChatInfoWithHeader(ClientID);
		}
		else if (str_comp_nocase(pCommand, "/spots") == 0 || str_comp_nocase(pCommand, "/domspots") == 0)
			SendChatStats(ClientID);
		else
			SendChat(ClientID, "Unknown command. Type '/help' for more information about this mod.");
	}
}

void CGameControllerDOM::SendChatInfoWithHeader(int ClientID)
{
	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "Domination Mod (%s) by Slayer, orig. by ziltoide and Oy.", GameServer()->ModVersion());
	SendChat(ClientID, aBuf);

	SendChat(ClientID, "——————————————————————————————");
	SendChatInfo(ClientID);
}

void CGameControllerDOM::SendChatInfo(int ClientID)
{
	SendChat(ClientID, "GAMETYPE: DOMINATION");
	SendChat(ClientID, "Capture domination spots.");
	SendChat(ClientID, "Tip: Capture together to reduce the required time.");
	SendChat(ClientID, "For each captured spot, your teams score increases over time.");
	SendChat(ClientID, "(This mod is enjoyed best with enabled broadcast color.)");
}

void CGameControllerDOM::SendChatStats(int ClientID)
{
	SendChat(ClientID, "——— Domination Spots ———");

	if (m_NumOfDominationSpots == 0)
		SendChat(ClientID, "No domination spots available on this map.");
	else
	{
		char aBuf[32];
		for (int i = 0; i < DOM_MAXDSPOTS; ++i)
			if (m_aDominationSpotsEnabled[i])
			{
				str_format(aBuf, sizeof(aBuf), "%s: %s", GetSpotName(i), GetTeamName(m_apDominationSpots[i]->GetTeam()));
				SendChat(ClientID, aBuf);
			}
	}
}