/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "strike_flag.h"

CStrikeFlag::CStrikeFlag(CGameWorld *pGameWorld, int Team, vec2 StandPos)
: CFlag(pGameWorld, Team, StandPos)
{
	m_Hidden = false;
}

void CStrikeFlag::Hide()
{
	m_Hidden = true;
}

void CStrikeFlag::Show()
{
	m_Hidden = false;
}

void CStrikeFlag::Reset()
{
	CFlag::Reset();
	m_Hidden = false;
}

void CStrikeFlag::Snap(int SnappingClient)
{
	if (m_Hidden)
		return;

	CFlag::Snap(SnappingClient);
}
