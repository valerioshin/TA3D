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
|                                        mesh.h                                      |
|  This module contains the base class for all meshes. The base mesh class is a      |
| virtual class so that a mesh type can be implemented separately                    |
|                                                                                    |
\-----------------------------------------------------------------------------------*/

#ifndef __TA3D_MESH_H__
#define __TA3D_MESH_H__

#include "../misc/string.h"
#include "../misc/hash_table.h"
#include "../ta3dbase.h"
#include "../gaf.h"
#include <vector>
#include <list>
#include "../misc/matrix.h"
#include "../gfx/glfunc.h"
#include "../gfx/shader.h"
#include "../scripts/script.data.h"

namespace TA3D
{

    /*--------------Classes pour l'animation---------------------------------------------*/

#define FLAG_HIDE               0x01
#define FLAG_WAIT_FOR_TURN      0x02
#define FLAG_NEED_COMPUTE       0x04
#define FLAG_EXPLODE            0x08
#define FLAG_ANIMATE            0x10
#define FLAG_ANIMATED_TEXTURE   0x20
#define FLAG_DONT_SHADE         0x40

    // a few things needed to handle explosions properly

#define EXPLODE_SHATTER                 1               // The piece will shatter instead of remaining whole
#define EXPLODE_EXPLODE_ON_HIT          2               // The piece will explode when it hits the ground
#define EXPLODE_FALL                    4               // The piece will fall due to gravity instead of just flying off
#define EXPLODE_SMOKE                   8               // A smoke trail will follow the piece through the air
#define EXPLODE_FIRE                    16              // A fire trail will follow the piece through the air
#define EXPLODE_BITMAPONLY              32              // The piece will not fly off or shatter or anything.  Only a bitmap explosion will be rendered.

#define EXPLODE_BITMAP1                 256
#define EXPLODE_BITMAP2                 512
#define EXPLODE_BITMAP3                 1024
#define EXPLODE_BITMAP4                 2048
#define EXPLODE_BITMAP5                 4096
#define EXPLODE_BITMAPNUKE              8192

    class AXE
    {
    public:
        float	move_speed;
        float	move_distance;
        float	rot_angle;
        float	rot_speed;
        float	rot_accel;
        float	angle;
        float	pos;
        bool	rot_limit;
        bool	rot_speed_limit;
        float	rot_target_speed;
        bool	is_moving;
        float	explode_time;

        void reset()
        {
            move_speed = 0.0f;
            move_distance = 0.0f;
            pos = 0.0f;
            rot_angle = 0.0f;
            rot_speed = 0.0f;
            rot_accel = 0.0f;
            angle = 0.0f;
            rot_limit = true;
            rot_speed_limit = false;
            rot_target_speed = 0.0f;
            is_moving = false;
        }

        void reset_move() {move_speed = 0.0f;}

        void reset_rot()
        {
            rot_angle = 0.0f;
            rot_accel = 0.0f;
            rot_limit = true;
            rot_speed_limit = false;
            rot_target_speed = 0.0f;
        }
    };

    class ANIMATION_DATA
    {
    public:
        int         nb_piece;
        AXE	        *axe[3];			// 3 axes (dans l'ordre x,y,z)
        short       *flag;
        short       *explosion_flag;
        float       explode_time;
        bool        explode;
        Vector3D    *pos;
        Vector3D    *dir;			// Orientation des objets (quand il n'y a qu'une ligne)
        Matrix      *matrix;		// Store local matrixes
        bool        is_moving;


        inline ANIMATION_DATA() {init();}

        void init();

        void destroy();

        inline ~ANIMATION_DATA() {destroy();}

        void load(const int nb);

        void move(const float dt,const float g = 9.81f);
    };

    /*-----------------------------------------------------------------------------------*/

#define ROTATION				0x01
#define ROTATION_PERIODIC		0x02
#define ROTATION_COSINE			0x04		// Default calculation is linear
#define TRANSLATION				0x10
#define TRANSLATION_PERIODIC	0x20
#define TRANSLATION_COSINE		0x40

#define MESH_TYPE_TRIANGLES         GL_TRIANGLES
#define MESH_TYPE_TRIANGLE_STRIP    GL_TRIANGLE_STRIP

    class ANIMATION				// Class used to set default animation to a model, this animation will play if no ANIMATION_DATA is provided (ie for map features)
    {
    public:
        byte	type;
        Vector3D	angle_0;
        Vector3D	angle_1;
        float	angle_w;
        Vector3D	translate_0;
        Vector3D	translate_1;
        float	translate_w;

        ANIMATION()
            :type(0), angle_w(0.), translate_w(0.)
        {}

        void animate( float &t, Vector3D &R, Vector3D& T);
    };

    class MESH                          // The basic mesh class
    {
    protected:
        short       nb_vtx;				// Nombre de points
        short       nb_prim;			// Nombre de primitives
        String      name;				// Nom de l'objet / Object name
        MESH        *next;				// Objet suivant / Next object
        MESH        *child;				// Objet fils / Child object
        Vector3D    *points;			// Points composant l'objet / Vertices
        short       nb_p_index;			// Nombre d'indices de points
        short       nb_l_index;			// Nombre d'indices de lignes
        short       nb_t_index;			// Nombre d'indices de triangles
        Vector3D    pos_from_parent;	// Position par rapport à l'objet parent
        GLushort    *p_index;			// Tableau d'indices pour les points isolés
        GLushort    *l_index;			// Tableau d'indices pour les lignes
        GLushort    *t_index;			// Tableau d'indices pour les triangles
        short       *nb_index;			// Nombre d'indices par primitive
        Vector3D    *N;					// Tableau de normales pour les sommet
        Vector3D    *F_N;				// face normals
        std::vector<GLuint> gltex;      // Texture pour le dessin de l'objet
        String::Vector  tex_cache_name; // Used for on-the-fly loading
        float       *tcoord;			// Tableau de coordonnées de texture
        GLushort    sel[4];				// Primitive de sélection
        sint16      script_index;		// Indice donné par le script associé à l'unité
        bool        emitter;			// This object can or has sub-objects which can emit particles
        bool        emitter_point;		// This object directly emits particles
        std::vector<GLuint> gl_dlist;   // Display lists to speed up the drawing process

        ANIMATION   *animation_data;

        sint16      selprim;			// Polygone de selection

    protected:

        GLushort    *shadow_index;		// Pour la géométrie du volume d'ombre
        short   *t_line;				// Repère les arêtes
        short   *line_v_idx[2];
        short   nb_line;
        byte    *line_on;
        byte    *face_reverse;
        GLuint  type;                   // Tell which type of geometric data we have here (triangles, quads, triangle strips)
        Vector3D    last_dir;			// To speed up things when shadow has already been cast
        uint16  last_nb_idx;			// Remember how many things we have drawn last time

        float   min_x, max_x;		// Used by hit_fast
        float   min_y, max_y;
        float   min_z, max_z;
        bool    compute_min_max;

        uint16  obj_id;				// Used to generate a random position on the object

        bool    fixed_textures;
    public:
        uint16  nb_sub_obj;

    public:

        void check_textures();
        void load_texture_id(int id);

        uint16 set_obj_id( uint16 id );

        void compute_coord(ANIMATION_DATA *data_s = NULL,
                           Vector3D *pos = NULL,
                           bool c_part = false,
                           int p_tex = 0,
                           Vector3D *target = NULL,
                           Vector3D *upos = NULL,
                           Matrix *M = NULL,
                           float size = 0.0f,
                           Vector3D *center = NULL,
                           bool reverse = false,
                           MESH *src = NULL,
                           ANIMATION_DATA *src_data = NULL);

        virtual bool draw(float t, ANIMATION_DATA *data_s = NULL, bool sel_primitive = false, bool alset = false, bool notex = false, int side = 0, bool chg_col = true, bool exploding_parts = false) = 0;
        virtual bool draw_nodl(bool alset = false) = 0;

        bool draw_shadow(Vector3D Dir, float t, ANIMATION_DATA *data_s = NULL, bool alset = false, bool exploding_parts = false);

        bool draw_shadow_basic(Vector3D Dir, float t, ANIMATION_DATA *data_s = NULL, bool alset = false, bool exploding_parts = false);

        int hit(Vector3D Pos, Vector3D Dir, ANIMATION_DATA *data_s, Vector3D *I, Matrix M);

        bool hit_fast(Vector3D Pos, Vector3D Dir, ANIMATION_DATA *data_s, Vector3D *I);

        int random_pos( ANIMATION_DATA *data_s, int id, Vector3D *vec );

        bool compute_emitter();

        bool compute_emitter_point(int &obj_idx);

        virtual ~MESH() { }

        void destroy();

        void init();

        void Identify(ScriptData *script);			// Identifie les pièces utilisées par le script

        void compute_center(Vector3D *center,Vector3D dec, int *coef);		// Calcule les coordonnées du centre de l'objet, objets liés compris

        float compute_size_sq(Vector3D center);		// Carré de la taille(on fera une racine après)

        float print_struct(const float Y, const float X, TA3D::Font *fnt);

        float compute_top( float top, Vector3D dec );

        float compute_bottom( float bottom, Vector3D dec );

        bool has_animation_data();
    };

    class MODEL					// Classe pour la gestion des modèles 3D
    {
    public:
        MODEL() {init();}
        ~MODEL() {destroy();}

        void init();
        void destroy();


        /*!
        ** \brief
        */
        void compute_topbottom();

        /*!
        ** \brief
        */
        void create_from_2d(SDL_Surface* bmp, float w, float h, float max_h);

        /*!
        ** \brief
        */
        void draw(float t, ANIMATION_DATA* data_s = NULL, bool sel = false, bool notex = false,
                  bool c_part = false, int p_tex = 0, Vector3D *target = NULL, Vector3D* upos = NULL,
                  Matrix* M = NULL, float Size = 0.0f, Vector3D* Center = NULL, bool reverse = false,
                  int side = 0, bool chg_col = true, MESH* src = NULL, ANIMATION_DATA* src_data = NULL);

        /*!
        ** \brief
        */
        void compute_coord(ANIMATION_DATA* data_s = NULL, Matrix* M = NULL);

        /*!
        ** \brief
        */
        void draw_shadow(const Vector3D& Dir, float t,ANIMATION_DATA* data_s = NULL);

        /*!
        ** \brief
        */
        void draw_shadow_basic(const Vector3D& Dir, float t, ANIMATION_DATA *data_s = NULL);

        /*!
        ** \brief
        */
        int hit(Vector3D &Pos, Vector3D &Dir, ANIMATION_DATA* data_s, Vector3D* I, Matrix& M)
        { return mesh->hit(Pos,Dir,data_s,I,M); }

        /*!
        ** \brief
        */
        bool hit_fast(Vector3D& Pos, Vector3D& Dir, ANIMATION_DATA* data_s, Vector3D* I, Matrix& M)
        { return mesh->hit_fast(Pos,Dir*M,data_s,I); }

        /*!
        ** \brief
        */
        void Identify(ScriptData *script)
        { mesh->Identify(script); }

        /*!
        ** \brief
        */
        void print_struct(const float Y, const float X, TA3D::Font *fnt)
        { mesh->print_struct(Y, X, fnt); }

        /*!
        ** \brief
        */
        void check_textures()
        { mesh->check_textures(); }

    public:
        void postLoadComputations();

    public:
        MESH		*mesh;			// Objet principal du modèle 3D
        Vector3D	center;			// Centre de l'objet pour des calculs d'élimination d'objets
        float	size;				// Square of the size of the sphere which contains the model
        float	size2;				// Same as above but it is its square root
        float	top;				// Max y coordinate found in the model
        float	bottom;				// Min y coordinate found in the model
        uint32	id;					// ID of the model in the model array

        GLuint	dlist;				// Display list to speed up drawings operations when no position data is given (trees, ...)
        bool	animated;
        bool	from_2d;
        uint16	nb_obj;

    public:
        static MODEL *load(const String &filename);

    }; // class MODEL



    class MODEL_MANAGER	// Classe pour la gestion des modèles 3D du jeu
    {
    public:
        //! \name Constructor & Destructor
        //@{
        //! Default Constructor
        inline MODEL_MANAGER() :nb_models(0), model(NULL) {}
        //! Destructor
        ~MODEL_MANAGER();
        //@}

        void init();
        void destroy();

        MODEL *get_model(const String& name);

        /*!
        ** \brief
        */
        int load_all(void (*progress)(float percent,const String &msg)=NULL);

        /*!
        ** \brief
        */
        void compute_ids();

        /*!
        ** \brief
        */
        void create_from_2d(SDL_Surface *bmp, float w, float h, float max_h, const String& filename);

    public:
        int	 nb_models;     // Nombre de modèles
        std::vector<MODEL*> model;       // Tableau de modèles
        String::Vector	name; // Tableau contenant les noms des modèles

    private:
        //! hashtable used to speed up operations on MODEL objects
        cHashTable<int> model_hashtable;
    }; // class MODEL_MANAGER


    extern MODEL_MANAGER	model_manager;
} // namespace TA3D

#endif