/*****************************************************************************
The Dark Mod GPL Source Code

This file is part of the The Dark Mod Source Code, originally based
on the Doom 3 GPL Source Code as published in 2011.

The Dark Mod Source Code is free software: you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3 of the License,
or (at your option) any later version. For details, see LICENSE.TXT.

Project: The Dark Mod (http://www.thedarkmod.com/)

******************************************************************************/
#include "precompiled.h"
#pragma hdrstop

#include "tr_local.h"

bool fboUsed;
// called when post-proceesing is about to start, needs pixels and sometimes depth as both input and output for water and smoke
void RB_FboAccessColorDepth( bool DepthToo = false ) {
	//if (!fboUsed) // we need to copy render separately for water/smoke and then again for bloom
	//	return;
	GL_SelectTexture( 0 );
	if ( !fboUsed || !r_fboSharedColor.GetBool() ) {
		globalImages->currentRenderImage->Bind();
		qglCopyTexImage2D( GL_TEXTURE_2D, 0, fboUsed && r_fboColorBits.GetInteger() == 15 ? GL_RGB5_A1 : GL_RGBA,
			0, 0, globalImages->currentRenderImage->uploadWidth, globalImages->currentRenderImage->uploadHeight, 0 );
	}
	if ( !(fboUsed && r_fboSharedDepth.GetBool()) && DepthToo ) {
		globalImages->currentDepthImage->Bind();
		qglCopyTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
			0, 0, globalImages->currentDepthImage->uploadWidth, globalImages->currentDepthImage->uploadHeight, 0 );
	}
}

// duzenko #4425: use framebuffer object for rendering in virtual resolution, separate stencil, lower color depth and depth precision, etc
GLuint fboId;

void RB_FboEnter() {
	if ( fboUsed && fboId )
		return;
	GL_CheckErrors(); // debug
	bool fboSeparateStencil = strcmp( glConfig.vendor_string, "Intelq" ) == 0; // may change after a vid_restart
	// virtual resolution as a modern alternative for actual desktop resolution affecting all other windows
	GLuint curWidth = r_fboResolution.GetFloat() * glConfig.vidWidth, curHeight = r_fboResolution.GetFloat() * glConfig.vidHeight;

	// reset textures 
	if ( curWidth != globalImages->currentRenderImage->uploadWidth || curHeight != globalImages->currentRenderImage->uploadHeight
		|| curWidth != globalImages->currentDepthImage->uploadWidth || curHeight != globalImages->currentDepthImage->uploadHeight
		|| r_fboColorBits.IsModified()
		) { // FIXME don't allocate memory if sharing color/depth
		r_fboColorBits.ClearModified();

		globalImages->currentRenderImage->Bind();
		globalImages->currentRenderImage->uploadWidth = curWidth; // used as a shader param
		globalImages->currentRenderImage->uploadHeight = curHeight;
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		qglTexImage2D( GL_TEXTURE_2D, 0, r_fboColorBits.GetInteger() == 15 ? GL_RGB5_A1 : GL_RGBA, curWidth, curHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL ); //NULL means reserve texture memory, but texels are undefined

		globalImages->currentRenderFbo->Bind();
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		qglTexImage2D( GL_TEXTURE_2D, 0, r_fboColorBits.GetInteger() == 15 ? GL_RGB5_A1 : GL_RGBA, curWidth, curHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL ); //NULL means reserve texture memory, but texels are undefined

		if ( fboSeparateStencil ) {
			globalImages->currentStencilFbo->Bind();
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			qglTexImage2D( GL_TEXTURE_2D, 0, GL_STENCIL_INDEX8, curWidth, curHeight, 0, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, 0 );
		}

		globalImages->currentDepthFbo->Bind();
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		if ( fboSeparateStencil ) {
			qglTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, curWidth, curHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0 );
		} else {
			qglTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL, curWidth, curHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 0 );
		}

		globalImages->currentDepthImage->Bind();
		globalImages->currentDepthImage->uploadWidth = curWidth; // used as a shader param
		globalImages->currentDepthImage->uploadHeight = curHeight;
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		qglTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		if ( fboSeparateStencil ) {
			qglTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, curWidth, curHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0 );
		} else {
			qglTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_STENCIL, curWidth, curHeight, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 0 );
		}
	}

	// (re-)attach textures to FBO
	if ( !fboId || r_fboSharedColor.IsModified() || r_fboSharedDepth.IsModified() ) {
		// create a framebuffer object, you need to delete them when program exits.
		if ( !fboId )
			glGenFramebuffers( 1, &fboId );
		r_fboSharedColor.ClearModified();
		r_fboSharedDepth.ClearModified();
		glBindFramebuffer( GL_FRAMEBUFFER_EXT, fboId );
		// attach a texture to FBO color attachement point
		if ( r_fboSharedColor.GetBool() )
			glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, globalImages->currentRenderImage->texnum, 0 );
		else
			glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, globalImages->currentRenderFbo->texnum, 0 );
		// attach a renderbuffer to depth attachment point
		GLuint depthTex = r_fboSharedDepth.GetBool() ? globalImages->currentDepthImage->texnum : globalImages->currentDepthFbo->texnum;
		glFramebufferTexture2D( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0 );
		if ( fboSeparateStencil )
			glFramebufferTexture2D( GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, globalImages->currentStencilFbo->texnum, 0 );
		else
			glFramebufferTexture2D( GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0 );
		int status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
		if ( GL_FRAMEBUFFER_COMPLETE != status ) { // something went wrong, fall back to default
			common->Printf( "glCheckFramebufferStatus %d\n", status );
			glDeleteFramebuffers( 1, &fboId );
			fboId = 0; // try from scratch next time
			r_useFbo.SetBool( false );
		}
		glBindFramebuffer( GL_FRAMEBUFFER, 0 ); // not obvious, but let it be 
	}
	glBindFramebuffer( GL_FRAMEBUFFER, fboId );
	qglClear( GL_COLOR_BUFFER_BIT ); // otherwise transparent skybox blends with previous frame
	fboUsed = true;
	GL_CheckErrors();
}

// switch from fbo to default framebuffer, copy content
void RB_FboLeave( viewDef_t* viewDef ) {
	if ( !fboUsed )
		return;
	GL_CheckErrors();
	RB_FboAccessColorDepth();
	// hasn't worked very well at the first approach, maybe retry later
	/*glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, globalImages->currentRenderImage->uploadWidth, globalImages->currentRenderImage->uploadHeight, 0, 0,
	glConfig.vidWidth, glConfig.vidHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST); */
	glBindFramebuffer( GL_FRAMEBUFFER_EXT, 0 );
	qglLoadIdentity();
	qglMatrixMode( GL_PROJECTION );
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho( 0, 1, 0, 1, -1, 1 );
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );

	GL_State( GLS_DEFAULT );
	//qglEnable( GL_TEXTURE_2D );
	qglDisable( GL_DEPTH_TEST );
	qglDisable( GL_STENCIL_TEST );
	qglColor3f( 1, 1, 1 );
	{
		switch ( r_fboDebug.GetInteger() )
		{
		case 1:
			globalImages->currentRenderImage->Bind();
			break;
		case 2:
			globalImages->currentDepthImage->Bind();
			break;
		case 3:
			globalImages->currentDepthFbo->Bind();
			break;
		default:
			if ( r_fboSharedColor.GetBool() )
				globalImages->currentRenderImage->Bind();
			else
				globalImages->currentRenderFbo->Bind();
		}
		RB_DrawFullScreenQuad();
	}
	qglEnable( GL_DEPTH_TEST );
	qglPopMatrix();
	qglMatrixMode( GL_MODELVIEW );
	GL_SelectTexture( 0 );
	if ( viewDef ) { // switch back to normal resolution for correct 2d
		tr.renderCrops[0].width = glConfig.vidWidth;
		tr.renderCrops[0].height = glConfig.vidHeight;
		viewDef->viewport.x2 = glConfig.vidWidth - 1;
		viewDef->viewport.y2 = glConfig.vidHeight - 1;
		viewDef->scissor.x2 = glConfig.vidWidth - 1;
		viewDef->scissor.y2 = glConfig.vidHeight - 1;
	}
	fboUsed = 0;
	GL_CheckErrors();
}

