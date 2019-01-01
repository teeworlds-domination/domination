/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_KDOM_H
#define GAME_SERVER_GAMEMODES_KDOM_H
#include "dom.h"

class CGameControllerKDOM : public CGameControllerDOM
{

public:
	CGameControllerKDOM(class CGameContext *pGameServer);

	virtual void UpdateScoring() override {};

	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	virtual void OnCapture(int SpotNumber, int Team) override;

	virtual void SendChatInfo(int ClientID) override;
};
#endif
