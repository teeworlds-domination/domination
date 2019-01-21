/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <float.h>
#include <stdio.h>

#include <engine/shared/config.h>

#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include <game/server/entities/character.h>
#include <game/server/entities/dspot.h>
#include <game/server/entities/strike_flag.h>
#include <game/server/entities/strike_pickup.h>
#include <math.h>

#include "strike.h"

CGameControllerSTRIKE::CGameControllerSTRIKE(CGameContext *pGameServer)
: CGameControllerDOM(pGameServer)
{
	m_pGameType = "CS:DOM";
	m_GameFlags = GAMEFLAG_TEAMS|GAMEFLAG_SURVIVAL;

	m_SentPersonalizedBroadcast = false;
	m_WinTick = -1;
	m_PurchaseTick = -1;

	m_NumOfStarterPickups = 0;
	
	m_apFlags[TEAM_RED] = 0;
	m_apFlags[TEAM_BLUE] = 0;

	m_BombPlacedCID = -1;

	SetCapTime(g_Config.m_SvStrikeCapTime);
}

void CGameControllerSTRIKE::Init()
{
	CGameControllerDOM::Init();

	if (m_NumOfDominationSpots)
	{
		m_apFlags[TEAM_BLUE] = new CStrikeFlag(&GameServer()->m_World, TEAM_BLUE, vec2(0.0f, 0.0f));
		if (!m_apFlags[TEAM_RED])
			GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "CS:DOM", "Red flag is missing: red team can not capture.");
	}
	else if (m_apFlags[TEAM_RED])
		m_apFlags[TEAM_RED]->Destroy();
}

void CGameControllerSTRIKE::OnReset()
{
	CGameControllerDOM::OnReset();

	m_GameInfo.m_TimeLimit = g_Config.m_SvStrikeTimelimit;
	UpdateGameInfo(-1);

	m_SentPersonalizedBroadcast = false;
	m_WinTick = -1;
	m_PurchaseTick = Server()->Tick() + Server()->TickSpeed() * g_Config.m_SvStrikeBuyTimelimit;
	m_BombPlacedCID = -1;
	if (m_apFlags[TEAM_BLUE])
		m_apFlags[TEAM_BLUE]->Hide();
}

void CGameControllerSTRIKE::Tick()
{
	CGameControllerDOM::Tick();

	if(m_GameState == IGS_GAME_RUNNING && !GameServer()->m_World.m_ResetRequested)
	{
		UpdatePickups();
		UpdateBomb();

		DoWincheckMatch();
	}
	else if (m_GameState != IGS_GAME_RUNNING)
	{
		if (m_WinTick != -1)
			++m_WinTick;
		if (m_PurchaseTick != -1)
			++m_PurchaseTick;
	}
}

bool CGameControllerSTRIKE::OnEntity(int Index, vec2 Pos)
{
	if (Index + ENTITY_OFFSET >= TILE_DOM_FLAG_A && Index + ENTITY_OFFSET < TILE_DOM_CAPAREA_E)
		return CGameControllerDOM::OnEntity(Index, Pos);

	int Type = -1;

	switch(Index)
	{
	case ENTITY_SPAWN:
		m_aaSpawnPoints[0][m_aNumSpawnPoints[0]++] = Pos;
		break;
	case ENTITY_SPAWN_RED:
		m_aaSpawnPoints[1][m_aNumSpawnPoints[1]++] = Pos;
		break;
	case ENTITY_SPAWN_BLUE:
		m_aaSpawnPoints[2][m_aNumSpawnPoints[2]++] = Pos;
		break;
	case ENTITY_ARMOR_1:
		Type = PICKUP_ARMOR;
		break;
	case ENTITY_HEALTH_1:
		Type = PICKUP_HEALTH;
		break;
	case ENTITY_WEAPON_SHOTGUN:
		Type = PICKUP_SHOTGUN;
		break;
	case ENTITY_WEAPON_GRENADE:
		Type = PICKUP_GRENADE;
		break;
	case ENTITY_WEAPON_LASER:
		Type = PICKUP_LASER;
		break;
	case ENTITY_POWERUP_NINJA:
		if(g_Config.m_SvPowerups)
			Type = PICKUP_NINJA;
		break;
	case ENTITY_AMMO:
		Type = PICKUP_AMMO;
	}

	if(Type != -1)
	{
		CStrikePickup *pPickup = new CStrikePickup(&GameServer()->m_World, Type, Pos, false);

		if (Type == WEAPON_SHOTGUN || Type == WEAPON_GRENADE || Type == WEAPON_LASER)
			m_apStarterPickups[m_NumOfStarterPickups++] = pPickup;

		return true;
	}

	int Team = -1;
	if(Index == ENTITY_FLAGSTAND_RED) Team = TEAM_RED;
	// if(Index == ENTITY_FLAGSTAND_BLUE) Team = TEAM_BLUE;
	if(Team == -1 || m_apFlags[Team])
		return false;

	m_apFlags[Team] = new CStrikeFlag(&GameServer()->m_World, Team, Pos);
	return true;
}

void CGameControllerSTRIKE::UpdatePickups()
{
	if (m_PurchaseTick != -1 && Server()->Tick() >= m_PurchaseTick)
	{
		m_PurchaseTick = -1;
		for (int Pickup = 0; Pickup < m_NumOfStarterPickups; ++Pickup)
			m_apStarterPickups[Pickup]->Despawn();
	}
}

void CGameControllerSTRIKE::DoWincheckMatch()
{
	// check score win condition
	if (m_GameInfo.m_ScoreLimit > 0 && (m_aTeamscore[TEAM_RED] >= m_GameInfo.m_ScoreLimit || m_aTeamscore[TEAM_BLUE] >= m_GameInfo.m_ScoreLimit))
		EndMatch();
}

void CGameControllerSTRIKE::DoWincheckRound()
 {
	int Count[2] = {0};
	for(int i = 0; i < MAX_CLIENTS; ++i)
	{
		if(GameServer()->m_apPlayers[i] && GameServer()->m_apPlayers[i]->GetTeam() != TEAM_SPECTATORS &&
			(!GameServer()->m_apPlayers[i]->m_RespawnDisabled ||
			(GameServer()->m_apPlayers[i]->GetCharacter() && GameServer()->m_apPlayers[i]->GetCharacter()->IsAlive())))
			++Count[GameServer()->m_apPlayers[i]->GetTeam()];
	}
	
	if (m_WinTick == -1)
	{
		// bomb not placed, yet
		if(Count[TEAM_RED] == 0
				|| (m_GameInfo.m_TimeLimit > 0 && (Server()->Tick()-m_GameStartTick) >= m_GameInfo.m_TimeLimit*Server()->TickSpeed()*60))
		{
			// all red dead or timelimit passed
			++m_aTeamscore[TEAM_BLUE];
			EndRound();
		}
	}
	else
	{
		if (m_WinTick != -1 && Server()->Tick() >= m_WinTick)
		{
			// bomb exploded
			++m_aTeamscore[TEAM_RED];
			if (m_BombPlacedCID > -1 && GameServer()->m_apPlayers[m_BombPlacedCID])
				GameServer()->m_apPlayers[m_BombPlacedCID]->m_Score += floorf(g_Config.m_SvDomCapPoints / 3.0f * 2);
			ExplodeBomb();
			EndRound();
		}
		else if(Count[TEAM_BLUE] == 0)
		{
			// bomb placed and all blue are dead
			++m_aTeamscore[TEAM_RED];
			EndRound();
		}
	}
}

void CGameControllerSTRIKE::EndRound()
{
	m_WinTick = -1;
	IGameController::EndRound();
	if (g_Config.m_SvStrikeHalftime && m_GameInfo.m_ScoreLimit > 1
			&& (m_aTeamscore[TEAM_RED] + m_aTeamscore[TEAM_BLUE] == m_GameInfo.m_ScoreLimit))
		GameServer()->SwapTeams();
}

void CGameControllerSTRIKE::ExplodeBomb()
{
	int Spot = -1;
	while ((Spot = GetNextSpot(Spot)) > -1)
	{
		if (m_apDominationSpots[Spot]->GetTeam() == DOM_RED)
		{
			vec2 Positions[9] = { vec2(0.0f, 0.0f), vec2(-256.0f, 0.0f), vec2(0.0f, -128.0f), vec2(256.0f, 0.0f), vec2(0.0f, 128.0f), vec2(128.0f, 256.0f), vec2(-128.0f, 256.0f), vec2(256.0f, -128.0f), vec2(-128.0f, -256.0f) };	// start, left, up, right, down
			for (int p = 0; p < 9; ++p)
			{
				GameServer()->CreateExplosion(m_apDominationSpots[Spot]->GetPos()+Positions[p], -1, WEAPON_GAME, 999);
				GameServer()->CreateSound(m_apDominationSpots[Spot]->GetPos()+Positions[p], SOUND_GRENADE_EXPLODE);
			}
		}
	}
	GameServer()->CreateGlobalSound(SOUND_GRENADE_EXPLODE);
}

bool CGameControllerSTRIKE::CanSpawn(int Team, vec2 *pOutPos)
{
	return IGameController::CanSpawn(Team, pOutPos);
}

void CGameControllerSTRIKE::OnStartCapturing(int Spot, int Team)
{
	GameServer()->CreateSound(m_apDominationSpots[Spot]->GetPos(), SOUND_PLAYER_SPAWN);

	if (m_apFlags[Team])
		m_apFlags[Team]->SetCapturing(true);
}

void CGameControllerSTRIKE::OnAbortCapturing(int Spot)
{
	if (m_apFlags[TEAM_RED] && m_apFlags[TEAM_RED]->IsCapturing())
	{
		m_apFlags[TEAM_RED]->SetCapturing(false);
	}
	else if (m_apFlags[TEAM_BLUE] && m_apFlags[TEAM_BLUE]->IsCapturing())
	{
		m_apFlags[TEAM_BLUE]->SetCapturing(false);
		m_apFlags[TEAM_BLUE]->Drop();
		m_apFlags[TEAM_BLUE]->Hide();
	}
}

void CGameControllerSTRIKE::OnCapture(int Spot, int Team, int NumOfCapCharacters, CCharacter* apCapCharacters[MAX_PLAYERS])
{
	if (m_apFlags[Team])
	{
		m_apFlags[Team]->SetCapturing(false);
		m_apFlags[Team]->Drop();
		m_apFlags[Team]->Hide();
	}

	if (Team == DOM_RED)
	{
		// bomb placed
		if (NumOfCapCharacters)
		{
			apCapCharacters[0]->GetPlayer()->m_Score += ceilf(g_Config.m_SvDomCapPoints / 3.0f);
			m_BombPlacedCID = apCapCharacters[0]->GetPlayer()->GetCID();
		}

		m_WinTick = Server()->Tick() + Server()->TickSpeed() * g_Config.m_SvStrikeExplodeTime;
		m_GameInfo.m_TimeLimit = 0;
		UpdateGameInfo(-1);
	}
	else
	{
		// bomb defused
		if (NumOfCapCharacters)
			apCapCharacters[0]->GetPlayer()->m_Score += g_Config.m_SvDomCapPoints;

		m_WinTick = -1;
		++m_aTeamscore[Team];
		EndRound();
	}
}

int CGameControllerSTRIKE::OnCharacterDeath(CCharacter *pVictim, CPlayer *pKiller, int Weapon)
{
	CGameControllerDOM::OnCharacterDeath(pVictim, pKiller, Weapon);

	if (pVictim && g_Config.m_SvStrikeDropAmmoOnDeath)
		DropAmmo(pVictim);

	int HadFlag = 0;

	for(int i = 0; i < 2; i++)
	{
		CFlag *F = m_apFlags[i];
		if(Weapon != WEAPON_GAME && F && pKiller && pKiller->GetCharacter() && F->GetCarrier() == pKiller->GetCharacter())
			HadFlag |= 2;
		if(F && F->GetCarrier() == pVictim)
		{
			GameServer()->SendGameMsg(GAMEMSG_CTF_DROP, -1);
			F->Drop();

			int CapSpot = -1;
			while ((CapSpot = GetNextSpot(CapSpot)) > -1)
			{
				if (m_apDominationSpots[CapSpot]->IsGettingCaptured())
				{
					m_apDominationSpots[CapSpot]->AbortCapturing();
					break;
				}
			}

			//if(pKiller && pKiller->GetTeam() != pVictim->GetPlayer()->GetTeam())
			//	pKiller->m_Score++;

			HadFlag |= 1;
		}
	}

	return HadFlag;
}

int CGameControllerSTRIKE::GetCharacterPrimaryWeaponAmmo(CCharacter *pChr) const
{
	if (!pChr)
		return 0;

	if (pChr->m_aWeapons[WEAPON_SHOTGUN].m_Got)
		return pChr->m_aWeapons[WEAPON_SHOTGUN].m_Ammo;
	else if (pChr->m_aWeapons[WEAPON_GRENADE].m_Got)
		return pChr->m_aWeapons[WEAPON_GRENADE].m_Ammo;
	else if (pChr->m_aWeapons[WEAPON_LASER].m_Got)
		return pChr->m_aWeapons[WEAPON_LASER].m_Ammo;
	else
		return 0;
}

void CGameControllerSTRIKE::DropAmmo(CCharacter *pChr) const
{
	int Amount = GetCharacterPrimaryWeaponAmmo(pChr);
	if (!Amount)
		return;

	int RemainingAmount = clamp(Amount, 0, g_Config.m_SvStrikeDropAmmoOnDeath);
	if (!RemainingAmount)
		return;

	vec2 CharacterPos;
	CharacterPos.x = static_cast<int>(pChr->GetPos().x - (static_cast<int>(pChr->GetPos().x) % 32)) + 16;
	CharacterPos.y = static_cast<int>(pChr->GetPos().y - (static_cast<int>(pChr->GetPos().y) % 32)) + 16;
	vec2 Positions[5] = { vec2(0.0f, 0.0f), vec2(-32.0f, 0.0f), vec2(0.0f, -32.0f), vec2(32.0f, 0.0f), vec2(0.0f, 32.0f) };	// start, left, up, right, down
	vec2 PickupPosAlt[4] = { vec2(8.0f, 0.0f), vec2(-8.0f, 0.0f), vec2(8.0f, -16.0f), vec2(-8.0f, -16.0f)};
	vec2 NextTilePos;
	for(int Index = 0; Index < 5 && RemainingAmount; ++Index)
	{
		NextTilePos = CharacterPos+Positions[Index];
		if(!GameServer()->Collision()->CheckPoint(NextTilePos))
		{
			for (int p = 0; p < 4 && RemainingAmount; ++p)
			{
				new CStrikePickup(&GameServer()->m_World, PICKUP_AMMO, NextTilePos+PickupPosAlt[p], true);
				--RemainingAmount;
			}
		}
	}
}

void CGameControllerSTRIKE::OnCharacterSpawn(CCharacter *pChr)
{
	// give start equipment
	pChr->IncreaseHealth(10);
	pChr->IncreaseArmor(g_Config.m_SvStrikeSpawnArmor);

	pChr->GiveWeapon(WEAPON_HAMMER, -1);
	pChr->GiveWeapon(WEAPON_GUN, 10);

	// prevent respawn
	pChr->GetPlayer()->m_RespawnDisabled = GetStartRespawnState();
}

void CGameControllerSTRIKE::OnPlayerDisconnect(CPlayer *pPlayer)
{
	if (pPlayer->GetCID() == m_BombPlacedCID)
		m_BombPlacedCID = -1;

	CGameControllerDOM::OnPlayerDisconnect(pPlayer);
}

int CGameControllerSTRIKE::CalcCaptureStrength(int Spot, CCharacter* pChr, bool IsFirst) const
{
	if (pChr->GetPlayer()->GetTeam() == TEAM_BLUE)
	{
		if (!m_apDominationSpots[Spot]->GetTeam() == DOM_RED)
			return 0;
		if (!m_apFlags[TEAM_BLUE]) // should not occur
			return BASE_CAPSTRENGTH;
		else if (m_apFlags[TEAM_BLUE]->GetCarrier())
			return m_apFlags[TEAM_BLUE]->GetCarrier() == pChr? BASE_CAPSTRENGTH : 0;
		else
		{
			// TODO should not being done here
			m_apFlags[TEAM_BLUE]->Grab(pChr);
			m_apFlags[TEAM_BLUE]->Show();
			return BASE_CAPSTRENGTH;
		}
	}

	return m_apFlags[TEAM_RED] && m_apFlags[TEAM_RED]->GetCarrier() == pChr? BASE_CAPSTRENGTH : 0;
}

bool CGameControllerSTRIKE::SendPersonalizedBroadcast(int ClientID)
{
	CCharacter *pChr = GameServer()->m_apPlayers[ClientID]->GetCharacter();
	if (m_PurchaseTick != -1)
	{
		if (!pChr || (!pChr->m_aWeapons[WEAPON_SHOTGUN].m_Got && !pChr->m_aWeapons[WEAPON_GRENADE].m_Got && !pChr->m_aWeapons[WEAPON_LASER].m_Got))
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "%sPick one weapon: %s%2i %sseconds left", GetTeamBroadcastColor(GameServer()->m_apPlayers[ClientID]->GetTeam() == TEAM_RED? DOM_RED : DOM_BLUE), GetTeamBroadcastColor(DOM_NEUTRAL), (m_PurchaseTick - Server()->Tick()) / Server()->TickSpeed(), GetTeamBroadcastColor(GameServer()->m_apPlayers[ClientID]->GetTeam() == TEAM_RED? DOM_RED : DOM_BLUE));
			CGameControllerDOM::SendBroadcast(ClientID, aBuf);
			m_SentPersonalizedBroadcast = true;
			return true;
		}
	}

	if (m_apFlags[TEAM_RED] && GameServer()->m_apPlayers[ClientID]->GetTeam() != TEAM_BLUE)
	{
		if ((m_apFlags[TEAM_RED]->IsAtStand() || !m_apFlags[TEAM_RED]->GetCarrier()) && !m_apFlags[TEAM_RED]->IsHidden())
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "%sThe flag has no carrier", GetTeamBroadcastColor(DOM_RED));
			CGameControllerDOM::SendBroadcast(ClientID, aBuf);
			m_SentPersonalizedBroadcast = true;
			return true;
		}
		else if (!m_apFlags[TEAM_RED]->IsHidden() && (!pChr || m_apFlags[TEAM_RED]->GetCarrier() == GameServer()->m_apPlayers[ClientID]->GetCharacter()))
		{
			char aBuf[128];
			str_format(aBuf, sizeof(aBuf), "%sPlace the flag on a spot", GetTeamBroadcastColor(DOM_RED));
			CGameControllerDOM::SendBroadcast(ClientID, aBuf);
			m_SentPersonalizedBroadcast = true;
			return true;
		}
	}

	if (m_WinTick != -1)
	{
		char aBuf[128];
		if (GameServer()->m_apPlayers[ClientID]->GetTeam() == TEAM_RED)
			str_format(aBuf, sizeof(aBuf), "%sDefend the spot: %s%2i %sseconds left.", GetTeamBroadcastColor(DOM_RED), GetTeamBroadcastColor(DOM_NEUTRAL)
					, (m_WinTick - Server()->Tick()) / Server()->TickSpeed(), GetTeamBroadcastColor(DOM_RED));
		else
			str_format(aBuf, sizeof(aBuf), "%sNeutralize the spot: %s%2i %sseconds left.", GetTeamBroadcastColor(DOM_BLUE), GetTeamBroadcastColor(DOM_NEUTRAL)
					, (m_WinTick - Server()->Tick()) / Server()->TickSpeed(), GetTeamBroadcastColor(DOM_BLUE));
		CGameControllerDOM::SendBroadcast(ClientID, aBuf);
		m_SentPersonalizedBroadcast = true;
		return true;
	}

	if (CGameControllerDOM::SendPersonalizedBroadcast(ClientID))
		return true;
	else if (m_SentPersonalizedBroadcast)
	{
		CGameControllerDOM::SendBroadcast(ClientID, "");
		m_SentPersonalizedBroadcast = false;
		return true;
	}
	return false;
}

void CGameControllerSTRIKE::SendChatInfo(int ClientID) const
{
	CGameControllerDOM::SendChat(ClientID, "GAMETYPE: STRIKE");
	CGameControllerDOM::SendChat(ClientID, "——————————————————————————");
	CGameControllerDOM::SendChat(ClientID, "Red: Pick the flag and capture a spot.");
	CGameControllerDOM::SendChat(ClientID, "Blue: Neutralize the spot again.");
	CGameControllerDOM::SendChat(ClientID, "Be aware of the timelimits!");
	CGameControllerDOM::SendChat(ClientID, "Survival: You can not respawn.");
}

void CGameControllerSTRIKE::ShowSpawns(int Spot) const
{
	GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "dom", "This command is not supported for the current gametype.");
}






bool CGameControllerSTRIKE::CanBeMovedOnBalance(int ClientID) const
{
	CCharacter* Character = GameServer()->m_apPlayers[ClientID]->GetCharacter();
	if(Character)
	{
		for(int fi = 0; fi < 2; fi++)
		{
			CFlag *F = m_apFlags[fi];
			if(F && F->GetCarrier() == Character)
				return false;
		}
	}
	return true;
}

void CGameControllerSTRIKE::UpdateBomb()
{
	CStrikeFlag *F = m_apFlags[TEAM_RED];

	if(!F)
		return;

	if (F->IsHidden())
		return;

	if(F->GetCarrier())
	{
		/*
		if(m_apFlags[fi^1] && m_apFlags[fi^1]->IsAtStand())
		{
			if(distance(F->GetPos(), m_apFlags[fi^1]->GetPos()) < CFlag::ms_PhysSize + CCharacter::ms_PhysSize)
			{
				// CAPTURE! \o/
				m_aTeamscore[fi^1] += 100;
				F->GetCarrier()->GetPlayer()->m_Score += 5;

				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "flag_capture player='%d:%s'",
					F->GetCarrier()->GetPlayer()->GetCID(),
					Server()->ClientName(F->GetCarrier()->GetPlayer()->GetCID()));
				GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

				GameServer()->SendGameMsg(GAMEMSG_CTF_CAPTURE, fi, F->GetCarrier()->GetPlayer()->GetCID(), Server()->Tick()-F->GetGrabTick(), -1);
				for(int i = 0; i < 2; i++)
					m_apFlags[i]->Reset();
			}
		} */
	}
	else
	{
		CCharacter *apCloseCCharacters[MAX_CLIENTS];
		int Num = GameServer()->m_World.FindEntities(F->GetPos(), CFlag::ms_PhysSize, (CEntity**)apCloseCCharacters, MAX_CLIENTS, CGameWorld::ENTTYPE_CHARACTER);
		for(int i = 0; i < Num; i++)
		{
			if(!apCloseCCharacters[i]->IsAlive() || apCloseCCharacters[i]->GetPlayer()->GetTeam() == TEAM_SPECTATORS || GameServer()->Collision()->IntersectLine(F->GetPos(), apCloseCCharacters[i]->GetPos(), NULL, NULL))
				continue;

			if(apCloseCCharacters[i]->GetPlayer()->GetTeam() != F->GetTeam())
			{
				/*
				// return the flag
				if(!F->IsAtStand())
				{
					CCharacter *pChr = apCloseCCharacters[i];
					pChr->GetPlayer()->m_Score += 1;

					char aBuf[256];
					str_format(aBuf, sizeof(aBuf), "flag_return player='%d:%s'",
						pChr->GetPlayer()->GetCID(),
						Server()->ClientName(pChr->GetPlayer()->GetCID()));
					GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
					GameServer()->SendGameMsg(GAMEMSG_CTF_RETURN, -1);
					F->Reset();
				}
				*/
			}
			else
			{
				// take the flag
				// if(F->IsAtStand())
				//	m_aTeamscore[fi^1]++;

				F->Grab(apCloseCCharacters[i]);

				// F->GetCarrier()->GetPlayer()->m_Score += 1;

				char aBuf[256];
				str_format(aBuf, sizeof(aBuf), "flag_grab player='%d:%s'",
					F->GetCarrier()->GetPlayer()->GetCID(),
					Server()->ClientName(F->GetCarrier()->GetPlayer()->GetCID()));
				GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);
				GameServer()->SendGameMsg(GAMEMSG_CTF_GRAB, 0, -1);
				break;
			}
		}
	}
}

void CGameControllerSTRIKE::Snap(int SnappingClient)
{
	CGameControllerDOM::Snap(SnappingClient);

	CNetObj_GameDataFlag *pGameDataFlag = static_cast<CNetObj_GameDataFlag *>(Server()->SnapNewItem(NETOBJTYPE_GAMEDATAFLAG, 0, sizeof(CNetObj_GameDataFlag)));
	if(!pGameDataFlag)
		return;

	pGameDataFlag->m_FlagDropTickRed = 0;
	if(m_apFlags[TEAM_RED] && !m_apFlags[TEAM_RED]->IsHidden())
	{
		if(m_apFlags[TEAM_RED]->IsAtStand())
			pGameDataFlag->m_FlagCarrierRed = FLAG_ATSTAND;
		else if(m_apFlags[TEAM_RED]->GetCarrier() && m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer())
		{
			int Spot = -1;
			while ((Spot = GetNextSpot(Spot)) > -1)
			{
				if (m_apDominationSpots[Spot]->IsGettingCaptured() && m_apDominationSpots[Spot]->GetCapTeam() == DOM_RED)
					break;
			}
			if (Spot > -1 && m_apDominationSpots[Spot]->IsGettingCaptured() && m_apDominationSpots[Spot]->GetCapTeam() == DOM_RED)
			{
				pGameDataFlag->m_FlagCarrierRed = FLAG_TAKEN;
				// pGameDataFlag->m_FlagDropTickRed = m_apFlags[TEAM_RED]->GetDropTick();
			}
			else
				pGameDataFlag->m_FlagCarrierRed = m_apFlags[TEAM_RED]->GetCarrier()->GetPlayer()->GetCID();
		}
		else
		{
			pGameDataFlag->m_FlagCarrierRed = FLAG_TAKEN;
			pGameDataFlag->m_FlagDropTickRed = m_apFlags[TEAM_RED]->GetDropTick();
		}
	}
	else
		pGameDataFlag->m_FlagCarrierRed = FLAG_MISSING;

	pGameDataFlag->m_FlagDropTickBlue = 0;
	if(m_apFlags[TEAM_BLUE] && !m_apFlags[TEAM_BLUE]->IsHidden())
	{
		if(m_apFlags[TEAM_BLUE]->IsAtStand())
			pGameDataFlag->m_FlagCarrierBlue = FLAG_ATSTAND;
		else if(m_apFlags[TEAM_BLUE]->GetCarrier() && m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer())
		{
			int Spot = -1;
			while ((Spot = GetNextSpot(Spot)) > -1)
			{
				if (m_apDominationSpots[Spot]->IsGettingCaptured() && m_apDominationSpots[Spot]->GetCapTeam() == DOM_BLUE)
					break;
			}
			if (Spot > -1 && m_apDominationSpots[Spot]->IsGettingCaptured() && m_apDominationSpots[Spot]->GetCapTeam() == DOM_BLUE)
			{
				pGameDataFlag->m_FlagCarrierBlue = FLAG_TAKEN;
				// pGameDataFlag->m_FlagDropTickBlue = m_apFlags[TEAM_BLUE]->GetDropTick();
			}
			else
				pGameDataFlag->m_FlagCarrierBlue = m_apFlags[TEAM_BLUE]->GetCarrier()->GetPlayer()->GetCID();
		}
		else
		{
			pGameDataFlag->m_FlagCarrierBlue = FLAG_TAKEN;
			pGameDataFlag->m_FlagDropTickBlue = m_apFlags[TEAM_BLUE]->GetDropTick();
		}
	}
	else
		pGameDataFlag->m_FlagCarrierBlue = FLAG_MISSING;
}