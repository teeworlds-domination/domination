/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_CSDOM_PICKUP_H
#define GAME_SERVER_ENTITIES_CSDOM_PICKUP_H

#include "pickup.h"

enum
{
	NO_RESPAWN = -2,
	PICKUP_AMMO = 99
};

class CCSDOMPickup : public CPickup
{
protected:
	bool m_IsWeapon;
	vec2 m_SnapPos;
	bool m_Respawn;
	bool m_IsTemporary;

	int  m_Ammo;
	int  m_DespawnTick;

private:
	CCSDOMPickup(CGameWorld *pGameWorld, int Type, vec2 Pos, bool Temporary, int Ammo, int DespawnTick);

	bool GiveCharacterWeapon(CCharacter *pChr, int Weapon, int Ammo);
	bool GiveCharacterAmmo(CCharacter *pChr) const;
	int  GetMaxAmmo(int Weapon) const;
	int  GetCharacterPrimaryWeapon(CCharacter *pChr) const;

public:
	CCSDOMPickup(CGameWorld *pGameWorld, int Type, vec2 Pos, bool Temporary);

	virtual void Reset() override;
	virtual void Tick() override;
	virtual void TickPaused() override;
	virtual void Snap(int SnappingClient) override;

	virtual void Despawn();
};

#endif
