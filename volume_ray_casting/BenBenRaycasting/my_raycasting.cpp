
// --------------------------------------------------------------------------
// GPU raycasting tutorial
// Made by Peter Trier jan 2007
//
// This file contains all the elements nessesary to implement a simple 
// GPU volume raycaster.
// Notice this implementation requires a shader model 3.0 gfxcard
// --------------------------------------------------------------------------

#include <windows.h>
#include <GL/glew.h>
//#include <Cg/cg.h>
//#include <Cg/cgGL.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <ctime>
#include <cassert>
#include <string>
#include "Vector3.h"
//////////////////////////////////////////////////////////////////////////
#include <memory>
#include "textfile.h"
#include "color.h"
//#include "reader.h"
#include "volume.h"
#include "k_means.h"
// NVIDIA OpenGL SDK
#include <nvGlutManipulators.h>
#include <nvGlutWidgets.h>
#include <nvVector.h>

//////////////////////////////////////////////////////////////////////////

#define MAX_KEYS 256
#define WINDOW_SIZE 700
#define VOLUME_TEX_SIZE 128

using namespace std;

// Globals ------------------------------------------------------------------
const double e = 2.7182818284590452353602874713526624977572470936999595749669676277240766303535;
const double pi = 3.1415926535;
bool gKeys[MAX_KEYS];
int width = WINDOW_SIZE, height = WINDOW_SIZE;
bool full_screen = false;
double a = 0.5, b = 0.6;
double epsilon = 10;
double max_distance;
float luminance = 10;
int low_hue = 0, high_hue =360;
//bool toggle_visuals = true;
//////////////////////////////////////////////////////////////////////////
short toggle_visuals = 0;
//////////////////////////////////////////////////////////////////////////
//CGcontext context; 
//CGprofile vertexProfile, fragmentProfile; 
//CGparameter param1,param2;
GLuint renderbuffer; 
GLuint framebuffer; 
GLuint loc_luminance;
//CGprogram vertex_main,fragment_main; // the raycasting shader programs
GLuint volume_texture; // the volume texture
GLuint transfer_texture; //transfer function texture
GLuint volume2;
GLuint backface_buffer; // the FBO buffers
GLuint final_image;
float stepsize = 1.0/1000.0;
//////////////////////////////////////////////////////////////////////////
GLuint frontface_buffer; // the FBO buffers
GLuint v,f,p;//,f2 // the OpenGL shaders
GLuint loc_stepsize;
volume Volume; 
//////////////////////////////////////////////////////////////////////////

nv::GlutExamine manipulator;
nv::GlutUIContext ui;

char file1[] = "E:\\BenBenRaycasting\\BenBenRaycasting\\data\\Vismale.dat";
char file2[] = "E:\\BenBenRaycasting\\BenBenRaycasting\\data\\Vismale.raw";
char * lable;

void NormalTest(void);

inline void updateButtonState( const nv::ButtonState &bs, nv::GlutManipulator &manip, int button)
{
	int modMask = 0;

	if (bs.state & nv::ButtonFlags_Alt) modMask |= GLUT_ACTIVE_ALT;
	if (bs.state & nv::ButtonFlags_Shift) modMask |= GLUT_ACTIVE_SHIFT;
	if (bs.state & nv::ButtonFlags_Ctrl) modMask |= GLUT_ACTIVE_CTRL;

	if (bs.state & nv::ButtonFlags_End)
		manip.mouse( button, GLUT_UP, modMask, bs.cursor.x, height - bs.cursor.y);
	if (bs.state & nv::ButtonFlags_Begin)
		manip.mouse( button, GLUT_DOWN, modMask, bs.cursor.x, height - bs.cursor.y);
}

void doUI()
{
	nv::Rect null;
	
	ui.begin();

	//ui.beginGroup( nv::GroupFlags_GrowDownFromLeft);

	//ui.doLabel( null, "Cg Geometry Program");
	//if (ui.doComboBox( null, NPROGS, programs, &current_prog_idx)) {
	//	current_prog = &prog[current_prog_idx];
	//}
	//ui.doCheckButton( null, "Wireframe", &options[OPTION_DISPLAY_WIREFRAME]);
	//ui.doCheckButton( null, "Use Model", &options[OPTION_DRAW_MODEL]);

	//ui.doLabel( null, "Num Curve Segments");

	//ui.beginGroup( nv::GroupFlags_GrowLeftFromTop);
	//ui.doLabel( null, " ");
	//float fsegments = segments;
	//ui.doHorizontalSlider( null, 2.0f, 100.0f, &fsegments);
	//segments = fsegments;
	//ui.endGroup();

	//ui.endGroup();
	nv::Rect full_slider(-5,0,800,0);
	ui.doHorizontalSlider(full_slider, 0.00001, 100, &luminance);
//	sprintf(str, "Luminance: %f", luminance);

	// Pass non-ui mouse events to the manipulator
	if (!ui.isOnFocus()) {
		const nv::ButtonState &lbState = ui.getMouseState( 0);
		const nv::ButtonState &mbState = ui.getMouseState( 1);
		const nv::ButtonState &rbState =  ui.getMouseState( 2);

		manipulator.motion( ui.getCursorX(), height - ui.getCursorY());

		updateButtonState( lbState, manipulator, GLUT_LEFT_BUTTON);
		updateButtonState( mbState, manipulator, GLUT_MIDDLE_BUTTON);
		updateButtonState( rbState, manipulator, GLUT_RIGHT_BUTTON);
	}

	ui.end();
}


typedef struct  
{	
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
}color_opacity;

int data_max, grad_max, df2_max, df3_max, dim_x, dim_y, dim_z;
color_opacity * tf;
/// Implementation ----------------------------------------

float norm(float min, float max, float x)  //convert data in range[min, max] to [0, 1]
{
	float result;
	if(fabs(max - min) < 1e-4)
		result = max;
	else
		result = (x - min) / (max - min);
	return result;
}

//////////////////////////////////////////////////////////////////////////
void printShaderInfoLog(GLuint obj)
{
	int infologLength = 0;
	int charsWritten  = 0;
	char *infoLog;

	glGetShaderiv(obj, GL_INFO_LOG_LENGTH,&infologLength);

	if (infologLength > 0)
	{
		infoLog = (char *)malloc(infologLength);
		glGetShaderInfoLog(obj, infologLength, &charsWritten, infoLog);
		printf("%s\n",infoLog);
		free(infoLog);
	}
}

void printProgramInfoLog(GLuint obj)
{
	int infologLength = 0;
	int charsWritten  = 0;
	char *infoLog;

	glGetProgramiv(obj, GL_INFO_LOG_LENGTH,&infologLength);

	if (infologLength > 0)
	{
		infoLog = (char *)malloc(infologLength);
		glGetProgramInfoLog(obj, infologLength, &charsWritten, infoLog);
		printf("%s\n",infoLog);
		free(infoLog);
	}
}

// Sets a uniform texture parameter
void setTextureUniform(GLuint program, const char* name, int number, GLenum target, GLuint texture) 
{
	GLuint location = glGetUniformLocation(program, name);
	glUniform1i(location, number);
	glActiveTexture(GL_TEXTURE0 + number);
	glBindTexture(target, texture);
}

void setShaders() {

	char *vs = NULL,*fs = NULL;// ,*fs2 = NULL;

	v = glCreateShader(GL_VERTEX_SHADER);
	f = glCreateShader(GL_FRAGMENT_SHADER);
	//f2 = glCreateShader(GL_FRAGMENT_SHADER);


	vs = textFileRead("simple_vertex.vert");
	//fs = textFileRead("simple_fragment.frag");
	fs = textFileRead("my_raycasting.frag");

	const char * vv = vs;
	const char * ff = fs;

	glShaderSource(v, 1, &vv,NULL);
	glShaderSource(f, 1, &ff,NULL);

	free(vs);free(fs);

	glCompileShader(v);
	glCompileShader(f);

	printShaderInfoLog(v);
	printShaderInfoLog(f);
	//printShaderInfoLog(f2);

	p = glCreateProgram();
	glAttachShader(p,v);
	glAttachShader(p,f);

	//Initial program setup.
	glLinkProgram(p); //Initial link

	glUseProgram(p);
	glUniform1f(loc_luminance, luminance);
	loc_stepsize = glGetUniformLocation(p, "stepsize");

	// set textures
	setTextureUniform(p, "front", 1, GL_TEXTURE_2D, frontface_buffer);
	setTextureUniform(p, "back", 2, GL_TEXTURE_2D, backface_buffer);
	setTextureUniform(p, "volume_texture", 3, GL_TEXTURE_3D, volume_texture);
	setTextureUniform(p, "transfer_texture", 4, GL_TEXTURE_3D, transfer_texture);
	setTextureUniform(p, "volume2", 5, GL_TEXTURE_3D, volume2);
	loc_luminance = glGetUniformLocation(p, "luminance");
	// restore active texture unit to GL_TEXTURE0
	glActiveTexture(GL_TEXTURE0);
	// disable the shader program
	glUseProgram(0);
}

//////////////////////////////////////////////////////////////////////////

//void cgErrorCallback()
//{
//	CGerror lastError = cgGetError(); 
//	if(lastError)
//	{
//		cout << cgGetErrorString(lastError) << endl;
//		if(context != NULL)
//			cout << cgGetLastListing(context) << endl;
//		exit(0);
//	}
//}

//// Sets a uniform texture parameter
//void set_tex_param(char* par, GLuint tex,const CGprogram &program,CGparameter param) 
//{
//	param = cgGetNamedParameter(program, par); 
//	cgGLSetTextureParameter(param, tex); 
//	cgGLEnableTextureParameter(param);
//}

//// load_vertex_program: loading a vertex program
//void load_vertex_program(CGprogram &v_program,char *shader_path, char *program_name)
//{
//	assert(cgIsContext(context));
//	v_program = cgCreateProgramFromFile(context, CG_SOURCE,shader_path,
//		vertexProfile,program_name, NULL);	
//	if (!cgIsProgramCompiled(v_program))
//		cgCompileProgram(v_program);
//
//	cgGLEnableProfile(vertexProfile);
//	cgGLLoadProgram(v_program);
//	cgGLDisableProfile(vertexProfile);
//}

//// load_fragment_program: loading a fragment program
//void load_fragment_program(CGprogram &f_program,char *shader_path, char *program_name)
//{
//	assert(cgIsContext(context));
//	f_program = cgCreateProgramFromFile(context, CG_SOURCE, shader_path,
//		fragmentProfile,program_name, NULL);	
//	if (!cgIsProgramCompiled(f_program))
//		cgCompileProgram(f_program);
//
//	cgGLEnableProfile(fragmentProfile);
//	cgGLLoadProgram(f_program);
//	cgGLDisableProfile(fragmentProfile);
//}

void enable_renderbuffers()
{
	glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, framebuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, renderbuffer);
}

void disable_renderbuffers()
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

//////////////////////////////////////////////////////////////////////////
int face_index = 0;
//////////////////////////////////////////////////////////////////////////
void vertex(float x, float y, float z)
{
	//////////////////////////////////////////////////////////////////////////
	float s, t;
	switch(face_index)
	{
	case 1:
		s = x;
		t = z;
		break;
	case 2:
		s = x;
		t = y;
		break;
	default:
		s = y;
		t = z;
	}
	glMultiTexCoord2f(GL_TEXTURE0, s, t);
	//////////////////////////////////////////////////////////////////////////
	glColor3f(x,y,z);
	glMultiTexCoord3f(GL_TEXTURE1, x, y, z);
	glVertex3f(x,y,z);
}
// this method is used to draw the front and backside of the volume
void drawQuads(float x, float y, float z)
{

	glBegin(GL_QUADS);
	/* Back side */
	//////////////////////////////////////////////////////////////////////////
	face_index = 2;
	//////////////////////////////////////////////////////////////////////////
	glNormal3f(0.0, 0.0, -1.0);
	vertex(0.0, 0.0, 0.0);
	vertex(0.0, y, 0.0);
	vertex(x, y, 0.0);
	vertex(x, 0.0, 0.0);

	/* Front side */
	//////////////////////////////////////////////////////////////////////////
	face_index = 2;
	//////////////////////////////////////////////////////////////////////////
	glNormal3f(0.0, 0.0, 1.0);
	vertex(0.0, 0.0, z);
	vertex(x, 0.0, z);
	vertex(x, y, z);
	vertex(0.0, y, z);

	/* Top side */
	//////////////////////////////////////////////////////////////////////////
	face_index = 1;
	//////////////////////////////////////////////////////////////////////////
	glNormal3f(0.0, 1.0, 0.0);
	vertex(0.0, y, 0.0);
	vertex(0.0, y, z);
	vertex(x, y, z);
	vertex(x, y, 0.0);

	/* Bottom side */
	//////////////////////////////////////////////////////////////////////////
	face_index = 1;
	//////////////////////////////////////////////////////////////////////////
	glNormal3f(0.0, -1.0, 0.0);
	vertex(0.0, 0.0, 0.0);
	vertex(x, 0.0, 0.0);
	vertex(x, 0.0, z);
	vertex(0.0, 0.0, z);

	/* Left side */
	//////////////////////////////////////////////////////////////////////////
	face_index = 0;
	//////////////////////////////////////////////////////////////////////////
	glNormal3f(-1.0, 0.0, 0.0);
	vertex(0.0, 0.0, 0.0);
	vertex(0.0, 0.0, z);
	vertex(0.0, y, z);
	vertex(0.0, y, 0.0);

	/* Right side */
	//////////////////////////////////////////////////////////////////////////
	face_index = 0;
	//////////////////////////////////////////////////////////////////////////
	glNormal3f(1.0, 0.0, 0.0);
	vertex(x, 0.0, 0.0);
	vertex(x, y, 0.0);
	vertex(x, y, z);
	vertex(x, 0.0, z);
	glEnd();

}

// ok let's start things up 

/*void readVolumeFile(char* filename) 
{
	int sizes[3];
	float dists[3];
	std::tr1::shared_ptr<void *> data_ptr(new void *);
	DataType type;
	int numComponents;

	readData(filename, sizes, dists, data_ptr.get(), &type, &numComponents);

	GLenum glType;
	switch (type)
	{
	case DATRAW_UCHAR:
		glType = GL_UNSIGNED_BYTE;
		break;
	case DATRAW_USHORT:
		glType = GL_UNSIGNED_SHORT;
		break;
	default:
		char s[300] = "Unsupported data type in ";
		strcat(s, filename);
		throw std::exception(s);
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glGenTextures(1, &volume_texture);
	glBindTexture(GL_TEXTURE_3D, volume_texture);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
	glTexImage3D(GL_TEXTURE_3D, 0, numComponents, sizes[0], sizes[1], sizes[2], 0, GL_LUMINANCE, glType, *data_ptr.get());

	free(*data_ptr.get());
	cout << "volume texture created from " << filename << endl;
}
*/



void create_volume_texture()
{
	GLenum glType;
	if(strcmp(Volume.getFormat(), "UCHAR") == 0)
		glType = GL_UNSIGNED_BYTE;
	else if(strcmp(Volume.getFormat(), "USHORT") ==0)
		glType = GL_UNSIGNED_SHORT;
	else
		cout<<"Invalid data type"<<endl;
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glGenTextures(1, &volume_texture);
	glBindTexture(GL_TEXTURE_3D, volume_texture);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, dim_x, dim_y, dim_z, 0, GL_LUMINANCE, glType, Volume.getDataAddr());
}
void create_volumetexture()
{
	int size = VOLUME_TEX_SIZE*VOLUME_TEX_SIZE*VOLUME_TEX_SIZE* 4;
	GLubyte *data = new GLubyte[size];

	for(int x = 0; x < VOLUME_TEX_SIZE; x++)
	{for(int y = 0; y < VOLUME_TEX_SIZE; y++)
	{for(int z = 0; z < VOLUME_TEX_SIZE; z++)
	{
		data[(x*4)   + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = z%250;
		data[(x*4)+1 + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = y%250;
		data[(x*4)+2 + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = 250;
		data[(x*4)+3 + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = 230;

		nv::vec3f p =	nv::vec3f(x,y,z)- nv::vec3f(VOLUME_TEX_SIZE-20,VOLUME_TEX_SIZE-30,VOLUME_TEX_SIZE-30);
		bool test = (length(p) < 42);
		if(test)
			data[(x*4)+3 + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = 0;

		p =	nv::vec3f(x,y,z)- nv::vec3f(VOLUME_TEX_SIZE/2,VOLUME_TEX_SIZE/2,VOLUME_TEX_SIZE/2);
		test = (length(p) < 24);
		if(test)
			data[(x*4)+3 + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = 0;


		if(x > 20 && x < 40 && y > 0 && y < VOLUME_TEX_SIZE && z > 10 &&  z < 50)
		{

			data[(x*4)   + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = 100;
			data[(x*4)+1 + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = 250;
			data[(x*4)+2 + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = y%100;
			data[(x*4)+3 + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = 250;
		}

		if(x > 50 && x < 70 && y > 0 && y < VOLUME_TEX_SIZE && z > 10 &&  z < 50)
		{

			data[(x*4)   + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = 250;
			data[(x*4)+1 + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = 250;
			data[(x*4)+2 + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = y%100;
			data[(x*4)+3 + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = 250;
		}

		if(x > 80 && x < 100 && y > 0 && y < VOLUME_TEX_SIZE && z > 10 &&  z < 50)
		{

			data[(x*4)   + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = 250;
			data[(x*4)+1 + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = 70;
			data[(x*4)+2 + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = y%100;
			data[(x*4)+3 + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = 250;
		}

		p =	nv::vec3f(x,y,z)- nv::vec3f(24,24,24);
		test = (length(p) < 40);
		if(test)
			data[(x*4)+3 + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4)] = 0;
	}}}

	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glGenTextures(1, &volume2);
	glBindTexture(GL_TEXTURE_3D, volume2);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
	glTexImage3D(GL_TEXTURE_3D, 0,GL_RGBA, VOLUME_TEX_SIZE, VOLUME_TEX_SIZE,VOLUME_TEX_SIZE,0, GL_RGBA, GL_UNSIGNED_BYTE,data);

	delete []data;
	cout << "volume texture created" << endl;
}

void setTransferfunc(void)
{
	int x, y, z, index;
	float temp1, temp2,temp3, temp4;
	double d, g, df2, a, alpha, elasity, a1, a2, opacity, k = 0.1, f1, f2;
	float center_x, center_y, center_z;
	double df_dx, df_dy, df_dz, gradient, df, Ra;
	float range, q;
	float H, S, L;
	dim_x = Volume.getX();
	dim_y = Volume.getY();
	dim_z = Volume.getZ();
	tf = (color_opacity *)malloc(sizeof(color_opacity) * dim_x * dim_y * dim_z);
	if(tf == NULL)
	{
		fprintf(stderr, "Not enough space for tf");
	}
	center_x = float(Volume.getX()) / 2.0; 
	center_y = float(Volume.getY()) / 2.0;
	center_z = float(Volume.getZ()) / 2.0;
	for(z = 0; z < dim_z; ++z)
		for(y = 0;y < dim_y; ++y)
			for(x = 0; x < dim_x; ++x)
			{
				index = Volume.getIndex(x, y, z);
				d = 1 / 3.0 * (dim_x + dim_y + dim_z);				
				range = Volume.getRange();
				H = double(Volume.getData(x ,y ,z)) / double(range) * 360.0; 

			//	S = 1 - pow(e , -1.0 * d *double(Volume.getData(x, y, z)));
			//	S = exp()
				S = norm(Volume.getMinData(), Volume.getMaxData(), Volume.getData(x, y, z));
				L = norm(Volume.getMinGrad(), Volume.getMaxGrad(), Volume.getGrad(x, y, z));
	//			L = double(x) + double(y) + double(z) / (3 * d);
		
				
				HSL2RGB(H, S, L, &temp1, &temp2, &temp3);
				temp1 = sqrt(temp1);
				temp2 = sqrt(temp2);
				temp3 = sqrt(temp3);
				tf[index].r  = (unsigned char)(temp1 * 255);
			
				tf[index].g = (unsigned char)(temp2 * 255);
				tf[index].b = (unsigned char)(temp3 * 255);

				
				
				elasity = Volume.getEp(x, y, z);
				gradient = Volume.getGrad(x, y, z);
				q = log(d);
			if(gradient < 20 ||  Volume.getDf3(x ,y , z) < 10 || Volume.getDf2(x, y, z) < 10)
					opacity = 0;
			
				else 
				{
				    Ra = - double(Volume.getDf2(x, y, z)) / double(Volume.getGrad(x, y, z));
				
			//		opacity = 1 - pow(e , -1.0 *  log(d)  * double(Volume.getMaxGrad() ) / gradient);
					opacity = 1 - pow(e, -1.0  * Ra);
					opacity = (exp(-1.0 * k * (1 - opacity)) - exp(-1.0 * q)) / (1 - exp(-1.0 * q));
			//		opacity = sqrt(opacity);
				opacity = sqrt(opacity);
				//	opdacity = sqrt(pow(x - center_x, 2.0) + pow())
				}	
				
				tf[index].a = unsigned char(opacity * 255);
			}
		
}


void setTransferfunc2(void)
{
int x, y, z, index, i, j;
float temp1, temp2,temp3, temp4;
double d, g, df2, a, alpha;
float range;
float max_alpha = 0.5;
float p, q, r, value, dF1;
float H, S, L;
bool b1;
bool b2;
int t1, t2;
float df1, df1_max, f, f_max, df2_max;
float sigma = 4;



dim_x = Volume.getX();
dim_y = Volume.getY();
dim_z = Volume.getZ();
tf = (color_opacity *)malloc(sizeof(color_opacity) * dim_x * dim_y * dim_z);
if(tf == NULL)
{
fprintf(stderr, "Not enough space for tf");
}
for(z = 0; z < dim_z; ++z)
for(y = 0;y < dim_y; ++y)
for(x = 0; x < dim_x; ++x)
{
index = Volume.getIndex(x, y, z);
/* H = (float(Volume.getData(x, y, z)) / float(Volume.getMaxData())) * 360.0; 
S = (float(Volume.getDf2(x, y, z)) / float(Volume.getMaxDf2()));
L = (float(Volume.getDf3(x, y, z)) / float(Volume.getMaxDf3()));*/
range = Volume.getRange();
if(Volume.getData(x, y, z) <= range / 6.0)
H = 30;
else if(Volume.getData(x, y, z) <= range * (1.0 / 3.0))
H = 90;
else if(Volume.getData(x, y, z) <= range * (1.0 / 2.0))
H = 150;
else if(Volume.getData(x, y, z) <= range * (2.0 / 3.0))
H = 210;
else if(Volume.getData(x, y, z) <= range * (5.0 / 6.0))
H = 270;
else
H = 330;


// H = Volume.getHistogram(Volume.getData(x, y, z)) / (Volume.getX() * Volume.getY() * Volume.getZ());
//S = 1;
S = norm(float(Volume.getMinGrad()), float(Volume.getMaxGrad()), float(Volume.getGrad(x, y, z))) * 360.0; 
//  H = norm(float(Volume.getMinDf2()), float(Volume.getMaxDf2()), float(Volume.getDf2(x, y, z)));
L = norm(float(Volume.getMinDf2()), float(Volume.getMaxDf2()), float(Volume.getDf2(x, y, z)));
// L = norm(float(Volume.getMinData()), float(Volume.getMaxData()), float(Volume.getData(x, y, z)));

// S = pow(double(1 / e), double(1.0) / double(Volume.getGrad(x, y, z)));*/

HSL2RGB(H, S, L, &temp1, &temp2, &temp3);
/*temp1 = (sqrt(sqrt(temp1)));
temp2 = (sqrt(sqrt(temp2)));
temp3 = (sqrt(sqrt(temp3)));*/
// cout<<temp1<<"\t"<<temp2<<"\t"<<temp3<<endl;
/* temp1 = pow(double(temp1), double(0.3333333));
temp2 = pow(double(temp2), double(0.3333333));
temp3 = pow(double(temp3), double(0.3333333));*/
temp1 *= 1.5;
temp2 *= 1.5;
temp3 *= 1.5;
if(temp1 > 1.0)
temp1 = 1.0;
if(temp2 > 1.0)
temp2 = 1.0;
if(temp3 > 1.0)
temp3 = 1.0;
tf[index].r  =  (unsigned char)(temp1 * 255);

tf[index].g = (unsigned char)(temp2 * 255);
tf[index].b =  (unsigned char)(temp3 * 255);


{
d = double(Volume.getData(x, y, z));
g = double(Volume.getGrad(x, y, z));
a = log(double(Volume.getX() + Volume.getY() + Volume.getZ()) / 3.0); 
temp4 = exp(-d / g);

df1 = (float)Volume.getGrad(x, y, z);
df1_max = (float)Volume.getMaxGrad();
f = (float)Volume.getData(x, y, z);
f_max = Volume.getMaxData();
df2 = double(Volume.getDf2(x,y ,z));
df2_max = Volume.getMaxDf2();
//temp4 = 1.6 *(df1) / df1_max *  f / f_max;
//temp4 = exp(df2 / df2_max * df1 / df1_max);
temp4 =	exp(- d / g);
alpha = temp4;
alpha = 1.0 + 1 / a * log((1.0 - pow(e, -a)) * temp4 + pow(e, -a)) / log(e);
//alpha = (exp(-a * (1.0 - temp4)) - exp(-a)) / (1 - exp(-a));

float ddd = sqrt(x / float(Volume.getX()) * x / (float)Volume.getX()
	                                                                  + y / float(Volume.getY() * y / float(Volume.getY()))
																	  + z / float(Volume.getZ() * z / float(Volume.getZ())));
//alpha =   (df1) / df1_max *  f / f_max;


	tf[index].a =  unsigned char(alpha * 255);
}



}

for(i = 1;i <= 11; ++i)
		for(j = 1;j <= 11;++j)
		{			
				for(x = 0; x < dim_x;++x)
					for(y = 0; y < dim_y; ++y)
						for(z = 0; z < dim_z; ++z)
						{
							value = float(Volume.getData(x, y, z));
							dF1 = float(Volume.getGrad(x, y, z));
							value /= float(Volume.getMaxData());
							dF1 /= float(Volume.getMaxGrad());
							t1 = int(value * 10);
							t2 = int(df1 * 10);
							if((Volume.getIntensity_gradient_histogram(i, j) < 100) && 
								(t1 >= i - 1) &&(t1 < i) && (t2 >= j - 1) && (t2 < j))
								tf[Volume.getIndex(x, y, z)].a = 0;
						}
		}
}

void setTransferFunc3()
{
	int x, y, z, index, i,j ;
float temp1, temp2,temp3, temp4;
double d, g, df2, a, alpha;
float range;
float max_alpha = 0.5;
float p, q, r, value, df1;
int t1, t2;
float H, S, L;
bool b1;
bool b2;
float dF1, df1_max, f, f_max, df2_max;
float sigma = 4;



dim_x = Volume.getX();
dim_y = Volume.getY();
dim_z = Volume.getZ();
tf = (color_opacity *)malloc(sizeof(color_opacity) * dim_x * dim_y * dim_z);
if(tf == NULL)
{
fprintf(stderr, "Not enough space for tf");
}
for(z = 0; z < dim_z; ++z)
for(y = 0;y < dim_y; ++y)
for(x = 0; x < dim_x; ++x)
{
index = Volume.getIndex(x, y, z);

range = Volume.getRange();
if(Volume.getData(x, y, z) <= range / 6.0)
H = 30;
else if(Volume.getData(x, y, z) <= range * (1.0 / 3.0))
H = 90;
else if(Volume.getData(x, y, z) <= range * (1.0 / 2.0))
H = 150;
else if(Volume.getData(x, y, z) <= range * (2.0 / 3.0))
H = 210;
else if(Volume.getData(x, y, z) <= range * (5.0 / 6.0))
H = 270;
else
H = 330;

S = norm(float(Volume.getMinGrad()), float(Volume.getMaxGrad()), float(Volume.getGrad(x, y, z))) * 360.0; 

L = norm(float(Volume.getMinDf2()), float(Volume.getMaxDf2()), float(Volume.getDf2(x, y, z)));


HSL2RGB(H, S, L, &temp1, &temp2, &temp3);
temp1 *= 1.5;
temp2 *= 1.5;
temp3 *= 1.5;
if(temp1 > 1.0)
temp1 = 1.0;
if(temp2 > 1.0)
temp2 = 1.0;
if(temp3 > 1.0)
temp3 = 1.0;

tf[index].r  =  (unsigned char)(temp1 * 255);

tf[index].g = (unsigned char)(temp2 * 255);
tf[index].b =  (unsigned char)(temp3 * 255);

d = double(Volume.getData(x, y, z));
g = double(Volume.getGrad(x, y, z));
a = log(double(Volume.getX() + Volume.getY() + Volume.getZ()) / 3.0); 
temp4 = exp(-d / g);

df1 = (float)Volume.getGrad(x, y, z);
df1_max = (float)Volume.getMaxGrad();
f = (float)Volume.getData(x, y, z);
f_max = Volume.getMaxData();
df2 = double(Volume.getDf2(x,y ,z));
df2_max = Volume.getMaxDf2();
//temp4 = 1.6 *(df1) / df1_max *  f / f_max;
//temp4 = exp(df2 / df2_max * df1 / df1_max);
temp4 =	exp(- d / g);
alpha = temp4;
//alpha = 1.0 + 1 / a * log((1.0 - pow(e, -a)) * temp4 + pow(e, -a)) / log(e);
//alpha = (exp(-a * (1.0 - temp4)) - exp(-a)) / (1 - exp(-a));
alpha = f / f_max * df1 / df1_max;

	tf[index].a =  unsigned char(alpha * 255);

	for(i = 1;i <= 11; ++i)
		for(j = 1;j <= 11;++j)
		{			
				for(x = 0; x < dim_x;++x)
					for(y = 0; y < dim_y; ++y)
						for(z = 0; z < dim_z; ++z)
						{
							value = float(Volume.getData(x, y, z));
							dF1 = float(Volume.getGrad(x, y, z));
							value /= float(Volume.getMaxData());
							dF1 /= float(Volume.getMaxGrad());
							t1 = int(value * 10);
							t2 = int(df1 * 10);
						//	if((Volume.getSpatialDistribution(i, j) > 50) && 
						//		(t1 >= i - 1) &&(t1 < i) && (t2 >= j - 1) && (t2 < j))
						//		tf[Volume.getIndex(x, y, z)].a = 0;
						}
		}


}

}
void create_transferfunc()
{
	setTransferfunc2();

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glGenTextures(1, &transfer_texture);
	glBindTexture(GL_TEXTURE_3D, transfer_texture);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
	glTexImage3D(GL_TEXTURE_3D, 0,GL_RGBA, dim_x, dim_y, dim_z, 0, GL_RGBA, GL_UNSIGNED_BYTE, tf);
}

void init()
{
	int x, y, z, index;
	cout << "glew init " << endl;
	GLenum err = glewInit();

	// initialize all the OpenGL extensions
	glewGetExtension("glMultiTexCoord2fvARB");  
	if(glewGetExtension("GL_EXT_framebuffer_object") )cout << "GL_EXT_framebuffer_object support " << endl;
	if(glewGetExtension("GL_EXT_renderbuffer_object"))cout << "GL_EXT_renderbuffer_object support " << endl;
	if(glewGetExtension("GL_ARB_vertex_buffer_object")) cout << "GL_ARB_vertex_buffer_object support" << endl;
	if(GL_ARB_multitexture)cout << "GL_ARB_multitexture support " << endl;

	if (glewGetExtension("GL_ARB_fragment_shader")      != GL_TRUE ||
		glewGetExtension("GL_ARB_vertex_shader")        != GL_TRUE ||
		glewGetExtension("GL_ARB_shader_objects")       != GL_TRUE ||
		glewGetExtension("GL_ARB_shading_language_100") != GL_TRUE)
	{
		cout << "Driver does not support OpenGL Shading Language" << endl;
		exit(1);
	}

	glEnable(GL_CULL_FACE);
//	glClearColor(0, 0, 0, 0);
	glClearColor(1, 1, 1, 1);
	glGenFramebuffersEXT(1, &framebuffer);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,framebuffer);

	//////////////////////////////////////////////////////////////////////////
	glGenTextures(1, &frontface_buffer);
	glBindTexture(GL_TEXTURE_2D, frontface_buffer);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA16F_ARB, WINDOW_SIZE, WINDOW_SIZE, 0, GL_RGBA, GL_FLOAT, NULL);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, frontface_buffer, 0);
	//////////////////////////////////////////////////////////////////////////

	glGenTextures(1, &backface_buffer);
	glBindTexture(GL_TEXTURE_2D, backface_buffer);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA16F_ARB, WINDOW_SIZE, WINDOW_SIZE, 0, GL_RGBA, GL_FLOAT, NULL);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, backface_buffer, 0);

	glGenTextures(1, &final_image);
	glBindTexture(GL_TEXTURE_2D, final_image);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA16F_ARB, WINDOW_SIZE, WINDOW_SIZE, 0, GL_RGBA, GL_FLOAT, NULL);

	glGenRenderbuffersEXT(1, &renderbuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, renderbuffer);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, WINDOW_SIZE, WINDOW_SIZE);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, renderbuffer);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	//////////////////////////////////////////////////////////////////////////
	// read volume data file
//	cout<<"Input file name:";
//	cin>>file;

	Volume.readVolFile(file1);
	cout<<"Data file name:"<<endl;
//	cin>>file;
	Volume.readData(file2);
	Volume.calHistogram();
//	Volume.calEp();
	Volume.calGrad();
	Volume.calDf2();
//	NormalTest();
//	Volume.calLH();
//	Volume.calDf3();
//	Volume.Intensity_gradient_histogram();
	Volume.statistics();
//	Volume.cluster();
//	Volume.getInfo();
	
	/*lable = (char *) malloc(sizeof(char) * Volume.getCount());
	if(lable == NULL)
		cout<<"Not enought space for lable"<<endl;
		memset(lable, '255', Volume.getCount());*/
//	k_means(&Volume, lable);
	create_volume_texture();
	create_transferfunc();
//	create_volumetexture();
	// init shaders
	setShaders();
//	//////////////////////////////////////////////////////////////////////////
}


// for contiunes keypresses
void ProcessKeys()
{
	// Process keys
	for (int i = 0; i < 256; i++)
	{
		if (!gKeys[i])  { continue; }
		switch (i)
		{
		case ' ':
			break;
		case 'w':
			stepsize += 1.0/2048.0;
			if(stepsize > 0.25) stepsize = 0.25;
			break;
		case 'e':
			stepsize -= 1.0/2048.0;
			if(stepsize <= 1.0/200.0) stepsize = 1.0/200.0;
			break;
		}
	}

}

void key(unsigned char k, int x, int y)
{
	gKeys[k] = true;
	if(k == 'a')
	{
		a += 0.5;
		 setTransferfunc();
		glutPostRedisplay();
	}	

	if(k == 's')
	{
		a -= 0.5;
		 setTransferfunc();
		glutPostRedisplay();
	}
	if(k == 'd')
	{
		b += 0.5;
		 setTransferfunc();
		glutPostRedisplay();
	}
	if(k == 'f')
	{
		b -= 0.5;
		 setTransferfunc();
		glutPostRedisplay();
	}
	if(k == 'H')
	{
		high_hue+=5;
		setTransferfunc();
		glutPostRedisplay();
	}
	if(k == 'h')
	{
		high_hue -=5;
		setTransferfunc();
		glutPostRedisplay();
	}
		if(k == 'L')
	{
		low_hue +=5;
		setTransferfunc();
		glutPostRedisplay();
	}
	if(k == 'l')
	{
		low_hue -=5;
		setTransferfunc();
		glutPostRedisplay();
	}

}

void KeyboardUpCallback(unsigned char key, int x, int y)
{
	gKeys[key] = false;

	switch (key)
	{
	case 27 :
		{
			exit(0); break; 
		}
	case ' ':
		//toggle_visuals = !toggle_visuals;
		//////////////////////////////////////////////////////////////////////////
		toggle_visuals = (toggle_visuals + 1) % 3;
		//////////////////////////////////////////////////////////////////////////
		break;
	}
}

// glut idle function
void idle_func()
{
	manipulator.idle();

	ProcessKeys();
	glutPostRedisplay();
}

void reshape_ortho(int w, int h)
{
	if (h == 0) h = 1;
	glViewport(0, 0,w,h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0, 1, 0, 1);
	glMatrixMode(GL_MODELVIEW);
}


void resize(int w, int h)
{
	if (h == 0) h = 1;
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, (GLfloat)w/(GLfloat)h, 0.01, 400.0);
	glMatrixMode(GL_MODELVIEW);

	manipulator.reshape(w, h);
}

void draw_fullscreen_quad()
{
	glDisable(GL_DEPTH_TEST);
	glBegin(GL_QUADS);

	glTexCoord2f(0,0); 
	glVertex2f(0,0);

	glTexCoord2f(1,0); 
	glVertex2f(1,0);

	glTexCoord2f(1, 1); 
	glVertex2f(1, 1);

	glTexCoord2f(0, 1); 
	glVertex2f(0, 1);

	glEnd();
	glEnable(GL_DEPTH_TEST);

}

// display the final image on the screen
void render_buffer_to_screen()
{
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glLoadIdentity();
	glEnable(GL_TEXTURE_2D);
	//if(toggle_visuals)
	//	glBindTexture(GL_TEXTURE_2D,final_image);
	//else
	//	glBindTexture(GL_TEXTURE_2D,backface_buffer);
	//////////////////////////////////////////////////////////////////////////
	switch(toggle_visuals)
	{
	case 1:
		glBindTexture(GL_TEXTURE_2D,backface_buffer);
		break;
	case 2:
		glBindTexture(GL_TEXTURE_2D,frontface_buffer);
		break;
	default:
		glBindTexture(GL_TEXTURE_2D,final_image);
	}
	//////////////////////////////////////////////////////////////////////////
	reshape_ortho(WINDOW_SIZE,WINDOW_SIZE);
	draw_fullscreen_quad();
	glDisable(GL_TEXTURE_2D);
}

//////////////////////////////////////////////////////////////////////////
// render the frontface to the offscreen buffer backface_buffer
void render_frontface()
{
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, frontface_buffer, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	drawQuads(1.0,1.0, 1.0);
	glDisable(GL_CULL_FACE);
}
//////////////////////////////////////////////////////////////////////////

// render the backface to the offscreen buffer backface_buffer
void render_backface()
{
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, backface_buffer, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	drawQuads(1.0,1.0, 1.0);
	glDisable(GL_CULL_FACE);
}

void raycasting_pass()
{
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, final_image, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	//cgGLEnableProfile(vertexProfile);
	//cgGLEnableProfile(fragmentProfile);
	//cgGLBindProgram(vertex_main);
	//cgGLBindProgram(fragment_main);
	//cgGLSetParameter1f( cgGetNamedParameter( fragment_main, "stepsize") , stepsize);
	//set_tex_param("tex",backface_buffer,fragment_main,param1);
	//set_tex_param("volume_tex",volume_texture,fragment_main,param2);
	//////////////////////////////////////////////////////////////////////////
	glUseProgram(p);
	glUniform1f(loc_stepsize, stepsize);
	//////////////////////////////////////////////////////////////////////////
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	drawQuads(1.0,1.0, 1.0);
	glDisable(GL_CULL_FACE);
	//cgGLDisableProfile(vertexProfile);
	//cgGLDisableProfile(fragmentProfile);
	//////////////////////////////////////////////////////////////////////////
	glUseProgram(0);
	//////////////////////////////////////////////////////////////////////////
}

// This display function is called once pr frame 
void display()
{
	static float rotate = 0; 
	rotate += 0.25;

	resize(WINDOW_SIZE,WINDOW_SIZE);
	enable_renderbuffers();

	glLoadIdentity();
	//glTranslatef(0,0,-2.25);
	//glRotatef(rotate,0,1,1);
	manipulator.applyTransform();
	glTranslatef(-0.5,-0.5,-0.5); // center the texturecube
	//////////////////////////////////////////////////////////////////////////
	render_frontface();
	//////////////////////////////////////////////////////////////////////////
	render_backface();
	raycasting_pass();
	disable_renderbuffers();
	render_buffer_to_screen();

	doUI();
	glutSwapBuffers();
}

void mouse(int button, int state, int x, int y)
{
	ui.mouse( button, state, glutGetModifiers(), x, y);
}

void motion(int x, int y)
{
	ui.mouseMotion( x, y);
}

void special_keys(int key, int x, int y)
{
	switch(key)
	{
	case GLUT_KEY_F1:
		full_screen = !full_screen;
		if(full_screen)
		{
			width = glutGet(GLUT_WINDOW_WIDTH);
			height = glutGet(GLUT_WINDOW_HEIGHT);
			glutFullScreen();
		}
		else
			glutReshapeWindow(width, height);
		break;
	default:
		break;
	}
}

void NormalTest()
{
	int x, y, z, i, j, k;
	int index, count = 0;
	int value[27], temp;
	float a[13] = { 0.4366, 0.3018, 0.2522, 0.2152, 0.1848, 0.1584, 0.1346,
	                       0.1128, 0.0923, 0.0728, 0.0540, 0.0358, 0.0178
	                      };
	float average, d1, d2, w;
	for(x = 1; x <= Volume.getX() - 1; ++x)
		for(y = 1;y <= Volume.getY() - 1; ++y)
			for(z = 1; z  <= Volume.getZ() - 1; ++z)
			{
				index = 1;
				for(i = x - 1; i <= x + 1; ++i)
					for(j = y - 1; j <= y +1; ++j)
						for(k = z - 1; k <= z + 1; ++k)
						{
							value[index] = Volume.getData(i, j, k);
							index++;
						}
				for(i = 0; i < 27; ++i)
					for(j = i + 1;j < 27; ++j)
					{
						if(value[j] < value[i])
						{
							temp = value[j];
							value[j] = value[i];
							value[i] = temp;
						}
					}
				average = 0;
				for(i = 0; i < 27;++i)
				{
					average += float(value[i]);
			//		cout<<value[i]<<"\t";
				}
					
				average /= 27;
				d1 = d2 = 0;
				for(i = 0;i < 13; ++i)
					d1 += a[i] * double(value[27 - i] - value[i]);
				d1 = d1 * d1;
				for(i = 0;i < 27;++i)
					d2 += float(value[i] - average) * float(value[i] - average);
				if(d2 == 0)
					w = 2;
				else
					w = d1 / d2;
		//		cout<<"w = "<<w<<endl;
				if(w - 0.894 < 0)     
					;// 0.923 0.935
		//			cout<<"��������̬�ֲ�"<<endl;
				else
				{
					count++;
		//			cout<<"������̬�ֲ�"<<endl<<endl<<endl;
				}
					
			}
			cout<<float(count) /  (float(Volume.getX()) * float(Volume.getY()) * float(Volume.getZ()));
}


int main(int argc, char* argv[])
{
	glutInit(&argc,argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutInitWindowPosition(50, 50);
//	glutInitWindowSize(700, 700);
	glutCreateWindow("BenBen Raycasgting");

	glutReshapeWindow(WINDOW_SIZE,WINDOW_SIZE);

	manipulator.setDollyActivate( GLUT_LEFT_BUTTON, GLUT_ACTIVE_CTRL);
	manipulator.setPanActivate( GLUT_LEFT_BUTTON, GLUT_ACTIVE_SHIFT);
	manipulator.setDollyPosition( -2.5f  );

	glutKeyboardFunc(key);
	glutKeyboardUpFunc(KeyboardUpCallback);

	glutDisplayFunc(display);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);
	glutIdleFunc(idle_func);
	glutPassiveMotionFunc(motion);
	glutReshapeFunc(resize);
	glutSpecialFunc(special_keys);
	resize(800,600);
	init();
	glutMainLoop();
	return 0;
}