// vim:ts=4:sw=4:cindent
/***************************************************************************
 *
 * PROJECT: The Dark Mod
 * $Revision: 4071 $
 * $Date: 2010-07-18 15:57:08 +0200 (Sun, 18 Jul 2010) $
 * $Author: tels $
 *
 ***************************************************************************/

// Copyright (C) 2010 Tels (Donated to The Dark Mod Team)

/*
   ModelGenerator

   Manipulate/Generate models at run time.

TODO: Multiple colors does somehow not work, using only the first color.
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

static bool init_version = FileVersionList("$Id: ModelGenerator.cpp 4071 2010-07-18 13:57:08Z tels $", init_version);

#include "ModelGenerator.h"

/*
===============
CModelGenerator::CModelGenerator
===============
*/
CModelGenerator::CModelGenerator( void ) {

}

/*
===============
CModelGenerator::Save
===============
*/
void CModelGenerator::Save( idSaveGame *savefile ) const {

}

/*
===============
CModelGenerator::Restore
===============
*/
void CModelGenerator::Restore( idRestoreGame *savefile ) {
}

/*
===============
ModelGenerator::Init
===============
*/
void CModelGenerator::Init( void ) {
}

/*
===============
CModelGenerator::Clear
===============
*/
void CModelGenerator::Clear( void ) {
}

/*
===============
CModelGenerator::Shutdown
===============
*/
void CModelGenerator::Shutdown( void ) {
}

/*
===============
CModelGenerator::DuplicateModel - Duplicate a render model

If the given list of model_ofs_t is filled, the model will be copied X times, each time
offset and rotated by the given values, also filling in the right vertex color.
===============
*/
idRenderModel * CModelGenerator::DuplicateModel ( const idRenderModel *source, const char* snapshotName, bool dupData, const idList<model_ofs_t> *offsets) {
	int numSurfaces;
	int numVerts, numIndexes;
	const modelSurface_t *surf;
	modelSurface_s newSurf;
	idList<model_ofs_t> ofs;
	model_ofs_t op;
	model_ofs_t op_zero;

	if (NULL == source)
	{
		gameLocal.Error("Dup model with NULL ptr.\n");
	}
	// allocate memory for the model
	idRenderModel *hModel = renderModelManager->AllocModel();
	// and init it as dynamic empty model
	hModel->InitEmpty( snapshotName );

	// get the number of base surfaces (minus decals) on the old model
	numSurfaces = source->NumBaseSurfaces();

	// count the tris
	numIndexes = 0; numVerts = 0;

	// if the given list of offsets is empty, replace it by a list with (0,0,0)
	if (NULL == offsets)
	{
		ofs.Clear();
		op_zero.offset = idVec3(0,0,0);
		op_zero.angles = idAngles(0,0,0);
		op_zero.color  = PackColor( idVec3(1,1,1) );
		op_zero.lod    = 0;
		ofs.Append( op_zero );
		offsets = &ofs;
	}
	else
	{
		// we cannot share the data if given a list of offsets
		if (!dupData)
		{
			gameLocal.Warning("Sharing data on model with different offsets will not work.\n");
			dupData = true;
		}
	}

	// duplicate everything and offset it
	// for each surface
	for (int i = 0; i < numSurfaces; i++)
	{
		//gameLocal.Warning("Duplicating surface %i.\n", i);

		surf = source->Surface( i );
		if (surf)
		{
			numVerts += surf->geometry->numVerts; 
			numIndexes += surf->geometry->numIndexes;

			// copy the material
			newSurf.shader = surf->shader;
			if (dupData)
			{
				int n = offsets->Num();

		//		gameLocal.Warning("Duplicating %i verts and %i indexes %i times.\n", surf->geometry->numVerts, surf->geometry->numIndexes, n );
				newSurf.geometry = hModel->AllocSurfaceTriangles( numVerts * n, numIndexes * n );

			//	gameLocal.Printf(" Surface data: numMirroredVerts %i.\n", surf->geometry->numMirroredVerts );
			//	gameLocal.Printf(" Surface data: numDupVerts %i.\n", surf->geometry->numDupVerts );
			//	gameLocal.Printf(" Surface data: numSilEdges %i.\n", surf->geometry->numSilEdges );
			//	gameLocal.Printf(" Surface data: numShadowIndexesNoFrontCaps %i.\n", surf->geometry->numShadowIndexesNoFrontCaps );
			//	gameLocal.Printf(" Surface data: shadowCapPlaneBits %i.\n", surf->geometry->shadowCapPlaneBits );

				int nV = 0;		// vertexes
				int nI = 0;		// indexes
				// for each offset
				for (int o = 0; o < n; o++)
				{
					op = offsets->Ptr()[o];
					//gameLocal.Warning(" Offset %0.2f, %0.2f, %0.2f.\n", op.offset.x, op.offset.y, op.offset.z );

					// precompute these
					idMat3 a = op.angles.ToMat3();

					// copy the vertexes
					for (int j = 0; j < surf->geometry->numVerts; j++)
					{
						newSurf.geometry->verts[nV] = surf->geometry->verts[j];
						idDrawVert *v = &newSurf.geometry->verts[nV];

						// rotate
						v->xyz *= a;
						// then offset
						v->xyz += op.offset;
						// Set "per-entity" color:
						v->SetColor( op.color );

/*						if (o == 1 || o == 2)
						{
						gameLocal.Printf ("Vert %i (%i): xyz %s st %s tangent %s %s normal %s color %i %i %i %i.\n",
								j, nV, v->xyz.ToString(), v->st.ToString(), v->tangents[0].ToString(), v->tangents[1].ToString(), v->normal.ToString(),
								v->color[0],
								v->color[1],
								v->color[2],
								v->color[3]
							   	);
						}
					*/
						nV ++;
					}

					// copy indexes
					int no = surf->geometry->numVerts * o;					// correction factor
					for (int j = 0; j < surf->geometry->numIndexes; j++)
					{
						newSurf.geometry->indexes[nI ++] = surf->geometry->indexes[j] + no;
					}
				} // end for each offset

				//newSurf.geometry->tangentsCalculated = true;
				newSurf.geometry->numVerts = nV;
				newSurf.geometry->numIndexes = nI;
				// calculate new bounds
				SIMDProcessor->MinMax( newSurf.geometry->bounds[0], newSurf.geometry->bounds[1], newSurf.geometry->verts, newSurf.geometry->numVerts );
			}
			else
			{
				// caller needs to make sure that the shared data is not deallocated twice
				newSurf.geometry = surf->geometry;
			}
			newSurf.id = 0;
			hModel->AddSurface( newSurf );

			// If this surface should create backsides, the next surface will be the automatically
			// created backsides, so skip them, as FinishSurfaces() below will recreate them:
			if (newSurf.shader->ShouldCreateBackSides())
			{
				// skip next surface
				i++;
			}
		}

	}

	// generate shadow hull as well as tris for twosided materials
	hModel->FinishSurfaces();

//	hModel->Print();

	return hModel;
}

/*
===============
CModelGenerator::FreeSharedModelData - manipulate memory of a duplicate model so that shared data does not get freed twice
===============
*/
void CModelGenerator::FreeSharedModelData ( const idRenderModel *source )
{
	const modelSurface_t *surf;

	// get the number of base surfaces (minus decals) on the old model
	int numSurfaces = source->NumBaseSurfaces();

	// for each surface
	for (int i = 0; i < numSurfaces; i++)
	{
		// get a pointer to a surface
		surf = source->Surface( i );
		if (surf)
		{
			// null out the geometry with the shared data
			surf->geometry->numVerts = 0;
			surf->geometry->verts = NULL;
			surf->geometry->numIndexes = 0;
			surf->geometry->indexes = NULL;
		}
	}
}

