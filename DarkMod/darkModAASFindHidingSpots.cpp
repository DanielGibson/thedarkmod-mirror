#include "../idlib/precompiled.h"
#pragma hdrstop

#include ".\darkmodaasfindhidingspots.h"
#include "..\darkmod\darkmodglobals.h"
#include "..\darkmod\darkModLAS.h"
#include "..\sys\sys_public.h"

// What amount of light is acceptable for a minimal quality hiding spot
#define HIDING_SPOT_MAX_LIGHT_QUOTIENT 0.10f

// Quality of a hiding spot ranges from 0.0 (HIDING_SPOT_MAX_LIGHT_QUOTIENT) to 1.0 (pitch black)
#define OCCLUSION_HIDING_SPOT_QUALITY 0.5

// The distance at which hiding spots will be combined if they have the same "type" properties
#define HIDING_SPOT_COMBINATION_DISTANCE 50.0f

// Static member for debugging hiding spot results
idList<darkModHidingSpot_t> darkModAASFindHidingSpots::DebugDrawList;


//----------------------------------------------------------------------------

// Constructor
darkModAASFindHidingSpots::darkModAASFindHidingSpots(const idVec3 &hideFromPos, idAAS* in_p_aas, idScriptBool* in_p_bool_threadDone)
{
	/*
	* Note that most of this code is similar to idAASFindCover for now
	*/

	int numPVSAreas = 0;

	// Start empty
	h_hidePVS.i = -1;
	h_hidePVS.h = NULL;

	// Remember the hide form position
	hideFromPosition = hideFromPos;

	// Remember variable to trigger when thread done running (in lieu of real sys level EVENT objects, use an atomic type)
	p_bool_threadDone = in_p_bool_threadDone;

	// Thread parameters not initialized
	p_inout_hidingSpots = NULL;
	p_aas = NULL;
	p_ignoreEntity = NULL;

	// No thread started yet
	searchThread.name = "";
	searchThread.threadHandle = 0;
	searchThread.threadId = 0;

	// Have the PVS system identify locations containing the hide from position
	PVSAreas[0] = gameLocal.pvs.GetPVSArea(hideFromPosition);
	numPVSAreas = 1;
	DM_LOG(LC_AI, LT_DEBUG).LogString("PVS Setup found %d areas", numPVSAreas);

	// Setup our local copy of the pvs node graph
	h_hidePVS = gameLocal.pvs.SetupCurrentPVS
	(
		PVSAreas, 
		numPVSAreas 
	);

}

//----------------------------------------------------------------------------

// Destructor
darkModAASFindHidingSpots::~darkModAASFindHidingSpots(void)
{
	// If there is a thread running, destroy it
	if (searchThread.threadHandle != 0)
	{
		// Threading support not in SDK
		//Sys_DestroyThread (searchThread);
	}

	// Be certain we free our PVS node graph
	if ((h_hidePVS.h != NULL) || (h_hidePVS.i != -1))
	{
		gameLocal.pvs.FreeCurrentPVS( h_hidePVS );
		h_hidePVS.h = NULL;
		h_hidePVS.i = -1;
	}
}

//----------------------------------------------------------------------------

// The search thread function
unsigned int darkModAASFindHidingSpots::FindHidingSpots_BackgroundThreadFunc
(
	void* p_voidThis
) 
{
	// Holds the center of the AAS area
	idVec3	areaCenter;

	// Center of the search bounds
	idVec3 searchCenter;

	// Radius of the search bounds
	float searchRadius;

	// The number of PVS areas held in local testing
	int		numPVSAreas;

	// An array for testing PVS areas
	int		PVSAreas[ idEntity::MAX_PVS_AREAS ];

	// Cast pointer to instance on which this thread is running
	darkModAASFindHidingSpots* p_this = (darkModAASFindHidingSpots*) p_voidThis;
	if (p_this == NULL)
	{
		return 55;
	}

	// Ensure the PVS to AAS table is initialized
	// If already initialized, this returns right away.
	LAS.pvsToAASMappingTable.buildMappings(0);

	// Compute center of bounds and radius of the bounds
	searchCenter = p_this->searchLimits.GetCenter();
	searchRadius = p_this->searchLimits.GetRadius();

	// Get the PVS areas intersecting the search bounds
	// Note, the id code below did this by expanding a bound out from the area center, regardless
	// of the size of the area.  This uses our function-local PVSArea array to
	// hold the intersecting PVS Areas.
	numPVSAreas = gameLocal.pvs.GetPVSAreas
	(
		p_this->searchLimits, 
		PVSAreas, 
		idEntity::MAX_PVS_AREAS 
	);

	

	// Iterate the PVS areas
	DM_LOG(LC_AI, LT_DEBUG).LogString("Iterating PVS areas, there are %d in the set\n", numPVSAreas);

	for (int pvsResultIndex = 0; pvsResultIndex < numPVSAreas; pvsResultIndex ++)
	{
		
		// Our current PVS given by h_hidePVS holds the list of areas visible from
		// the "hide from" point.
		// If the area is not in our h_hidePVS set, then it cannot be seen, and it is 
		// thus considered hidden.
		if ( !gameLocal.pvs.InCurrentPVS( p_this->h_hidePVS, PVSAreas[pvsResultIndex]) )
		{
			// Only put these in here if PVS based hiding spots are allowed
			if ((p_this->hidingSpotTypesAllowed & PVS_AREA_HIDING_SPOT_TYPE) != 0)
			{
				// Add a goal for the center of each area within this pvs area
				idList<int> aasAreaIndices;
				LAS.pvsToAASMappingTable.getAASAreasForPVSArea (PVSAreas[pvsResultIndex], aasAreaIndices);

				DM_LOG(LC_AI, LT_DEBUG).LogString("Non-visible PVS area %d contains %d AAS areas\n", PVSAreas[pvsResultIndex], aasAreaIndices.Num());

				for (int ia = 0; ia < aasAreaIndices.Num(); ia ++)
				{
					int aasAreaIndex = aasAreaIndices[ia];

					// This whole area is not visible
					// Add its center and we are done
					darkModHidingSpot_t hidingSpot;
					hidingSpot.goal.areaNum = aasAreaIndex;
					hidingSpot.goal.origin = p_this->p_aas->AreaCenter(aasAreaIndex);
					hidingSpot.hidingSpotTypes = PVS_AREA_HIDING_SPOT_TYPE;

					// Since there is total occlusion, base quality on the distance from the center
					// of the search compared to the total search radius
					float distanceFromCenter = (searchCenter - hidingSpot.goal.origin).Length();
					if (searchRadius > 0.0)
					{
						hidingSpot.quality = (searchRadius - distanceFromCenter) / searchRadius;
						if (hidingSpot.quality < 0.0)
						{
							hidingSpot.quality = (float) 0.0;
						}
					}
					else
					{
						hidingSpot.quality = (float) 0.1;
					}

					// Insert if it is any good
					if (hidingSpot.quality > 0.0)
					{
						p_this->insertHidingSpotWithQualitySorting (hidingSpot, *(p_this->p_inout_hidingSpots));
					}
	
					DM_LOG(LC_AI, LT_DEBUG).LogString("Hiding spot added for PVS non-visible area %d, AAS area %d, quality \n", PVSAreas[pvsResultIndex], hidingSpot.goal.areaNum);
				}

			}
		}
		else
		{

			// The area is visible through the PVS system.

			// Test each AAS area within the pvs area
			idList<int> aasAreaIndices;
			LAS.pvsToAASMappingTable.getAASAreasForPVSArea (PVSAreas[pvsResultIndex], aasAreaIndices);

			DM_LOG(LC_AI, LT_DEBUG).LogString("Visible PVS area %d contains %d AAS areas\n", PVSAreas[pvsResultIndex], aasAreaIndices.Num());

			for (int ia = 0; ia < aasAreaIndices.Num(); ia ++)
			{
				// Get AAS area index for this AAS area
				int aasAreaIndex = aasAreaIndices[ia];

				// Check area flags
				int areaFlags = p_this->p_aas->AreaFlags (aasAreaIndex);

				if ((areaFlags & AREA_FLOOR) != 0)
				{
					// This area is traversable by the hiding entity
					// Test for other reasons for hidability such as lighting, visual occlusion etc...
					p_this->FindHidingSpotsInVisibleAASArea 
					(
						*(p_this->p_inout_hidingSpots),
						p_this->hidingHeight,
						p_this->p_aas, 
						aasAreaIndex,
						p_this->searchLimits,
						p_this->hidingSpotTypesAllowed,
						p_this->p_ignoreEntity
					);
				}
			}

		} // PVS area is not fully occluded, search within it for local hiding regions

	} // Consider next PVS area within our search limits

	// Combine redundant hiding spots
	p_this->CombineRedundantHidingSpots ( *(p_this->p_inout_hidingSpots), HIDING_SPOT_COMBINATION_DISTANCE);

	// Already sorted during list insertion

	// Trigger thread exited flag
	if (p_this->p_bool_threadDone != NULL)
	{
		*(p_this->p_bool_threadDone) = TRUE;
	}

	// Done: no error
	return 0;
}

//----------------------------------------------------------------------------

// Internal helper
void darkModAASFindHidingSpots::FindHidingSpotsInVisibleAASArea 
(
	idList<darkModHidingSpot_t>& inout_hidingSpots,
	float hidingHeight,
	const idAAS* aas, 
	int AASAreaNum, 
	idBounds searchLimits,
	int hidingSpotTypesAllowed, 
	idEntity* p_ignoreEntity
)
{
	// The area is visible from the hide-from point. So, we have to do a more complicated
	// search for points of darkness or visual occlusion due to objstacles etc...

	idVec3 areaCenter = aas->AreaCenter (AASAreaNum);
	DM_LOG(LC_AI, LT_DEBUG).LogString("Center of AAS area %d is %f, %f, %f", AASAreaNum, areaCenter.x, areaCenter.y, areaCenter.z);

	// Get the area bounds
	idBounds areaBounds = aas->GetAreaBounds (AASAreaNum);

	// Get search area properties
	idVec3 searchCenter = searchLimits.GetCenter();
	float searchRadius = searchLimits.GetRadius();

	// Hiding search bounds is intersection of search bounds and area bounds
	idBounds hidingBounds = searchLimits.Intersect (areaBounds);

	// Iterate a gridding within these bounds
	float hideSearchGridSpacing = 40.0f;
	
	idVec3 boundMins = hidingBounds[0];
	idVec3 boundMaxes = hidingBounds[1];


	DM_LOG(LC_AI, LT_DEBUG).LogString("Iterating hide gridding for AAS area %d,  X:%f to %f, Y:%f to %f, Z:%f to %f \n", AASAreaNum, boundMins.x, boundMaxes.x, boundMins.y, boundMaxes.y, boundMins.z, boundMaxes.z);

	// Iterate the coordinates to search
	// We don't use for loops here so that we can control the end of the iteration
	// to check up against the boundary regardless of divisibility

	// Iterate X grid
	float XPoint = boundMins.x;
	while (XPoint <= boundMaxes.x)
	{
		float YPoint = boundMins.y;
		while (YPoint <= boundMaxes.y)
		{
			// For now, only consider top of floor
			float ZPoint = boundMaxes.z + 1.0;

			// Test the point
			idVec3 testPoint (XPoint, YPoint, ZPoint);

			darkModHidingSpot_t hidingSpot;
			hidingSpot.hidingSpotTypes = TestHidingPoint 
			(
				testPoint, 
				searchCenter,
				searchRadius,
				hidingHeight,
				hidingSpotTypesAllowed,
				p_ignoreEntity,
				hidingSpot.quality
			);

			// If there are any hiding qualities, insert a hiding spot
			if 
			(
				(hidingSpot.hidingSpotTypes != NONE_HIDING_SPOT_TYPE) && 
				(hidingSpot.quality > 0.0)
			)
			{
				// Insert a hiding spot for this test point
				hidingSpot.goal.areaNum = AASAreaNum;
				hidingSpot.goal.origin = testPoint;
				insertHidingSpotWithQualitySorting (hidingSpot, inout_hidingSpots);
				DM_LOG(LC_AI, LT_DEBUG).LogString("Found hiding spot within AAS area %d at (X:%f, Y:%f, Z:%f) with type bitflags %d, quality %f\n", AASAreaNum, testPoint.x, testPoint.y, testPoint.z, hidingSpot.hidingSpotTypes, hidingSpot.quality);
			}

			// Increase search coordinate. Ensure we search along bounds, which might be a
			// wall or other cover providing surface.
			if ((YPoint < boundMaxes.y) && (YPoint + hideSearchGridSpacing) > (boundMaxes.y))
			{
				YPoint = boundMaxes.y;
			}
			else
			{
				YPoint += hideSearchGridSpacing;
			}

		} // Y iteration

		// Increase search coordinate. Ensure we search along bounds, which might be a
		// wall or other cover providing surface.
		if ((XPoint < boundMaxes.x) && (XPoint + hideSearchGridSpacing) > (boundMaxes.x))
		{
			XPoint = boundMaxes.x;
		}
		else
		{
			XPoint += hideSearchGridSpacing;
		}
		

	} // X iteration

	DM_LOG(LC_AI, LT_DEBUG).LogString("Finished hide grid iteration for AAS area %d\n", AASAreaNum);

	// Done
}

//----------------------------------------------------------------------------

// Internal helper
int darkModAASFindHidingSpots::TestHidingPoint 
(
	idVec3 testPoint, 
	idVec3 searchCenter,
	float searchRadius,
	float hidingHeight,
	int hidingSpotTypesAllowed, 
	idEntity* p_ignoreEntity,
	float& out_quality
)
{
	int out_hidingSpotTypesThatApply = NONE_HIDING_SPOT_TYPE;
	out_quality = 0.0f; // none found yet

	// Is it dark?
	if ((hidingSpotTypesAllowed & DARKNESS_HIDING_SPOT_TYPE) != 0)
	{
		// Test the lighting level of this position
		//DM_LOG(LC_AI, LT_DEBUG).LogString("Testing hiding-spot lighting at point %f,%f,%f\n", testPoint.x, testPoint.y, testPoint.z);
		idVec3 testLineTop = testPoint;
		testLineTop.z += hidingHeight;
		
		float LightQuotient = LAS.queryLightingAlongLine 
		(
			testPoint,
			testLineTop,
			p_ignoreEntity,
			true
		);

		//DM_LOG(LC_AI, LT_DEBUG).LogString("Done testing hiding-spot lighting at point %f,%f,%f\n", testPoint.x, testPoint.y, testPoint.z);
		if (LightQuotient < HIDING_SPOT_MAX_LIGHT_QUOTIENT)
		{
			//DM_LOG(LC_AI, LT_DEBUG).LogString("Found hidable darkness of %f at point %f,%f,%f\n", LightQuotient, testPoint.x, testPoint.y, testPoint.z);
			out_hidingSpotTypesThatApply |= DARKNESS_HIDING_SPOT_TYPE;

			float darknessQuality = 0.0;
			darknessQuality = (HIDING_SPOT_MAX_LIGHT_QUOTIENT - LightQuotient) / HIDING_SPOT_MAX_LIGHT_QUOTIENT;
			if (darknessQuality > out_quality)
			{
				out_quality = darknessQuality;
			}

		}
	}

	// Does a ray to the test point from the hide from point get occluded?
	if ((hidingSpotTypesAllowed & VISUAL_OCCLUSION_HIDING_SPOT_TYPE) != 0)
	{
		//idVec3 fakePoint = hideFromPosition;
		//fakePoint.z -= 5.0f;

		// Check a point above the test point to account for the size
		// of a hiding object. Generally, we use the "top" of the hiding
		// object size, because AI's don't expect something to hang
		// from the back of the occluder and pull its feet upward.
		idVec3 occlusionTestPoint = testPoint;
		occlusionTestPoint.z += hidingHeight;


		trace_t rayResult;
		//DM_LOG(LC_AI, LT_DEBUG).LogString("Testing hiding-spot occlusion at point %f,%f,%f\n", testPoint.x, testPoint.y, testPoint.z);
		if (gameLocal.clip.TracePoint 
		(
			rayResult, 
			hideFromPosition,
			testPoint,
			//MASK_SOLID | MASK_WATER | MASK_OPAQUE,
			MASK_SOLID,
			NULL
		))
		{
			// Some sort of occlusion
			//DM_LOG(LC_AI, LT_DEBUG).LogString("Found hiding-spot occlusion at point %f,%f,%f, fraction of %f\n", testPoint.x, testPoint.y, testPoint.z, rayResult.fraction);
			out_hidingSpotTypesThatApply |= VISUAL_OCCLUSION_HIDING_SPOT_TYPE;

			// Occlusions are 50% good
			if (out_quality < OCCLUSION_HIDING_SPOT_QUALITY)
			{
				out_quality = OCCLUSION_HIDING_SPOT_QUALITY;
			}
		}
		//DM_LOG(LC_AI, LT_DEBUG).LogString("Done testing hiding-spot occlusion at point %f,%f,%f\n", testPoint.x, testPoint.y, testPoint.z);
	}

	// Reduce quality by distance from search center
	float distanceFromCenter = (searchCenter - testPoint).Length();
	if ((searchRadius > 0.0) && (out_quality > 0.0))
	{
		out_quality = out_quality * ((searchRadius - distanceFromCenter) / searchRadius);
		if (out_quality < 0.0)
		{
			out_quality = 0.0;
		}
	}
	else
	{
		out_quality = 0.0;
	}


	// Done
	//DM_LOG(LC_AI, LT_DEBUG).LogString("Done testing for hidability at point %f,%f,%f\n", testPoint.x, testPoint.y, testPoint.z);
	return out_hidingSpotTypesThatApply;
}

//----------------------------------------------------------------------------

void darkModAASFindHidingSpots::insertHidingSpotWithQualitySorting
(
	darkModHidingSpot_t& hidingSpot,
	idList<darkModHidingSpot_t>& inout_hidingSpots
)
{
	// Find the right place
	int numSpots = inout_hidingSpots.Num();

	int spotIndex = 0;
	for (spotIndex = 0; spotIndex < numSpots; spotIndex ++)
	{
		if (inout_hidingSpots[spotIndex].quality <= hidingSpot.quality)
		{
			// Insert it before this spot (at this spot and move all rest down)
			break;
		}
	}

	// Do insertion
	inout_hidingSpots.Insert (hidingSpot, spotIndex);
	
}

//----------------------------------------------------------------------------

void darkModAASFindHidingSpots::CombineRedundantHidingSpots
(
	idList<darkModHidingSpot_t>& inout_hidingSpots,
	float distanceAtWhichToCombine
)
{
	//idList<darkModHidingSpot_t> consolidatedList;

	int listLength = inout_hidingSpots.Num();

	for (int index = 0; index < listLength; index ++)
	{
		// Get the hiding spot
		darkModHidingSpot_t spot = inout_hidingSpots[index];

		// compare with other hiding spots later in the list
		for (int otherIndex = index+1; otherIndex < listLength; otherIndex ++)
		{
			darkModHidingSpot_t otherSpot = inout_hidingSpots[otherIndex];
			float distance = abs((spot.goal.origin - otherSpot.goal.origin).Length());
			if ((spot.hidingSpotTypes == otherSpot.hidingSpotTypes) && (distance < distanceAtWhichToCombine))
			{
				// Remove the other spot 
				inout_hidingSpots.RemoveIndex(otherIndex);
				listLength --;

				// A point may have been pulled down into this other index
				otherIndex --;
			}
		}

	}
	

}

//----------------------------------------------------------------------------

/*
void pivotOp
(
	int pivotIndex,
	idList<darkModHidingSpot_t>& inout_List
)
{
	idList<darkModHidingSpot_t>& lessThanPivot;
	idList<darkModHidingSpot_t>& notLessThanPivot;
	int listLength;
	int nextPivotIndex;

	// Get pivot
	darkModHidingSpot_t pivotSpot = inout_List[pivotIndex];

	// 1) Split list on pivot
	int listLength = inout_List.Num();
	while (listLength > 0)
	{
		// Remove spot from input list
		darkModHidingSpot_t spot = inout_List[0];
		inout_List.RemoveIndex(0);

		// Test against pivot
		if (spot.quality < pivotSpot.quality)
		{
			// Add to the less than list
			lessThanPivot.Append (spot);
		}
		else
		{
			// Add to the not less than list
			notLessThanPivot.Append (spot);
		}

		// One less in input list
		listLength --;
	}

	// 2) Now perform the pivot operation on both the less than pivot list and the
	// not less than pivot list
	listLength = lessThanPivot.Num();
	if (listLength > 0)
	{
		int pivotIndexA = lessThanPivot[listLength/2];
		pivotOp (pivotIndexA, lessThanPivot);
	}

	listLength = notLessThanPivot.Num();
	if (listLength > 0)
	{
		int pivotIndexB = notLessThanPivot[listLength/2];
		pivotOp (pivotIndexB, notLessThanPivot);
	}

	// 3) Concatenate our two sorted lists into the output
	inout_List = lessThanPivot;
	inout_List.Append (notLessThanPivot);

	// Done
}
*/

//----------------------------------------------------------------------------

int DarkModHidingSpotList_SortCompareQuality (const darkModHidingSpot_t* p_a, const darkModHidingSpot_t* p_b)
{
	if (p_a->quality > p_b->quality)
	{
		// Sort highest to lowest
		return -1;
	}
	else if (p_a->quality < p_b->quality)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

//----------------------------------------------------------------------------

void darkModAASFindHidingSpots::sortByQuality
(
	idList<darkModHidingSpot_t>& inout_hidingSpots
)
{
	idList<darkModHidingSpot_t>::cmp_t* p_compareFunction = &DarkModHidingSpotList_SortCompareQuality;
	inout_hidingSpots.Sort (p_compareFunction);

	/*
	// Choose a pivot;
	int listLength = inout_hidingSpots.Num();
	if (listLength > 0)
	{
		int pivotIndex = listLength / 2;
		pivotOp (pivotIndex, inout_hidingSpots;
	}
	darkModHidingSpot_t pivotSpot = inout_hidingSpots[pivotIndex];

	idList<darkModHidingSpot_t> lessThanPivot;
	idList<darkModHidingSpot_t> notLessThanPivot;

	for (int index = 0; index < listLength; index ++)
	{
		// Get the hiding spot
		darkModHidingSpot_t spot = inout_hidingSpots[pivotIndex];	
	*/
}


//----------------------------------------------------------------------------


// Primary external interface
void darkModAASFindHidingSpots::getNearbyHidingSpots 
(
	idList<darkModHidingSpot_t>& out_hidingSpots,
	idAAS *in_p_aas, 
	float in_hidingHeight,
	idBounds in_searchLimits, 
	int in_hidingSpotTypesAllowed, 
	idEntity* in_p_ignoreEntity
)
{
	// Clear hiding spot list
	out_hidingSpots.Clear();

	// Log
	DM_LOG(LC_AI, LT_DEBUG).LogString("Hide from position %f, %f, %f\n", hideFromPosition.x, hideFromPosition.y, hideFromPosition.z);

	// Test paramters
	if (in_p_aas == NULL)
	{
		// TODO: Log error
		DM_LOG(LC_AI, LT_ERROR).LogString("Parameter in_p_aas is NULL");
		return;
	}

	// Set parameters in this instance
	p_inout_hidingSpots = &out_hidingSpots;
	p_aas = in_p_aas;
	hidingHeight = in_hidingHeight;
	searchLimits = in_searchLimits;
    hidingSpotTypesAllowed = in_hidingSpotTypesAllowed;
	p_ignoreEntity = in_p_ignoreEntity;

	/*
	// Start the background thread
	Sys_CreateThread
	(
		darkModAASFindHidingSpots::FindHidingSpots_BackgroundThreadFunc, 
		(void*) this,
		THREAD_NORMAL,
		searchThread,
		"HidingSpotSearch",
		g_threads,
		&g_thread_count
	);
	// Search thread will run in background while we continue on with other things
	*/

	// Not running in background thread, sincy Sys_CreateThread is not exposed in the
	// SDK and there is no mutex support even declared
	unsigned int Res = FindHidingSpots_BackgroundThreadFunc ((void*) this);

 }

//----------------------------------------------------------------------------

// Debug functions

void darkModAASFindHidingSpots::debugClearHidingSpotDrawList()
{
	// Clear the list
	darkModAASFindHidingSpots::DebugDrawList.Clear();

}

//----------------------------------------------------------------------------

void darkModAASFindHidingSpots::debugAppendHidingSpotsToDraw (const idList<darkModHidingSpot_t>& hidingSpotsToAppend)
{
	// Append to the list
	darkModAASFindHidingSpots::DebugDrawList.Append (hidingSpotsToAppend);

}

//----------------------------------------------------------------------------

void darkModAASFindHidingSpots::debugDrawHidingSpots(int viewLifetime)
{
	// Set up some depiction values 
	idVec4 DarknessMarkerColor(0.0f, 0.0f, 1.0f, 0.0);
	idVec4 OcclusionMarkerColor(0.0f, 1.0f, 0.0f, 0.0);
	idVec4 PortalMarkerColor(1.0f, 0.0f, 0.0f, 0.0);

	idVec3 markerArrowLength (0.0, 0.0, 1.0f);

	
	// Iterate the hiding spot debug draw list
	size_t spotCount = darkModAASFindHidingSpots::DebugDrawList.Num();
	for (size_t spotIndex = 0; spotIndex < spotCount; spotIndex ++)
	{
		idVec4 markerColor(0.0f, 0.0f, 0.0f, 0.0f);
		
		if ((DebugDrawList[spotIndex].hidingSpotTypes & PVS_AREA_HIDING_SPOT_TYPE) != 0)
		{
			markerColor += PortalMarkerColor;
		}
		
		if ((DebugDrawList[spotIndex].hidingSpotTypes & DARKNESS_HIDING_SPOT_TYPE) != 0)
		{
			markerColor += DarknessMarkerColor;
		}
		
		if ((DebugDrawList[spotIndex].hidingSpotTypes & VISUAL_OCCLUSION_HIDING_SPOT_TYPE) != 0)
		{
			markerColor += OcclusionMarkerColor;
		}

		// Scale from blackness to the color
		for (int i = 0; i < 4; i ++)
		{
			markerColor[i] *= DebugDrawList[spotIndex].quality;
		}

		// Render this hiding spot
		gameRenderWorld->DebugArrow
		(
			markerColor,
			DebugDrawList[spotIndex].goal.origin + markerArrowLength,
			DebugDrawList[spotIndex].goal.origin,
			1.0f,
			viewLifetime
		);
	}

	// Done
}

//----------------------------------------------------------------------------

// Test stub
void darkModAASFindHidingSpots::testFindHidingSpots 
(
	idVec3 hideFromLocation, 
	float hidingHeight,
	idBounds hideSearchBounds, 
	idEntity* p_ignoreEntity, 
	idAAS* p_aas
)
{

	idScriptBool b_threadDone;
	b_threadDone = FALSE;
	darkModAASFindHidingSpots HidingSpotFinder (hideFromLocation, p_aas, &b_threadDone);

	idList<darkModHidingSpot_t> hidingSpotList;

	DM_LOG(LC_AI, LT_DEBUG).LogVector ("Hide search mins", hideSearchBounds[0]);
	DM_LOG(LC_AI, LT_DEBUG).LogVector ("Hide search maxes", hideSearchBounds[1]);

	HidingSpotFinder.getNearbyHidingSpots
	(
		hidingSpotList,
		p_aas,
		hidingHeight,
		hideSearchBounds,
		ANY_HIDING_SPOT_TYPE,
		p_ignoreEntity
	);


	// Clear the debug list and add these
	darkModAASFindHidingSpots::debugClearHidingSpotDrawList();
	darkModAASFindHidingSpots::debugAppendHidingSpotsToDraw (hidingSpotList);
	darkModAASFindHidingSpots::debugDrawHidingSpots (15000);


	// Done
	
}

