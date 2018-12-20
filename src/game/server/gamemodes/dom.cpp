/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <float.h>
#include <stdio.h>

#include <engine/shared/config.h>

#include <game/mapitems.h>
#include <game/version.h>

#include <game/server/entity.h>
#include <game/server/entities/character.h>
#include <game/server/entities/flag.h>
#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include "dom.h"

CGameControllerDOM::CGameControllerDOM(CGameContext *pGameServer)
: IGameController(pGameServer)
{
	m_apDominationSpots[0] = m_apDominationSpots[1] = m_apDominationSpots[2] = m_apDominationSpots[3] = m_apDominationSpots[4] = 0;
	m_pGameType = "DOM";

	m_GameFlags = GAMEFLAG_TEAMS|GAMEFLAG_FLAGS;

	m_aTeamscoreTick[0] = 0;
	m_aTeamscoreTick[1] = 0;

	m_aTeamDominationSpots[0] = 0;
	m_aTeamDominationSpots[1] = 0;

	m_UpdateBroadcast = false;
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
}

void CGameControllerDOM::OnReset()
{
	IGameController::OnReset();

	m_aTeamscoreTick[0] = 0;
	m_aTeamscoreTick[1] = 0;
	m_aTeamDominationSpots[0] = 0;
	m_aTeamDominationSpots[1] = 0;
	m_UpdateBroadcast = false;
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
	case TILE_DOM_FLAG_A: if (!m_apDominationSpots[0]) GameServer()->m_World.InsertEntity(m_apDominationSpots[0] = new CDominationSpot(&GameServer()->m_World, Pos, 0)); break;
	case TILE_DOM_FLAG_B: if (!m_apDominationSpots[1]) GameServer()->m_World.InsertEntity(m_apDominationSpots[1] = new CDominationSpot(&GameServer()->m_World, Pos, 1)); break;
	case TILE_DOM_FLAG_C: if (!m_apDominationSpots[2]) GameServer()->m_World.InsertEntity(m_apDominationSpots[2] = new CDominationSpot(&GameServer()->m_World, Pos, 2)); break;
	case TILE_DOM_FLAG_D: if (!m_apDominationSpots[3]) GameServer()->m_World.InsertEntity(m_apDominationSpots[3] = new CDominationSpot(&GameServer()->m_World, Pos, 3)); break;
 	case TILE_DOM_FLAG_E: if (!m_apDominationSpots[4]) GameServer()->m_World.InsertEntity(m_apDominationSpots[4] = new CDominationSpot(&GameServer()->m_World, Pos, 4)); break;
	default: return false;
	}
	
	Init(); // TODO request once to improve performance
	return true;
}

int CGameControllerDOM::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, Weapon);
	int HadFlag = 0;

	//	add flags to kill messages and give capture tee killer extra points
	for (int i = 0; i < DOM_MAXDSPOTS; ++i)
	{
		if (!m_aDominationSpotsEnabled[i] || !m_apDominationSpots[i]->m_IsGettingCaptured)
			continue;
		if (pKiller && m_apDominationSpots[i]->m_pCapCharacter == pKiller->GetCharacter())
			HadFlag |= 2;
		if (m_apDominationSpots[i]->m_pCapCharacter == pVictim)
		{
			m_apDominationSpots[i]->m_pCapCharacter = 0;
			// if(pKiller && IsFriendlyFire(pKiller->GetCID(), pVictim->GetPlayer()->GetCID())) // OPT re-add this if needed
			// 	pKiller->m_Score += g_Config.SvDomCapKillPoints;
			HadFlag |= 1;
		}
	}
	return HadFlag;
}

float CGameControllerDOM::GetRespawnDelay(bool Self)
{
	return max(IGameController::GetRespawnDelay(Self), Self ? g_Config.m_SvDomRespawnDelay + 3.0f : g_Config.m_SvDomRespawnDelay);
}

bool CGameControllerDOM::EvaluateSpawnPos2(CSpawnEval *pEval, vec2 Pos) const
{
	float BadDistance = FLT_MAX;
	float GoodDistance = FLT_MAX;
	for(int i = 0; i < DOM_MAXDSPOTS; i++)
	{
		if (!m_aDominationSpotsEnabled[i])
			continue;
		if (m_apDominationSpots[i]->m_Team == pEval->m_FriendlyTeam) // own dspot
			GoodDistance = min(GoodDistance, distance(Pos, m_apDominationSpots[i]->m_Pos));
		else	// neutral or enemy dspot
			BadDistance = min(BadDistance, distance(Pos, m_apDominationSpots[i]->m_Pos));
	}
	return GoodDistance <= BadDistance;
}

//	choose a random spawn point near an own domination spot, else a random one
void CGameControllerDOM::EvaluateSpawnType(CSpawnEval *pEval, int Type) const
{
	if (!m_aNumSpawnPoints[Type])
		return;
	// get spawn point
	int NumStartpoints = 0;
	int *pStartpoint = new int[m_aNumSpawnPoints[Type]];
	for(int i  = 0; i < m_aNumSpawnPoints[Type]; i++)
		if (EvaluateSpawnPos2(pEval, m_aaSpawnPoints[Type][i]))
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

	CSpawnEval Eval;

	Eval.m_FriendlyTeam = Team;

	// try first try own team spawn, then normal spawn
	EvaluateSpawnType(&Eval, 1+(Team&1));
	if(!Eval.m_Got)
	{
		EvaluateSpawnType(&Eval, 0);
		if(!Eval.m_Got)
			EvaluateSpawnType(&Eval, 1+((Team+1)&1));
	}

	*pOutPos = Eval.m_Pos;
	return Eval.m_Got;
}

void CGameControllerDOM::UpdateCaptureProcess()
{
	int aaPlayerStats[DOM_MAXDSPOTS][DOM_NUMOFTEAMS];	//	number of players per team per capturing area
	CCharacter* aaapCapPlayers[DOM_NUMOFTEAMS][DOM_MAXDSPOTS][MAX_CLIENTS];	//	capturing players in a capturing area
	mem_zero(aaPlayerStats, DOM_MAXDSPOTS * DOM_NUMOFTEAMS * sizeof(int));
	mem_zero(aaapCapPlayers, DOM_MAXDSPOTS * MAX_CLIENTS * sizeof(CCharacter*));
	int aTeamSize[2] = {0, 0};
	CPlayer* pPlayer;
	int DomCaparea;

	// check all players for being in a capture area
	for (int i = 0; i < MAX_CLIENTS; ++i)
	{
		pPlayer = GameServer()->m_apPlayers[i];
		if(pPlayer && pPlayer->GetTeam() != TEAM_SPECTATORS)
			++aTeamSize[pPlayer->GetTeam()];
		if (!pPlayer || pPlayer->GetTeam() == TEAM_SPECTATORS ||
			!pPlayer->GetCharacter() || !pPlayer->GetCharacter()->IsAlive())
			continue;
		DomCaparea = min(GameServer()->Collision()->GetCollisionAt(static_cast<int>(pPlayer->GetCharacter()->GetPos().x),
			static_cast<int>(pPlayer->GetCharacter()->GetPos().y)) >> 3, 5) - 1;
		if (DomCaparea != -1 && m_aDominationSpotsEnabled[DomCaparea])
			aaapCapPlayers[pPlayer->GetTeam()][DomCaparea][aaPlayerStats[DomCaparea][pPlayer->GetTeam()]++] = pPlayer->GetCharacter();
	}


	//	update capturing characters
	for (int i = 0; i < DOM_MAXDSPOTS; ++i)
		if (m_aDominationSpotsEnabled[i] && m_apDominationSpots[i]->m_IsGettingCaptured && (!m_apDominationSpots[i]->m_pCapCharacter ||
			min(GameServer()->Collision()->GetCollisionAt(static_cast<int>(m_apDominationSpots[i]->m_pCapCharacter->GetPos().x), static_cast<int>(m_apDominationSpots[i]->m_pCapCharacter->GetPos().y)) >> 3, 5) - 1 != i))
		{
			m_apDominationSpots[i]->m_pCapCharacter = aaPlayerStats[i][m_apDominationSpots[i]->m_CapTeam] ?
				aaapCapPlayers[m_apDominationSpots[i]->m_CapTeam][i][Server()->Tick() % aaPlayerStats[i][m_apDominationSpots[i]->m_CapTeam]] : 0;
		}

	//	capturing process for all dspots
	for (int i = 0; i < DOM_MAXDSPOTS; ++i)
	{
		if (!m_aDominationSpotsEnabled[i])
			continue;
		
		if (m_apDominationSpots[i]->m_IsGettingCaptured)
		{
			//	capturing process already running
			if ((m_apDominationSpots[i]->m_Team == DOM_NEUTRAL && aaPlayerStats[i][m_apDominationSpots[i]->m_CapTeam] && !aaPlayerStats[i][m_apDominationSpots[i]->m_CapTeam ^ 1]) ||
				(m_apDominationSpots[i]->m_Team != DOM_NEUTRAL && aaPlayerStats[i][m_apDominationSpots[i]->m_CapTeam]))
			{
				if (m_apDominationSpots[i]->UpdateCapturing(aaPlayerStats[i][m_apDominationSpots[i]->m_CapTeam]))
				{
					//	capturing successful
					char aBuf[256];	//	capture chat message
					if (m_apDominationSpots[i]->m_Team == DOM_NEUTRAL)
					{
						--m_aTeamDominationSpots[m_apDominationSpots[i]->m_CapTeam ^ 1];
						str_format(aBuf, sizeof(aBuf), "%s took spot %s away from the %s. (%s: %i/%i, %s: %i/%i)",
							Server()->ClientName(m_apDominationSpots[i]->m_pCapCharacter->GetPlayer()->GetCID()), m_apDominationSpots[i]->GetSpotName(), m_apDominationSpots[i]->GetTeamName(m_apDominationSpots[i]->m_CapTeam ^ 1),
							m_apDominationSpots[i]->GetTeamName(0), m_aTeamDominationSpots[0], m_NumOfDominationSpots, m_apDominationSpots[i]->GetTeamName(1), m_aTeamDominationSpots[1], m_NumOfDominationSpots);
						if (!aaPlayerStats[i][m_apDominationSpots[i]->m_CapTeam ^ 1])
							m_apDominationSpots[i]->StartCapturing(m_apDominationSpots[i]->m_CapTeam, aTeamSize[m_apDominationSpots[i]->m_CapTeam], aTeamSize[m_apDominationSpots[i]->m_CapTeam ^ 1]);
					}
					else
					{
						++m_aTeamDominationSpots[m_apDominationSpots[i]->m_Team];
						OnCapture(m_apDominationSpots[i]->m_Team);
						str_format(aBuf, sizeof(aBuf), "%s captured spot %s for the %s. (%s: %i/%i, %s: %i/%i)",
							Server()->ClientName(m_apDominationSpots[i]->m_pCapCharacter->GetPlayer()->GetCID()), m_apDominationSpots[i]->GetSpotName(), m_apDominationSpots[i]->GetTeamName(m_apDominationSpots[i]->m_Team),
							m_apDominationSpots[i]->GetTeamName(0), m_aTeamDominationSpots[0], m_NumOfDominationSpots, m_apDominationSpots[i]->GetTeamName(1), m_aTeamDominationSpots[1], m_NumOfDominationSpots);
					}
					GameServer()->SendChat(-1, CHAT_ALL, -1, aBuf);
				}
			}
			else		//	stop capturing process
				m_apDominationSpots[i]->StopCapturing();
			m_UpdateBroadcast = true;
		}
		else
		{
			//	start capturing process
			if (m_apDominationSpots[i]->m_Team == DOM_NEUTRAL)
			{
				if (!aaPlayerStats[i][DOM_RED] && aaPlayerStats[i][DOM_BLUE])
					m_apDominationSpots[i]->StartCapturing(DOM_BLUE, aTeamSize[DOM_BLUE], aTeamSize[DOM_RED]);
				if (!aaPlayerStats[i][DOM_BLUE] && aaPlayerStats[i][DOM_RED])
					m_apDominationSpots[i]->StartCapturing(DOM_RED, aTeamSize[DOM_RED], aTeamSize[DOM_BLUE]);
			}
			else
				if (aaPlayerStats[i][m_apDominationSpots[i]->m_Team ^ 1])
					m_apDominationSpots[i]->StartCapturing(m_apDominationSpots[i]->m_Team ^ 1, aTeamSize[m_apDominationSpots[i]->m_Team ^ 1], aTeamSize[m_apDominationSpots[i]->m_Team]);
		}
	}
}

void CGameControllerDOM::UpdateBroadcast()
{
	//	update caturing broadcast timer
	if (m_LastBroadcastTick != -1 && m_LastBroadcastTick + Server()->TickSpeed() <= Server()->Tick())
		m_LastBroadcastTick = -1;

	//	update caturing broadcast message
	if (m_LastBroadcastTick == -1 && m_UpdateBroadcast)
	{
		char aBuf[256] = {0};
		int TextLen = 0;
		for (int i = 0; i < DOM_MAXDSPOTS; ++i)
		{
			// TODO resolve buffer overflow warning
			if (m_aDominationSpotsEnabled[i] && m_apDominationSpots[i]->m_IsGettingCaptured)
				TextLen += m_apDominationSpots[i]->m_Team == DOM_NEUTRAL ?
					sprintf(aBuf + TextLen, "%c: %s %2i,", m_apDominationSpots[i]->GetSpotName()[0], m_apDominationSpots[i]->m_CapTeam == DOM_RED ? "Red" : "Blue", (m_apDominationSpots[i]->m_Timer - 1) / Server()->TickSpeed() + 1) :
					sprintf(aBuf + TextLen, "%c: %s %2i,", m_apDominationSpots[i]->GetSpotName()[0], m_apDominationSpots[i]->m_Team == DOM_RED ? "No Red" : "No Blue", (m_apDominationSpots[i]->m_Timer - 1) / Server()->TickSpeed() + 1);
		}
		if (TextLen)
			aBuf[TextLen - 1] = 0;
		GameServer()->SendBroadcast(aBuf, -1);
		m_LastBroadcastTick = Server()->Tick();
		m_UpdateBroadcast = false;
	}
}

void CGameControllerDOM::UpdateScoring()
{
	// add captured score to the teamscores
	for (int i = 0; i < DOM_NUMOFTEAMS; ++i)
	{
		if (!m_aTeamDominationSpots[i])
			continue;
		double AddScore;
		m_aTeamscoreTick[i] = modf(m_aTeamscoreTick[i] + static_cast<float>(m_aTeamDominationSpots[i]) * m_DompointsCounter, &AddScore);
		m_aTeamscore[i] += static_cast<int>(AddScore);
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
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), "Domination Mod (%s) by Slayer, orig. by ziltoide and Oy.", GameServer()->ModVersion());
			SendChat(ClientID, aBuf);

			SendChat(ClientID, "——————————————————————————————");
			SendChatInfo(ClientID);
		}
		else if (str_comp_nocase(pCommand, "/spots") == 0 || str_comp_nocase(pCommand, "/domspots") == 0)
			SendChatStats(ClientID);
		else
			SendChat(ClientID, "Unknown command. Type '/help' for more information about this mod.");
	}
}

void CGameControllerDOM::SendChatInfo(int ClientID)
{
	SendChat(ClientID, "Capture domination spots.");
	SendChat(ClientID, "Tip: Capture together to reduce the required time.");
	SendChat(ClientID, "For each captured spot, your teams score increases over time.");
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
				str_format(aBuf, sizeof(aBuf), "%s: %s", m_apDominationSpots[i]->GetSpotName(), m_apDominationSpots[i]->GetTeamName());
				SendChat(ClientID, aBuf);
			}
	}
}


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
	for (int i = 1; i < MAX_CLIENTS / DOM_NUMOFTEAMS + 1; ++i)
		if (m_aCapTimes[i] < 0)
			m_aCapTimes[i] = 0;
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
	m_Timer = m_aCapTimes[min(CaptureTeamSize, MAX_CLIENTS / DOM_NUMOFTEAMS + 1)] * Server()->TickSpeed();
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