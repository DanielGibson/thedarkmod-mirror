/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision: 866 $
 * $Date: 2007-03-23 22:25:02 +0100 (Fr, 23 Mar 2007) $
 * $Author: angua $
 *
 ***************************************************************************/
#include "../idlib/precompiled.h"
#pragma hdrstop

static bool init_version = FileVersionList("$Id: PositionWithinRangeFinder.cpp 870 2007-03-27 14:21:59Z greebo $", init_version);

#include "PositionWithinRangeFinder.h"

PositionWithinRangeFinder::PositionWithinRangeFinder(const idAI* self, 
			const idMat3 &gravityAxis, const idVec3 &targetPos, const idVec3 &eyeOffset, float maxDistance) :
	_targetPos(targetPos),
	_eyeOffset(eyeOffset),
	_self(self),
	_gravityAxis(gravityAxis),
	_maxDistance(maxDistance),
	_haveBestGoal(false)
{
	// setup PVS

	

	idBounds bounds( targetPos - idVec3( 16, 16, 0 ), targetPos + idVec3( 16, 16, 64 ) );
	numPVSAreas = gameLocal.pvs.GetPVSAreas( bounds, PVSAreas, idEntity::MAX_PVS_AREAS );
	targetPVS	= gameLocal.pvs.SetupCurrentPVS( PVSAreas, numPVSAreas );
}

PositionWithinRangeFinder::~PositionWithinRangeFinder() 
{
	gameLocal.pvs.FreeCurrentPVS( targetPVS );
}

bool PositionWithinRangeFinder::TestArea( const idAAS *aas, int areaNum )
{
	idVec3 areaCenter = aas->AreaCenter( areaNum );
	areaCenter[ 2 ] += 1.0f;

	// calculate the world transform of the view position
	idVec3 dir = _targetPos - areaCenter;

	idVec3 local_dir;
	_gravityAxis.ProjectVector( dir, local_dir );
	local_dir.z = 0.0f;
	local_dir.ToVec2().Normalize();
	idMat3 axis = local_dir.ToMat3();

	idVec3 fromPos = areaCenter + _eyeOffset * axis;


	float distance = (fromPos - _targetPos).LengthFast();
	if (distance > _maxDistance)
	{
		// Can't use this point, it's too far
		return false;
	}
	else
	{
		// Run trace
		trace_t results;
		if (!gameLocal.clip.TracePoint( results, fromPos, _targetPos, MASK_SOLID, _self ))
		{
			// Remember best result
			if (!_haveBestGoal || distance < bestGoalDistance)
			{
				_haveBestGoal = true;
				bestGoalDistance = distance;
				bestGoal.areaNum = areaNum;
				bestGoal.origin = areaCenter;
			}
			return true;
		}
		else
		{
			return false;
		}
	}
}

bool PositionWithinRangeFinder::GetBestGoalResult(float& out_bestGoalDistance, aasGoal_t& out_bestGoal)
{
	if (_haveBestGoal)
	{
		out_bestGoalDistance = bestGoalDistance;
		out_bestGoal = bestGoal;
		return true;
	}
	else
	{
		return false;
	}
}





