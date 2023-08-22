#include "../inc/display.hpp"

float camRotX=0,camRotY=0;
Display display;

void Timer(int){
    float xyonu=(isKeyPressed('d')*-0.1)+(isKeyPressed('a')*0.1);
    float zyonu=(isKeyPressed('s')*-0.1)+(isKeyPressed('w')*0.1);
    display.HareketKontrol(xyonu,zyonu,camRotX);
    if(isKeyPressed('r'))
    display.camposy=-200;
    glutPostRedisplay();
    glutTimerFunc(15,Timer,0);
}

void mauseFunc(int x, int y){
    camRotX=1*(x/(glutGet(GLUT_WINDOW_WIDTH)/360.0))+180;
    camRotY=1*(y/(glutGet(GLUT_WINDOW_HEIGHT)/200.0))-80;
    lockMause(x,y);
}

int main(){
    display.SetTimerFunc(Timer);
    function<void()> Update=[&](){
        glRotatef(camRotY,1,0,0);
        glRotatef(camRotX,0,1,0);
        glTranslatef( display.camposx,display.camposy-0.7,display.camposz);
        for(int i=0;i<50;i++){
            for(int j=0;j<50;j++){
                cubeGround(i-25,-3,j-25);
            }
        }
        tasKup(0,-2,0);
        cubeGround(0,-1,-4);
    };
    display.SetMauseFunc(mauseFunc);
    display.SetUpdateFunc(Update);
    display.Start(500,500,"Hello world!!!");
    return 0;
}
