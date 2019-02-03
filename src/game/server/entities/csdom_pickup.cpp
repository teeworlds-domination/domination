/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <generated/server_data.h>

#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include "character.h"
#include "csdom_pickup.h"

CCSDOMPickup::CCSDOMPickup(CGameWorld *pGameWorld, int Type, vec2 Pos, bool Temporary, int Ammo, int DespawnTick)
: CPickup(pGameWorld, Type, Pos)
		, m_SnapPos(Pos)
		, m_IsTemporary(Temporary)
		, m_Ammo(Ammo)
		, m_DespawnTick(DespawnTick)
{
	m_IsWeapon = (Type == PICKUP_SHOTGUN || Type == PICKUP_GRENADE || Type == PICKUP_LASER);

	if (m_IsTemporary)
		m_SpawnTick = max(Server()->Tick() + Server()->TickSpeed(), m_SpawnTick);
}

CCSDOMPickup::CCSDOMPickup(CGameWorld *pGameWorld, int Type, vec2 Pos, bool Temporary)
: CPickup(pGameWorld, Type, Pos)
		, m_SnapPos(Pos)
		, m_IsTemporary(Temporary)
		, m_DespawnTick(-1)
{
	m_IsWeapon = (Type == PICKUP_SHOTGUN || Type == PICKUP_GRENADE || Type == PICKUP_LASER);

	if (m_IsTemporary)
		m_SpawnTick = max(Server()->Tick() + Server()->TickSpeed(), m_SpawnTick);

	if (m_IsWeapon)
		m_Ammo = GetMaxAmmo(Type == PICKUP_SHOTGUN? WEAPON_SHOTGUN
							: Type == PICKUP_GRENADE? WEAPON_GRENADE : WEAPON_LASER);
}

void CCSDOMPickup::Reset()
{
	CPickup::Reset();

	if (m_IsTemporary)
	{
		GameWorld()->RemoveEntity(this);
		return;
	}

	if (m_IsWeapon)
		m_DespawnTick = Server()->Tick() + Server()->TickSpeed() * g_Config.m_SvCsdomBuyTimelimit;
}

void CCSDOMPickup::Tick()
{
	// wait for respawn
	if(m_SpawnTick > 0)
	{
		if(Server()->Tick() > m_SpawnTick)
		{
			// respawn
			m_SpawnTick = -1;

			// if(m_Type == PICKUP_GRENADE || m_Type == PICKUP_SHOTGUN || m_Type == PICKUP_LASER)
			// 	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN);
		}
		else
			return;
	}

	if (m_SpawnTick == NO_RESPAWN)
		return;

	if (m_DespawnTick > 0 && Server()->Tick() > m_DespawnTick)
	{
		Despawn();
		return;
	}

	// Check if a player intersected us
	CCharacter *pChr = (CCharacter *)GameServer()->m_World.ClosestEntity(m_Pos, 20.0f, CGameWorld::ENTTYPE_CHARACTER, 0);
	if(pChr && pChr->IsAlive())
	{
		// player picked us up, is someone was hooking us, let them go
		bool Picked = false;
		switch (m_Type)
		{
			case PICKUP_HEALTH:
				if(pChr->IncreaseHealth(1))
				{
					Picked = true;
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_HEALTH);
				}
				break;

			case PICKUP_ARMOR:
				if(pChr->IncreaseArmor(1))
				{
					Picked = true;
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_ARMOR);
				}
				break;

			case PICKUP_GRENADE:
				if(GiveCharacterWeapon(pChr, WEAPON_GRENADE, m_Ammo))
				{
					Picked = true;
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_GRENADE);
					if(pChr->GetPlayer())
						GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), WEAPON_GRENADE);
				}
				break;
			case PICKUP_SHOTGUN:
				if(GiveCharacterWeapon(pChr, WEAPON_SHOTGUN, m_Ammo))
				{
					Picked = true;
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);
					if(pChr->GetPlayer())
						GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), WEAPON_SHOTGUN);
				}
				break;
			case PICKUP_LASER:
				if(GiveCharacterWeapon(pChr, WEAPON_LASER, m_Ammo))
				{
					Picked = true;
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);
					if(pChr->GetPlayer())
						GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), WEAPON_LASER);
				}
				break;

			case PICKUP_NINJA:
				{
					Picked = true;
					// activate ninja on target player
					pChr->GiveNinja();

					// loop through all players, setting their emotes
					CCharacter *pC = static_cast<CCharacter *>(GameServer()->m_World.FindFirst(CGameWorld::ENTTYPE_CHARACTER));
					for(; pC; pC = (CCharacter *)pC->TypeNext())
					{
						if (pC != pChr)
							pC->SetEmote(EMOTE_SURPRISE, Server()->Tick() + Server()->TickSpeed());
					}

					pChr->SetEmote(EMOTE_ANGRY, Server()->Tick() + 1200 * Server()->TickSpeed() / 1000);
					break;
				}
			case PICKUP_AMMO:
				if(GiveCharacterAmmo(pChr))
				{
					Picked = true;
					GameServer()->CreateSound(m_Pos, SOUND_PICKUP_SHOTGUN);
					// if(pChr->GetPlayer())
					//	GameServer()->SendWeaponPickup(pChr->GetPlayer()->GetCID(), WEAPON_LASER);
				}
				break;

			default:
				break;
		};

		if(Picked)
		{
			char aBuf[256];
			str_format(aBuf, sizeof(aBuf), "pickup player='%d:%s' item=%d",
				pChr->GetPlayer()->GetCID(), Server()->ClientName(pChr->GetPlayer()->GetCID()), m_Type);
			GameServer()->Console()->Print(IConsole::OUTPUT_LEVEL_DEBUG, "game", aBuf);

			if (m_Type == PICKUP_NINJA)
			{
				int RespawnTime = g_pData->m_aPickups[m_Type].m_Respawntime;
				if(RespawnTime >= 0)
					m_SpawnTick = Server()->Tick() + Server()->TickSpeed() * RespawnTime;
			}
			else
				m_SpawnTick = m_IsWeapon && g_Config.m_SvCsdomWeaponRespawn? Server()->Tick() + Server()->TickSpeed()/3 : NO_RESPAWN;
		}
	}
}

void CCSDOMPickup::TickPaused()
{
	if(m_DespawnTick != -1)
		++m_DespawnTick;
}

void CCSDOMPickup::Snap(int SnappingClient)
{
	if(m_SpawnTick == NO_RESPAWN)
		return;

	if (m_Type != PICKUP_AMMO)
	{
		CPickup::Snap(SnappingClient);
		return;
	}

	CNetObj_Projectile *pProj = static_cast<CNetObj_Projectile *>(Server()->SnapNewItem(NETOBJTYPE_PROJECTILE, GetID(), sizeof(CNetObj_Projectile)));
	if(pProj)
	{
		static float s_Time = 0.0f;
		static float s_LastLocalTime = Server()->Tick();

		s_Time += (Server()->Tick()-s_LastLocalTime)/Server()->TickSpeed();

		float Offset = m_SnapPos.y/32.0f + m_SnapPos.x/32.0f;
		m_SnapPos.x = m_Pos.x + cosf(s_Time*2.0f+Offset)*2.5f;
		m_SnapPos.y = m_Pos.y + sinf(s_Time*2.0f+Offset)*2.5f;
		s_LastLocalTime = Server()->Tick();

		pProj->m_X = m_SnapPos.x;
		pProj->m_Y = m_SnapPos.y;

		pProj->m_VelX = 0;
		pProj->m_VelY = 0;
		pProj->m_StartTick = 0;
		pProj->m_Type = WEAPON_LASER;
	}
}

void CCSDOMPickup::Despawn()
{
	m_DespawnTick = -1;
	m_SpawnTick = NO_RESPAWN;
	GameServer()->CreateSound(m_Pos, SOUND_WEAPON_SPAWN);
}

bool CCSDOMPickup::GiveCharacterWeapon(CCharacter *pChr, int Weapon, int Ammo)
{
	int OldWeapon = GetCharacterPrimaryWeapon(pChr);
	int OldWeaponAmmo = OldWeapon != -1? pChr->m_aWeapons[OldWeapon].m_Ammo : 0;
	bool GaveWeapon = false;

	if (!pChr->m_aWeapons[Weapon].m_Got)
	{
		// change weapon
		if (OldWeapon != -1)
			pChr->m_aWeapons[OldWeapon].m_Got = false;

		if (pChr->GiveWeapon(Weapon, Ammo))
		{
			pChr->m_ActiveWeapon = Weapon;
			GaveWeapon = true;
		}
	}
	else if (pChr->m_aWeapons[Weapon].m_Ammo < Ammo)
	{
		// refill
		GaveWeapon = pChr->GiveWeapon(Weapon, Ammo);
	}

	if (GaveWeapon)
	{
		if (OldWeapon != -1 && !g_Config.m_SvCsdomWeaponRespawn)
		{
			// drop weapon
			int PickupType;
			switch (OldWeapon)
			{
				case WEAPON_SHOTGUN: PickupType = PICKUP_SHOTGUN; break;
				case WEAPON_GRENADE: PickupType = PICKUP_GRENADE; break;
				default:             PickupType = PICKUP_LASER; break;
			}
			new CCSDOMPickup(GameWorld(), PickupType, m_Pos, true, OldWeaponAmmo, m_DespawnTick);
		}
	}
	return GaveWeapon;
}

bool CCSDOMPickup::GiveCharacterAmmo(CCharacter *pChr) const
{
	int PrimaryWeapon = GetCharacterPrimaryWeapon(pChr);
	if (PrimaryWeapon != -1 && pChr->m_aWeapons[PrimaryWeapon].m_Ammo < GetMaxAmmo(PrimaryWeapon))
		return pChr->GiveWeapon(PrimaryWeapon, pChr->m_aWeapons[PrimaryWeapon].m_Ammo + 1);
	return false;
}

int CCSDOMPickup::GetCharacterPrimaryWeapon(CCharacter *pChr) const
{
	if (pChr->m_aWeapons[WEAPON_SHOTGUN].m_Got)
		return WEAPON_SHOTGUN;
	else if (pChr->m_aWeapons[WEAPON_GRENADE].m_Got)
		return WEAPON_GRENADE;
	else if (pChr->m_aWeapons[WEAPON_LASER].m_Got)
		return WEAPON_LASER;
	else
		return -1;
}

int CCSDOMPickup::GetMaxAmmo(int Weapon) const
{
	switch (Weapon)
	{
	case WEAPON_SHOTGUN: return 10;
	case WEAPON_GRENADE: return 7;
	case WEAPON_LASER:   return 5;
	default:             return 10; // should not occur
	}
}