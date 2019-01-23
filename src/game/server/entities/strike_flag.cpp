/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include "character.h"

#include "strike_flag.h"

CStrikeFlag::CStrikeFlag(CGameWorld *pGameWorld, int Team, vec2 StandPos)
: CFlag(pGameWorld, Team, StandPos)
	, m_Hidden(false)
	, m_IsCapturing(false)
{

}

void CStrikeFlag::Reset()
{
	CFlag::Reset();
	m_Hidden = false;
	m_IsCapturing = false;
}

void CStrikeFlag::Snap(int SnappingClient)
{
	if (m_Hidden)
		return;

	if (m_IsCapturing && GetCarrier() && SnappingClient == GetCarrier()->GetPlayer()->GetCID())
		return;

	CFlag::Snap(SnappingClient);
}

void CStrikeFlag::Hide()
{
	m_Hidden = true;
}

void CStrikeFlag::Show()
{
	m_Hidden = false;
}

void CStrikeFlag::SetCapturing(bool IsCapturing)
{
	m_IsCapturing = IsCapturing;
}