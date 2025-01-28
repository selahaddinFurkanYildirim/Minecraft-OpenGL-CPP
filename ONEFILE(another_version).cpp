#include <GL/freeglut.h>
#include <vector>
#include <map>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <ctime>

// Yapılar ve global değişkenler
struct Vector3 { float x, y, z; };
struct Block { int x, y, z; };

// Bloklar ve renkleri
std::map<std::tuple<int, int, int>, Vector3> blocks;
bool keys[256];
int mx, my;
float yaw = 0.0f, pitch = 0.0f;
Vector3 position = {0.0f, 1.0f, 0.0f};
Vector3 front = {0.0f, 0.0f, -1.0f};
Vector3 right = {1.0f, 0.0f, 0.0f};
const float speed = 0.1f;
const float sensitivity = 0.1f;
float velocityY = 0.0f;          // Yeni: Dikey hız
const float gravity = -0.0098f;  // Yeni: Yerçekimi
const float jumpForce = 0.15f;   // Yeni: Zıplama kuvveti

void lockMause(int x,int y){
    int screenWidth = glutGet(GLUT_WINDOW_WIDTH);
    int screenHeight = glutGet(GLUT_WINDOW_HEIGHT);
    if (x >= screenWidth - 5) {
        glutWarpPointer(6, y);
    }
    if (x <= 5) {
        glutWarpPointer(screenWidth-6, y);
    }
    if (y >= screenHeight - 5) {
        glutWarpPointer(x, screenHeight - 6);
    }
    if (y <= 5) {
        glutWarpPointer(x, 6);
    }
}
void drawCircle(float x, float y, float radius, float r, float g, float b) {
    glColor3f(r, g, b); // Yuvarlağın rengini ayarla
    glBegin(GL_POLYGON); // Yuvarlak çizmek için POLYGON kullanıyoruz
    for (int i = 0; i < 360; i++) {
        float angle = i * 3.14159f / 180.0f; // Dereceyi radiyana çevir
        float dx = radius * cos(angle); // X koordinatını hesapla
        float dy = radius * sin(angle); // Y koordinatını hesapla
        glVertex2f(x + dx, y + dy); // Yuvarlak üzerindeki noktaları çiz
    }
    glEnd();
}

// Rastgele renk üretme
Vector3 getRandomColor() {
    return {
        static_cast<float>(rand()) / RAND_MAX,
        static_cast<float>(rand()) / RAND_MAX,
        static_cast<float>(rand()) / RAND_MAX
    };
}

// Blokları başlat
void initBlocks() {
    srand(static_cast<unsigned int>(time(0))); // Rastgelelik için seed
    for(int x = -100; x <= 100; x++)
        for(int z = -100; z <= 100; z++)
            blocks[{x, 0, z}] = getRandomColor(); // Her blok için rastgele renk
    blocks[{0, 1, 5}] = getRandomColor();
}

// Çarpışma kontrolü
bool checkCollision(float x, float y, float z) {
    int px1 = static_cast<int>(std::round(x-0.25));
    int py1 = static_cast<int>(std::round(y));
    int pz1 = static_cast<int>(std::round(z-0.25));
    int px2 = static_cast<int>(std::round(x+0.25));
    int py2 = static_cast<int>(std::round(y));
    int pz2 = static_cast<int>(std::round(z+0.25));
    int px3 = static_cast<int>(std::round(x-0.25));
    int py3 = static_cast<int>(std::round(y));
    int pz3 = static_cast<int>(std::round(z+0.25));
    int px4 = static_cast<int>(std::round(x+0.25));
    int py4 = static_cast<int>(std::round(y));
    int pz4 = static_cast<int>(std::round(z-0.25));
    int px5 = static_cast<int>(std::round(x));
    int py5 = static_cast<int>(std::round(y));
    int pz5 = static_cast<int>(std::round(z));
    return blocks.count({px1, py1, pz1}) || blocks.count({px2, py2, pz2}) || blocks.count({px3, py3, pz3}) || blocks.count({px4, py4, pz4}) || blocks.count({px5, py5, pz5});
}

// Kamera güncelleme
void updateCamera() {
    front.x = cos(yaw) * cos(pitch);
    front.y = sin(pitch);
    front.z = sin(yaw) * cos(pitch);
    
    right.x = cos(yaw - 3.14f/2);
    right.z = sin(yaw - 3.14f/2);
}

// Klavye input
void keyboard(unsigned char key, int, int) {
    keys[key] = true;
    // Yeni: Space tuşu için zıplama kontrolü
    if(key == ' ') {
        bool onGround = checkCollision(position.x, position.y - 1.0f, position.z);
        if(onGround) {
            velocityY = jumpForce;
        }
    }
}

void keyboardUp(unsigned char key, int, int) {
    keys[key] = false;
}

// Fare input
void mouse(int x, int y) {
    int centerX = glutGet(GLUT_WINDOW_WIDTH)/2-5;
    int centerY = glutGet(GLUT_WINDOW_HEIGHT)/2-5;
    
    yaw = (x)*(6.28/(centerX*2.0));
    pitch = (y)*(-3/(centerY*2.0))+3.14/2;
    
    updateCamera();
    glutPostRedisplay();
    lockMause(x,y);
}

// Sol tıklama ile blok silme, sağ tıklama ile blok ekleme
void mouseClick(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // Sol tıklama: Blok silme
        Vector3 rayOrigin = position;
        Vector3 rayDirection = front;

        float maxDistance = 50.0f; // Maksimum ışın mesafesi
        float step = 0.5f;         // Işın adımı

        for (float t = 0.0f; t < maxDistance; t += step) {
            Vector3 rayPoint = {
                rayOrigin.x + rayDirection.x * t,
                rayOrigin.y + rayDirection.y * t,
                rayOrigin.z + rayDirection.z * t
            };

            int px = static_cast<int>(std::round(rayPoint.x));
            int py = static_cast<int>(std::round(rayPoint.y));
            int pz = static_cast<int>(std::round(rayPoint.z));

            if (blocks.count({px, py, pz})) {
                // Blok bulundu, sil
                blocks.erase({px, py, pz});
                glutPostRedisplay();
                break;
            }
        }
    } else if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
        // Sağ tıklama: Blok ekleme
        Vector3 rayOrigin = position;
        Vector3 rayDirection = front;

        float maxDistance = 50.0f; // Maksimum ışın mesafesi
        float step = 0.1f;         // Işın adımı

        for (float t = 0.0f; t < maxDistance; t += step) {
            Vector3 rayPoint = {
                rayOrigin.x + rayDirection.x * t,
                rayOrigin.y + rayDirection.y * t,
                rayOrigin.z + rayDirection.z * t
            };

            int px = static_cast<int>(std::round(rayPoint.x));
            int py = static_cast<int>(std::round(rayPoint.y));
            int pz = static_cast<int>(std::round(rayPoint.z));

            if (blocks.count({px, py, pz})) {
                // Blok bulundu, baktığımız yüzeyi belirle
                Vector3 normal = {0.0f, 0.0f, 0.0f};
                Vector3 hitPoint = {rayPoint.x, rayPoint.y, rayPoint.z};

                // Işın bloğun hangi yüzeyine çarptı?
                if (std::abs(hitPoint.x - px - 0.5f) < 0.1f) normal.x = 1.0f; // Sağ yüzey
                else if (std::abs(hitPoint.x - px + 0.5f) < 0.1f) normal.x = -1.0f; // Sol yüzey
                else if (std::abs(hitPoint.y - py - 0.5f) < 0.1f) normal.y = 1.0f; // Üst yüzey
                else if (std::abs(hitPoint.y - py + 0.5f) < 0.1f) normal.y = -1.0f; // Alt yüzey
                else if (std::abs(hitPoint.z - pz - 0.5f) < 0.1f) normal.z = 1.0f; // Ön yüzey
                else if (std::abs(hitPoint.z - pz + 0.5f) < 0.1f) normal.z = -1.0f; // Arka yüzey

                // Yeni blok ekle
                int newX = px + static_cast<int>(normal.x);
                int newY = py + static_cast<int>(normal.y);
                int newZ = pz + static_cast<int>(normal.z);

                if (!blocks.count({newX, newY, newZ})) {
                    blocks[{newX, newY, newZ}] = getRandomColor(); // Yeni blok için rastgele renk
                    glutPostRedisplay();
                }
                break;
            }
        }
    }
     while(checkCollision(position.x,position.y-0.5,position.z)) position.y+=0.01;
}

// Hareket güncelleme
void update() {
    // printf("%lf,%lf,%lf\n",position.y,position.z,position.x);
    if(position.y< -250){
        position.y=1,position.z=0,position.x=0;
        velocityY=0;
    }
    Vector3 newPos = position;
    updateCamera();

    // Yatay hareket
    if(keys['w']) { newPos.x += front.x * speed; newPos.z += front.z * speed; }
    if(keys['s']) { newPos.x -= front.x * speed; newPos.z -= front.z * speed; }
    if(keys['d']) { newPos.x -= right.x * speed; newPos.z -= right.z * speed; }
    if(keys['a']) { newPos.x += right.x * speed; newPos.z += right.z * speed; }

    // Yerçekimi
    velocityY += gravity;

    // Dikey hareket
    newPos.y += velocityY;

    // Zemin kontrolü
    bool onGround = checkCollision(newPos.x, newPos.y - 0.5f, newPos.z);

    if (onGround) {
        newPos.y = std::floor(newPos.y); // Zemine sabitle
        velocityY = 0;                  // Düşmeyi durdur
    } else {
        position.y = newPos.y; // Havada serbest düşüş
    }
	while(checkCollision(position.x,position.y-0.5,position.z)) position.y+=0.01;
    // Çarpışma kontrolleri
    if (!checkCollision(newPos.x, position.y, newPos.z)) {
        position.x = newPos.x;
        position.z = newPos.z;
    }

    glutPostRedisplay();
}

// Blok çizme (tüm yüzler aynı renk)
void drawBlock(int x, int y, int z, const Vector3& color) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glColor3f(color.x, color.y, color.z); // Bloğun rengi

    // Tüm yüzler aynı renk
    glBegin(GL_QUADS);
        // Ön yüz
        glVertex3f(-0.5f, -0.5f,  0.5f);
        glVertex3f( 0.5f, -0.5f,  0.5f);
        glVertex3f( 0.5f,  0.5f,  0.5f);
        glVertex3f(-0.5f,  0.5f,  0.5f);

        // Arka yüz
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f, -0.5f);
        glVertex3f(-0.5f,  0.5f, -0.5f);

        // Üst yüz
        glVertex3f(-0.5f,  0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f,  0.5f);
        glVertex3f(-0.5f,  0.5f,  0.5f);

        // Alt yüz
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f, -0.5f,  0.5f);
        glVertex3f(-0.5f, -0.5f,  0.5f);

        // Sağ yüz
        glVertex3f( 0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f,  0.5f);
        glVertex3f( 0.5f, -0.5f,  0.5f);

        // Sol yüz
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f(-0.5f,  0.5f, -0.5f);
        glVertex3f(-0.5f,  0.5f,  0.5f);
        glVertex3f(-0.5f, -0.5f,  0.5f);
    glEnd();

    glPopMatrix();
}

// Ekran çizme
void drawText(const char* text, float x, float y, float r, float g, float b) {
    glColor3f(r, g, b); // Yazı rengini ayarla
    glRasterPos2f(x, y); // Metnin ekran konumunu ayarla
    for (const char* c = text; *c != '\0'; ++c) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c); // Her karakteri çiz
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // 3D kamera ayarları
    gluLookAt(position.x, position.y, position.z,
              position.x + front.x, position.y + front.y, position.z + front.z,
              0.0f, 1.0f, 0.0f);

    // Blokları çiz
    for(auto& block : blocks) {
        int x = std::get<0>(block.first);
        int y = std::get<1>(block.first);
        int z = std::get<2>(block.first);
        Vector3 color = block.second;
        drawBlock(x, y, z, color);
    }
    
    // 2D metin çizim moduna geç
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 800, 0, 600); // 2D koordinat sistemi
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Pozisyonu metin olarak yaz
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "X: %.2f Y: %.2f Z: %.2f", position.x, position.y, position.z);
    drawText(buffer, 10, 570, 1.0f, 0.0f, 0.0f); // Sol üst (10, 570) konumunda kırmızı renk
    drawCircle(400, 300, 5, 0.0f, 0.0f, 1.0f);    
    // 2D moddan çık
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);

    glutSwapBuffers();
}

// Pencere boyutu
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, (double)w/h, 0.1, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(800, 600);
    glutCreateWindow("Minecraft Benzeri Oyun");
    
    initBlocks();
    glEnable(GL_DEPTH_TEST);	
    
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutMotionFunc(mouse);
    glutPassiveMotionFunc(mouse);
    glutMouseFunc(mouseClick); // Fare tıklama olayını yakala
    glutSetCursor(GLUT_CURSOR_NONE);
    glutIdleFunc(update);
    
    glutMainLoop();
    return 0;
}
