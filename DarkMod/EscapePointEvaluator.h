/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision: 866 $
 * $Date: 2007-03-23 22:25:02 +0100 (Fr, 23 Mar 2007) $
 * $Author: greebo $
 *
 ***************************************************************************/
#ifndef ESCAPE_POINT_EVALUATOR__H
#define ESCAPE_POINT_EVALUATOR__H

#include "../idlib/precompiled.h"

#include <boost/shared_ptr.hpp>

// Forward Declaration
struct EscapePoint;
struct EscapeConditions;

/**
 * greebo: An EscapePointEvaluator gets fed with one EscapePoint after the
 *         other (by any algorithm) and inspects each of them. The most
 *         suitable one is stored internally and can be retrieved at any time
 *         by using GetBestEscapePoint().
 */
class EscapePointEvaluator
{
protected:
	// This holds the ID of the best escape point so far
	int _bestId;

public:
	// Default Constructor
	EscapePointEvaluator() :
		_bestId(-1) // Set the ID to invalid
	{}

	/**
	 * greebo: Evaluate the given escape point.
	 *
	 * @returns: FALSE means that the evaluation can be stopped (prematurely),
	 *           no more EscapePoints need to be passed.
	 */
	virtual bool Evaluate(EscapePoint& escapePoint) = 0;

	/**
	 * greebo: Returns the ID of the best found escape point.
	 *
	 * @returns: an ID of -1 is returned if no point was found to be suitable.
	 */
	virtual inline int GetBestEscapePoint()
	{
		return _bestId;
	}
};
typedef boost::shared_ptr<EscapePointEvaluator> EscapePointEvaluatorPtr;

/**
 * ==== EVALUATOR IMPLEMENTATIONS === 
 */

/**
 * greebo: This visitor returns the escape point which is farthest away
 *         from the threatening entity.
 */
class FarthestEscapePointFinder :
	public EscapePointEvaluator
{
	// The escape conditions for reference
	const EscapeConditions& _conditions;

	// The origin of the threatening entity
	idVec3 _threatOrigin;

	// The area number the AI starts to flee in
	int _startAreaNum;

	// The best travel time so far
	int _bestTime;

public:
	FarthestEscapePointFinder(const EscapeConditions& conditions);

	virtual bool Evaluate(EscapePoint& escapePoint);
};

/**
 * greebo: This visitor tries to locate the nearest
 *         guarded escape point.
 */
class NearestGuardedEscapePointFinder :
	public EscapePointEvaluator
{
	// The escape conditions for reference
	const EscapeConditions& _conditions;

	// The origin of the threatening entity
	idVec3 _threatOrigin;

	// The area number the AI starts to flee in
	int _startAreaNum;

	// The best travel time so far
	int _bestTime;

public:
	NearestGuardedEscapePointFinder(const EscapeConditions& conditions);

	virtual bool Evaluate(EscapePoint& escapePoint);
};

#endif /* ESCAPE_POINT_EVALUATOR__H */