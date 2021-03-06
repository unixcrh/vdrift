/************************************************************************/
/*                                                                      */
/* This file is part of VDrift.                                         */
/*                                                                      */
/* VDrift is free software: you can redistribute it and/or modify       */
/* it under the terms of the GNU General Public License as published by */
/* the Free Software Foundation, either version 3 of the License, or    */
/* (at your option) any later version.                                  */
/*                                                                      */
/* VDrift is distributed in the hope that it will be useful,            */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/* GNU General Public License for more details.                         */
/*                                                                      */
/* You should have received a copy of the GNU General Public License    */
/* along with VDrift.  If not, see <http://www.gnu.org/licenses/>.      */
/*                                                                      */
/************************************************************************/

#include "render_input_postprocess.h"
#include "glutil.h"
#include "graphicsstate.h"
#include "matrix4.h"
#include "shader.h"

RenderInputPostprocess::RenderInputPostprocess() :
	shader(NULL),
	writealpha(true),
	writecolor(true),
	writedepth(false),
	depth_mode(GL_LEQUAL),
	clearcolor(false),
	cleardepth(false),
	blendmode(BlendMode::DISABLED),
	contrast(1.0)
{
	//ctor
}

RenderInputPostprocess::~RenderInputPostprocess()
{
	//dtor
}

void RenderInputPostprocess::SetSourceTextures(const std::vector <TextureInterface*> & textures)
{
	source_textures = textures;
}

void RenderInputPostprocess::SetShader(Shader * newshader)
{
	shader = newshader;
}

void RenderInputPostprocess::Render(GraphicsState & glstate, std::ostream & error_output)
{
	assert(shader);

	CheckForOpenGLErrors("postprocess begin", error_output);

	glstate.SetColorMask(writecolor, writealpha);
	glstate.SetDepthMask(writedepth);

	if (clearcolor && cleardepth)
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	else if (clearcolor)
		glClear(GL_COLOR_BUFFER_BIT);
	else if (cleardepth)
		glClear(GL_DEPTH_BUFFER_BIT);

	shader->Enable();

	CheckForOpenGLErrors("postprocess shader enable", error_output);

	Mat4 projMatrix, viewMatrix;
	projMatrix.SetOrthographic(0, 1, 0, 1, -1, 1);
	viewMatrix.LoadIdentity();

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(projMatrix.GetArray());

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(viewMatrix.GetArray());

	glstate.SetColor(1,1,1,1);

	SetBlendMode(glstate);

	if (writedepth || depth_mode != GL_ALWAYS)
		glstate.Enable(GL_DEPTH_TEST);
	else
		glstate.Disable(GL_DEPTH_TEST);
	glDepthFunc( depth_mode );
	glstate.Enable(GL_TEXTURE_2D);

	glActiveTexture(GL_TEXTURE0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);

	CheckForOpenGLErrors("postprocess flag set", error_output);


	float maxu = 1.f;
	float maxv = 1.f;

	int num_nonnull = 0;
	for (unsigned int i = 0; i < source_textures.size(); i++)
	{
		//std::cout << i << ": " << source_textures[i] << std::endl;
		glActiveTexture(GL_TEXTURE0+i);
		if (source_textures[i])
		{
			source_textures[i]->Activate();
			num_nonnull++;
			if (source_textures[i]->IsRect())
			{
				maxu = source_textures[i]->GetW();
				maxv = source_textures[i]->GetH();
			}
		}
	}
	if (source_textures.size() && !num_nonnull)
	{
		error_output << "Out of the " << source_textures.size() << " input textures provided as inputs to this postprocess stage, zero are available. This stage will have no effect." << std::endl;
		return;
	}
	glActiveTexture(GL_TEXTURE0);

	CheckForOpenGLErrors("postprocess texture set", error_output);

	// build the frustum corners
	float ratio = w/h;
	std::vector <Vec3 > frustum_corners(4);
	frustum_corners[0].Set(-lod_far,-lod_far,-lod_far);	//BL
	frustum_corners[1].Set(lod_far,-lod_far,-lod_far);	//BR
	frustum_corners[2].Set(lod_far,lod_far,-lod_far);	//TR
	frustum_corners[3].Set(-lod_far,lod_far,-lod_far);	//TL
	Mat4 inv_proj;
	inv_proj.InvPerspective(camfov, ratio, 0.1, lod_far);
	for (int i = 0; i < 4; i++)
	{
		inv_proj.TransformVectorOut(frustum_corners[i][0], frustum_corners[i][1], frustum_corners[i][2]);
		frustum_corners[i][2] = -lod_far;
	}
	// frustum corners in world space for dynamic sky shader
	std::vector <Vec3 > frustum_corners_w(4);
	Mat4 inv_view_rot;
	(-cam_rotation).GetMatrix4(inv_view_rot);
	for (int i = 0; i < 4; i++)
	{
		frustum_corners_w[i] = frustum_corners[i];
		inv_view_rot.TransformVectorOut(frustum_corners_w[i][0], frustum_corners_w[i][1], frustum_corners_w[i][2]);
	}

	// send shader parameters
	{
		Vec3 lightvec = lightposition;
		cam_rotation.RotateVector(lightvec);
		shader->UploadActiveShaderParameter3f("directlight_eyespace_direction", lightvec);
		shader->UploadActiveShaderParameter1f("contrast", contrast);
		shader->UploadActiveShaderParameter1f("znear", 0.1);
		//std::cout << lightvec << std::endl;
		shader->UploadActiveShaderParameter3f("frustum_corner_bl", frustum_corners[0]);
		shader->UploadActiveShaderParameter3f("frustum_corner_br_delta", frustum_corners[1]-frustum_corners[0]);
		shader->UploadActiveShaderParameter3f("frustum_corner_tl_delta", frustum_corners[3]-frustum_corners[0]);
	}

	// draw a quad
	unsigned faces[2 * 3] = {
		0, 1, 2,
		2, 3, 0,
	};
	float pos[4 * 3] = {
		0.0f,  0.0f, 0.0f,
		1.0f,  0.0f, 0.0f,
		1.0f,  1.0f, 0.0f,
		0.0f,  1.0f, 0.0f,
	};
	// send the UV corners in UV set 0
	float tc0[4 * 2] = {
		0.0f, 0.0f,
		maxu, 0.0f,
		maxu, maxv,
		0.0f, maxv,
	};
	// send the frustum corners in UV set 1
	float tc1[4 * 3] = {
		frustum_corners[0][0], frustum_corners[0][1], frustum_corners[0][2],
		frustum_corners[1][0], frustum_corners[1][1], frustum_corners[1][2],
		frustum_corners[2][0], frustum_corners[2][1], frustum_corners[2][2],
		frustum_corners[3][0], frustum_corners[3][1], frustum_corners[3][2],
	};
	// fructum corners in world space in uv set 2
	float tc2[4 * 3] = {
		frustum_corners_w[0][0], frustum_corners_w[0][1], frustum_corners_w[0][2],
		frustum_corners_w[1][0], frustum_corners_w[1][1], frustum_corners_w[1][2],
		frustum_corners_w[2][0], frustum_corners_w[2][1], frustum_corners_w[2][2],
		frustum_corners_w[3][0], frustum_corners_w[3][1], frustum_corners_w[3][2],
	};

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, pos);

	glClientActiveTexture(GL_TEXTURE0);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, 0, tc0);

	glClientActiveTexture(GL_TEXTURE1);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(3, GL_FLOAT, 0, tc1);

	glClientActiveTexture(GL_TEXTURE2);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(3, GL_FLOAT, 0, tc2);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, faces);

	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glClientActiveTexture(GL_TEXTURE1);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glClientActiveTexture(GL_TEXTURE0);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glDisableClientState(GL_VERTEX_ARRAY);

	CheckForOpenGLErrors("postprocess draw", error_output);

	glstate.Enable(GL_DEPTH_TEST);
	glstate.Disable(GL_TEXTURE_2D);

	for (unsigned int i = 0; i < source_textures.size(); i++)
	{
		//std::cout << i << ": " << source_textures[i] << std::endl;
		glActiveTexture(GL_TEXTURE0+i);
		if (source_textures[i])
			source_textures[i]->Deactivate();
	}
	glActiveTexture(GL_TEXTURE0);

	CheckForOpenGLErrors("postprocess end", error_output);
}

void RenderInputPostprocess::SetWriteColor(bool write)
{
	writecolor = write;
}

void RenderInputPostprocess::SetWriteAlpha(bool write)
{
	writealpha = write;
}

void RenderInputPostprocess::SetWriteDepth(bool write)
{
	writedepth = write;
}

void RenderInputPostprocess::SetDepthMode(int mode)
{
	depth_mode = mode;
}

void RenderInputPostprocess::SetClear(bool newclearcolor, bool newcleardepth)
{
	clearcolor = newclearcolor;
	cleardepth = newcleardepth;
}

void RenderInputPostprocess::SetBlendMode(BlendMode::BLENDMODE mode)
{
	blendmode = mode;
}

void RenderInputPostprocess::SetContrast(float value)
{
	contrast = value;
}

void RenderInputPostprocess::SetCameraInfo(
	const Vec3 & newpos,
	const Quat & newrot,
	float newfov, float newlodfar,
	float neww, float newh)
{
	cam_position = newpos;
	cam_rotation = newrot;
	camfov = newfov;
	lod_far = newlodfar;
	w = neww;
	h = newh;
}

void RenderInputPostprocess::SetSunDirection(const Vec3 & newsun)
{
	lightposition = newsun;
}

void RenderInputPostprocess::SetBlendMode(GraphicsState & glstate)
{
	assert(blendmode != BlendMode::ALPHATEST);
	switch (blendmode)
	{
		case BlendMode::DISABLED:
		{
			glstate.Disable(GL_ALPHA_TEST);
			glstate.Disable(GL_BLEND);
			glstate.Disable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		}
		break;

		case BlendMode::ADD:
		{
			glstate.Disable(GL_ALPHA_TEST);
			glstate.Enable(GL_BLEND);
			glstate.Disable(GL_SAMPLE_ALPHA_TO_COVERAGE);
			glstate.SetBlendFunc(GL_ONE, GL_ONE);
		}
		break;

		case BlendMode::ALPHABLEND:
		{
			glstate.Disable(GL_ALPHA_TEST);
			glstate.Enable(GL_BLEND);
			glstate.Disable(GL_SAMPLE_ALPHA_TO_COVERAGE);
			glstate.SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		break;

		case BlendMode::PREMULTIPLIED_ALPHA:
		{
			glstate.Disable(GL_ALPHA_TEST);
			glstate.Enable(GL_BLEND);
			glstate.Disable(GL_SAMPLE_ALPHA_TO_COVERAGE);
			glstate.SetBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		}
		break;

		default:
		assert(0);
		break;
	}
}
