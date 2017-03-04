/*  TA3D, a remake of Total Annihilation
    Copyright (C) 2005  Roland BROCHARD

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA*/

/*-----------------------------------------------------------------------------------\
|                                     instancing.cpp                                 |
|  This module contains objects used to render a set of similar meshes or quads      |
|                                                                                    |
\-----------------------------------------------------------------------------------*/

#include <stdafx.h>
#include <TA3D_NameSpace.h>
#include "mesh.h"
#include "instancing.h"


namespace TA3D
{
    namespace INSTANCING
    {
        bool water = false;
        float sealvl = 0.0f;
    }

    DrawingTable::DrawingTable()
    {
        hash_table.reserve(DrawingTable_SIZE);
    }

    DrawingTable::~DrawingTable()
    {
        hash_table.clear();
    }


	void DrawingTable::queue_Instance( uint32 model_id, const Instance &instance )
    {
        hash_table[model_id].queue.push_back(instance);
    }


    void DrawingTable::draw_all()
    {
        for(auto rq = hash_table.begin() ; rq != hash_table.end() ; ++rq)
            rq.value().draw_queue(rq.key());
    }


    void RenderQueue::draw_queue(uint32 model_id) const
    {
        if (queue.empty())
            return;
        glPushMatrix();

		Model *model = model_manager.model[ model_id ];

		if (model->from_2d)
            glEnable(GL_ALPHA_TEST);

        glEnable(GL_LIGHTING);
        glDisable(GL_BLEND);
		if (!model->dlist)	// Build the display list if necessary
        {
            model->check_textures();
            model->dlist = glGenLists (1);
            glNewList (model->dlist, GL_COMPILE);
            model->mesh->draw_nodl();
            glEndList();
        }

        for (const Instance &i : queue)
        {
            glPopMatrix();
            glPushMatrix();
            glTranslatef( i.pos.x, i.pos.y, i.pos.z );
            if (lp_CONFIG->underwater_bright && INSTANCING::water && i.pos.y < INSTANCING::sealvl)
			{
                double eqn[4]= { 0.0f, -1.0f, 0.0f, INSTANCING::sealvl - i.pos.y };
				glClipPlane(GL_CLIP_PLANE2, eqn);
			}
            glRotatef( i.angle, 0.0f, 1.0f, 0.0f );
            glColor4ubv( (GLubyte*) &i.col );
			glCallList( model->dlist );
            if (lp_CONFIG->underwater_bright && INSTANCING::water && i.pos.y < INSTANCING::sealvl)
			{
				glEnable(GL_CLIP_PLANE2);
				glEnable( GL_BLEND );
				glBlendFunc( GL_ONE, GL_ONE );
				glDepthFunc( GL_EQUAL );
				glColor4ub( 0x7F, 0x7F, 0x7F, 0x7F );
				model->draw(0.0f, NULL, false, true, false, 0, NULL, NULL, NULL, 0.0f, NULL, false, 0, false);
				glColor4ub( 0xFF, 0xFF, 0xFF, 0xFF );
				glDepthFunc( GL_LESS );
				glDisable( GL_BLEND );
				glDisable(GL_CLIP_PLANE2);
			}
        }

        if (model->from_2d)
            glDisable(GL_ALPHA_TEST);

        glPopMatrix();
    }


    QUAD_TABLE::QUAD_TABLE()
    {
        hash_table.reserve(DrawingTable_SIZE);
    }

    QUAD_TABLE::~QUAD_TABLE()
    {
        hash_table.clear();
    }


    void QUAD_TABLE::queue_quad(const GfxTexture::Ptr &texture_id, const QUAD &quad)
    {
        hash_table[texture_id].queue.push_back(quad);
    }


    void QUAD_TABLE::draw_all()
    {
        uint32	max_size = 0;
        for (const auto &qq : hash_table)
            max_size = std::max<uint32>(max_size, qq.queue.size());
		if (max_size == 0U)
			return;

		static Vector3D	*P = NULL;
		static uint32	*C = NULL;
		static GLfloat	*T = NULL;
		static uint32 capacity = 0U;

		if (max_size > capacity)
		{
			if (capacity)
			{
				delete P;
				delete C;
				delete T;
			}
			capacity = 2 << Math::Log2(max_size);
			P = new Vector3D[ capacity << 2 ];
			C = new uint32[ capacity << 2 ];
			T = new GLfloat[ capacity << 3 ];
			for (GLfloat *i = T, *end = T + (capacity << 3) ; i != end ; ++i)
			{
				*i = 0.0f;	++i;
				*i = 0.0f;	++i;

				*i = 1.0f;	++i;
				*i = 0.0f;	++i;

				*i = 1.0f;	++i;
				*i = 1.0f;	++i;

				*i = 0.0f;	++i;
				*i = 1.0f;
			}
		}

        glEnableClientState(GL_VERTEX_ARRAY);		// vertex coordinates
        glEnableClientState(GL_COLOR_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glEnable( GL_TEXTURE_2D );
        glColorPointer(4,GL_UNSIGNED_BYTE,0,C);
        glVertexPointer( 3, GL_FLOAT, 0, P);
        glTexCoordPointer(2, GL_FLOAT, 0, T);

        for (auto qq = hash_table.begin() ; qq != hash_table.end() ; ++qq)
                qq.value().draw_queue(qq.key(), P, C);

        glDisableClientState(GL_COLOR_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }


    void QUAD_QUEUE::draw_queue(const GfxTexture::Ptr &texture_id, Vector3D *P, uint32 *C) const
    {
        if (queue.empty())
            return;

        Vector3D *p = P;
        uint32 *c = C;
        for (const QUAD &e : queue)
        {
            p->x = e.pos.x - e.size_x;
            p->y = e.pos.y;
            p->z = e.pos.z - e.size_z;
            *c = e.col;
			++c;
            ++p;

            p->x = e.pos.x + e.size_x;
            p->y = e.pos.y;
            p->z = e.pos.z - e.size_z;
            *c = e.col;
			++c;
			++p;

            p->x = e.pos.x + e.size_x;
            p->y = e.pos.y;
            p->z = e.pos.z + e.size_z;
            *c = e.col;
			++c;
			++p;

            p->x = e.pos.x - e.size_x;
            p->y = e.pos.y;
            p->z = e.pos.z + e.size_z;
            *c = e.col;
			++c;
			++p;
        }
        texture_id->bind();
		glDrawArrays(GL_QUADS, 0, (GLsizei)queue.size() << 2);		// draw those quads

        if (lp_CONFIG->underwater_bright && INSTANCING::water)
        {
            p = P;
			uint32 i = 0U;
            for (const QUAD &e : queue)
            {
                if (e.pos.y >= INSTANCING::sealvl) continue;
                p->x = e.pos.x - e.size_x;
                p->y = e.pos.y;
                p->z = e.pos.z - e.size_z;
                ++p;

                p->x = e.pos.x + e.size_x;
                p->y = e.pos.y;
                p->z = e.pos.z - e.size_z;
                ++p;

                p->x = e.pos.x + e.size_x;
                p->y = e.pos.y;
                p->z = e.pos.z + e.size_z;
                ++p;

                p->x = e.pos.x - e.size_x;
                p->y = e.pos.y;
                p->z = e.pos.z + e.size_z;
                ++p;
                ++i;
            }

            if (i > 0)
            {
				glColor4ub(0x7F,0x7F,0x7F,0x7F);
				glDisableClientState(GL_COLOR_ARRAY);
				glEnable( GL_BLEND );
                glDisable( GL_TEXTURE_2D );
                glBlendFunc( GL_ONE, GL_ONE );
                glDepthFunc( GL_EQUAL );
                glDrawArrays(GL_QUADS, 0, i << 2);		// draw those quads
                glDepthFunc( GL_LESS );
                glEnable( GL_TEXTURE_2D );
                glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
				glEnableClientState(GL_COLOR_ARRAY);
			}
        }
    }

} // namespace TA3D

