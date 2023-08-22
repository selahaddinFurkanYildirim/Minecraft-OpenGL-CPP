#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <bits/stdc++.h>
using namespace std;

void TamEkran();
bool canIgo(float x,float y,float z);
void cube(float x,float y,float z,int color=rand()%7);
bool isKeyPressed(char c);
void lockMause(int x,int y);
void cubeGround(float x,float y,float z);
void tasKup(float x,float y,float z);

class Display
{
private:
    static void Reshape(int w,int h);
    Display(const Display &);
    void (*myTimerFunc)(int);
    void (*myMauseFunc)(int x, int y);
    function<void()> init=[](){};
    static void Update();
    static void Update(function<void()>);
public:
    float camposx=0,camposy=0,camposz=0;
    float camPosXartis=0,camPosYartis=0,camPosZartis=0;
    
    void SetInitFunc(function<void()>);
    void Start(int,int,string);
    void SetUpdateFunc(function<void()>);
    void SetMauseFunc(void (*func)(int x, int y));
    void SetTimerFunc(void (*)(int));
    void SetBKColor(float,float,float,float);
    void HareketKontrol(float,float,float);
    Display();
    ~Display();
};

#endif
