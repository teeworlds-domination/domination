/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_STRIKE_H
#define GAME_SERVER_GAMEMODES_STRIKE_H

#include "dom.h"

class CGameControllerSTRIKE : public CGameControllerDOM
{
private:
	bool m_SentPersonalizedBroadcast;
	int  m_WinTick;
	int  m_PurchaseTick;

	class CStrikePickup* m_apStarterPickups[64];
	int  m_NumOfStarterPickups;

private:
	class CStrikeFlag *m_pFlag;

protected:
	int  m_BombPlacedCID;

private:
	int  GetCharacterPrimaryWeaponAmmo(CCharacter *pChr) const;
	void ExplodeBomb();
	void DropAmmo(CCharacter *pChr) const;

private:
	virtual bool CanBeMovedOnBalance(int ClientID) const;

protected:
	virtual void Init() override;

	void UnlockSpot(int Spot, int Team);
	void LockSpot(int Spot, int Team);

	virtual void UpdateScoring() override {};
	virtual void UpdatePickups();
	virtual void UpdateBomb();

	virtual int CalcCaptureStrength(CCharacter *pChr) const override;

	virtual bool SendPersonalizedBroadcast(int ClientID);

	virtual bool WithAdditiveCapStrength() const override { return false; }
	virtual bool WithHandicap() const override { return false; }
	virtual bool WithHardCaptureAbort() const { return true; }
	virtual bool WithNeutralState() const override { return false; }

public:
	CGameControllerSTRIKE(class CGameContext *pGameServer);
	virtual void Tick() override;
	virtual bool CanSpawn(int Team, vec2 *pOutPos) override;

	virtual void DoWincheckMatch();
	virtual void DoWincheckRound();

	void EndRound();

	virtual bool OnEntity(int Index, vec2 Pos) override;
	virtual void OnPlayerDisconnect(class CPlayer *pPlayer) override;
	virtual void OnReset() override;

	virtual void OnStartCapturing(int Spot, int Team) override;
	virtual void OnAbortCapturing(int Spot) override;
	virtual void OnCapture(int Spot, int Team, int NumOfCapCharacters, CCharacter* apCapCharacters[MAX_PLAYERS]) override;
	virtual int  OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int Weapon) override;
	virtual void OnCharacterSpawn(class CCharacter *pChr) override;

	virtual void SendChatInfo(int ClientID) const override;

	virtual void ShowSpawns(int Spot) const override;

public:
	// general
	virtual void Snap(int SnappingClient) override;
};
#endif
