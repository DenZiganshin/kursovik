#define _CRT_SECURE_NO_WARNINGS

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <gl\gl.h>
#pragma comment(lib,"ws2_32.lib")

HDC			hDC=NULL;		
HGLRC		hRC=NULL;		
HWND		hWnd=NULL;		
HINSTANCE	hInstance;	

#define CLEFT 0x1
#define CRIGHT 0x2
#define CUP 0x4
#define CDOWN 0x8

bool	keys[256];	

LRESULT	CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int width,height;
char* title;
SOCKET servSock;
SOCKET clientSock;
int SizeX,SizeY;
float QuadWidth;
bool ImServ = 0;
bool connected = 0; 
bool stopListen = 0;


void DrawGLScene(GLvoid);

void init(char *_title,int _width, int _height){
		width = _width;
		height = _height;
		title = _title;
	}

GLvoid ReSizeGLScene(GLsizei width, GLsizei height)		// Resize And Initialize The GL Window
{
	if (height==0)										// Prevent A Divide By Zero By
	{
		height=1;										// Making Height Equal One
	}

	glViewport(0,0,width,height);						// Reset The Current Viewport

	glMatrixMode(GL_PROJECTION);						// Select The Projection Matrix
	glLoadIdentity();									// Reset The Projection Matrix

	// Calculate The Aspect Ratio Of The Window
	glOrtho(0, width/2, height/2, 0, -10, 10); 

	glMatrixMode(GL_MODELVIEW);							// Select The Modelview Matrix
	glLoadIdentity();									// Reset The Modelview Matrix
}

int InitGL(GLvoid)										// All Setup For OpenGL Goes Here
{
	glShadeModel(GL_SMOOTH);							// Enable Smooth Shading
	glClearColor(0.0f, 255.0f, 0.0f, 0.5f);				
	glClearDepth(1.0f);									// Depth Buffer Setup
	glEnable(GL_DEPTH_TEST);							// Enables Depth Testing
	glDepthFunc(GL_LEQUAL);								// The Type Of Depth Testing To Do
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	return TRUE;										// Initialization Went OK
}

GLvoid KillGLWindow(GLvoid)								// Properly Kill The Window
{
	if (hRC)											// Do We Have A Rendering Context?
	{
		if (!wglMakeCurrent(NULL,NULL))					// Are We Able To Release The DC And RC Contexts?
		{
			MessageBox(NULL,"Release Of DC And RC Failed.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		}

		if (!wglDeleteContext(hRC))						// Are We Able To Delete The RC?
		{
			MessageBox(NULL,"Release Rendering Context Failed.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		}
		hRC=NULL;										// Set RC To NULL
	}

	if (hDC && !ReleaseDC(hWnd,hDC))					// Are We Able To Release The DC
	{
		MessageBox(NULL,"Release Device Context Failed.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		hDC=NULL;										// Set DC To NULL
	}

	if (hWnd && !DestroyWindow(hWnd))					// Are We Able To Destroy The Window?
	{
		MessageBox(NULL,"Could Not Release hWnd.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		hWnd=NULL;										// Set hWnd To NULL
	}

	if (!UnregisterClass("OpenGL",hInstance))			// Are We Able To Unregister Class
	{
		MessageBox(NULL,"Could Not Unregister Class.","SHUTDOWN ERROR",MB_OK | MB_ICONINFORMATION);
		hInstance=NULL;									// Set hInstance To NULL
	}
}

BOOL CreateGLWindow()
{
	GLuint		PixelFormat;			// Holds The Results After Searching For A Match
	WNDCLASS	wc;						// Windows Class Structure
	DWORD		dwExStyle;				// Window Extended Style
	DWORD		dwStyle;				// Window Style
	RECT		WindowRect;				// Grabs Rectangle Upper Left / Lower Right Values
	WindowRect.left=(long)0;			// Set Left Value To 0
	WindowRect.right=(long)width;		// Set Right Value To Requested Width
	WindowRect.top=(long)0;				// Set Top Value To 0
	WindowRect.bottom=(long)height;		// Set Bottom Value To Requested Height


	hInstance			= GetModuleHandle(NULL);				// Grab An Instance For Our Window
	wc.style			= CS_HREDRAW | CS_VREDRAW | CS_OWNDC;	// Redraw On Size, And Own DC For Window.
	wc.lpfnWndProc		= (WNDPROC) WndProc;					// WndProc Handles Messages
	wc.cbClsExtra		= 0;									// No Extra Window Data
	wc.cbWndExtra		= 0;									// No Extra Window Data
	wc.hInstance		= hInstance;							// Set The Instance
	wc.hIcon			= LoadIcon(NULL, IDI_WINLOGO);			// Load The Default Icon
	wc.hCursor			= LoadCursor(NULL, IDC_ARROW);			// Load The Arrow Pointer
	wc.hbrBackground	= NULL;									// No Background Required For GL
	wc.lpszMenuName		= NULL;									// We Don't Want A Menu
	wc.lpszClassName	= "OpenGL";								// Set The Class Name

	if (!RegisterClass(&wc))									// Attempt To Register The Window Class
	{
		MessageBox(NULL,"Failed To Register The Window Class.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;											// Return FALSE
	}
	

		dwExStyle=WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;			// Window Extended Style
		dwStyle=WS_OVERLAPPEDWINDOW;							// Windows Style
	

	AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);		// Adjust Window To True Requested Size

	// Create The Window
	if (!(hWnd=CreateWindowEx(	dwExStyle,							// Extended Style For The Window
								"OpenGL",							// Class Name
								title,								// Window Title
								dwStyle |							// Defined Window Style
								WS_CLIPSIBLINGS |					// Required Window Style
								WS_CLIPCHILDREN,					// Required Window Style
								0, 0,								// Window Position
								WindowRect.right-WindowRect.left,	// Calculate Window Width
								WindowRect.bottom-WindowRect.top,	// Calculate Window Height
								NULL,								// No Parent Window
								NULL,								// No Menu
								hInstance,							// Instance
								NULL)))								// Dont Pass Anything To WM_CREATE
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Window Creation Error.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;								// Return FALSE
	}

	static	PIXELFORMATDESCRIPTOR pfd=				// pfd Tells Windows How We Want Things To Be
	{
		sizeof(PIXELFORMATDESCRIPTOR),				// Size Of This Pixel Format Descriptor
		1,											// Version Number
		PFD_DRAW_TO_WINDOW |						// Format Must Support Window
		PFD_SUPPORT_OPENGL |						// Format Must Support OpenGL
		PFD_DOUBLEBUFFER,							// Must Support Double Buffering
		PFD_TYPE_RGBA,								// Request An RGBA Format
		16,										// Select Our Color Depth
		0, 0, 0, 0, 0, 0,							// Color Bits Ignored
		0,											// No Alpha Buffer
		0,											// Shift Bit Ignored
		0,											// No Accumulation Buffer
		0, 0, 0, 0,									// Accumulation Bits Ignored
		16,											// 16Bit Z-Buffer (Depth Buffer)  
		0,											// No Stencil Buffer
		0,											// No Auxiliary Buffer
		PFD_MAIN_PLANE,								// Main Drawing Layer
		0,											// Reserved
		0, 0, 0										// Layer Masks Ignored
	};
	
	if (!(hDC=GetDC(hWnd)))							// Did We Get A Device Context?
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Can't Create A GL Device Context.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;								// Return FALSE
	}

	if (!(PixelFormat=ChoosePixelFormat(hDC,&pfd)))	// Did Windows Find A Matching Pixel Format?
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Can't Find A Suitable PixelFormat.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;								// Return FALSE
	}

	if(!SetPixelFormat(hDC,PixelFormat,&pfd))		// Are We Able To Set The Pixel Format?
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Can't Set The PixelFormat.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;								// Return FALSE
	}

	if (!(hRC=wglCreateContext(hDC)))				// Are We Able To Get A Rendering Context?
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Can't Create A GL Rendering Context.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;								// Return FALSE
	}

	if(!wglMakeCurrent(hDC,hRC))					// Try To Activate The Rendering Context
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Can't Activate The GL Rendering Context.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;								// Return FALSE
	}

	ShowWindow(hWnd,SW_SHOW);						// Show The Window
	SetForegroundWindow(hWnd);						// Slightly Higher Priority
	SetFocus(hWnd);									// Sets Keyboard Focus To The Window
	ReSizeGLScene(width, height);					// Set Up Our Perspective GL Screen

	if (!InitGL())									// Initialize Our Newly Created GL Window
	{
		KillGLWindow();								// Reset The Display
		MessageBox(NULL,"Initialization Failed.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return FALSE;								// Return FALSE
	}

	return TRUE;									// Success
}

class movment;
class plate;
class bomb;
class tmr;
class hero;
class pauseGame;
class loadIni;
void elemInit();
DWORD WINAPI Accept(LPVOID lpParameter);
DWORD WINAPI ListenSock(LPVOID lpParameter);

class loadTex{
	unsigned long sizeX,sizeY;
	char *data;
	int LoadImg(char *name){
		FILE *f;
		unsigned long size;
		f = fopen(name,"rb");
		if(f == NULL)
			return 0;
		fread(&sizeX,4,1,f);
		fread(&sizeY,4,1,f);
		fread(&size,4,1,f);
		data = (char*)malloc(size);
		if(data == NULL)
			return 0;
		fread(data,size,1,f);
		return 1;
	}
public:
	void LoadGLTextures(char* name , GLuint *texture) {
		if(!LoadImg(name))
			exit(0);	
	    glGenTextures(1, texture);
	    glBindTexture(GL_TEXTURE_2D, texture[0]); 
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST); 
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, 4, sizeX, sizeY, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
		//free(data);
	}
}insTex;

class pauseGame{
	int stat;
	GLuint wait;
	GLuint win;
	GLuint loose;
public:
	friend tmr;
	friend hero;
	friend void restart(int WhoWin);
	friend void DrawGLScene(GLvoid);
	pauseGame(){
		stat = 1;
	}
	void setStat(int i){
		stat = i;
	}
	void setWaiting(){
		glBindTexture(GL_TEXTURE_2D, wait);
		glBegin(GL_QUADS);
		  glTexCoord2f(0.0f, 1.0f);		glVertex3f((float)width/8,					(float)height/8,					0.1f);		
		  glTexCoord2f(1.0f, 1.0f);		glVertex3f((float)width/8+(float)width/4,	(float)height/8,					0.1f);		
		  glTexCoord2f(1.0f, 0.0f);		glVertex3f((float)width/8+(float)width/4,	(float)height/8+(float)height/4,	0.1f);			
		  glTexCoord2f(0.0f, 0.0f);		glVertex3f((float)width/8,					(float)height/8+(float)height/4,	0.1f);		
		glEnd();
	}
	void setLoose(){
		glBindTexture(GL_TEXTURE_2D, loose);
		glBegin(GL_QUADS);
		  glTexCoord2f(0.0f, 1.0f);		glVertex3f((float)width/8,					(float)height/8,					0.1f);		
		  glTexCoord2f(1.0f, 1.0f);		glVertex3f((float)width/8+(float)width/4,	(float)height/8,					0.1f);		
		  glTexCoord2f(1.0f, 0.0f);		glVertex3f((float)width/8+(float)width/4,	(float)height/8+(float)height/4,	0.1f);			
		  glTexCoord2f(0.0f, 0.0f);		glVertex3f((float)width/8,					(float)height/8+(float)height/4,	0.1f);		
		glEnd();
	}
	void setWin(){
		glBindTexture(GL_TEXTURE_2D, win);
		glBegin(GL_QUADS);
		  glTexCoord2f(0.0f, 1.0f);		glVertex3f((float)width/8,					(float)height/8,					0.1f);		
		  glTexCoord2f(1.0f, 1.0f);		glVertex3f((float)width/8+(float)width/4,	(float)height/8,					0.1f);		
		  glTexCoord2f(1.0f, 0.0f);		glVertex3f((float)width/8+(float)width/4,	(float)height/8+(float)height/4,	0.1f);			
		  glTexCoord2f(0.0f, 0.0f);		glVertex3f((float)width/8,					(float)height/8+(float)height/4,	0.1f);		
		glEnd();
	}
	void control(){
		if(stat == 1)
			setWaiting();
		if(stat == 2)
			setLoose();
		if(stat == 3)
			setWin();
	}
	void init(char *w,char *l,char *p){
		insTex.LoadGLTextures(w,&win);
		insTex.LoadGLTextures(l,&loose);
		insTex.LoadGLTextures(p,&wait);
	}
}insPause;

class matr{
	bool newInit;
	unsigned char **data;
public:
	friend plate;
	friend movment;
	friend bomb;
	friend tmr;
	friend void restart(int WhoWin);
	matr(){
		newInit = 0;
		data = NULL;
	}
	void init(int x,int y){
		int i,j,r;
		if(newInit == 0){
			srand((unsigned)time(NULL));
			data = (unsigned char**)malloc(y);
			for(i=0;i<y;i++){
				data[i] = (unsigned char*)malloc(x);
			}
		}
		newInit = 1;
		for(i=0;i<y;i++)
			for(j=0;j<x;j++)
				data[i][j] = 0;
		for(i=0;i<x;i++){  //верх и низ
			data[0][i] = 1; 
			data[y-1][i]=1;
		}
		for(i=0;i<y;i++){ // лево и право
			data[i][0] = 1; 
			data[i][x-1]= 1;
		}
		for(i=0;i<y-1;i++) //шахматы
			for(j=0;j<x-1;j++)
				if((i%2 != 0) && (j%2 != 0))
					data[i+1][j+1] = 1;
		for(i=0;i<y-1;i++) //блоки
			for(j=0;j<x-1;j++){
				r = rand()%2;
				if((data[i+1][j+1] != 1) && (r==1)){
					data[i+1][j+1] = 2;
				}
			}
		data[1][2] = 0;
		data[1][1] = 0;
		data[2][1] = 0;
		data[3][1] = 0;
		data[y-2][x-3] = 0;
		data[y-2][x-2] = 0;
		data[y-3][x-2] = 0;
		data[y-4][x-2] = 0;
	}
	void getMatr(int x,int y,int serv){
		int px,py;
		int retVal;
		int i,j;
		char c[5];

		if(serv == 1){
			//init(x,y);
			
			c[0]=x;
			c[1]=y;
			retVal = send(clientSock,c,2,0);
			retVal = recv(clientSock,c,4,0);
			for(i=0;i<y;i++)
				for(j=0;j<x;j++){
					c[0] = data[i][j];
					retVal = send(clientSock,c,1,0);
				}
			//send(clientSock,data[i][j],sizeof(unsigned char),0);
		}else{
			retVal = recv(clientSock,c,2,0);
			px = c[0];
			py = c[1];
			if(newInit==0){
				data = (unsigned char**)malloc(py);
				for(i=0;i<py;i++){
					data[i] = (unsigned char*)malloc(px);
				}
			}
			newInit = 1;
			strcpy(c,"done");
			retVal = send(clientSock,c,4,0);
			for(i=0;i<py;i++)
				for(j=0;j<px;j++){
					retVal = recv(clientSock,c,1,0);
					data[i][j] = c[0];
				}
			SizeX = px;
			SizeY = py;
		}
	}
}insMatr;

class hero{
	//======hero=========
	int x,y; //клетки
	char drawX,drawY;
	char bombs;
	GLuint texture;
	//===enemy==========
	int ex,ey;
	char EdrawX,EdrawY;
	GLuint texEnem;
	//=================
	float plateX,plateY;
	float width;
	float step;
	float z;
	int stepsIn;
public:
	friend bomb;
	friend movment;
	friend tmr;
	friend loadIni;
	friend pauseGame;
	friend DWORD WINAPI ListenSock(LPVOID lpParameter);
	friend DWORD WINAPI Accept(LPVOID lpParameter);
	friend void restart(int WhoWin);
	hero(){
		x=0;			y=0;
		drawX = 0;		drawY = 0;
		width = 0.0f;	bombs = 0;
	}
	void draw(){
		glBindTexture(GL_TEXTURE_2D, texture);
		glBegin(GL_QUADS);
		  glTexCoord2f(0.0f, 1.0f);		glVertex3f(plateX+(drawX*step)-width/2,	plateY+(drawY*step)-width/2, z);		
		  glTexCoord2f(1.0f, 1.0f);		glVertex3f(plateX+(drawX*step)+width/2,	plateY+(drawY*step)-width/2, z);		
		  glTexCoord2f(1.0f, 0.0f);		glVertex3f(plateX+(drawX*step)+width/2,	plateY+(drawY*step)+width/2, z);		
		  glTexCoord2f(0.0f, 0.0f);		glVertex3f(plateX+(drawX*step)-width/2,	plateY+(drawY*step)+width/2, z);	
		glEnd();
	}
	void drawEnemy(){		
		glBindTexture(GL_TEXTURE_2D, texEnem);
		glBegin(GL_QUADS);
		  glTexCoord2f(0.0f, 1.0f);		glVertex3f(plateX+(EdrawX*step)-width/2,	plateY+(EdrawY*step)-width/2, z+0.1);		
		  glTexCoord2f(1.0f, 1.0f);		glVertex3f(plateX+(EdrawX*step)+width/2,	plateY+(EdrawY*step)-width/2, z+0.1);		
		  glTexCoord2f(1.0f, 0.0f);		glVertex3f(plateX+(EdrawX*step)+width/2,	plateY+(EdrawY*step)+width/2, z+0.1);		
		  glTexCoord2f(0.0f, 0.0f);		glVertex3f(plateX+(EdrawX*step)-width/2,	plateY+(EdrawY*step)+width/2, z+0.1);	
		glEnd();
	}
	void init(int _x,int _y,float _width,float _z,char *hero,char *enemy,float pX,float pY,float _step){
		 //верхний левый угол px
		x = _x;
		y = _y;
		width = _width;
		plateX = pX;
		plateY = pY;
		step = _step;
		stepsIn = (int)(width / step);
		drawX = stepsIn*x+(stepsIn/2);
		drawY = stepsIn*y+(stepsIn/2);
		insTex.LoadGLTextures(hero,&texture);
		insTex.LoadGLTextures(enemy,&texEnem);
		z = _z;
	}
}insHero;

class plate{
	float drawX,drawY;
	int numQuadsX,numQuadsY;
	unsigned char stepsInQuad;
	float qWidth;
	float z;
	GLuint texture1;
	GLuint texture2;
	GLuint texture3;
	GLuint texture4;
	GLuint texture5;
public:
	friend movment;
	friend bomb;
	friend DWORD WINAPI ListenSock(LPVOID lpParameter);
	friend loadIni;
	friend DWORD WINAPI Accept(LPVOID lpParameter);
	plate(){
		drawX = 0.0f;
		drawY = 0.0f;
	}
	void draw(){
		int i,j;
		//glBindTexture(GL_TEXTURE_2D, texture1);
		for(i=0;i<numQuadsY;i++){
			for(j=0;j<numQuadsX;j++){
				if(insMatr.data[i][j] == 0)
					glBindTexture(GL_TEXTURE_2D, texture1);
				if(insMatr.data[i][j] == 1)
					glBindTexture(GL_TEXTURE_2D, texture2);
				if(insMatr.data[i][j] == 2)
					glBindTexture(GL_TEXTURE_2D, texture3);
				if(insMatr.data[i][j] == 3)
					glBindTexture(GL_TEXTURE_2D, texture4);
				if(insMatr.data[i][j] == 4)
					glBindTexture(GL_TEXTURE_2D, texture5);
				glBegin(GL_QUADS);
				  glTexCoord2f(1.0f, 0.0f);		glVertex2f(drawX+(qWidth*j),			drawY+qWidth+(qWidth*i)	);		
				  glTexCoord2f(0.0f, 0.0f);		glVertex2f(drawX+qWidth+(qWidth*j),		drawY+qWidth+(qWidth*i)	);		
				  glTexCoord2f(0.0f, 1.0f);		glVertex2f(drawX+qWidth+(qWidth*j),		drawY+(qWidth*i)		);		
				  glTexCoord2f(1.0f, 1.0f);		glVertex2f(drawX+(qWidth*j),			drawY+(qWidth*i)		);	
				glEnd();
			}
		}
		
	}
	void init(float _drawX,float _drawY,int _numQuadsX,int _numQuadsY,float _qWidth,float _z,char *name1,char *name2,char *name3,char *boomName,char *bombName,float step){
		drawX = _drawX;
		drawY = _drawY;
		numQuadsX = _numQuadsX;
		numQuadsY = _numQuadsY;
		qWidth = _qWidth;
		stepsInQuad = (unsigned char)(qWidth/step);
		z = _z;
		insTex.LoadGLTextures(name1,&texture1);
		insTex.LoadGLTextures(name2,&texture2);
		insTex.LoadGLTextures(name3,&texture3);
		insTex.LoadGLTextures(boomName,&texture4);
		insTex.LoadGLTextures(bombName,&texture5);
	}
}insPlate;

class movment{
public:
	void corAxs(int x,int y){
		if(insMatr.data[y+1][x] != 0){//  низ (0 +1)
			if(insHero.drawY > insPlate.stepsInQuad*y+(insPlate.stepsInQuad/2)){ //нижняя стенка hero и верхняя стенка plate
				//insHero.drawY = (y+1)*insPlate.qWidth+insPlate.drawY-(insHero.width/2); //выровняли
				insHero.drawY = insPlate.stepsInQuad*y+(insPlate.stepsInQuad/2);
			}
		}
		if(insMatr.data[y-1][x] != 0){//  верх (0 -1)
			if(insHero.drawY < insPlate.stepsInQuad*y+(insPlate.stepsInQuad/2)){ //+1
				insHero.drawY = insPlate.stepsInQuad*y+(insPlate.stepsInQuad/2); //выровняли
			}
		}
		if(insMatr.data[y][x+1] != 0){//  право (+1 0)
			if(insHero.drawX > insPlate.stepsInQuad*x+(insPlate.stepsInQuad/2)){ 
				insHero.drawX = insPlate.stepsInQuad*x+(insPlate.stepsInQuad/2); //выровняли
			}
		}
		if(insMatr.data[y][x-1] != 0){//  лево (-1 0)
			if(insHero.drawX < insPlate.stepsInQuad*x+(insPlate.stepsInQuad/2)){ 
				insHero.drawX = insPlate.stepsInQuad*x+(insPlate.stepsInQuad/2);//выровняли
			}
		}
	}
	void mov(unsigned char v){
		switch(v){
		case CLEFT: 
			insHero.drawX -= 1;
			insHero.drawY = (unsigned char)(insHero.y*insPlate.stepsInQuad+(insPlate.stepsInQuad/2));
			break;
		case CRIGHT: 
			insHero.drawX += 1; 
			insHero.drawY = (unsigned char)(insHero.y*insPlate.stepsInQuad+(insPlate.stepsInQuad/2));
			break;
		case CUP: 
			insHero.drawY -= 1;
			insHero.drawX = (unsigned char)(insHero.x*insPlate.stepsInQuad+(insPlate.stepsInQuad/2));
			break;
		case CDOWN: 
			insHero.drawY += 1;
			insHero.drawX = (unsigned char)(insHero.x*insPlate.stepsInQuad+(insPlate.stepsInQuad/2));
			break;
		}
		corAxs(insHero.x,insHero.y);
		insHero.x = (int)(insHero.drawX/insPlate.stepsInQuad);
		insHero.y = (int)(insHero.drawY/insPlate.stepsInQuad);
	}
}insMov;

struct bs{
	char x,y;
	char time;
	char whosBomb;
};

class bomb{
	bs data[20];
	int num;
	int LookOnBoom;
	int lon;//длина взрыва

public:
	friend tmr;
	friend DWORD WINAPI ListenSock(LPVOID lpParameter);
	friend void restart(int WhoWin);
	bomb(){
		lon = 0;
		LookOnBoom = 30;
		num = 0;
	}
	void clearNum(){
		num = 0;
	}
	void tmrPlus(){
		for(int i=0;i<num;i++){
			if(data[i].time==0){
				BoomBomb(i);
			}
			data[i].time--;			
			if(data[i].time<=-LookOnBoom){
				ResetBomb(i);
			}
		}
	}
	void setBoom(char v,char h,int n){
		int i;
		for(i=0;i<lon;i++){
			if(insMatr.data[data[n].y+(i*v)][data[n].x+(i*h)] == 0){
				insMatr.data[data[n].y+(i*v)][data[n].x+(i*h)] = 3;
			}
			if(insMatr.data[data[n].y+(i*v)][data[n].x+(i*h)] == 2){
				insMatr.data[data[n].y+(i*v)][data[n].x+(i*h)] = 3;
				break;
			}
			if(insMatr.data[data[n].y+(i*v)][data[n].x+(i*h)] == 1){
				break;
			}
		}
	}
	void resetBoom(char v,char h,int n){
		int i;
		for(i=1;i<lon;i++){
			if(insMatr.data[data[n].y+(i*v)][data[n].x+(i*h)] == 3){
				insMatr.data[data[n].y+(i*v)][data[n].x+(i*h)] = 0;
			}else{
				break;
			}
		}
	}
	int SetBomb(char x,char y,char t,char who){
		if(insMatr.data[y][x] != 0)
			return 0;
		data[num].x = x;
		data[num].y = y;
		data[num].time = t;
		data[num].whosBomb = who;
		insMatr.data[data[num].y][data[num].x] = 4;
		num++;
		return 1;
	}
	void BoomBomb(int n){
		insMatr.data[data[n].y][data[n].x] = 0;
		setBoom(-1, 0, n);
		setBoom( 1, 0, n);
		setBoom( 0,-1, n);
		setBoom( 0, 1, n);
	}
	void ResetBomb(int n){
		int i;
		resetBoom(-1, 0, n);
		resetBoom( 1, 0, n);
		resetBoom( 0,-1, n);
		resetBoom( 0, 1, n);
		insMatr.data[data[n].y][data[n].x] = 0;
		if(data[n].whosBomb == 1)
			insHero.bombs--;
		for(i=n;i<num-1;i++){
			data[i]=data[i+1];
		}
		num--;
	}
	
	void init(int _lon){
		lon = _lon;
	}
}insBomb;

void restart(int WhoWin){
	// 0 - loose
	// 1 - win
	char buf[3];
	if(WhoWin){
		buf[0] = 0;
		buf[1] = 0;
		buf[2] = 0;
		stopListen = 1;
		send(clientSock,buf,3,0);
	}else{
		buf[0] = 'd';
		buf[1] = 'i';
		buf[2] = 'e';
		stopListen = 1;
		send(clientSock,buf,3,0);
	}
	insPause.stat = 1;
	for(int i=0;i<insBomb.num;i++){
		insBomb.data[i].time = -100;
	}
	insBomb.num = 0;

	if(ImServ){
		insHero.x=1;
		insHero.y=1;
		insMatr.init(SizeX,SizeY);
	}else{
		insHero.x=SizeX-2;
		insHero.y=SizeY-2;
	}
	insHero.bombs = 0;
	insHero.drawX = insHero.stepsIn*insHero.x+(insHero.stepsIn/2);
	insHero.drawY = insHero.stepsIn*insHero.y+(insHero.stepsIn/2);
	if(ImServ){
		insMatr.getMatr(SizeX,SizeY,1);
	}else{
		insMatr.getMatr(0,0,0);
	}
	if(WhoWin)
		insPause.stat = 3;
	else
		insPause.stat = 2;
}

class tmr{
	clock_t t1;
	clock_t t2;
	int fps;
	int countDraw;
	int countKeyB;
	int sync;
	char buf[3];
public:
	tmr(){
		countDraw = 0;
		countKeyB = 0;
		sync = 0;
		t1 = 0;
		t2 = 0;
		buf[0] = 'b';
		buf[1] = 's';
		buf[2] = '0';
		fps = 10;
	}
	int MovKeys(){
		if(keys[VK_LEFT]){	
			insMov.mov(CLEFT);
			return 1;
		}
		if(keys[VK_RIGHT]){
			insMov.mov(CRIGHT);
			return 1;
		}
		if(keys[VK_UP]){		
			insMov.mov(CUP);
			return 1;
		}
		if(keys[VK_DOWN]){
			insMov.mov(CDOWN);
			return 1;
		}
		return 0;		
	}
	void tmrGet(){		
		t2=clock();
		if(t2-t1>=10){
			countDraw++;
			if(!insPause.stat){
				insBomb.tmrPlus();
				countKeyB++;
				sync++;
				if(insMatr.data[insHero.y][insHero.x] == 3){
					stopListen = 1;
					insBomb.num = 0;
					restart(0);
				}
			}
			if((keys[VK_RETURN]) && (insPause.stat>1)){
				stopListen = 0;
				buf[0] = 'c';
				buf[1] = 'o';
				buf[2] = 'n';
				stopListen = 0;
				//keys[VK_SPACE] = 0;
				send(clientSock,buf,3,0);
				buf[0] = 'h';
				buf[1] = insHero.drawX;
				buf[2] = insHero.drawY;
				send(clientSock,buf,3,0);
				CreateThread(NULL,0,ListenSock,NULL,0,NULL);
				insPause.stat = 1;
			}
			t1 = t2;
		}	
		if(countDraw >= 5){
			DrawGLScene();
			countDraw = 0;
		}
		if(sync >= 5){
			buf[0] = 'h';
			buf[1] = insHero.drawX;
			buf[2] = insHero.drawY;
			send(clientSock,buf,3,0);
			sync = 0;
		}
		if(countKeyB >= 3){
			if((keys[VK_SPACE]) && (insHero.bombs<3)){
				if(connected){
					buf[0] = 'b';
					buf[1] = 's';
					buf[2] = '0';
					send(clientSock,buf,3,0);
				}
				if(insBomb.SetBomb(insHero.x,insHero.y,100,1))
					insHero.bombs++;
			}
			MovKeys();
			countKeyB = 0;
		}
	}
}insTmr;

DWORD WINAPI ListenSock(LPVOID lpParameter){
	char buf[4];
	int retVal;
	while(1){
		if(stopListen == 1){
			break;
		}
		retVal = recv(clientSock,buf,3,0);
		if(retVal >= 3){
			if(buf[0] == 'h'){
				insHero.EdrawX=buf[1];
				insHero.EdrawY=buf[2];
				insHero.ex = (int)(insHero.EdrawX/insPlate.stepsInQuad);
				insHero.ey = (int)(insHero.EdrawY/insPlate.stepsInQuad);
			}
			if((buf[0] == 'b') && (buf[1] == 's')){
					insBomb.SetBomb(insHero.ex,insHero.ey,100,0);
			}
			if((buf[0] == 'd') && (buf[1] == 'i')){
				restart(1);
				break;
			}
			if((buf[0] == 'c') && (buf[1] == 'o')){
				insPause.setStat(0);
			}
		}
	}
	return 0;
}


DWORD WINAPI Accept(LPVOID lpParameter){
	WSADATA wsaData;
	SOCKADDR_IN sin;
	char buf[3];
	int retVal;
	WSAStartup(0x0101, &wsaData); //используемая версия библиотеки
	servSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //настройки для tcp
	sin.sin_family = AF_INET; //формат адреса tcp/ip
	sin.sin_port = htons(80); //номер порта
	sin.sin_addr.S_un.S_addr = INADDR_ANY; //адрес
	retVal = bind(servSock, (LPSOCKADDR)&sin, sizeof(sin)); //установка параметров
	retVal = listen(servSock, 30); // ожидание запросов (сокет,макс. длина очереди)
	SOCKADDR_IN from;
	int fromlen=sizeof(from);
	clientSock = accept(servSock,(struct sockaddr*)&from, &fromlen);//сокет через который прошел запрос соединения
	insMatr.getMatr(SizeX,SizeY,1);
	connected = 1;
	insPause.setStat(0);
	buf[0] = 'h';
	buf[1] = insHero.drawX;
	buf[2] = insHero.drawY;
	send(clientSock,buf,3,0);
	CreateThread(NULL,0,ListenSock,NULL,0,NULL);
	return 0;
}


class loadIni{
private:
	int serv;
	int SzX;
	int SzY;
	char ip[17];
	char a,b,c,d;
	FILE *f;
public:
	void load(){
		char buf[500];
		char *st;
		int i;

		f = fopen("game.ini","rt");
		if(f == NULL){
			SzY = 15;
			SzX = 13;
			serv = 1;
		}
		i=0;
		while(!feof(f)){
			buf[i] = fgetc(f);
			i++;
		}

		fclose(f);
		st = strstr(buf,"Server");
		sscanf(&st[8],"%d",&serv);
		if(serv == 1){
			st = strstr(buf,"SizeX");
			sscanf(&st[7],"%d",&SzX);
			st = strstr(buf,"SizeY");
			sscanf(&st[7],"%d",&SzY);
			SzX = (SzX<5)?5:SzX;
			SzX += (SzX%2 == 0)?1:0;
			SzY = (SzY<5)?5:SzY;
			SzY += (SzY%2 == 0)?1:0;
		}else{
			st = strstr(buf,"ip");
			sscanf(&st[5],"%s",ip);
			sscanf(ip,"%d.%d.%d.%d",&a,&b,&c,&d);
		}

		serv = (serv<=0)?0:1;
		init();
	}
	int initClient(char *adr){
		WSADATA wsaData;
		int retVal=0;
		SOCKADDR_IN serverInfo;
		//struct hostent *host;
		char req[20];


		req[0] = '\0';
		//host = gethostbyname("127.0.0.1");
		WSAStartup(0x0101,&wsaData);
		clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		serverInfo.sin_family = AF_INET;
		serverInfo.sin_addr.S_un.S_un_b.s_b1 = a;
		serverInfo.sin_addr.S_un.S_un_b.s_b2 = b;
		serverInfo.sin_addr.S_un.S_un_b.s_b3 = c;
		serverInfo.sin_addr.S_un.S_un_b.s_b4 = d;
		serverInfo.sin_port = htons(80);
		retVal=connect(clientSock,(LPSOCKADDR)&serverInfo, sizeof(serverInfo));//сокет,структура,размер
		if(retVal == 0){
				insMatr.getMatr(SzX,SzY,0);
				connected = 1;
				return 1;
		}
		closesocket(clientSock);
		return 0;
	}
	void init(){
		float wid;
		float x,y;
		char buf[3];
		wid = 15;
		QuadWidth = wid;
		if(serv == 1){
			ImServ = 1;
			SizeX = SzX;
			SizeY = SzY;
			insMatr.init(SzX,SzY);
			CreateThread(NULL,0,Accept,NULL,0,NULL);
			x = (width/4)-((SizeX*QuadWidth)/2);
			y = (height/4)-((SizeY*QuadWidth)/2);
			insHero.init(1,1,QuadWidth,0.2f,"tex\\hero.bits","tex\\enemy.bits",x,y,3.0f);
		}else{
			ImServ = 0;
			if(!initClient(ip))
				exit(1);
			insPause.setStat(0);
			x = (width/4)-((SizeX*QuadWidth)/2);
			y = (height/4)-((SizeY*QuadWidth)/2);
			insHero.init(SizeX-2,SizeY-2,QuadWidth,0.2f,"tex\\hero.bits","tex\\enemy.bits",x,y,3.0f);
			
		}
		insPlate.init(x,y,SizeX,SizeY,QuadWidth,0.0f,"tex\\plate.bits","tex\\block.bits","tex\\ruin.bits","tex\\boom.bits","tex\\bomb.bits",3);
		insBomb.init(3);
		buf[0] = 'h';
		buf[1] = insHero.drawX;
		buf[2] = insHero.drawY;
		send(clientSock,buf,3,0);
		CreateThread(NULL,0,ListenSock,NULL,0,NULL);
	}
}insLoader;


void DrawGLScene(GLvoid){
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if(!insPause.stat){
		insPlate.draw();
		insHero.draw();
		insHero.drawEnemy();
	}else
		insPause.control();
	glLoadIdentity();									
	SwapBuffers(hDC);										
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow){
	MSG		msg;								
	BOOL	done=FALSE;						

	init("window",640,480);

	if (CreateGLWindow() == FALSE){
		return 0;									
	}
	insPause.init("tex\\win.bits","tex\\loose.bits","tex\\pause.bits");
	//elemInit();
	insLoader.load();
	while(!done){
		if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)){
			if (msg.message==WM_QUIT){
				done=TRUE;
			}else{
				TranslateMessage(&msg);				
				DispatchMessage(&msg);				
			}
		}else{
			if (keys[VK_ESCAPE]){
				done=TRUE;						
			}
			insTmr.tmrGet();							
		}
	}
	closesocket(clientSock);
	closesocket(servSock);
	KillGLWindow();									
	return (msg.wParam);							
}

LRESULT CALLBACK WndProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam){
	switch (uMsg){
		case WM_CLOSE:								
		{
			PostQuitMessage(0);					
			return 0;								
		}
		case WM_KEYDOWN:							
		{
			keys[wParam] = TRUE;					
			return 0;								
		}

		case WM_KEYUP:								
		{
			keys[wParam] = FALSE;				
			return 0;								
		}

	}

	return DefWindowProc(hWnd,uMsg,wParam,lParam);
}