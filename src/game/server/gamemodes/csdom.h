#ifndef GAME_SERVER_GAMEMODES_CSDOM_H
#define GAME_SERVER_GAMEMODES_CSDOM_H

#include "dom.h"

class CGameControllerCSDOM : public CGameControllerDOM
{
private:
	int  m_BombPlacedCID;
	int  m_PurchaseTick;
	int  m_WinTick;

private:
	class CCSDOMFlag *m_apFlags[2];

private:
	int  GetCharacterPrimaryWeaponAmmo(CCharacter *pChr) const;
	void ExplodeBomb();
	void DropAmmo(CCharacter *pChr) const;

private:
	virtual bool CanBeMovedOnBalance(int ClientID) const;

protected:
	virtual void Init() override;
	virtual void UpdateScoring() override {};
	virtual void UpdatePickups();
	virtual void UpdateBomb();

	virtual int CalcCaptureStrength(int Spot, CCharacter *pChr, bool IsFirst) const override;

	virtual bool SendPersonalizedBroadcast(int ClientID);

	virtual bool WithAdditiveCapStrength() const override { return false; }
	virtual bool WithHandicap() const override { return false; }
	virtual bool WithHardCaptureAbort() const { return true; }
	virtual bool WithNeutralState() const override { return false; }

public:
	CGameControllerCSDOM(class CGameContext *pGameServer);
	virtual void Tick() override;

	virtual bool DoWincheckMatch() override;
	virtual void DoWincheckRound() override;

	void EndRound();

	virtual bool OnEntity(int Index, vec2 Pos) override;
	virtual void OnInit() override;
	virtual void OnPlayerDisconnect(class CPlayer *pPlayer) override;

	virtual void OnStartCapturing(int Spot, int Team) override;
	virtual void OnAbortCapturing(int Spot) override;
	virtual void OnCapture(int Spot, int Team, int NumOfCapCharacters, CCharacter* apCapCharacters[MAX_PLAYERS]) override;
	virtual int  OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	virtual void OnCharacterSpawn(class CCharacter *pChr) override;

	virtual void SendChatInfo(int ClientID) const override;

public:
	// general
	virtual void Snap(int SnappingClient) override;
};
#endif
