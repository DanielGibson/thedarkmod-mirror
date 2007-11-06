/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision: 866 $
 * $Date: 2007-03-23 22:25:02 +0100 (Fr, 23 Mar 2007) $
 * $Author: angua $
 *
 ***************************************************************************/
#ifndef POSITION_WITHIN_RANGE_FINDER__H
#define POSITION_WITHIN_RANGE_FINDER__H

#include "../idlib/precompiled.h"

class PositionWithinRangeFinder : 
	public idAASCallback
{
private:
	idVec3 _targetPos;
	idVec3 _eyeOffset;
	const idAI* _self;
	idMat3 _gravityAxis;
	float _maxDistance;
	bool _haveBestGoal;

	
	idBounds			excludeBounds;
	pvsHandle_t			targetPVS;
	int					PVSAreas[ idEntity::MAX_PVS_AREAS ];
	int	numPVSAreas;

	aasGoal_t		bestGoal; 
	float bestGoalDistance;

	
public:
	PositionWithinRangeFinder(const idAI *self, const idMat3 &gravityAxis, 
			const idVec3 &targetPos, const idVec3 &eyeOffset, float maxDistance);

	~PositionWithinRangeFinder();

	bool TestArea( const idAAS *aas, int areaNum );

	bool GetBestGoalResult(float& out_bestGoalDistance, aasGoal_t& out_bestGoal);
};

#endif /* POSITION_WITHIN_RANGE_FINDER__H */
