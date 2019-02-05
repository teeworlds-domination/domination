#ifndef GAME_SERVER_GAMEMODES_KDOM_H
#define GAME_SERVER_GAMEMODES_KDOM_H
#include "dom.h"

class CGameControllerKDOM : public CGameControllerDOM
{

public:
	CGameControllerKDOM(class CGameContext *pGameServer);

	virtual void UpdateScoring() override {};

	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	virtual void OnCapture(int SpotNumber, int Team, int NumOfCapCharacters, CCharacter* apCapCharacters[MAX_PLAYERS]) override;

	virtual void SendChatInfo(int ClientID) const override;
};
#endif
