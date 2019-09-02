#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
//#include <stdio.h>
#include <GL/gl.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

unsigned char ttf_buffer[1<<20];
unsigned char temp_bitmap[512*512];

stbtt_bakedchar cdata[96]; // ASCII 32..126 is 95 glyphs
GLuint ftex = 0;



#pragma comment( lib, "OpenGL32.lib" )
using namespace Gdiplus;

#define MAX_INT (1<<31-1)
#define MIN_INT (-MAX_INT)
#define MAX_LINE_COUNT 100000
#define MAX_PATH_COUNT 100000
#define MINZOOM 0.5f
#define MAXZOOM 2.5f

#define VK_A 0x41
#define VK_C 0x43
#define VK_E 0x45

bool InRange(float a,float min,float max)
{
	return a>= min && a<=max;
}
#pragma comment (lib,"Gdi32.lib")
#pragma comment (lib,"Gdiplus.lib")
#pragma comment (lib,"User32.lib")



float pointX = 10.f;
float pointY = 10.f;
float oldPointX = 0;
float oldPointY = 0;
float zoom = 1;

struct line
{
	int sx,sy,fx,fy;
};
line lines[MAX_LINE_COUNT];

int currentLineCount = 0;

struct camera
{
	float x;
	float y;
};
RECT rect;
camera c = {-20,-20};

Point points[100000000];


struct path_index 
{
	int s;
	int f;
};

struct path_bb
{
	int minX;
	int minY;
	int maxX;
	int maxY;
};

struct path_data
{
	path_index pathIndex;
	Color pathColor;
	path_bb pathBB;
	bool visible;
	int offsetX;
	int offsetY;	
};

path_data paths[MAX_PATH_COUNT];
int selectedPath = -1;
int pathsCount= -1;

struct saveData
{	
	int pathsCount;
	int linesCount;
	path_data * paths;
	line *lines;	
};

void Save(char * path)
{
	file_ptr file = OpenFile(path);
	WriteInt(file,pathsCount);
	WriteInt(file,currentLineCount);
	WriteBuffer(file,paths,pathsCount*sizeof(path_data));
	WriteBuffer(file,paths,currentLineCount*sizeof(line));
	CloseFile(file);
}

void Load(char * path)
{
	saveData sd;
	file_ptr file = OpenFile(path);
	sd.pathsCount = ReadInt(file);
	sd.lineCount = ReadInt(file);
	ReadCount(file,sd.pathsCount*sizeof(path_data),paths);
	ReadCount(file,sd.linesCount*sizeof(linesCount),lines);		
	CloseFile(file);
}

void InitPaths()
{
	for(int i = 0 ; i< MAX_PATH_COUNT;i++)
	{
		paths[i].pathBB.minX = MAX_INT;
		paths[i].pathBB.minY = MAX_INT;
		paths[i].pathBB.maxX = MIN_INT;
		paths[i].pathBB.maxY = MIN_INT;
		paths[i].visible = true;		

	}
}

struct rgba
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
	unsigned char a;
};

void UpdateBB(path_data &path,float x ,float y)
{
	if(path.pathBB.minX > x){path.pathBB.minX = x; } 
	if(path.pathBB.maxX < x){path.pathBB.maxX = x; }
	if(path.pathBB.minY > y){path.pathBB.minY = y; }	
	if(path.pathBB.maxY < y){path.pathBB.maxY = y; }
}

int drawing;
int dragging;
int panning;
int editing;
int copying;
int pickingColor = 0;
int pickingColorActive = 0;

int showSystemData = 0;

float dragX;
float dragY;
int pointCount=  0;

Bitmap *colorPicker = 0;

int screenWidth = 0;
int screenHeight = 0;

Color pickedColor;
Rect colorPickerOpenerRect = Rect(0,0,100,20);
Rect colorPickerSelectRect = Rect(0,22,140,140);
Rect systemDataRect = Rect(100,0,100,20);

float fontHeight  = 22;

void my_stbtt_initfont(void)
{
   fread(ttf_buffer, 1, 1<<20, fopen("c:/windows/fonts/times.ttf", "rb"));
   stbtt_BakeFontBitmap(ttf_buffer,0, fontHeight, temp_bitmap,512,512, 32,96, cdata); // no guarantee this fits!
   // can free ttf_buffer at this point

   rgba *bitmapColor = (rgba*)malloc(sizeof(rgba)*512*512);
   for(int i = 0;i<512*512;i++)
   {	   
		rgba color = {0,0,255,temp_bitmap[i]};
		bitmapColor[i]	= color;
   }
   glGenTextures(1, &ftex);
   glBindTexture(GL_TEXTURE_2D, ftex);
  
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 512,512, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmapColor);
   
   // can free temp_bitmap at this point
   free(bitmapColor);

   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
}

void my_stbtt_print(float x, float y,float sX,float sY,float denom,char *text)
{
	glColor4f(1, 1, 1, 1);
   // assume orthographic projection with units = screen pixels, origin at top left
   glEnable(GL_TEXTURE_2D);
   glBindTexture(GL_TEXTURE_2D, ftex);
   glBegin(GL_QUADS);
   while (*text) {
      if (*text >= 32 && *text < 128) {
         stbtt_aligned_quad q;
         stbtt_GetBakedQuad(cdata, 512,512, *text-32, &x,&y,&q,1);//1=opengl & d3d10+,0=d3d9

		 q.x0 = q.x0/denom +sX;
		 q.y0 = -q.y0/denom + sY;
		 q.x1 = q.x1/denom +sX;
		 q.y1 = -q.y1/denom + sY;
		 //
		 //float fWidth =  q.x1 - q.x0;
		 //float fHeight = q.y1 - q.y0;

         glTexCoord2f(q.s0,q.t0); glVertex2f(q.x0,q.y0);
         glTexCoord2f(q.s1,q.t0);glVertex2f(q.x1,q.y0);
         glTexCoord2f(q.s1,q.t1); glVertex2f(q.x1,q.y1);
          glTexCoord2f(q.s0,q.t1);glVertex2f(q.x0,q.y1);
      }
      ++text;
   }
   glEnd();
   glDisable(GL_TEXTURE_2D);
}



void AddLineToPointArray(float sx ,float sy, float fx,float fy)
{				
	points[pointCount++] = Point((int)sx,(int)sy);
	points[pointCount++] = Point((int)fx,(int)fy);		
}

bool PointInRect(int pointX,int pointY,RECT r)
{ 
	return pointX>=r.left && pointX <= r.right && pointY>=r.top && pointY<=r.bottom;
}


static HGLRC  hglrc = 0; 

GLuint colorPickerTexture = 0;

void DrawRectGL(Rect r,rgba color)
{
		glColor4f((float)color.r/255,(float)color.g/255,(float)color.b/255,(float)color.a/255);
		glLineWidth(0.5);
		glBegin(GL_LINE_STRIP);		   
		glVertex2f(((float)r.GetLeft()/screenWidth)*2-1,-(((float)r.GetTop()/screenHeight)*2 -1));
		glVertex2f(((float)r.GetRight()/screenWidth)*2-1,-(((float)r.GetTop()/screenHeight)*2 -1));
		glVertex2f(((float)r.GetRight()/screenWidth)*2-1,-(((float)r.GetBottom()/screenHeight)*2 -1));
		glVertex2f(((float)r.GetLeft()/screenWidth)*2-1,-(((float)r.GetBottom()/screenHeight)*2 -1));
		glVertex2f(((float)r.GetLeft()/screenWidth)*2-1,-(((float)r.GetTop()/screenHeight)*2 -1));
		glEnd();
}

void DrawRectGL(RECT r)
{
		glBegin(GL_LINE_STRIP);		   
		glVertex2f(((float)r.left/screenWidth)*2-1,-(((float)r.top/screenHeight)*2 -1));
		glVertex2f(((float)r.right/screenWidth)*2-1,-(((float)r.top/screenHeight)*2 -1));
		glVertex2f(((float)r.right/screenWidth)*2-1,-(((float)r.bottom/screenHeight)*2 -1));
		glVertex2f(((float)r.left/screenWidth)*2-1,-(((float)r.bottom/screenHeight)*2 -1));
		glVertex2f(((float)r.left/screenWidth)*2-1,-(((float)r.top/screenHeight)*2 -1));
		glEnd();
}

HBITMAP OnPaintGL(HDC hdc)
{
	PIXELFORMATDESCRIPTOR pfd;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |PFD_GENERIC_ACCELERATED;
    pfd.iPixelType = PFD_TYPE_RGBA;
    //pfd.cColorBits = 24;
	int pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);	
	if(!hglrc)
	{
		hglrc = wglCreateContext(hdc);		
	}

	wglMakeCurrent (hdc, hglrc);
   glEnable(GL_LINE_SMOOTH);
   glEnable(GL_BLEND);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   glClearColor(1,1,1,1);
   glClear(GL_COLOR_BUFFER_BIT);

   if(!dragging)
		selectedPath = -1;	
   
   int iterationCount = 0;

	for(int j = 0 ; j<=pathsCount ; j++)
	{   
		if(!paths[j].visible)
			continue;

		if(editing)
		{
				RECT r = {zoom*(paths[j].pathBB.minX -c.x +paths[j].offsetX),
							zoom*(paths[j].pathBB.minY -c.y + paths[j].offsetY),
							zoom*(paths[j].pathBB.maxX -c.x + paths[j].offsetX),zoom*(paths[j].pathBB.maxY -c.y +paths[j].offsetY)};	
		   if(PointInRect(pointX,pointY,r))
		   {
			   if(!drawing && !dragging)
					selectedPath = j;
			   glColor3f(1,0,0);
		   }
		   else 
			   glColor3f(0,0,0);
		   glLineWidth(1);
		  DrawRectGL(r);
		}
		if(editing && copying && selectedPath != -1)
		{
			paths[++pathsCount] = paths[selectedPath];
			paths[pathsCount].offsetX +=20;
			paths[pathsCount].offsetY +=20;

			//Note(should I prevent multiple copies of the same path)
			//previouslyCopied = selectedPath;

			copying = false;
		}

		glLineWidth(zoom*2);
		glBegin(GL_LINE_STRIP);
		for(int i = paths[j].pathIndex.s ; i<paths[j].pathIndex.f-1;i+=2)
		{
			   iterationCount++;
			   if(i >currentLineCount)
					break;
			   glColor3f(float(paths[j].pathColor.GetR())/255,float(paths[j].pathColor.GetG())/255,float(paths[j].pathColor.GetB())/255);
			   glVertex2f((zoom*(lines[i].sx-c.x+paths[j].offsetX)/rect.right)*2-1,-((zoom*(lines[i].sy-c.y + paths[j].offsetY)/rect.bottom)*2-1));
			   glVertex2f((zoom*(lines[i+1].sx-c.x+paths[j].offsetX)/rect.right)*2-1,-((zoom*(lines[i+1].sy-c.y + paths[j].offsetY)/rect.bottom)*2-1));
		}
		glEnd();
	}		
	glColor4f(1, 1, 1, 1);
   if(colorPickerTexture && pickingColor)
   {
	   glBindTexture(GL_TEXTURE_2D, colorPickerTexture);    
	   glEnable(GL_TEXTURE_2D);
	   glBegin(GL_QUADS);
			{	float l = colorPickerSelectRect.GetLeft();
				float r = colorPickerSelectRect.GetRight();
				float b = colorPickerSelectRect.GetBottom();
				float t = colorPickerSelectRect.GetTop();
				glTexCoord2f(0, 1);glVertex2f((l/screenWidth)*2-1, -((b/screenHeight)*2-1));
				glTexCoord2f(1, 1);glVertex2f((r/screenWidth)*2-1, -((b/screenHeight)*2-1));
				glTexCoord2f(1, 0);glVertex2f((r/screenWidth)*2-1, -((t/screenHeight)*2-1));
				glTexCoord2f(0, 0);glVertex2f((l/screenWidth)*2-1,-((t/screenHeight)*2-1));
			}
	    glEnd();
		
	    glDisable(GL_TEXTURE_2D);
	    glBindTexture(GL_TEXTURE_2D, 0);
   }
   {
		glBegin(GL_QUADS);
		
		float cr = (float)pickedColor.GetR()/255;
		float cg = (float)pickedColor.GetG()/255;
		float cb = (float)pickedColor.GetB()/255;
		float l = colorPickerOpenerRect.GetLeft();
		float r = colorPickerOpenerRect.GetRight();
		float b = colorPickerOpenerRect.GetBottom();
		float t = colorPickerOpenerRect.GetTop();
		glColor3f(cr,cg,cb);
		glVertex2f((l/screenWidth)*2-1, -((b/screenHeight)*2-1));
		glVertex2f((r/screenWidth)*2-1, -((b/screenHeight)*2-1));
		glVertex2f((r/screenWidth)*2-1, -((t/screenHeight)*2-1));
		glVertex2f((l/screenWidth)*2-1,-((t/screenHeight)*2-1));
		
		glEnd();
	}

	if(!ftex)
		my_stbtt_initfont();

	
	{
		rgba tColor = {0,0,0,255};
		DrawRectGL(systemDataRect,tColor);
	}

	my_stbtt_print(0,0,-0.85,0.95,600,"sys_data");


	if(showSystemData)
	{
		glColor3f(1,1,1);
		char buffer[256] ={};
		sprintf(buffer,"x:%.3f, y:%.3f lineCount:%d pathsCount:%d zoom:%.3f, editing:%d",c.x,c.y,currentLineCount,pathsCount,zoom,editing);
		my_stbtt_print(0,0,-1,0.89,600,buffer);
	}
	
	if(!colorPickerTexture && colorPicker){
		glGenTextures(1,&colorPickerTexture);
		glBindTexture(GL_TEXTURE_2D, colorPickerTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		BitmapData bmpd;
		Rect rect(0, 0, colorPicker->GetWidth(),colorPicker->GetHeight());
		colorPicker->LockBits(&rect,ImageLockModeRead,PixelFormat32bppPARGB,&bmpd);
		rgba * pixels = (rgba*)bmpd.Scan0;
		for(int i = 0 ; i< colorPicker->GetWidth()*colorPicker->GetHeight();i++)
		{
			unsigned char r =pixels[i].r;			
			unsigned char b =pixels[i].b;
			pixels[i].r = b;
			pixels[i].b = r;
		}
		glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,colorPicker->GetWidth(),colorPicker->GetHeight(),0,GL_RGBA,GL_UNSIGNED_BYTE,bmpd.Scan0);
		glBindTexture(GL_TEXTURE_2D, 0);
		colorPicker->UnlockBits(&bmpd);
	}
	
	
	SwapBuffers(hdc);
	wglMakeCurrent (NULL, NULL);
	//int Error = glGetError();
	return 0;
}
VOID OnPaintGDI(HDC hdc)
{	      
   HDC mdc = CreateCompatibleDC(hdc);
   HBITMAP mbmp = CreateCompatibleBitmap(mdc,rect.right,rect.bottom);

   if(!mbmp)
	   return;
   HBITMAP moldbmp = (HBITMAP)SelectObject(mdc,mbmp);   

   Graphics graphics(mdc);   
   graphics.SetCompositingMode( CompositingModeSourceCopy );
   //graphics.SetCompositingQuality( CompositingQualityHighSpeed );
   graphics.SetPixelOffsetMode( PixelOffsetModeNone );
   graphics.SetSmoothingMode( SmoothingModeNone );
   graphics.SetInterpolationMode( InterpolationModeDefault );
   //graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);
   //graphics.SetSmoothingMode(SmoothingModeAntiAlias);

   
   Pen pen(Color(255, 0, 0, 255),zoom*2);


   Pen rectPen(Color(255, 0, 0, 0),2);

   graphics.DrawRectangle(&rectPen, colorPickerOpenerRect);	

   graphics.DrawRectangle(&rectPen, systemDataRect);

   SelectObject(mdc,GetStockObject(DC_BRUSH));

   int iterationCount = 0;

	if(!dragging)
		selectedPath = -1;	

	for(int j = 0 ; j<=pathsCount ; j++)
	{   
		if(!paths[j].visible)
			continue;
		if(editing)
		{
		   RECT r = {zoom*(paths[j].pathBB.minX -c.x +paths[j].offsetX),zoom*(paths[j].pathBB.minY -c.y + paths[j].offsetY),
			   zoom*(paths[j].pathBB.maxX -c.x + paths[j].offsetX),zoom*(paths[j].pathBB.maxY -c.y +paths[j].offsetY)};
	
		   if(PointInRect(pointX,pointY,r))
		   {
			   if(!drawing && !dragging)
					selectedPath = j;
			   SetDCBrushColor(mdc,0x000000ff);
		   }
		   else 
			   SetDCBrushColor(mdc,0);
		   FrameRect(mdc,&r,NULL);
		}
		   
			for(int i = paths[j].pathIndex.s ; i<paths[j].pathIndex.f-1;i+=2)
			{
				   iterationCount++;
				   if(i >currentLineCount)
						break;
				   AddLineToPointArray(zoom*(lines[i].sx-c.x+paths[j].offsetX),zoom*(lines[i].sy-c.y + paths[j].offsetY),zoom*(lines[i+1].sx-c.x +paths[j].offsetX), zoom*(lines[i+1].sy - c.y+paths[j].offsetY));
			}
	
			pen.SetColor(paths[j].pathColor);
			graphics.DrawLines(&pen,points,pointCount);
			pointCount = 0;
	}

	SolidBrush  brush(Color(255, 0, 0, 255));
	FontFamily  fontFamily(L"Times New Roman");
	Font        fontColorPicker(&fontFamily, 15, FontStyleRegular, UnitPixel);
	graphics.DrawString(L"Color-Picker",-1,&fontColorPicker,PointF(2,0),&brush);
	graphics.DrawString(L"System-Data",-1,&fontColorPicker,PointF(102,0),&brush);

	if(showSystemData)
	{
		Font        font(&fontFamily, 24, FontStyleRegular, UnitPixel);		
		PointF      pointF(10, 20);		
		wchar_t buffer[256] ={};
		swprintf(buffer,sizeof(buffer),L"x:%.3f, y:%.3f lineCount:%d pathsCount:%d zoom:%.3f, editing:%d",c.x,c.y,currentLineCount,pathsCount,zoom,editing);
		graphics.DrawString((WCHAR*)buffer, -1, &font, pointF, &brush);		
	}

   BitBlt(hdc, 0, 0, rect.right, rect.bottom, mdc , 0, 0, SRCCOPY);   
   DeleteObject(mbmp);
   DeleteDC(mdc);
}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, INT iCmdShow)
{   
   HWND                hWnd;
   MSG                 msg;
   WNDCLASS            wndClass;
   GdiplusStartupInput gdiplusStartupInput;
   ULONG_PTR           gdiplusToken;
    
   // Initialize GDI+.


   GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

   InitPaths();  

   colorPicker = new Bitmap(L"colorPicker.jpg");

   wndClass.style          = CS_HREDRAW | CS_VREDRAW;
   wndClass.lpfnWndProc    = WndProc;
   wndClass.cbClsExtra     = 0;
   wndClass.cbWndExtra     = 0;
   wndClass.hInstance      = hInstance;
   wndClass.hIcon          = LoadIcon(NULL, IDI_APPLICATION);
   wndClass.hCursor        = LoadCursor(NULL, IDC_ARROW);
   wndClass.hbrBackground  = (HBRUSH)GetStockObject(WHITE_BRUSH);
   wndClass.lpszMenuName   = NULL;
   wndClass.lpszClassName  = TEXT("GettingStarted");
   
   RegisterClass(&wndClass);
   
   hWnd = CreateWindow(
      TEXT("GettingStarted"),   // window class name
      TEXT("Getting Started"),  // window caption
      WS_OVERLAPPEDWINDOW,      // window style
      CW_USEDEFAULT,            // initial x position
      CW_USEDEFAULT,            // initial y position
      CW_USEDEFAULT,            // initial x size
      CW_USEDEFAULT,            // initial y size
      NULL,                     // parent window handle
      NULL,                     // window menu handle
      hInstance,                // program instance handle
      NULL);                    // creation parameters
   
   ShowWindow(hWnd, iCmdShow);
   UpdateWindow(hWnd);
   GetClientRect(hWnd, &rect);

   screenWidth = rect.right;
   screenHeight = rect.bottom;
   

   while(GetMessage(&msg, NULL, 0, 0))
   {
	  TranslateMessage(&msg);
      DispatchMessage(&msg);  
   }

   wglDeleteContext (hglrc);
   GdiplusShutdown(gdiplusToken);
   return msg.wParam;
}  // WinMain

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, 
   WPARAM wParam, LPARAM lParam)
{
   
   PAINTSTRUCT  ps;
   HDC hdc;
   switch(message)
   {
	   case WM_KEYDOWN:
		   if(wParam == VK_SPACE)
			   panning = true;

		   if(wParam == VK_E)
				editing =!editing;

		   if(wParam == VK_C)
				copying = true;
			break;
	   case WM_KEYUP:
		   if(wParam == VK_SPACE)
			   panning = false;			   
		   if(wParam == VK_C)
			   copying = false;
			break;
	   case WM_PAINT:
			hdc = BeginPaint(hWnd,&ps);						
			OnPaintGL(hdc);			
			EndPaint(hWnd,&ps);
			InvalidateRect(hWnd,NULL,false);
			return 1;
	    case WM_ERASEBKGND:
			return(1);
		case WM_MOUSEMOVE:
			{
			pointX = GET_X_LPARAM(lParam); 
			pointY = GET_Y_LPARAM(lParam);
			
			int pointXDir = pointX - oldPointX;
			int pointYDir = pointY - oldPointY;

			pickingColorActive =  pickingColor && colorPickerSelectRect.Contains(pointX,pointY);				
			
			//if(pickingColorActive)
			//	while(ShowCursor(false) >= 0);
			//else
			//	while(ShowCursor(true) < 0);	

			if(wParam == MK_LBUTTON)
			{	
				if(panning)
				{					
					c.x +=-(pointXDir)/zoom;
					c.y +=-(pointYDir)/zoom;
				}
				else if (pickingColorActive)
				{	
					float dx = pointX - colorPickerSelectRect.GetLeft();
					float dy = pointY - colorPickerSelectRect.GetTop();
					float rectWidth = colorPickerSelectRect.GetRight() - colorPickerSelectRect.GetLeft();
					float rectHeight = colorPickerSelectRect.GetBottom() - colorPickerSelectRect.GetTop();

					float uvx= dx/rectWidth;
					float uvy= dy/rectHeight;
					float imageX = colorPicker->GetWidth()*uvx;
					float imageY = colorPicker->GetHeight()*uvy;				
					colorPicker->GetPixel(imageX,imageY,&pickedColor);
				}		
				else if(!editing && !systemDataRect.Contains(pointX,pointY))
				{
					if(!drawing)
					{
						pathsCount++;
						drawing = true;
						paths[pathsCount].pathIndex.s = currentLineCount;
					}	
					if(pathsCount<MAX_PATH_COUNT)
					{
						line toAdd = {(pointX/zoom+c.x),(pointY/zoom+c.y),(oldPointX/zoom+c.x),(oldPointY/zoom+c.y)};
						lines[currentLineCount++]=toAdd;
						paths[pathsCount].pathIndex.f = currentLineCount;
						paths[pathsCount].pathColor = pickedColor;
						UpdateBB(paths[pathsCount],pointX/zoom + c.x,pointY/zoom +c.y);
					}					
				}
			}
			if(wParam == MK_RBUTTON)
			{
				if(editing && selectedPath!=-1)
				{
					if(!dragging)
					{
						float bbX = paths[selectedPath].offsetX + paths[selectedPath].pathBB.minX;
						float bbY = paths[selectedPath].offsetY + paths[selectedPath].pathBB.minY;
						//
						// the difference betwwen the bbX and new 
						float deltaPointX = bbX - (pointX/zoom + c.x);
						float deltaPointY = bbY - (pointY/zoom + c.y);

						dragX = deltaPointX;
						dragY = deltaPointY;
					}
					// world space 

					paths[selectedPath].offsetX = (pointX/zoom + c.x) + dragX - paths[selectedPath].pathBB.minX;
					paths[selectedPath].offsetY = (pointY/zoom + c.y) + dragY - paths[selectedPath].pathBB.minY;
					dragging = true;
				}
			}
			oldPointX=pointX;
			oldPointY=pointY;
			break;
		}
		case WM_LBUTTONUP:
			{

				if(colorPickerOpenerRect.Contains(pointX,pointY))
				{
					pickingColor = !pickingColor;										
				}
				if(systemDataRect.Contains(pointX,pointY))
				{
					showSystemData = !showSystemData;
				}
				if(!panning && editing && selectedPath!=-1)
				{
					paths[selectedPath].visible = false;
				}
				drawing = false;
				break;
			}
		case WM_RBUTTONUP:
			{
				dragging = false;
			}	
		case WM_MOUSEWHEEL:
			{
				float x = rect.right/2;
				float y = rect.bottom/2;

				int wpres = GET_WHEEL_DELTA_WPARAM(wParam);

				if(InRange((1.f/wpres + zoom),MINZOOM,MAXZOOM))
				{
					float newZoom = zoom + 1.f/wpres;

					c.x+= x *(1.f/zoom-1.f/newZoom);
					c.y+= y *(1.f/zoom-1.f/newZoom);
					zoom = newZoom;
				}
			}
			break;
		case WM_DESTROY:
		   PostQuitMessage(0);
		   return 0;

		default:
		   return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 1;
} // WndProc