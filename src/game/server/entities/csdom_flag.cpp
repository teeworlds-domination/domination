/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include <game/server/gamecontext.h>
#include <game/server/player.h>

#include "character.h"

#include "csdom_flag.h"

CCSDOMFlag::CCSDOMFlag(CGameWorld *pGameWorld, int Team, vec2 StandPos)
: CFlag(pGameWorld, Team, StandPos)
	, m_Hidden(false)
	, m_IsCapturing(false)
{

}

void CCSDOMFlag::Reset()
{
	CFlag::Reset();
	m_Hidden = false;
	m_IsCapturing = false;
}

void CCSDOMFlag::Snap(int SnappingClient)
{
	if (m_Hidden)
		return;

	if (m_IsCapturing && GetCarrier() && SnappingClient == GetCarrier()->GetPlayer()->GetCID())
		return;

	CFlag::Snap(SnappingClient);
}

void CCSDOMFlag::Hide()
{
	m_Hidden = true;
}

void CCSDOMFlag::Show()
{
	m_Hidden = false;
}

void CCSDOMFlag::SetCapturing(bool IsCapturing)
{
	m_IsCapturing = IsCapturing;
}