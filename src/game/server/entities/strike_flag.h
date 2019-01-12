/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_ENTITIES_STRIKE_FLAG_H
#define GAME_SERVER_ENTITIES_STRIKE_FLAG_H

#include "flag.h"

class CStrikeFlag : public CFlag
{
private:
	bool m_Hidden;

protected:
	virtual bool CanBeResetAfterDrop() const override { return false; }

public:
	CStrikeFlag(CGameWorld *pGameWorld, int Team, vec2 StandPos);

	virtual void Reset() override;
	virtual void Snap(int SnappingClient) override;

	void Hide();
	void Show();
	bool IsHidden() const { return m_Hidden; }
};

#endif
