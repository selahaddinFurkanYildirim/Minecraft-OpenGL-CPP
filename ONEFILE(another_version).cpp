#include <GL/freeglut.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <array>
#include <algorithm>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

// ---------------- Global Ayarlar ----------------

// Dünya boyutları ve ofset değerleri
const int WORLD_WIDTH  = 1000;   // X yönü
const int WORLD_HEIGHT = 100;    // Y yönü
const int WORLD_DEPTH  = 1000;   // Z yönü
const int OFFSET_X = WORLD_WIDTH / 2;
const int OFFSET_Z = WORLD_DEPTH / 2;

// Chunk (parça) boyutu (X-Z düzleminde)
const int CHUNK_SIZE = 16;
// Chunk’lerin adedi
const int NUM_CHUNKS_X = (WORLD_WIDTH + CHUNK_SIZE - 1) / CHUNK_SIZE;
const int NUM_CHUNKS_Z = (WORLD_DEPTH + CHUNK_SIZE - 1) / CHUNK_SIZE;

// Görüş mesafesi (dünya koordinatlarında – bu değer üzerinden chunk yükleme/boşaltma yapılacak)
const int renderDistance = 32;

bool isCTRL = false;
int sel = 1;

// ---------------- Vektör ve Blok Tanımları ----------------

struct Vector3 {
    float x, y, z;
    bool operator==(const Vector3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct Block {
    bool exists = false;
    bool isWater = false;
    std::array<Vector3, 6> colors;
};

struct Chunk {
    int chunkX, chunkZ; // Chunk’ın dünya içindeki koordinatları
    bool dirty = false; // Üzerinde değişiklik yapılmış mı?
    // Blok dizisi: [localX][y][localZ]
    Block blocks[CHUNK_SIZE][WORLD_HEIGHT][CHUNK_SIZE];
};

// Global chunk dizisi, dinamik yükleme/boşaltma için pointer’lı tutuyoruz.
Chunk* worldChunks[NUM_CHUNKS_X][NUM_CHUNKS_Z] = { { nullptr } };

// Diğer global değişkenler
bool keys[256];
int mx, my;
float yaw = 0.0f, pitch = 0.0f;
Vector3 position = {0.0f, 10.0f, 20.0f};
Vector3 front = {0.0f, 0.0f, -1.0f};
Vector3 right = {1.0f, 0.0f, 0.0f};
const float speed = 0.2f;
const float sensitivity = 0.1f;
float velocityY = 0.0f;
const float gravity = -0.009f;
const float jumpForce = 0.18f;

// FPS hesaplaması
int frameCount = 0;
int lastTime = 0;
float currentFPS = 0.0f;

// ---------------- Asenkron Disk Yazma İçin Global Değişkenler ----------------

std::queue<std::pair<int,int>> chunkSaveQueue;
std::mutex queueMutex;
std::condition_variable cv;
std::mutex diskMutex; // Save işlemi sırasında eşzamanlılık için
bool exitThread = false;

// ---------------- Yardımcı Fonksiyonlar ----------------

std::array<Vector3, 6> getGrassBlockColor() {
    std::array<Vector3, 6> colors;
    colors[2] = {0.0f, 1.0f, 0.0f}; // Üst yüz
    for (int i = 0; i < 6; i++) {
        if(i != 2)
            colors[i] = {0.55f, 0.27f, 0.07f};
    }
    return colors;
}

std::array<Vector3, 6> getDirtBlockColor() {
    std::array<Vector3, 6> colors;
    for (int i = 0; i < 6; ++i) {
        colors[i] = {0.40f, 0.20f, 0.07f};
    }
    return colors;
}

std::array<Vector3, 6> getStoneBlockColor() {
    std::array<Vector3, 6> colors;
    for (int i = 0; i < 6; ++i) {
        colors[i] = {0.7f, 0.7f, 0.7f};
    }
    return colors;
}

std::array<Vector3, 6> getWaterBlockColor() {
    std::array<Vector3, 6> colors;
    for (int i = 0; i < 6; i++) {
        colors[i] = {0.0f, 0.3f, 1.0f};
    }
    return colors;
}

// Dünya koordinatlarını global indekse çevirir.
inline int worldToGlobalX(int worldX) {
    return worldX + OFFSET_X;
}
inline int worldToGlobalZ(int worldZ) {
    return worldZ + OFFSET_Z;
}

// Dünya koordinatlarını chunk ve yerel (local) koordinatlara çevirir.
bool worldToChunkCoords(int worldX, int worldY, int worldZ,
                        int &chunkX, int &chunkZ,
                        int &localX, int &localZ) {
    int globalX = worldToGlobalX(worldX);
    int globalZ = worldToGlobalZ(worldZ);
    if (globalX < 0 || globalX >= WORLD_WIDTH ||
        worldY < 0 || worldY >= WORLD_HEIGHT ||
        globalZ < 0 || globalZ >= WORLD_DEPTH)
        return false;
    chunkX = globalX / CHUNK_SIZE;
    chunkZ = globalZ / CHUNK_SIZE;
    localX = globalX % CHUNK_SIZE;
    localZ = globalZ % CHUNK_SIZE;
    return true;
}

// Verilen dünya koordinatındaki bloğu elde eder.
Block* getBlock(int worldX, int worldY, int worldZ) {
    int chunkX, chunkZ, localX, localZ;
    if (!worldToChunkCoords(worldX, worldY, worldZ, chunkX, chunkZ, localX, localZ))
        return nullptr;
    if(worldChunks[chunkX][chunkZ] == nullptr)
        return nullptr;
    return &worldChunks[chunkX][chunkZ]->blocks[localX][worldY][localZ];
}

bool blockExistsAt(int worldX, int worldY, int worldZ) {
    Block* block = getBlock(worldX, worldY, worldZ);
    return (block && block->exists);
}

// Değişiklik yapılan chunk’ı "dirty" olarak işaretleyip asenkron kaydetme kuyruğuna ekler.
void markChunkDirty(int worldX, int worldY, int worldZ) {
    int chunkX, chunkZ, localX, localZ;
    if (!worldToChunkCoords(worldX, worldY, worldZ, chunkX, chunkZ, localX, localZ))
        return;
    if(worldChunks[chunkX][chunkZ] != nullptr) {
        worldChunks[chunkX][chunkZ]->dirty = true;
        std::lock_guard<std::mutex> lock(queueMutex);
        chunkSaveQueue.push({chunkX, chunkZ});
        cv.notify_one();
    }
}

void setBlock(int worldX, int worldY, int worldZ, const std::array<Vector3, 6>& colors) {
    Block* block = getBlock(worldX, worldY, worldZ);
    if (!block) return;
    block->exists = true;
    block->isWater = (colors == getWaterBlockColor());
    block->colors = colors;
    markChunkDirty(worldX, worldY, worldZ);
}

void removeBlock(int worldX, int worldY, int worldZ) {
    Block* block = getBlock(worldX, worldY, worldZ);
    if (!block) return;
    block->exists = false;
    markChunkDirty(worldX, worldY, worldZ);
}

bool isBlockExposed(int worldX, int worldY, int worldZ) {
    return !blockExistsAt(worldX+1, worldY, worldZ) ||
           !blockExistsAt(worldX-1, worldY, worldZ) ||
           !blockExistsAt(worldX, worldY+1, worldZ) ||
           !blockExistsAt(worldX, worldY-1, worldZ) ||
           !blockExistsAt(worldX, worldY, worldZ+1) ||
           !blockExistsAt(worldX, worldY, worldZ-1);
}

void lockMause(int x, int y) {
    int screenWidth = glutGet(GLUT_WINDOW_WIDTH);
    int screenHeight = glutGet(GLUT_WINDOW_HEIGHT);
    if (x >= screenWidth - 5) {
        glutWarpPointer(6, y);
    }
    if (x <= 5) {
        glutWarpPointer(screenWidth - 6, y);
    }
    if (y >= screenHeight - 5) {
        glutWarpPointer(x, screenHeight - 6);
    }
    if (y <= 5) {
        glutWarpPointer(x, 6);
    }
}

void drawCircle(float x, float y, float radius, float r, float g, float b) {
    glColor3f(r, g, b);
    glBegin(GL_POLYGON);
    for (int i = 0; i < 360; i++) {
        float angle = i * 3.14159f / 180.0f;
        float dx = radius * cos(angle) / 1.25f;
        float dy = radius * sin(angle);
        glVertex2f(x + dx, y + dy);
    }
    glEnd();
}

void drawText(const char* text, float x, float y, float r, float g, float b) {
    glColor3f(r, g, b);
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; ++c) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
    }
}

// ---------------- Arazi Oluşturma (Gürültü Tabanlı) ----------------

float noise(int x, int z) {
    int n = x + z * 57;
    n = (n << 13) ^ n;
    int nn = (n * (n * n * 15731 + 789221) + 1376312589);
    return 1.0f - (nn & 0x7fffffff) / 1073741824.0f;
}

float interpolate(float a, float b, float t) {
    return a * (1 - t) + b * t;
}

float smoothNoise(float x, float z) {
    int intX = static_cast<int>(std::floor(x));
    int intZ = static_cast<int>(std::floor(z));
    float fracX = x - intX;
    float fracZ = z - intZ;
    
    float v1 = noise(intX, intZ);
    float v2 = noise(intX + 1, intZ);
    float v3 = noise(intX, intZ + 1);
    float v4 = noise(intX + 1, intZ + 1);
    
    float i1 = interpolate(v1, v2, fracX);
    float i2 = interpolate(v3, v4, fracX);
    
    return interpolate(i1, i2, fracZ);
}

float fractalNoise(float x, float z, int octaves = 4, float persistence = 0.5f) {
    float total = 0.0f;
    float frequency = 0.01f;
    float amplitude = 10.0f;
    float maxValue = 0.0f;
    
    for (int i = 0; i < octaves; i++) {
        total += smoothNoise(x * frequency, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2.0f;
    }
    return total / maxValue;
}

float getHeight(int x, int z) {
    float n = fractalNoise(static_cast<float>(x), static_cast<float>(z), 5, 0.5f);
    float height = n * 20.0f + 10.0f;
    if (height < 0) height = 0;
    if (height >= WORLD_HEIGHT) height = WORLD_HEIGHT - 1;
    return std::round(height);
}

std::array<Vector3, 6> getColor() {
    if(sel == 1) return getGrassBlockColor();
    if(sel == 2) return getDirtBlockColor();
    if(sel == 3) return getStoneBlockColor();
    if(sel == 4) return getWaterBlockColor();
    std::array<Vector3, 6> colors;
    for (int i = 0; i < 6; ++i) {
        colors[i] = { static_cast<float>(rand()) / RAND_MAX,
                      static_cast<float>(rand()) / RAND_MAX,
                      static_cast<float>(rand()) / RAND_MAX };
    }
    return colors;
}

// ---------------- Disk İşlemleri: Chunk Kaydetme ve Yükleme ----------------

std::string getChunkFilename(int cx, int cz) {
    std::ostringstream oss;
    oss << "chunk_" << cx << "_" << cz << ".bin";
    return oss.str();
}

// Synchronized (mutex ile korunan) save işlemi.
void saveChunk(int cx, int cz) {
    std::lock_guard<std::mutex> lock(diskMutex);
    if (worldChunks[cx][cz] == nullptr)
        return;
    std::string filename = getChunkFilename(cx, cz);
    std::ofstream out(filename, std::ios::binary);
    if (!out) return;
    out.write(reinterpret_cast<char*>(&worldChunks[cx][cz]->chunkX), sizeof(int));
    out.write(reinterpret_cast<char*>(&worldChunks[cx][cz]->chunkZ), sizeof(int));
    for (int lx = 0; lx < CHUNK_SIZE; lx++) {
        for (int y = 0; y < WORLD_HEIGHT; y++) {
            for (int lz = 0; lz < CHUNK_SIZE; lz++) {
                Block &block = worldChunks[cx][cz]->blocks[lx][y][lz];
                out.write(reinterpret_cast<char*>(&block.exists), sizeof(bool));
                out.write(reinterpret_cast<char*>(block.colors.data()), sizeof(Vector3) * 6);
            }
        }
    }
    out.close();
    // Kaydedildi; artık dirty bayrağını sıfırlıyoruz.
    worldChunks[cx][cz]->dirty = false;
}

void saveAllChunks() {
    for (int cx = 0; cx < NUM_CHUNKS_X; cx++) {
        for (int cz = 0; cz < NUM_CHUNKS_Z; cz++) {
            Chunk* chunk = worldChunks[cx][cz];
            if (chunk != nullptr && chunk->dirty) {
                saveChunk(cx, cz);
            }
        }
    }
    printf("Tüm aktif chunk'lar kaydedildi.\n");
}

void loadChunk(int cx, int cz) {
    if (worldChunks[cx][cz] != nullptr)
        return;
    std::string filename = getChunkFilename(cx, cz);
    Chunk* chunk = new Chunk();
    std::ifstream in(filename, std::ios::binary);
    if (in) {
        in.read(reinterpret_cast<char*>(&chunk->chunkX), sizeof(int));
        in.read(reinterpret_cast<char*>(&chunk->chunkZ), sizeof(int));
        for (int lx = 0; lx < CHUNK_SIZE; lx++) {
            for (int y = 0; y < WORLD_HEIGHT; y++) {
                for (int lz = 0; lz < CHUNK_SIZE; lz++) {
                    Block &block = chunk->blocks[lx][y][lz];
                    in.read(reinterpret_cast<char*>(&block.exists), sizeof(bool));
                    in.read(reinterpret_cast<char*>(block.colors.data()), sizeof(Vector3) * 6);
                }
            }
        }
        in.close();
        chunk->dirty = false;
    } else {
        chunk->chunkX = cx;
        chunk->chunkZ = cz;
        for (int lx = 0; lx < CHUNK_SIZE; lx++) {
            for (int lz = 0; lz < CHUNK_SIZE; lz++) {
                int worldX = cx * CHUNK_SIZE + lx - OFFSET_X;
                int worldZ = cz * CHUNK_SIZE + lz - OFFSET_Z;
                int h = static_cast<int>(getHeight(worldX, worldZ));
                if (h >= WORLD_HEIGHT)
                    h = WORLD_HEIGHT - 1;
                for (int y = 0; y < WORLD_HEIGHT; y++) {
                    Block &block = chunk->blocks[lx][y][lz];
                    if (y <= h) {
                        block.exists = true;
                        if (y == h)
                            block.colors = getGrassBlockColor();
                        else
                            block.colors = getDirtBlockColor();
                    } else {
                        block.exists = false;
                    }
                }
            }
        }
        chunk->dirty = false;
        saveChunk(cx, cz);
    }
    worldChunks[cx][cz] = chunk;
}

void unloadChunk(int cx, int cz) {
    if (worldChunks[cx][cz] != nullptr) {
        // Eğer chunk dirty ise, unload sırasında senkron kaydet.
        if(worldChunks[cx][cz]->dirty)
            saveChunk(cx, cz);
        delete worldChunks[cx][cz];
        worldChunks[cx][cz] = nullptr;
    }
}

// Oyuncunun bulunduğu chunk çevresindeki chunk’ları yükler.
void loadChunksInView() {
    int playerGlobalX = position.x + OFFSET_X;
    int playerGlobalZ = position.z + OFFSET_Z;
    int playerChunkX = playerGlobalX / CHUNK_SIZE;
    int playerChunkZ = playerGlobalZ / CHUNK_SIZE;
    int loadRadius = renderDistance / CHUNK_SIZE + 1;
    for (int cx = std::max(0, playerChunkX - loadRadius); cx < std::min(NUM_CHUNKS_X, playerChunkX + loadRadius + 1); cx++) {
        for (int cz = std::max(0, playerChunkZ - loadRadius); cz < std::min(NUM_CHUNKS_Z, playerChunkZ + loadRadius + 1); cz++) {
            loadChunk(cx, cz);
        }
    }
}

// Görüş alanından uzak kalan chunk’ları boşaltır.
void unloadOutOfViewChunks() {
    int playerGlobalX = position.x + OFFSET_X;
    int playerGlobalZ = position.z + OFFSET_Z;
    int playerChunkX = playerGlobalX / CHUNK_SIZE;
    int playerChunkZ = playerGlobalZ / CHUNK_SIZE;
    int unloadRadius = renderDistance / CHUNK_SIZE + 2;
    for (int cx = 0; cx < NUM_CHUNKS_X; cx++) {
        for (int cz = 0; cz < NUM_CHUNKS_Z; cz++) {
            if (worldChunks[cx][cz] != nullptr) {
                if (abs(cx - playerChunkX) > unloadRadius || abs(cz - playerChunkZ) > unloadRadius) {
                    unloadChunk(cx, cz);
                }
            }
        }
    }
}

// ---------------- Çarpışma Kontrolü ----------------

bool playerCollidesAt(const Vector3& pos) {
    const float halfWidth = 0.3f;
    float pMinX = pos.x - halfWidth;
    float pMaxX = pos.x + halfWidth;
    float pMinY = pos.y - 1.5f;
    float pMaxY = pos.y - 0.5f;
    float pMinZ = pos.z - halfWidth;
    float pMaxZ = pos.z + halfWidth;
    
    int iMin = static_cast<int>(std::floor(pMinX + 0.5f));
    int iMax = static_cast<int>(std::floor(pMaxX + 0.5f));
    int jMin = static_cast<int>(std::floor(pMinY + 0.5f));
    int jMax = static_cast<int>(std::floor(pMaxY + 0.5f));
    int kMin = static_cast<int>(std::floor(pMinZ + 0.5f));
    int kMax = static_cast<int>(std::floor(pMaxZ + 0.5f));
    
    for (int i = iMin; i <= iMax; ++i) {
        for (int j = jMin; j <= jMax; ++j) {
            for (int k = kMin; k <= kMax; ++k) {
                if (blockExistsAt(i, j, k)) {
                    float bMinX = i - 0.5f, bMaxX = i + 0.5f;
                    float bMinY = j - 0.5f, bMaxY = j + 0.5f;
                    float bMinZ = k - 0.5f, bMaxZ = k + 0.5f;
                    if (pMaxX > bMinX && pMinX < bMaxX &&
                        pMaxY > bMinY && pMinY < bMaxY &&
                        pMaxZ > bMinZ && pMinZ < bMaxZ)
                        return true;
                }
            }
        }
    }
    return false;
}

bool isOnGround() {
    Vector3 testPos = position;
    testPos.y -= 0.15f;
    return playerCollidesAt(testPos);
}

// ---------------- Kamera ve Giriş ----------------

void updateCamera() {
    front.x = cos(yaw) * cos(pitch);
    front.y = sin(pitch);
    front.z = sin(yaw) * cos(pitch);
    
    right.x = cos(yaw - 3.14f/2);
    right.z = sin(yaw - 3.14f/2);
}

void keyboard(unsigned char key, int x, int y) {
    keys[key] = true;
    isCTRL = false;
    if (key == '1') { sel = 1; }
    if (key == '2') { sel = 2; }
    if (key == '3') { sel = 3; }
    if (key == '4') { sel = 4; }
    if (key == '5') { sel = 5; }
    if (key <= 23) { isCTRL = true; keys[key + 96] = true; }
    if (key == 'o' || key == 'O') {
        saveAllChunks();
    }
    if (key == 0) {
        isCTRL = true;
        if (isOnGround()) { velocityY = jumpForce; }
    }
    if (key == ' ') {
        if (isOnGround()) { velocityY = jumpForce; }
    }
}

void keyboardUp(unsigned char key, int, int) {
    keys[key] = false;
}

void mouse(int x, int y) {
    int centerX = glutGet(GLUT_WINDOW_WIDTH) / 2 - 5;
    int centerY = glutGet(GLUT_WINDOW_HEIGHT) / 2 - 5;
    
    yaw = (x) * (6.28f / (centerX * 2.0f));
    pitch = (y) * (-3.0f / (centerY * 2.0f)) + 3.14f / 2.0f;
    
    updateCamera();
    glutPostRedisplay();
    lockMause(x, y);
}

void mouseClick(int button, int state, int x, int y) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        Vector3 rayOrigin = position;
        Vector3 rayDirection = front;
        float maxDistance = 20.0f;
        float step = 0.5f;
        for (float t = 0.0f; t < maxDistance; t += step) {
            Vector3 rayPoint = {
                rayOrigin.x + rayDirection.x * t,
                rayOrigin.y + rayDirection.y * t,
                rayOrigin.z + rayDirection.z * t
            };
            int ix = static_cast<int>(std::round(rayPoint.x));
            int iy = static_cast<int>(std::round(rayPoint.y));
            int iz = static_cast<int>(std::round(rayPoint.z));
            if (blockExistsAt(ix, iy, iz)) {
                removeBlock(ix, iy, iz);
                glutPostRedisplay();
                break;
            }
        }
    } else if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
        Vector3 rayOrigin = position;
        Vector3 rayDirection = front;
        float maxDistance = 20.0f;
        float step = 0.1f;
        for (float t = 0.0f; t < maxDistance; t += step) {
            Vector3 rayPoint = {
                rayOrigin.x + rayDirection.x * t,
                rayOrigin.y + rayDirection.y * t,
                rayOrigin.z + rayDirection.z * t
            };
            int ix = static_cast<int>(std::round(rayPoint.x));
            int iy = static_cast<int>(std::round(rayPoint.y));
            int iz = static_cast<int>(std::round(rayPoint.z));
            if (blockExistsAt(ix, iy, iz)) {
                Vector3 normal = {0.0f, 0.0f, 0.0f};
                Vector3 hitPoint = rayPoint;
                if (std::abs(hitPoint.x - ix - 0.5f) < 0.1f) normal.x = 1.0f;
                else if (std::abs(hitPoint.x - ix + 0.5f) < 0.1f) normal.x = -1.0f;
                else if (std::abs(hitPoint.y - iy - 0.5f) < 0.1f) normal.y = 1.0f;
                else if (std::abs(hitPoint.y - iy + 0.5f) < 0.1f) normal.y = -1.0f;
                else if (std::abs(hitPoint.z - iz - 0.5f) < 0.1f) normal.z = 1.0f;
                else if (std::abs(hitPoint.z - iz + 0.5f) < 0.1f) normal.z = -1.0f;
                
                int newX = ix + static_cast<int>(normal.x);
                int newY = iy + static_cast<int>(normal.y);
                int newZ = iz + static_cast<int>(normal.z);
                if (!blockExistsAt(newX, newY, newZ)) {
                    setBlock(newX, newY, newZ, getColor());
                    glutPostRedisplay();
                }
                break;
            }
        }
    }
    glutPostRedisplay();
}

// ---------------- Hareket Güncelleme ----------------

void update() {
    if (position.y < -250) {
        position = {0.0f, 10.0f, 20.0f};
        velocityY = 0;
    }
    
    float moveSpeed = speed;
    if (isCTRL) moveSpeed *= 3;
    float dx = 0.0f, dz = 0.0f;
    if (keys['w']) { dx += front.x * moveSpeed; dz += front.z * moveSpeed; }
    if (keys['s']) { dx -= front.x * moveSpeed; dz -= front.z * moveSpeed; }
    if (keys['a']) { dx += right.x * moveSpeed; dz += right.z * moveSpeed; }
    if (keys['d']) { dx -= right.x * moveSpeed; dz -= right.z * moveSpeed; }
    
    Vector3 testPosX = position;
    testPosX.x += dx;
    if (!playerCollidesAt(testPosX))
        position.x = testPosX.x;
    
    Vector3 testPosZ = position;
    testPosZ.z += dz;
    if (!playerCollidesAt(testPosZ))
        position.z = testPosZ.z;
    
    velocityY += gravity;
    Vector3 testPosY = position;
    testPosY.y += velocityY;
    if (!playerCollidesAt(testPosY))
        position.y = testPosY.y;
    else {
        velocityY = 0;
        while (playerCollidesAt(position))
            position.y += 0.001f;
    }
    
    loadChunksInView();
    unloadOutOfViewChunks();
    
    glutPostRedisplay();
}

// ---------------- Blok Çizimi ----------------

void drawBlock(int worldX, int worldY, int worldZ, const std::array<Vector3, 6>& colors, float distance) {
    if (distance > renderDistance) return;
    glPushMatrix();
    glTranslatef(static_cast<float>(worldX), static_cast<float>(worldY), static_cast<float>(worldZ));
    
    float fadeFactor = 1.0f - (std::log(distance + 1.0f) / 7.0f);
    fadeFactor = std::max(0.5f, fadeFactor);
    
    bool isWater = (colors[0].x == 0.0f && colors[0].y == 0.3f && colors[0].z == 1.0f);
    if(isWater) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    
    glBegin(GL_QUADS);
        // Ön yüz (0)
        if(isWater) glColor4f(colors[0].x * fadeFactor, colors[0].y * fadeFactor, colors[0].z * fadeFactor, 0.5f);
        else glColor3f(colors[0].x * fadeFactor, colors[0].y * fadeFactor, colors[0].z * fadeFactor);
        glVertex3f(-0.5f, -0.5f,  0.5f);
        glVertex3f( 0.5f, -0.5f,  0.5f);
        glVertex3f( 0.5f,  0.5f,  0.5f);
        glVertex3f(-0.5f,  0.5f,  0.5f);
        
        // Arka yüz (1)
        if(isWater) glColor4f(colors[1].x * fadeFactor, colors[1].y * fadeFactor, colors[1].z * fadeFactor, 0.5f);
        else glColor3f(colors[1].x * fadeFactor, colors[1].y * fadeFactor, colors[1].z * fadeFactor);
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f, -0.5f);
        glVertex3f(-0.5f,  0.5f, -0.5f);
        
        // Üst yüz (2)
        if(isWater) glColor4f(colors[2].x * fadeFactor, colors[2].y * fadeFactor, colors[2].z * fadeFactor, 0.5f);
        else glColor3f(colors[2].x * fadeFactor, colors[2].y * fadeFactor, colors[2].z * fadeFactor);
        glVertex3f(-0.5f,  0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f,  0.5f);
        glVertex3f(-0.5f,  0.5f,  0.5f);
        
        // Alt yüz (3)
        if(isWater) glColor4f(colors[3].x * fadeFactor, colors[3].y * fadeFactor, colors[3].z * fadeFactor, 0.5f);
        else glColor3f(colors[3].x * fadeFactor, colors[3].y * fadeFactor, colors[3].z * fadeFactor);
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f, -0.5f,  0.5f);
        glVertex3f(-0.5f, -0.5f,  0.5f);
        
        // Sağ yüz (4)
        if(isWater) glColor4f(colors[4].x * fadeFactor, colors[4].y * fadeFactor, colors[4].z * fadeFactor, 0.5f);
        else glColor3f(colors[4].x * fadeFactor, colors[4].y * fadeFactor, colors[4].z * fadeFactor);
        glVertex3f( 0.5f, -0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f, -0.5f);
        glVertex3f( 0.5f,  0.5f,  0.5f);
        glVertex3f( 0.5f, -0.5f,  0.5f);
        
        // Sol yüz (5)
        if(isWater) glColor4f(colors[5].x * fadeFactor, colors[5].y * fadeFactor, colors[5].z * fadeFactor, 0.5f);
        else glColor3f(colors[5].x * fadeFactor, colors[5].y * fadeFactor, colors[5].z * fadeFactor);
        glVertex3f(-0.5f, -0.5f, -0.5f);
        glVertex3f(-0.5f,  0.5f, -0.5f);
        glVertex3f(-0.5f,  0.5f,  0.5f);
        glVertex3f(-0.5f, -0.5f,  0.5f);
    glEnd();
    
    if(isWater)
        glDisable(GL_BLEND);
    
    glPopMatrix();
}

// ---------------- Ekran Çizimi ----------------

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    
    gluLookAt(position.x, position.y, position.z,
              position.x + front.x, position.y + front.y, position.z + front.z,
              0.0f, 1.0f, 0.0f);
    
    // Bellekte yüklü chunk’ları tarayarak blokları çiz.
    for (int cx = 0; cx < NUM_CHUNKS_X; cx++) {
        for (int cz = 0; cz < NUM_CHUNKS_Z; cz++) {
            if (worldChunks[cx][cz] == nullptr)
                continue;
            for (int lx = 0; lx < CHUNK_SIZE; lx++) {
                for (int y = 0; y < WORLD_HEIGHT; y++) {
                    for (int lz = 0; lz < CHUNK_SIZE; lz++) {
                        Block &block = worldChunks[cx][cz]->blocks[lx][y][lz];
                        if (!block.exists && !block.isWater)
                            continue;
                        int worldX = cx * CHUNK_SIZE + lx - OFFSET_X;
                        int worldY = y;
                        int worldZ = cz * CHUNK_SIZE + lz - OFFSET_Z;
                        Vector3 toBlock = { static_cast<float>(worldX) - position.x,
                                            static_cast<float>(worldY) - position.y,
                                            static_cast<float>(worldZ) - position.z };
                        float dot = toBlock.x * front.x + toBlock.y * front.y + toBlock.z * front.z;
                        if (dot < 0)
                            continue;
                        float dx = worldX - position.x;
                        float dy = worldY - position.y;
                        float dz = worldZ - position.z;
                        float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                        if (!isBlockExposed(worldX, worldY, worldZ))
                            continue;
                        drawBlock(worldX, worldY, worldZ, block.colors, distance);
                    }
                }
            }
        }
    }
    
    // FPS hesaplaması.
    frameCount++;
    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    int deltaTime = currentTime - lastTime;
    if(deltaTime > 1000) {
        currentFPS = frameCount * 1000.0f / deltaTime;
        lastTime = currentTime;
        frameCount = 0;
    }
    
    // HUD (2D mod)
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 800, 0, 600);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "X: %.2f Y: %.2f Z: %.2f", position.x, position.y, position.z);
    drawText(buffer, 10, 570, 1.0f, 0.0f, 0.0f);
    snprintf(buffer, sizeof(buffer), "FPS: %.2f", currentFPS);
    drawText(buffer, 10, 550, 1.0f, 1.0f, 0.0f);
    drawCircle(400, 300, 5, 0.0f, 0.0f, 1.0f);
    
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    
    glutSwapBuffers();
}

// ---------------- Pencere Yeniden Boyutlandırma ----------------

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, static_cast<double>(w)/h, 0.1, renderDistance);
    glMatrixMode(GL_MODELVIEW);
}

// ---------------- Asenkron Disk Yazma İş Parçacığı ----------------

void diskWriteWorker() {
    while (true) {
        std::pair<int,int> chunkCoords;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            cv.wait(lock, []{ return !chunkSaveQueue.empty() || exitThread; });
            if (exitThread && chunkSaveQueue.empty())
                break;
            chunkCoords = chunkSaveQueue.front();
            chunkSaveQueue.pop();
        }
        // Eğer ilgili chunk hâlâ bellekte mevcutsa kaydet.
        if(worldChunks[chunkCoords.first][chunkCoords.second] != nullptr &&
           worldChunks[chunkCoords.first][chunkCoords.second]->dirty)
        {
            saveChunk(chunkCoords.first, chunkCoords.second);
        }
    }
}

// ---------------- main ----------------

int main(int argc, char** argv) {
    srand(static_cast<unsigned int>(time(0)));
    
    // Asenkron disk yazma iş parçacığını başlat.
    std::thread writerThread(diskWriteWorker);
    
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(1920, 1080);
    glutCreateWindow("Minecraft Benzeri Oyun - Asenkron Disk Yazma ve Dirty Flag");
    
    glEnable(GL_DEPTH_TEST);
    
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutMotionFunc(mouse);
    glutPassiveMotionFunc(mouse);
    glutMouseFunc(mouseClick);
    glutSetCursor(GLUT_CURSOR_NONE);
    glutIdleFunc(update);
    
    lastTime = glutGet(GLUT_ELAPSED_TIME);
    glutMainLoop();
    
    // Çıkış sırasında writerThread'i durdur.
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        exitThread = true;
    }
    cv.notify_all();
    writerThread.join();
    
    return 0;
}
