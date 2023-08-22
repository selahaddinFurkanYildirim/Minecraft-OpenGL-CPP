#include "../inc/display.hpp"

bool keyValues[256];
bool specKeyValues[256];

void lockMause(int x,int y){
    int screenWidth = glutGet(GLUT_WINDOW_WIDTH);
    int screenHeight = glutGet(GLUT_WINDOW_HEIGHT);
    if (x >= screenWidth - 1) {
        glutWarpPointer(2, y);
    }
    if (x <= 1) {
        glutWarpPointer(screenWidth-2, y);
    }
    if (y >= screenHeight - 1) {
        glutWarpPointer(x, screenHeight - 2);
    }
    if (y <= 1) {
        glutWarpPointer(x, 2);
    }
}
void TamEkran() {
    static bool fullScreen = false;
    fullScreen = !fullScreen;
    if (fullScreen) {
        glutFullScreen();
    }
    else{
        glutReshapeWindow(500, 500);
    }
}
void klavye(int key, int x, int y) {
    specKeyValues[key]=1;
    cout<<x<<y;
    if (key == GLUT_KEY_F11) {
        TamEkran();
    }
}
void klavyeUp(int key, int x, int y) {
    cout<<x<<y;
    specKeyValues[key]=0;
}
void keyUp(unsigned char c,int x,int y){
    cout<<x<<y;
    if(c<27) keyValues[int(c)+96]=0;
    if(c==0) keyValues[32]=0;
    keyValues[c]=0;
}
void keyDown(unsigned char c,int x,int y){
    cout<<x<<y;
    if(c<27) keyValues[int(c)+96]=1;
    if(c==0) keyValues[32]=1;
    keyValues[c]=1;
}
bool isKeyPressed(char c){
    return keyValues[int(c)];
}
bool isSpecKeyPressed(char c){
    return specKeyValues[int(c)];
}
void Display::Reshape(int width,int height){
    if (height == 0) height = 1;
    GLfloat aspect = (GLfloat)width / (GLfloat)height;
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, aspect, 0.1f, 100.0f);
}
Display::Display(){
    int argc=1;
    char *argv[1];
    argv[0][0]='.';
    argv[0][1]='/';
    argv[0][2]='a';
    glutInit(&argc,argv);
    return;
}
void Display::SetUpdateFunc(function<void()> func){
    Update(func);
}
void Display::HareketKontrol(float xyonu,float zyonu,float camRotX){
    if(isSpecKeyPressed(114) || isSpecKeyPressed(112)) xyonu*=2.2,zyonu*=2.2;
    float tmp=camRotX;
    float camposxartis=0;
    if(camRotX>360){
        camRotX=fmod(camRotX+450,540);
        camposxartis=xyonu*((abs(camRotX-360)-90)/-90.0)*-1;
    }else{
        camRotX+=90;
        camposxartis=xyonu*((abs(camRotX-360)-90)/-90.0);
    }
    if(canIgo(camposx+xyonu*((abs(tmp-360)-90)/-90.0),camposy,camposz-camposxartis)) {
        camposx+=xyonu*((abs(tmp-360)-90)/-90.0);
        camposz-=camposxartis;
    }
    camRotX=tmp;
    tmp=camRotX;
    camposxartis=0;
    if(camRotX>360){
        camRotX=fmod(camRotX+450,540);
        camposxartis=zyonu*((abs(camRotX-360)-90)/-90.0)*-1;
    }else{
        camRotX+=90;
        camposxartis=zyonu*((abs(camRotX-360)-90)/-90.0);
    }
    if(canIgo(camposx+camposxartis,camposy,camposz+zyonu*((abs(tmp-360)-90)/-90.0))) {
        camposz+=zyonu*((abs(tmp-360)-90)/-90.0);
        camposx+=camposxartis;
    }
    camRotX=tmp;
    
    static float yyon=0,yyone=0;
    if(canIgo(camposx,camposy+0.2,camposz) && yyone==0){
        yyone=0.1;
    }
    if(isKeyPressed(' ') && !canIgo(camposx,camposy+0.2,camposz)){
        yyon=0.22;
        yyone=0;
    }
    if(yyon>0){
        if(!canIgo(camposx,camposy-1.0,camposz)) yyon=0,yyone=0;
        yyon/=1.015;
        camposy-=yyon;
    }
    if(yyone>0){
        yyone*=1.02;
        for(float i=0.0;i<=yyone;i+=0.1) if(!canIgo(camposx,camposy+i,camposz)) yyone=0,yyon=0,camposy+=i-0.1;
        camposy+=yyone;
    }
}
void Display::Update(function<void()> func){
    static function<void()> a;
    if(func==NULL){
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        a();
        glutSwapBuffers();
    }
    else{
        a=func;
    }
}
void Display::Update(){
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    Update(NULL);
}
void Display::Start(int width,int height,string title){
    glutInitDisplayMode( GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
    glutInitWindowPosition(0,0);
    glutInitWindowSize(width,height);
    glutCreateWindow(title.c_str());
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1);
    glShadeModel(GL_SMOOTH);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glutSetKeyRepeat(GLUT_KEY_REPEAT_OFF);
    glutDisplayFunc(Update);
    glutPassiveMotionFunc(myMauseFunc);
    glutReshapeFunc(Reshape);
    glutKeyboardFunc(keyDown);
    glutSpecialFunc(klavye);
    glutSpecialUpFunc(klavyeUp);
    glutKeyboardUpFunc(keyUp);
    glutTimerFunc(30, myTimerFunc, 0);
    glClearColor(1,1,1,1);
    glColor3f(0,0,0);
    init();
    glutMainLoop();
}
void Display::SetInitFunc(function<void()> func){
    init=func;
}
void Display::SetTimerFunc(void (*func)(int)){
    myTimerFunc=func;
}
void Display::SetMauseFunc(void (*func)(int x, int y)){
    myMauseFunc=func;
}
void Display::SetBKColor(float r,float g,float b,float a){
    glClearColor(r,g,b,a);
    return;
}
map<pair<pair<int,int>,int>,int> isColorSetted;
bool canIgo(float x,float y,float z){
    return !isColorSetted[make_pair(make_pair(x,y+0.5),z)] && !isColorSetted[make_pair(make_pair(x,y),z)];
}
pair<double,double> cemberinAcidakiDegerleri(double x, double y, double r, double d)
{
    pair<double,double> a;
    double radyan = d * 3.14159265358979 / 180.0;
    a.first = cos(radyan) * r + x;
    a.second = sin(radyan) * r + y;
    return a;
}
void cube(float x,float y,float z,int color){
    if(isColorSetted[make_pair(make_pair(x,-1*y),-1*z)]==0)
        isColorSetted[make_pair(make_pair(x,-1*y),-1*z)]=color+1;
    color=isColorSetted[make_pair(make_pair(x,-1*y),-1*z)]-1;
    glColor3f((color/4)%2,(color/2)%2,color%2);
    float x1=x-0.5f;
    float x2=x+0.5f;
    
    float y1=y-0.5f;
    float y2=y+0.5f;
    
    float z1=z-0.5f;
    float z2=z+0.5f;
    glBegin(GL_QUADS);
        glVertex3f(x2,y2,z1);
        glVertex3f(x1,y2,z1);
        glVertex3f(x1,y2,z2);
        glVertex3f(x2,y2,z2);

        // Bottom face (y = -0.5f)
        glVertex3f(x2,y1,z2);
        glVertex3f(x1,y1,z2);
        glVertex3f(x1,y1,z1);
        glVertex3f(x2,y1,z1);

        // Front face  (z = +0.5f)
        glVertex3f(x2,y2,z2);
        glVertex3f(x1,y2,z2);
        glVertex3f(x1,y1,z2);
        glVertex3f(x2,y1,z2);

        // Back face (z = -0.5f)
        glVertex3f(x2,y1,z1);
        glVertex3f(x1,y1,z1);
        glVertex3f(x1,y2,z1);
        glVertex3f(x2,y2,z1);

        // Left face (x = -0.5f)
        glVertex3f(x1, y2,z2);
        glVertex3f(x1, y2,z1);
        glVertex3f(x1, y1,z1);
        glVertex3f(x1, y1,z2);

        // Right face (x = +0.5f)
        glVertex3f(x2, y2,z1);
        glVertex3f(x2, y2,z2);
        glVertex3f(x2, y1,z2);
        glVertex3f(x2, y1,z1);
    glEnd();
}
void cubeGround(float x,float y,float z){
    isColorSetted[make_pair(make_pair(x,-1*y),-1*z)]=1;
    float x1=x-0.5f;
    float x2=x+0.5f;
    
    float y1=y-0.5f;
    float y2=y+0.5f;
    
    float z1=z-0.5f;
    float z2=z+0.5f;
    glBegin(GL_QUADS);
        glColor3f(0.1f, 0.4f, 0.1f);
        glVertex3f(x2,y2,z1);
        glColor3f(0.0f, 0.3f, 0.0f);
        glVertex3f(x1,y2,z1);
        glColor3f(0.1f, 0.4f, 0.1f);
        glVertex3f(x1,y2,z2);
        glColor3f(0.0f, 0.3f, 0.0f);
        glVertex3f(x2,y2,z2);

        // Bottom face (y = -0.5f)
        glColor3f(0.2f, 0.1f, 0.1f);
        glVertex3f(x2,y1,z2);
        glColor3f(0.1f, 0.0f, 0.0f);
        glVertex3f(x1,y1,z2);
        glColor3f(0.2f, 0.1f, 0.1f);
        glVertex3f(x1,y1,z1);
        glColor3f(0.1f, 0.0f, 0.0f);
        glVertex3f(x2,y1,z1);

        // Front face  (z = +0.5f)
        glColor3f(0.2f, 0.1f, 0.1f);
        glVertex3f(x2,y2-0.3,z2);
        glColor3f(0.1f, 0.0f, 0.0f);
        glVertex3f(x1,y2-0.3,z2);
        glColor3f(0.2f, 0.1f, 0.1f);
        glVertex3f(x1,y1,z2);
        glColor3f(0.1f, 0.0f, 0.0f);
        glVertex3f(x2,y1,z2);
        
        glColor3f(0.1f, 0.4f, 0.1f);
        glVertex3f(x2,y2,z2);
        glColor3f(0.0f, 0.3f, 0.0f);
        glVertex3f(x1,y2,z2);
        glColor3f(0.1f, 0.4f, 0.1f);
        glVertex3f(x1,y1+0.7,z2);
        glColor3f(0.0f, 0.3f, 0.0f);
        glVertex3f(x2,y1+0.7,z2);

        // Back face (z = -0.5f)
        glColor3f(0.2f, 0.1f, 0.1f);
        glVertex3f(x2,y1,z1);
        glColor3f(0.1f, 0.0f, 0.0f);
        glVertex3f(x1,y1,z1);
        glColor3f(0.2f, 0.1f, 0.1f);
        glVertex3f(x1,y2-0.3,z1);
        glColor3f(0.1f, 0.0f, 0.0f);
        glVertex3f(x2,y2-0.3,z1);
        
        glColor3f(0.1f, 0.4f, 0.1f);
        glVertex3f(x2,y1+0.7,z1);
        glColor3f(0.0f, 0.3f, 0.0f);
        glVertex3f(x1,y1+0.7,z1);
        glColor3f(0.1f, 0.4f, 0.1f);
        glVertex3f(x1,y2,z1);
        glColor3f(0.0f, 0.3f, 0.0f);
        glVertex3f(x2,y2,z1);
        
        

        // Left face (x = -0.5f)
        glColor3f(0.2f, 0.1f, 0.1f);
        glVertex3f(x1, y2-0.3,z2);
        glColor3f(0.1f, 0.0f, 0.0f);
        glVertex3f(x1, y2-0.3,z1);
        glColor3f(0.2f, 0.1f, 0.1f);
        glVertex3f(x1, y1,z1);
        glColor3f(0.1f, 0.0f, 0.0f);
        glVertex3f(x1, y1,z2);
        
        glColor3f(0.1f, 0.4f, 0.1f);
        glVertex3f(x1, y2,z2);
        glColor3f(0.0f, 0.3f, 0.0f);
        glVertex3f(x1, y2,z1);
        glColor3f(0.1f, 0.4f, 0.1f);
        glVertex3f(x1, y1+0.7,z1);
        glColor3f(0.0f, 0.3f, 0.0f);
        glVertex3f(x1, y1+0.7,z2);

        // Right face (x = +0.5f)
        glColor3f(0.2f, 0.1f, 0.1f);
        glVertex3f(x2, y2-0.3,z1);
        glColor3f(0.1f, 0.0f, 0.0f);
        glVertex3f(x2, y2-0.3,z2);
        glColor3f(0.2f, 0.1f, 0.1f);
        glVertex3f(x2, y1,z2);
        glColor3f(0.1f, 0.0f, 0.0f);
        glVertex3f(x2, y1,z1);
        
        glColor3f(0.1f, 0.4f, 0.1f);
        glVertex3f(x2, y2,z1);
        glColor3f(0.0f, 0.3f, 0.0f);
        glVertex3f(x2, y2,z2);
        glColor3f(0.1f, 0.4f, 0.1f);
        glVertex3f(x2, y1+0.7,z2);
        glColor3f(0.0f, 0.3f, 0.0f);
        glVertex3f(x2, y1+0.7,z1);
    glEnd();
}
void tasKup(float x,float y,float z){
    isColorSetted[make_pair(make_pair(x,-1*y),-1*z)]=1;
    float x1=x-0.5f;
    float x2=x+0.5f;
    
    float y1=y-0.5f;
    float y2=y+0.5f;
    
    float z1=z-0.5f;
    float z2=z+0.5f;
    glBegin(GL_QUADS);
        glColor3f(0.1f, 0.1f, 0.1f);
        glVertex3f(x2,y2,z1);
        glColor3f(0.2f, 0.2f, 0.2f);
        glVertex3f(x1,y2,z1);
        glColor3f(0.1f, 0.1f, 0.1f);
        glVertex3f(x1,y2,z2);
        glColor3f(0.2f, 0.2f, 0.2f);
        glVertex3f(x2,y2,z2);

        // Bottom face (y = -0.5f)
        glColor3f(0.1f, 0.1f, 0.1f);
        glVertex3f(x2,y1,z2);
        glColor3f(0.2f, 0.2f, 0.2f);
        glVertex3f(x1,y1,z2);
        glColor3f(0.1f, 0.1f, 0.1f);
        glVertex3f(x1,y1,z1);
        glColor3f(0.2f, 0.2f, 0.2f);
        glVertex3f(x2,y1,z1);

        // Front face  (z = +0.5f)
        glColor3f(0.1f, 0.1f, 0.1f);
        glVertex3f(x2,y2,z2);
        glColor3f(0.2f, 0.2f, 0.2f);
        glVertex3f(x1,y2,z2);
        glColor3f(0.1f, 0.1f, 0.1f);
        glVertex3f(x1,y1,z2);
        glColor3f(0.2f, 0.2f, 0.2f);
        glVertex3f(x2,y1,z2);

        // Back face (z = -0.5f)
        glColor3f(0.1f, 0.1f, 0.1f);
        glVertex3f(x2,y1,z1);
        glColor3f(0.2f, 0.2f, 0.2f);
        glVertex3f(x1,y1,z1);
        glColor3f(0.1f, 0.1f, 0.1f);
        glVertex3f(x1,y2,z1);
        glColor3f(0.2f, 0.2f, 0.2f);
        glVertex3f(x2,y2,z1);
        
        

        // Left face (x = -0.5f)
        glColor3f(0.1f, 0.1f, 0.1f);
        glVertex3f(x1, y2,z2);
        glColor3f(0.2f, 0.2f, 0.2f);
        glVertex3f(x1, y2,z1);
        glColor3f(0.1f, 0.1f, 0.1f);
        glVertex3f(x1, y1,z1);
        glColor3f(0.2f, 0.2f, 0.2f);
        glVertex3f(x1, y1,z2);

        // Right face (x = +0.5f)
        glColor4f(0.1f, 0.1f, 0.1f,0.5);
        glVertex3f(x2, y2,z1);
        glColor3f(0.2f, 0.2f, 0.2f);
        glVertex3f(x2, y2,z2);
        glColor3f(0.1f, 0.1f, 0.1f);
        glVertex3f(x2, y1,z2);
        glColor3f(0.2f, 0.2f, 0.2f);
        glVertex3f(x2, y1,z1);
    glEnd();
}
Display::~Display(){
    
}
