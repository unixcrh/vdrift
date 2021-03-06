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

#ifndef _RENDERMODELEXTDRAWABLE
#define _RENDERMODELEXTDRAWABLE

#include "gl3v/rendermodelext.h"

class VertexArray;
class Drawable;

class RenderModelExtDrawable : public RenderModelExt
{
	friend class Drawable;
	public:
		void SetVertArray(const VertexArray* value) {vert_array = value;if (vert_array) enabled = true;}
		virtual void draw(GLWrapper & gl) const;

		RenderModelExtDrawable() : vert_array(NULL) {}
		~RenderModelExtDrawable() {}

	private:
		const VertexArray * vert_array;
        void SetLineSize(float size) { linesize = size; }

        float linesize;
};

#endif
