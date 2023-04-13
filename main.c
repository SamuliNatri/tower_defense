
// main.c : Tower defense.

#define WIN32_LEAN_AND_MEAN
#define _USE_MATH_DEFINES
#define COBJMACROS

#define MEGABYTE (1024 * 1024)
#define DEFAULT_MEMORY 10 * MEGABYTE

#define MAX_SHADERS 10
#define MAX_TEXTURES 10
#define MAX_MESHES 10
#define MAX_CONSTANT_BUFFERS 10
#define MAX_INPUT_LAYOUTS 10
#define MAX_BLEND_STATES 10
#define MAX_DEFAULT_ENTITIES 10

#define X_TILES 10
#define Y_TILES 10

/*
Creates a rectangle entity with some default properties.
Usage:
entity Entity = ENTITY_RECTANGLE(.Scale = { 2.0f, 2.0f, 1.0f }, .Rotation = 30.0f);
    DrawEntity(&Entity); // call inside Draw()
*/
#define ENTITY_RECTANGLE(...) ((entity) { .Alive = 1, .Mesh = DEFAULT_MESH_RECTANGLE, .Shader = DEFAULT_SHADER_POSITION, .InputLayout = DEFAULT_INPUT_LAYOUT_POSITION, .PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, .Color = ColorLightBlue, .Scale = {1.0f, 1.0f, 1.0f}, ##__VA_ARGS__ });

#define ENTITY_TRIANGLE(...) ((entity) { .Alive = 1, .Mesh = DEFAULT_MESH_TRIANGLE, .Shader = DEFAULT_SHADER_POSITION, .InputLayout = DEFAULT_INPUT_LAYOUT_POSITION, .PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, .Color = ColorLightBlue, .Scale = {1.0f, 1.0f, 1.0f}, ##__VA_ARGS__ });

#include <stdio.h>
#include <stdint.h>
#include <windows.h>
#include <windowsx.h>
#include <hidusage.h>
#include <d3d11_1.h>
#include <assert.h>
#include <time.h>
#include <float.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Types.

enum {
	DEFAULT_INPUT_LAYOUT_NONE,
	DEFAULT_INPUT_LAYOUT_POSITION,
	DEFAULT_INPUT_LAYOUT_POSITION_UV,
	DEFAULT_INPUT_LAYOUT_COUNT,
};
enum {
	DEFAULT_SHADER_NONE,
	DEFAULT_SHADER_POSITION,
	DEFAULT_SHADER_POSITION_UV,
	DEFAULT_SHADER_POSITION_UV_ATLAS,
	DEFAULT_SHADER_COUNT,
};
enum {
	DEFAULT_MESH_NONE,
	DEFAULT_MESH_TRIANGLE,
	DEFAULT_MESH_RECTANGLE,
	DEFAULT_MESH_RECTANGLE_UV,
	DEFAULT_MESH_RECTANGLE_LINES,
	DEFAULT_MESH_LINE,
	DEFAULT_MESH_POINT,
	DEFAULT_MESH_COUNT,
};
enum {
	DEFAULT_BLEND_STATE_NONE,
	DEFAULT_BLEND_STATE,
	DEFAULT_BLEND_STATE_COUNT,
};

enum {
	DEFAULT_TEXTURE_NONE,
	DEFAULT_TEXTURE_FONT,
	DEFAULT_TEXTURE_COUNT,
};

enum {
	DEFAULT_ENTITY_NONE,
	DEFAULT_ENTITY_RECTANGLE,
	DEFAULT_ENTITY_RECTANGLE_LINES,
	DEFAULT_ENTITY_LINE,
	DEFAULT_ENTITY_POINT,
	DEFAULT_ENTITY_COUNT,
};

enum {
	UP, LEFT, DOWN, RIGHT, SPACE,
	A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
	KEYSAMOUNT
};

typedef uint32_t u32;
typedef struct { float X, Y, Z; } v3;
typedef struct { float X, Y, Z, W; } v4;
typedef struct { float M[4][4]; } matrix;
typedef struct { float R, G, B, A; } color;
typedef struct { v3 A, B, C; } triangle;

typedef struct {
	unsigned char *Data;
	size_t Length;
	size_t Offset;
} memory;

typedef struct {
	ID3D11ShaderResourceView *ShaderResourceView;
	ID3D11SamplerState *SamplerState;
	float UOffset;
	float VOffset;
	float USize;
	float VSize;
} texture;

typedef struct {
	float UOffset;
	float VOffset;
	float USize;
	float VSize;
} textureInfo;

typedef struct {
	ID3D11Buffer *Buffer;
	float *Vertices;
	int NumVertices;
	int Stride;
	int Offset;
} mesh;

typedef struct {
	float Left;
	float Right;
	float Top;
	float Bottom;
} rectangle;

typedef struct {
	v3 A;
	v3 B;
} line;

typedef struct {
	v3 Origin;
	v3 Direction;
	float Length;
} lineODL;

typedef struct {
	matrix Model;
	matrix View;
	matrix Projection;
	matrix Rotation;
	matrix Scale;
	color Color;
	float UOffset;
	float VOffset;
	float USize;
	float VSize;
} constants;

typedef struct {
	UINT ByteWidth;
} constantBufferInfo;

typedef struct {
	LARGE_INTEGER StartingCount;
	LARGE_INTEGER EndingCount;
	LARGE_INTEGER TotalCount;
	LARGE_INTEGER CountsPerSecond;
	double ElapsedMilliSeconds;
} timer;

typedef struct {
	int X;
	int Y;
	int LeftButtonPressed;
	int RightButtonPressed;
	int MiddleButtonDown;
	int WheelUp;
	int WheelDown;
} mouse;

typedef struct {
	v3 Position;
	float DragSensitivity;
	float Speed;
} camera;

typedef struct {
	v3 Position;
	color Color;
	int Mesh;
	int Size;
	int Width;
	int Height;
	ID3D11VertexShader *VertexShader;
	ID3D11PixelShader *PixelShader;
	ID3D11InputLayout *InputLayout;
	ID3D10Blob *VSBlob;
	ID3D10Blob *PSBlob;
} grid;

typedef struct {
	ID3D11VertexShader *VertexShader;
	ID3D11PixelShader *PixelShader;
	ID3D10Blob *VSBlob;
	ID3D10Blob *PSBlob;
} shader;

typedef struct {
	const D3D_SHADER_MACRO *VSMacros;
	const D3D_SHADER_MACRO *PSMacros;
	// TODO: add more fields
	// https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3dcompilefromfile
} shaderInfo;

typedef struct {
	v3 Position;
	v3 Velocity;
	v3 Scale;
	color Color;
	float Speed;
	float Rotation;
    float Health;
    float MaxHealth;
    float Damage;
    float MaxRange;
	int Mesh;
	int Texture;
	int Shader;
	int ConstantBuffer;
	int InputLayout;
	int PrimitiveTopology;
    int ShowHealthBar;
    int Alive;
    int VisitedTiles[100];
    int VisitedTileCount;
    timer Timer;
    timer ShootTimer;
} entity;

typedef struct {
	entity *Items;
	int Length;
	int Capacity;
	int Index;
} entityArray;

// Globals.

// GAME

timer SpawnTimer;

entityArray Enemies;
entityArray Turrets;
entityArray Bullets;

entity Base;

int Level[100] = {
    1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 0, 1, 1, 1, 0, 0,
    0, 0, 0, 1, 0, 1, 0, 1, 0, 0,
    0, 0, 0, 1, 0, 1, 0, 1, 0, 0,
    0, 0, 0, 1, 0, 1, 0, 1, 1, 1,
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};


// COMMON

void *MemoryBackend;
memory Memory;
timer GlobalTimer;

LARGE_INTEGER PerformanceCounterFrequency;
LARGE_INTEGER PerformanceCounterStart;
LARGE_INTEGER PerformanceCounterEnd;
float DeltaTime;

// Zero index is not used in these arrays.

shader Shaders[MAX_SHADERS];
mesh Meshes[MAX_MESHES];
texture Textures[MAX_TEXTURES];
ID3D11InputLayout *InputLayouts[MAX_INPUT_LAYOUTS];
ID3D11BlendState *BlendStates[MAX_BLEND_STATES];
ID3D11Buffer *ConstantBuffers[MAX_CONSTANT_BUFFERS];
entity DefaultEntities[MAX_DEFAULT_ENTITIES];

int ShaderCount = DEFAULT_SHADER_COUNT;
int MeshCount = DEFAULT_MESH_COUNT;
int TextureCount = DEFAULT_TEXTURE_COUNT;
int InputLayoutCount = DEFAULT_INPUT_LAYOUT_COUNT;
int BlendStateCount = DEFAULT_BLEND_STATE_COUNT;
int EntityCount = DEFAULT_ENTITY_COUNT;

int ConstantBufferCount;

// For DrawRectangle(), testing function.

int TestRectangleMesh;

camera Camera = {
	.Position = {0.0f, 0.0f, -18.0f},
	.DragSensitivity = 0.1f,
	.Speed = 20.0f,
};

mouse Mouse;

v3 DefaultScale = { 1.0f, 1.0f, 1.0f };

// Colors.

color 
ColorLavenderPurple, 
ColorThistle, 
ColorGreenEmerald,
ColorGreenJungle,
ColorGreenJade,
ColorGreenFern,
ColorGreenEucalyptus
;

color ColorLightBlue = { 0.2f, 0.4f, 0.8f, 1.0f };
color ColorRed = { 1.0f, 0.0f, 0.0f, 1.0f };
color ColorOrange = { 1.0f, 0.6f, 0.0f, 1.0f };
color ColorOrangeDark = { 1.0f, 0.4f, 0.0f, 1.0f };
color ColorOrangeRed = { 1.0f, 0.2f, 0.0f, 1.0f };
color ColorOrangeTomato = { 1.0f, 0.3f, 0.2f, 1.0f };
color ColorGreen = { 0.0f, 1.0f, 0.0f, 1.0f };
color ColorBlue = { 0.0f, 0.0f, 1.0f, 1.0f };
color ColorYellow = { 1.0f, 1.0f, 0.0f, 1.0f };
color ColorFuchsia = { 1.0f, 0.0f, 1.0f, 1.0f };
color ColorWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
color ColorBlack = { 0.0f, 0.0f, 0.0f, 1.0f };
color ColorGray10 = { 0.1f, 0.1f, 0.1f, 1.0f };
color ColorGray15 = { 0.15f, 0.15f, 0.15f, 1.0f };
color ColorGray20 = { 0.2f, 0.2f, 0.2f, 1.0f };
color ColorGray25 = { 0.25f, 0.25f, 0.25f, 1.0f };
color ColorGray30 = { 0.3f, 0.3f, 0.3f, 1.0f };
color ColorGray35 = { 0.35f, 0.35f, 0.35f, 1.0f };
color ColorGray40 = { 0.4f, 0.4f, 0.4f, 1.0f };
color ColorGray45 = { 0.45f, 0.45f, 0.45f, 1.0f };
color ColorGray50 = { 0.5f, 0.5f, 0.5f, 1.0f };
color ColorGray55 = { 0.55f, 0.55f, 0.55f, 1.0f };
color ColorGray60 = { 0.6f, 0.6f, 0.6f, 1.0f };
color ColorGray65 = { 0.65f, 0.65f, 0.65f, 1.0f };
color ColorGray70 = { 0.7f, 0.7f, 0.7f, 1.0f };
color ColorGray75 = { 0.75f, 0.75f, 0.75f, 1.0f };
color ColorGray80 = { 0.8f, 0.8f, 0.8f, 1.0f };
color ColorGray85 = { 0.85f, 0.85f, 0.85f, 1.0f };
color ColorGray90 = { 0.9f, 0.9f, 0.9f, 1.0f };
color ColorGray95 = { 0.95f, 0.95f, 0.95f, 1.0f };

color EngineColorBackground = { 0.05f, 0.05f, 0.05f, 1.0f };
color EngineColorGrid = { 0.05f, 0.05f, 0.05f, 1.0f };
color EngineColorGridRed = { 1.0f, 0.05f, 0.05f, 1.0f };

int MeshTriangle;
int MeshRectangle;

int KeyDowns[512];
int KeyPresses[512];
int KeyReleases[512]; 

int Running = 1;
int Pause;

int ScreenWidth;
int ScreenHeight;
int ClientWidth;
int ClientHeight;
DWORD WindowStyle;

D3D11_VIEWPORT Viewport;

ID3D11Device1 *Device;
ID3D11DeviceContext1 *Context;
ID3D11Buffer *Buffer;

matrix ProjectionMatrix;
matrix ViewMatrix;

// Declarations.

// GAME

// COMMON

// Define these:
void Init();
void Input();
void Update();
void Draw();

void UpdateDeltaTime();

int ValueInIntArray(int *Array, int Size, int Value);

void AddEntityToArray(entityArray *Array, entity *Entity);
void UpdateEntityPosition(entity *Entity);
void DrawEntity(entity *Entity);
void DrawEntityArray(entityArray *Array);
void KeepEntityWithinBounds(entity *Entity, rectangle Bounds);
entityArray NewEntityArray(int Capacity);

int GetSurroundingTilesWithValue(int *Tiles, int Value, int XCenter, int YCenter, int Width, int Height, int **Result);

void GetRectangleCorners(v3 CornerPositions[4], v3 Scale, v3 Position, float Rotation);

void HandleCamera();

void DrawEntity(entity *Entity);
void DrawEntityArgs(v3 Position, v3 Scale, float Rotation, color Color, int Mesh, int Texture, int Shader, int ConstantBuffer,
                    int InputLayout, int PrimitiveTopology);

void DrawLineOAL(v3 Origin, float Angle, float Length, color Color);
void DrawLineODL(v3 Origin, v3 Direction, float Length, color Color);
void DrawLineAB(v3 A, v3 B, color Color);
void DrawLine(line Line, color Color);
void DrawPoint(v3 Position, color Color);

MemoryInit(size_t Size);
void *MemoryAlloc(size_t Size);

void DrawString(v3 Position, char *String, color Color, v3 Scale);

void GridInit(grid *Grid);
void GridDraw(grid *Grid);

int CreateMesh(float *Vertices, size_t Size, int Stride, int Offset, int MeshIndex);
int CreateBlendState();

void CreatetInputLayout(shader *Shader, D3D11_INPUT_ELEMENT_DESC *Desc, size_t Size, int InputLayoutIndex);

void CreateDefaultMeshes();
void CreateDefaultEntities();
void CreateDefaultInputLayouts();
void CreateDefaultShaders();
void CreateDefaultBlendStates();
void CreateDefaultTextures();

int PickMeshRectangle(int MouseX, int MouseY, v3 Position, mesh *Mesh);
int RayTriangleIntersect(v3 RayOrigin, v3 RayDirection, triangle *Triangle);
int RectanglesIntersect(rectangle A, rectangle B);
int RotatedRectanglesCollide(v3 Scale, v3 Position, float Rotation, v3 Scale2, v3 Position2, float Rotation2);
int RectangleBoundsCollision(rectangle Rectangle, rectangle Bounds, v3 *Normal);
rectangle GetRectangleByPositionAndScale(v3 Position, v3 Scale);

int IsRepeat(LPARAM LParam);

void StartTimer(timer *Timer);
void UpdateTimer(timer *Timer);
int TimeElapsed(float Seconds, timer *Timer);

float GetRandomZeroToOne();

color GetRandomColor();
color GetRandomShadeOfGray();
color GetColorByRGB(int R, int G, int B);
void DrawRectangle(v3 Position, float Rotation, v3 Scale, color Color);
void InitColors();
int ColorIsZero(color Color);

void Debug(char *Format, ...);
void DebugV3(char *Message, v3 *V);
void DebugMatrix(char *Message, matrix *M);

v3 V3Add(v3 A, v3 B);
v3 V3Subtract(v3 A, v3 B);
v3 V3CrossProduct(v3 A, v3 B);
v3 V3AddScalar(v3 A, float B);
v3 V3MultiplyScalar(v3 A, float B);
v3 V3Inverse(v3 V);
v3 V3RotateZ(v3 Vector, float Rotation);
v3 V3GetDirection(v3 A, v3 B);

float V3GetDistance(v3 A, v3 B);
float V3GetAngle(v3 V);
float V3DotProduct(v3 A, v3 B);
float V3Length(v3 *V);
float DegreesToRadians(float Degrees);
float RadiansToDegrees(float Radians);

int V3IsZero(v3 Vector);
int v3Compare(v3 A, v3 B);

void V3Zero(v3 *Vector);
void V3Normalize(v3 *V);

v3 V3TransformCoord(v3 *V, matrix *M);
v3 V3TransformNormal(v3 *V, matrix *M);
v3 MatrixV3Multiply(matrix M, v3 V);

matrix MatrixTranslation(v3 V);
matrix MatrixMultiply(matrix *A, matrix *B);
matrix MatrixRotationZ(float AngleDegrees);

void MatrixInverse(matrix *Source, matrix *Target);

// GAME FUNCTIONS

void SpawnEnemy() {
    entity Enemy = ENTITY_RECTANGLE(.Position =  {0.0f, Y_TILES-1, 0.0f},
                                    .Health = 100.0f,
                                    .MaxHealth = 100.0f,
                                    .ShowHealthBar = 1);
    Enemy.VisitedTiles[Enemy.VisitedTileCount++] = 0;
    AddEntityToArray(&Enemies, &Enemy);
}

int InVisitedTiles(int *VisitedTiles, int TileIndex) {
    for(int Index = 0; Index < 100; ++Index) {
        if(VisitedTiles[Index] == TileIndex) {
            return 1;
        }
    }
    return 0;
}

entity* GetClosestEnemy(entity *Entity) {
    
    entity *Closest = &Enemies.Items[0];
    
    float Distance = V3GetDistance(Entity->Position, Closest->Position);
    
    for(int Index = 0; Index < Enemies.Length; ++Index) {
        
        float NewDistance = V3GetDistance(Entity->Position, Enemies.Items[Index].Position);
        if(NewDistance < Distance) {
            Distance = NewDistance;
            Closest = &Enemies.Items[Index];
        }
    }
    
    return Closest;
}

void Update() {
    
    // Add turrets.
    
    if(MouseLeftButtonPressed()) {
        
        for(int Y = 0; Y < Y_TILES; ++Y) {
            for(int X = 0; X < X_TILES; ++X) {
                if(Level[Y * X_TILES + X] != 1) { 
                    v3 Position = {X, Y_TILES - 1 - Y, 0.0f};
                    if(PickMeshRectangle(Mouse.X, 
                                         Mouse.Y,
                                         Position, 
                                         &Meshes[DEFAULT_MESH_RECTANGLE])) {
                        
                        entity Turret = ENTITY_TRIANGLE(.Position = Position, 
                                                        .Scale = {0.5f, 0.5f, 0.0f});
                        AddEntityToArray(&Turrets, &Turret);
                    }
                }
            }
        }
    }
    
    // Turn turrets facing closest enemy.
    
    for(int Index = 0; Index < Turrets.Length; ++Index) {
        
        entity *Turret = &Turrets.Items[Index];
        
        entity *ClosestEnemy = GetClosestEnemy(Turret);
        
        v3 Direction = V3GetDirection(Turret->Position, ClosestEnemy->Position);
        
        float Angle = V3GetAngle(Direction);
        
        Turret->Rotation = -90.0f + Angle;
        
        float Distance = V3GetDistance(Turret->Position, ClosestEnemy->Position);
        
        // Shoot a bullet.
        
        if(Distance <= 5.0f && TimeElapsed(0.8f, &Turret->ShootTimer)) {
            entity Bullet = ENTITY_RECTANGLE(.Position = Turret->Position,
                                             .Color = ColorGray60,
                                             .Velocity = Direction,
                                             .Speed = 1.2f,
                                             .Scale = {0.1f, 0.1f, 0.1f});
            AddEntityToArray(&Bullets, &Bullet);
        }
        
    }
    
    // Bullets.
    
    for(int Index = 0; Index < Bullets.Length; ++Index) {
        entity *Bullet = &Bullets.Items[Index];
        
        if(Bullet->Position.X <= 0 - 0.5f ||
           Bullet->Position.X >= X_TILES - 1.0f + 0.5f ||
           Bullet->Position.Y < 0 - 0.5f ||
           Bullet->Position.Y >= Y_TILES - 1.0f + 0.5f) {
            Bullet->Alive = 0;
        }
        
        if(!Bullet->Alive) continue;
        
        UpdateEntityPosition(Bullet);
        
        rectangle BulletRectangle = GetRectangleByPositionAndScale(Bullet->Position, Bullet->Scale);
        
        for(int Index = 0; Index < Enemies.Length; ++Index) {
            entity *Enemy = &Enemies.Items[Index];
            rectangle EnemyRectangle = GetRectangleByPositionAndScale(Enemy->Position, Enemy->Scale);
            
            if(RectanglesIntersect(BulletRectangle, EnemyRectangle)) {
                Enemy->Health -= 10.0f;
                if(Enemy->Health <= 0.0f) {
                    Enemy->Alive = 0;
                }
                Bullet->Alive = 0;
            }
        }
        
    }
    
    // Spawn enemies.
    
    if(TimeElapsed(1.0f, &SpawnTimer)) {
        SpawnEnemy();
    }
    
    // Move enemies.
    
    for(int Index = 0; Index < Enemies.Length; ++Index) {
        entity *Enemy = &Enemies.Items[Index];
        
        if(!Enemy->Alive) continue;
        
        if(TimeElapsed(0.2f, &Enemy->Timer)) {
            
            // Get next position.
            
            int *SurroundingTiles = NULL;
            
            int Size = GetSurroundingTilesWithValue(Level, 
                                                    1, 
                                                    Enemy->Position.X, 
                                                    Y_TILES - 1 - Enemy->Position.Y, 
                                                    X_TILES, Y_TILES, &SurroundingTiles);
            
            for(int Index = 0; Index < Size; ++Index) {
                Debug("SurroundingTiles[Index]: %d\n", SurroundingTiles[Index]);
                
                v3 NewPosition = {SurroundingTiles[Index] % X_TILES, 
                    Y_TILES - 1 - SurroundingTiles[Index] / X_TILES, 
                    0.0f};
                
                if(V3Compare(NewPosition, Base.Position)) {
                    Base.Health -= 10.0f;
                    if(Base.Health <= 0.0f) {
                        Base.Alive = 0;
                    }
                } else if(!InVisitedTiles(Enemy->VisitedTiles, SurroundingTiles[Index])) {
                    Enemy->Position = NewPosition;
                    Enemy->VisitedTiles[Enemy->VisitedTileCount++] = SurroundingTiles[Index];
                    break;
                }
                
            }
            
        }
        
    }
    
}

void DrawLevel(int *Level) {
    for(int Y = 0; Y < Y_TILES; ++Y) {
        for(int X = 0; X < X_TILES; ++X) {
            int Index = ((Y_TILES - 1) - Y) * X_TILES + X;
            
            if(Level[Index] == 1) {
                DrawRectangle((v3){X, Y, 0.0f}, 
                              0.0f, 
                              (v3){1.0f, 1.0f, 1.0f}, 
                              ColorGray30);
            } else {
                DrawRectangle((v3){X, Y, 0.0f}, 
                              0.0f, 
                              (v3){1.0f, 1.0f, 1.0f}, 
                              ColorGray15);
            }
        }
    }
}

void Init() {
    
    Camera.Position.X = X_TILES / 2.0f;
    Camera.Position.Y = Y_TILES / 2.0f;
    
    Enemies = NewEntityArray(64);
    Turrets = NewEntityArray(64);
    Bullets = NewEntityArray(128);
    
    SpawnEnemy();
    
    Base = ENTITY_RECTANGLE(.Position =  {9.0f, 2.0f, 0.0f},
                            .Color = ColorOrangeRed,
                            .Health = 100.0f,
                            .MaxHealth = 100.0f,
                            .ShowHealthBar = 1);
    
    
    
    /*     
        entity Enemy1 = ENTITY_RECTANGLE(.Color = ColorLightBlue, 
                                         .Rotation = 45.0f,
                                         .Velocity = {-1.0f, 0.0f, 0.0},
                                         .Speed = 1.0f,
                                         );
        
        entity Enemy2 = ENTITY_RECTANGLE(.Color = ColorLightBlue, 
                                         .Rotation = 45.0f,
                                         .Velocity = {1.0f, 0.0f, 0.0},
                                         .Speed = 1.0f,
                                         .Position = {2.0f, 0.0f, 0.0f},
                                         );
        
        AddEntityToArray(&Enemies, &Enemy1);
        AddEntityToArray(&Enemies, &Enemy2);
             */
    
    
}

void Draw() {
    DrawLevel(Level);
    DrawEntityArray(&Enemies);
    DrawEntityArray(&Turrets);
    DrawEntityArray(&Bullets);
    DrawEntity(&Base);
}

void Input() {
    
}

// COMMON FUNCTIONS

void UpdateDeltaTime() {
    QueryPerformanceCounter(&PerformanceCounterEnd);
    DeltaTime = (double)(PerformanceCounterEnd.QuadPart - PerformanceCounterStart.QuadPart) / (double)PerformanceCounterFrequency.QuadPart;
    PerformanceCounterStart = PerformanceCounterEnd;
}

int KeyDown(int Key) {
    if(KeyDowns[Key]) return 1;
    return 0;
}

int KeyPressed(int Key) {
    if(KeyPresses[Key]) {
        KeyPresses[Key] = 0;
        return 1;
    }
    return 0;
}

int KeyReleased(int Key) {
    if(KeyReleases[Key]) {
        KeyReleases[Key] = 0;
        return 1;
    }
    return 0;
}

int MouseLeftButtonPressed() {
    if(Mouse.LeftButtonPressed) {
        Mouse.LeftButtonPressed = 0;
        return 1;
    }
    return 0;
}

int MouseRightButtonPressed() {
    if(Mouse.RightButtonPressed) {
        Mouse.RightButtonPressed = 0;
        return 1;
    }
    return 0;
}

void DrawRectangleCorners(v3 Corners[4]) {
    for (int Index = 0; Index < 4; ++Index) {
        color Color = ColorWhite;
        if (Index == 0)
            Color = ColorGreen;
        DrawRectangle(Corners[Index], 0.0f, (v3) { 0.05f, 0.05f, 0.05f }, Color);
    }
}

void GetRectangleAxes(lineODL Lines[2], v3 Scale, v3 Position, float Rotation) {
    Lines[0] = (lineODL){ Position, V3RotateZ((v3) { 1.0f, 0.0f, 0.0f }, Rotation), Scale.Y / 2.0f };
    Lines[1] = (lineODL){ Position, V3RotateZ((v3) { 0.0f, 1.0f, 0.0f }, Rotation), Scale.X / 2.0f };
}

void DrawRectangleAxes(lineODL Lines[4]) {
    for (int Index = 0; Index < 2; ++Index) {
        DrawLineODL(Lines[Index].Origin, Lines[Index].Direction, 5.0f, ColorGray40);
    }
}

void DrawCornerToAxisProjections(v3 Corners[4], v3 Projections[8]) {
    int ProjectionsIndex = 0;
    for (int Index = 0; Index < 4; ++Index) {
        
        DrawLineAB(Corners[Index], Projections[ProjectionsIndex], ColorGray40);
        DrawLineAB(Corners[Index], Projections[ProjectionsIndex + 1], ColorGray40);
        
        DrawRectangle(Projections[ProjectionsIndex], 0.0f,
                      (v3) {
                          0.05f, 0.05f, 0.05f
                      }, ColorYellow);
        DrawRectangle(Projections[ProjectionsIndex + 1], 0.0f,
                      (v3) {
                          0.05f, 0.05f, 0.05f
                      }, ColorYellow);
        
        ProjectionsIndex += 2;
    }
}

void GetCornerToAxisProjections(v3 Projections[8], v3 Corners[4],
                                lineODL Axes[2]) {
    
    int ProjectionsIndex = 0;
    
    for (int Index = 0; Index < 4; ++Index) {
        float Dot1 = Axes[0].Direction.X * (Corners[Index].X - Axes[0].Origin.X) +
            Axes[0].Direction.Y * (Corners[Index].Y - Axes[0].Origin.Y);
        v3 Proj1 = { Axes[0].Origin.X + Axes[0].Direction.X * Dot1,
            Axes[0].Origin.Y + Axes[0].Direction.Y * Dot1 };
        
        float Dot2 = Axes[1].Direction.X * (Corners[Index].X - Axes[1].Origin.X) +
            Axes[1].Direction.Y * (Corners[Index].Y - Axes[1].Origin.Y);
        v3 Proj2 = { Axes[1].Origin.X + Axes[1].Direction.X * Dot2,
            Axes[1].Origin.Y + Axes[1].Direction.Y * Dot2 };
        
        Projections[ProjectionsIndex++] = Proj1;
        Projections[ProjectionsIndex++] = Proj2;
    }
}

int CompareProjectionsX(const void *First, const void *Second) {
    v3 A = *((v3 *)First);
    v3 B = *((v3 *)Second);
    if (A.X < B.X) return -1;
    if (A.X > B.X) return 1;
    return 0;
}

int CompareProjectionsY(const void *First, const void *Second) {
    v3 A = *((v3 *)First);
    v3 B = *((v3 *)Second);
    if (A.Y < B.Y) return -1;
    if (A.Y > B.Y) return 1;
    return 0;
}

void SortProjections(v3 Projections[4], int Axis) {
    
    if (Axis == 0) {
        qsort(Projections, 4, sizeof(*Projections), CompareProjectionsX);
    } else {
        qsort(Projections, 4, sizeof(*Projections), CompareProjectionsY);
    }
}

void GetSegments(line *XSegment, line *YSegment, v3 Projections[8]) {
    
    // Sort projections.
    
    v3 XProjections[4] = { Projections[0], Projections[2], Projections[4], Projections[6] };
    v3 YProjections[4] = { Projections[1], Projections[3], Projections[5], Projections[7] };
    
    SortProjections(XProjections, 0);
    SortProjections(YProjections, 1);
    
    *XSegment = (line){ XProjections[0], XProjections[3] };
    *YSegment = (line){ YProjections[0], YProjections[3] };
}

int AxisSegmentIntersectsRectangle(line Line, v3 Position, v3 Scale, int Axis) {
    
    float Half = (Axis ? Scale.Y / 2.0f : Scale.X / 2.0f);
    
    // Get vectors from rectangle center to segment ends.
    
    v3 AVector = V3Subtract(Line.A, Position);
    float ALength = V3Length(&AVector);
    v3 BVector = V3Subtract(Line.B, Position);
    float BLength = V3Length(&BVector);
    
    // If these vectors point in similar directions and their magnitude is greater
    // than the corresponding rectangle half-width, the segment doesn't intersect
    // with the rectangle.
    
    if (V3DotProduct(AVector, BVector) > 0 && ALength > Half && BLength > Half) return 0;
    
    return 1;
}

void DrawSegments(line XSegment, line YSegment, v3 Position, v3 Scale) {
    
    color Color = ColorRed;
    
    if (AxisSegmentIntersectsRectangle(XSegment, Position, Scale, 0)) {
        Color = ColorYellow;
    }
    
    DrawLine(XSegment, Color);
    
    Color = ColorBlue;
    
    if (AxisSegmentIntersectsRectangle(YSegment, Position, Scale, 1)) {
        Color = ColorYellow;
    }
    
    DrawLine(YSegment, Color);
}

void GetRectangleCorners(v3 CornerPositions[4], v3 Scale, v3 Position,
                         float Rotation) {
    
    CornerPositions[0] =
        V3Add((v3) { Scale.X / 2.0f, 0.0f, 0.0f }, (v3) { 0.0f, Scale.Y / 2.0f, 0.0f });
    CornerPositions[1] = V3Add((v3) { -Scale.X / 2.0f, 0.0f, 0.0f },
                               (v3) {
                                   0.0f, Scale.Y / 2.0f, 0.0f
                               });
    CornerPositions[2] = V3Add((v3) { -Scale.X / 2.0f, 0.0f, 0.0f },
                               (v3) {
                                   0.0f, -Scale.Y / 2.0f, 0.0f
                               });
    CornerPositions[3] = V3Add((v3) { Scale.X / 2.0f, 0.0f, 0.0f },
                               (v3) {
                                   0.0f, -Scale.Y / 2.0f, 0.0f
                               });
    
    for (int Index = 0; Index < 4; ++Index) {
        CornerPositions[Index] = V3RotateZ(CornerPositions[Index], Rotation);
        CornerPositions[Index] = V3Add(Position, CornerPositions[Index]);
    }
}

void DrawLineOAL(v3 Origin, float Angle, float Length, color Color) {
    DrawEntityArgs(Origin, (v3) { Length, 1.0f, 1.0f }, Angle, Color,
                   DefaultEntities[DEFAULT_ENTITY_LINE].Mesh,
                   DefaultEntities[DEFAULT_ENTITY_LINE].Texture,
                   DefaultEntities[DEFAULT_ENTITY_LINE].Shader,
                   DefaultEntities[DEFAULT_ENTITY_LINE].ConstantBuffer,
                   DefaultEntities[DEFAULT_ENTITY_LINE].InputLayout,
                   DefaultEntities[DEFAULT_ENTITY_LINE].PrimitiveTopology);
}

void DrawLineODL(v3 Origin, v3 Direction, float Length, color Color) {
    
    V3Normalize(&Direction);
    float Angle = RadiansToDegrees(asin(Direction.Y / 1.0f));
    
    if (Direction.X < 0.0f) Angle = 180.0f - Angle;
    
    DrawEntityArgs(Origin, (v3) { Length, 1.0f, 1.0f }, Angle, Color,
                   DefaultEntities[DEFAULT_ENTITY_LINE].Mesh,
                   DefaultEntities[DEFAULT_ENTITY_LINE].Texture,
                   DefaultEntities[DEFAULT_ENTITY_LINE].Shader,
                   DefaultEntities[DEFAULT_ENTITY_LINE].ConstantBuffer,
                   DefaultEntities[DEFAULT_ENTITY_LINE].InputLayout,
                   DefaultEntities[DEFAULT_ENTITY_LINE].PrimitiveTopology);
}

void DrawLineAB(v3 A, v3 B, color Color) {
    v3 Direction = V3Subtract(B, A);
    float Length = V3Length(&Direction);
    DrawLineODL(A, Direction, Length, Color);
}

void DrawLine(line Line, color Color) { DrawLineAB(Line.A, Line.B, Color); }

void DrawPoint(v3 Position, color Color) {
    DrawEntityArgs(Position, (v3) { 1.0f, 1.0f, 1.0f }, 0.0f, Color,
                   DefaultEntities[DEFAULT_ENTITY_POINT].Mesh,
                   DefaultEntities[DEFAULT_ENTITY_POINT].Texture,
                   DefaultEntities[DEFAULT_ENTITY_POINT].Shader,
                   DefaultEntities[DEFAULT_ENTITY_POINT].ConstantBuffer,
                   DefaultEntities[DEFAULT_ENTITY_POINT].InputLayout,
                   DefaultEntities[DEFAULT_ENTITY_POINT].PrimitiveTopology);
}

void DrawRectangle(v3 Position, float Rotation, v3 Scale, color Color) {
    DrawEntityArgs(Position, Scale, Rotation, Color,
                   DefaultEntities[DEFAULT_ENTITY_RECTANGLE].Mesh,
                   DefaultEntities[DEFAULT_ENTITY_RECTANGLE].Texture,
                   DefaultEntities[DEFAULT_ENTITY_RECTANGLE].Shader,
                   DefaultEntities[DEFAULT_ENTITY_RECTANGLE].ConstantBuffer,
                   DefaultEntities[DEFAULT_ENTITY_RECTANGLE].InputLayout,
                   DefaultEntities[DEFAULT_ENTITY_RECTANGLE].PrimitiveTopology);
}

void DrawHealthBar(entity *Entity) {
    
    if(Entity->Health >= Entity->MaxHealth) return;
    
    // Background.
    v3 Position = Entity->Position;
    v3 Scale = Entity->Scale;
    Scale.Y = 0.1f;
    Position.Y += Entity->Scale.Y / 2.0f - Scale.Y / 2.0f;
    DrawRectangle(Position, 0.0f, Scale, ColorGray40);
    
    // Health.
    float HealthPercent = Entity->Health / Entity->MaxHealth;
    v3 NewScale = Scale;
    NewScale.X *= HealthPercent;
    Position.X -= (Scale.X - NewScale.X) / 2.0f;
    DrawRectangle(Position, 0.0f, NewScale, ColorGreenJade);
}

void InitColors() {
    ColorLavenderPurple = GetColorByRGB(149, 125, 173);
    ColorThistle = GetColorByRGB(224, 187, 228);
    ColorGreenEmerald = GetColorByRGB(80, 200, 120);
    ColorGreenJungle = GetColorByRGB(42, 170, 138);
    ColorGreenJade = GetColorByRGB(0, 163, 108);
    ColorGreenFern = GetColorByRGB(79, 121, 66);
    ColorGreenEucalyptus = GetColorByRGB(95, 133, 117);
}

// TODO: better function to change brightness.

color ColorMultiply(color A, float Value) {
    
    color Result = { 0 };
    
    float R = A.R * Value;
    float G = A.G * Value;
    float B = A.B * Value;
    
    if (R > 1.0f) R = 1.0f;
    if (G > 1.0f) G = 1.0f;
    if (B > 1.0f) B = 1.0f;
    
    if (R < 0.0f) R = 0.0f;
    if (G < 0.0f) G = 0.0f;
    if (B < 0.0f) B = 0.0f;
    
    Result.R = R;
    Result.G = G;
    Result.B = B;
    Result.A = 1.0f;
    
    return Result;
}

LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM WParam,
                            LPARAM LParam) {
    switch (Message) {
        
        /*
        TODO: 
        - Clean up input.
- Better mouse handling.
        - Key mappings. Camera controls and exit key are hard coded.
        */
        
        /*
INPUT
- 'O' exits the program.
Camera.
- Move with WASD. 
- Zoom in / out with QE or mouse wheel.
- Grap and move it by holding the mouse middle button.
Check key inputs like this:
- if(KeyDown('A'));
- if(KeyPressed('A'));  // sets KeyPresses['A'] to 0.
- if(KeyReleased('A')); // sets KeyReleases['A'] to 0.
- TODO: Better way to handle key actions that need to happen only once? 
*/
        
        case WM_INPUT: {
            if (Mouse.MiddleButtonDown) {
                UINT DataSize = sizeof(RAWINPUT);
                static BYTE Data[sizeof(RAWINPUT)];
                
                GetRawInputData((HRAWINPUT)LParam, RID_INPUT, Data, &DataSize,
                                sizeof(RAWINPUTHEADER));
                
                RAWINPUT *Raw = (RAWINPUT *)Data;
                
                if (Raw->header.dwType == RIM_TYPEMOUSE) {
                    
                    if (Raw->data.mouse.lLastX != 0) {
                        Camera.Position.X -=
                        (float)Raw->data.mouse.lLastX * Camera.DragSensitivity;
                    }
                    
                    if (Raw->data.mouse.lLastY != 0) {
                        Camera.Position.Y +=
                        (float)Raw->data.mouse.lLastY * Camera.DragSensitivity;
                    }
                }
            }
        } break;
        
        case WM_MOUSEWHEEL: {
            int Delta = GET_WHEEL_DELTA_WPARAM(WParam);
            if (Delta > 0) { Mouse.WheelUp = 1; } else { Mouse.WheelDown = 1; }
        } break;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP: {
            Mouse.MiddleButtonDown = (Message == WM_MBUTTONDOWN ? 1 : 0);
        } break;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN: {
            if (!IsRepeat(LParam)) {
                if (Message == WM_LBUTTONDOWN) { 
                    Mouse.LeftButtonPressed = 1; 
                } else { 
                    Mouse.RightButtonPressed = 1; 
                }
                Mouse.X = GET_X_LPARAM(LParam);
                Mouse.Y = GET_Y_LPARAM(LParam);
            }
        } break;
        
        case WM_KEYUP:
        case WM_KEYDOWN: {
            int IsDown = (Message == WM_KEYDOWN); 
            // 30 bit: The previous key state. The value is 1 if the key is down before the message is sent, or it is zero if the key is up.
            int WasUp = !((LParam >> 30) & 1);
            KeyDowns[WParam] = IsDown; // Key is held down.
            KeyPresses[WParam] = (WasUp && IsDown); // Key was up, but is now down.
            KeyReleases[WParam] = (!WasUp && !IsDown); // Key was down, but is now up.
        } break;
        
        case WM_DESTROY: { PostQuitMessage(0); } break;
        default: { return DefWindowProc(Window, Message, WParam, LParam); }
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR CmdLine,
                   int CmdShow) {
    
    MemoryInit(DEFAULT_MEMORY);
    StartTimer(&GlobalTimer);
    srand((unsigned int)time(NULL));
    
    EngineColorBackground = ColorGray20;
    
    WNDCLASS WindowClass = { 0 };
    const char ClassName[] = "Window";
    WindowClass.lpfnWndProc = WindowProc;
    WindowClass.hInstance = Instance;
    WindowClass.lpszClassName = ClassName;
    WindowClass.hCursor = LoadCursor(NULL, IDC_CROSS);
    
    if (!RegisterClass(&WindowClass)) {
        MessageBox(0, "RegisterClass failed", 0, 0);
        return GetLastError();
    }
    
    // Create a fullscreen window.
    
    HWND Window = CreateWindowEx(0, ClassName, ClassName, WS_VISIBLE, 0, 0, 0, 0, 0, 0, Instance, 0);
    
    MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
    GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo);
    
    ScreenWidth = MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left;
    ScreenHeight = MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top;
    
    WindowStyle = GetWindowLong(Window, GWL_STYLE);
    SetWindowLong(Window, GWL_STYLE, WindowStyle & ~WS_OVERLAPPEDWINDOW);
    SetWindowPos(Window, HWND_TOP, 0, 0, ScreenWidth, ScreenHeight, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    
    if (!Window) {
        MessageBox(0, "CreateWindowEx failed", 0, 0);
        return GetLastError();
    }
    
    // Get client width and height.
    
    RECT ClientRect = { 0 };
    if (GetClientRect(Window, &ClientRect)) {
        ClientWidth = ClientRect.right;
        ClientHeight = ClientRect.bottom;
    } else {
        MessageBox(0, "GetClientRect() failed", 0, 0);
        return GetLastError();
    }
    
    // Device & Context.
    
    ID3D11Device *BaseDevice;
    ID3D11DeviceContext *BaseContext;
    
    UINT CreationFlags = 0;
#ifdef _DEBUG
    CreationFlags = D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    
    HRESULT Result = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, CreationFlags, FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION, &BaseDevice, 0, &BaseContext);
    
    if (FAILED(Result)) {
        MessageBox(0, "D3D11CreateDevice failed", 0, 0);
        return GetLastError();
    }
    
    Result = ID3D11Device1_QueryInterface(BaseDevice, &IID_ID3D11Device1, (void **)&Device);
    assert(SUCCEEDED(Result));
    ID3D11Device1_Release(BaseDevice);
    
    Result = ID3D11DeviceContext1_QueryInterface(BaseContext, &IID_ID3D11DeviceContext1, (void **)&Context);
    assert(SUCCEEDED(Result));
    ID3D11Device1_Release(BaseContext);
    
    // Swap chain.
    
    IDXGIDevice2 *DxgiDevice;
    Result = ID3D11Device1_QueryInterface(Device, &IID_IDXGIDevice2, (void **)&DxgiDevice);
    assert(SUCCEEDED(Result));
    
    IDXGIAdapter *DxgiAdapter;
    Result = IDXGIDevice2_GetAdapter(DxgiDevice, &DxgiAdapter);
    assert(SUCCEEDED(Result));
    ID3D11Device1_Release(DxgiDevice);
    
    IDXGIFactory2 *DxgiFactory;
    Result = IDXGIDevice2_GetParent(DxgiAdapter, &IID_IDXGIFactory2, (void **)&DxgiFactory);
    assert(SUCCEEDED(Result));
    IDXGIAdapter_Release(DxgiAdapter);
    
    DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = { 0 };
    SwapChainDesc.Width = 0;
    SwapChainDesc.Height = 0;
    SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.SampleDesc.Quality = 0;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.BufferCount = 1;
    SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    SwapChainDesc.Flags = 0;
    
    IDXGISwapChain1 *SwapChain;
    Result = IDXGIFactory2_CreateSwapChainForHwnd(DxgiFactory, (IUnknown *)Device, Window, &SwapChainDesc, 0, 0, &SwapChain);
    assert(SUCCEEDED(Result));
    IDXGIFactory2_Release(DxgiFactory);
    
    // Render target view.
    
    ID3D11Texture2D *FrameBuffer;
    Result = IDXGISwapChain1_GetBuffer(SwapChain, 0, &IID_ID3D11Texture2D, (void **)&FrameBuffer);
    assert(SUCCEEDED(Result));
    
    ID3D11RenderTargetView *RenderTargetView;
    Result = ID3D11Device1_CreateRenderTargetView(Device, (ID3D11Resource *)FrameBuffer, 0, &RenderTargetView);
    assert(SUCCEEDED(Result));
    ID3D11Texture2D_Release(FrameBuffer);
    
    // Constant buffer.
    
    CreateConstantBuffer(sizeof(constants), NULL);
    
    // Viewport.	
    
    Viewport = (D3D11_VIEWPORT){
        .Width = (float)ClientWidth,
        .Height = (float)ClientHeight,
        .MaxDepth = 1.0f,
    };
    
    // Projection matrix.
    
    float AspectRatio = (float)ClientWidth / (float)ClientHeight;
    float Height = 1.0f;
    float Near = 1.0f;
    float Far = 100.0f;
    
    ProjectionMatrix = (matrix){
        2.0f * Near / AspectRatio, 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f * Near / Height, 0.0f, 0.0f,
        0.0f, 0.0f, Far / (Far - Near), 1.0f,
        0.0f, 0.0f, Near * Far / (Near - Far), 0.0f
    };
    
    // View matrix.
    
    ViewMatrix = (matrix){
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        -Camera.Position.X, -Camera.Position.Y, -Camera.Position.Z, 1.0f,
    };
    
    // So we can get raw input data from mouse in WinProc.
    
    RAWINPUTDEVICE Rid[1];
    Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
    Rid[0].dwFlags = RIDEV_INPUTSINK;
    Rid[0].hwndTarget = Window;
    RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));
    
    // Defaults.
    
    CreateDefaultMeshes();
    CreateDefaultShaders();
    CreateDefaultInputLayouts();
    CreateDefaultBlendStates();
    CreateDefaultTextures();
    CreateDefaultEntities();
    InitColors();
    
    // For calculating DeltaTime.
    
    QueryPerformanceFrequency(&PerformanceCounterFrequency);
    QueryPerformanceCounter(&PerformanceCounterStart);
    
    Init(); // You must define this.
    
    // Loop.
    
    while (Running) {
        
        MSG Message;
        while (PeekMessage(&Message, NULL, 0, 0, PM_REMOVE)) {
            if (Message.message == WM_QUIT)
                Running = 0;
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
        
        Input(); // You must define this.
        
        if(KeyPressed('O')) Running = 0;
        if(KeyPressed('P')) Pause = (Pause) ? 0 : 1;
        
        HandleCamera();
        
        Update(); // You must define this.
        
        float ClearColor[] = { EngineColorBackground.R, EngineColorBackground.G, EngineColorBackground.B };
        
        ID3D11DeviceContext1_ClearRenderTargetView(Context, RenderTargetView, ClearColor);
        ID3D11DeviceContext1_RSSetViewports(Context, 1, &Viewport);
        
        float BlendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        UINT BlendSampleMask = 0xffffffff;
        ID3D11DeviceContext1_OMSetBlendState(Context, BlendStates[DEFAULT_BLEND_STATE], BlendFactor, BlendSampleMask);
        
        ID3D11DeviceContext1_OMSetRenderTargets(Context, 1, &RenderTargetView, 0);
        ID3D11DeviceContext1_VSSetConstantBuffers(Context, 0, 1, &ConstantBuffers[0]);
        
        Draw(); // You must define this.
        
        IDXGISwapChain1_Present(SwapChain, 1, 0);
        
        UpdateDeltaTime();
        
    }
    
    return 0;
}

void CreateEntity(entity *Entity, int EntityIndex) {
    int Index = EntityIndex;
    if (Index == 0) Index = EntityCount++;
    DefaultEntities[Index] = *Entity;
}

void CreateDefaultEntities() {
    
    // Rectangle.
    
    {
        entity Entity = (entity){
            .Mesh = DEFAULT_MESH_RECTANGLE,
            .Shader = DEFAULT_SHADER_POSITION,
            .InputLayout = DEFAULT_INPUT_LAYOUT_POSITION,
            .PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
            .Color = ColorLightBlue,
            .Scale = {1.0f, 1.0f, 1.0f},
        };
        
        CreateEntity(&Entity, DEFAULT_ENTITY_RECTANGLE);
    }
    
    // Rectangle with lines.
    
    {
        entity Entity = (entity){
            .Mesh = DEFAULT_MESH_RECTANGLE_LINES,
            .Shader = DEFAULT_SHADER_POSITION,
            .InputLayout = DEFAULT_INPUT_LAYOUT_POSITION,
            .PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
            .Color = ColorLightBlue,
            .Scale = {1.0f, 1.0f, 1.0f},
        };
        
        CreateEntity(&Entity, DEFAULT_ENTITY_RECTANGLE_LINES);
    }
    
    // Line.
    
    {
        entity Entity = (entity){
            .Mesh = DEFAULT_MESH_LINE,
            .Shader = DEFAULT_SHADER_POSITION,
            .InputLayout = DEFAULT_INPUT_LAYOUT_POSITION,
            .PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST,
            .Color = ColorLightBlue,
            .Scale = {1.0f, 1.0f, 1.0f},
        };
        
        CreateEntity(&Entity, DEFAULT_ENTITY_LINE);
    }
    
    // Point.
    
    {
        entity Entity = (entity){
            .Mesh = DEFAULT_MESH_POINT,
            .Shader = DEFAULT_SHADER_POSITION,
            .InputLayout = DEFAULT_INPUT_LAYOUT_POSITION,
            .PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST,
            .Color = ColorLightBlue,
            .Scale = {1.0f, 1.0f, 1.0f},
        };
        
        CreateEntity(&Entity, DEFAULT_ENTITY_POINT);
    }
}

void CreateDefaultTextures() {
    
    // Font.
    
    textureInfo FontTextureInfo = { .USize = 1.0f / 16.0f, .VSize = 1.0f / 16.0f };
    
    CreateTexture("font_64_64.png", &FontTextureInfo, DEFAULT_TEXTURE_FONT);
}

void CreateDefaultShaders() {
    CreateShader(L"default_shaders_position.hlsl", NULL, DEFAULT_SHADER_POSITION);
    CreateShader(L"default_shaders_position_uv.hlsl", NULL, DEFAULT_SHADER_POSITION_UV);
    CreateShader(L"default_shaders_position_uv_atlas.hlsl", NULL, DEFAULT_SHADER_POSITION_UV_ATLAS);
}

void CreateDefaultInputLayouts() {
    
    {
        // INPUT_LAYOUT_POSITION
        
        D3D11_INPUT_ELEMENT_DESC
            InputElementDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        
        shader TempShader = { 0 };
        CreateShaderDiscard(L"dummy_shaders_position.hlsl", NULL, &TempShader);
        
        CreateInputLayout(&TempShader, InputElementDesc,
                          ARRAYSIZE(InputElementDesc),
                          DEFAULT_INPUT_LAYOUT_POSITION);
    }
    
    {
        // INPUT_LAYOUT_POSITION_UV
        
        D3D11_INPUT_ELEMENT_DESC
            InputElementDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"UV", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT,
                D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        
        shader TempShader = { 0 };
        CreateShaderDiscard(L"dummy_shaders_position_uv.hlsl", NULL, &TempShader);
        
        CreateInputLayout(&TempShader, InputElementDesc,
                          ARRAYSIZE(InputElementDesc),
                          DEFAULT_INPUT_LAYOUT_POSITION_UV);
    }
}

// TODO: handle info.

int CreateConstantBuffer(size_t Size, constantBufferInfo *Info) {
    
    D3D11_BUFFER_DESC ConstantBufferDesc = {
        .ByteWidth = Size,
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };
    
    HRESULT Result =
        ID3D11Device1_CreateBuffer(Device, &ConstantBufferDesc, NULL,
                                   &ConstantBuffers[ConstantBufferCount++]);
    assert(SUCCEEDED(Result));
    
    return ConstantBufferCount - 1;
}

// Returns index to Textures array.

int CreateTexture(const char *File, textureInfo *Info, int TextureIndex) {
    
    int Index = TextureIndex;
    if (Index == 0) Index = TextureCount++;
    texture *Texture = &Textures[Index];
    
    Texture->USize = 1.0f;
    Texture->VSize = 1.0f;
    
    if (Info != NULL) {
        Texture->UOffset = Info->UOffset;
        Texture->VOffset = Info->VOffset;
        Texture->USize = Info->USize;
        Texture->VSize = Info->VSize;
    }
    
    // Load image.
    
    int ImageWidth;
    int ImageHeight;
    int ImageChannels;
    int ImageDesiredChannels = 4;
    
    unsigned char *ImageData = stbi_load(File, &ImageWidth, &ImageHeight, &ImageChannels, ImageDesiredChannels);
    assert(ImageData);
    
    int ImagePitch = ImageWidth * 4;
    
    // Texture.
    
    D3D11_TEXTURE2D_DESC ImageTextureDesc = { 0 };
    
    ImageTextureDesc.Width = ImageWidth;
    ImageTextureDesc.Height = ImageHeight;
    ImageTextureDesc.MipLevels = 1;
    ImageTextureDesc.ArraySize = 1;
    ImageTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    ImageTextureDesc.SampleDesc.Count = 1;
    ImageTextureDesc.SampleDesc.Quality = 0;
    ImageTextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ImageTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    
    D3D11_SUBRESOURCE_DATA ImageSubresourceData = { 0 };
    
    ImageSubresourceData.pSysMem = ImageData;
    ImageSubresourceData.SysMemPitch = ImagePitch;
    
    ID3D11Texture2D *ImageTexture;
    
    HRESULT Result = ID3D11Device1_CreateTexture2D(
                                                   Device, &ImageTextureDesc, &ImageSubresourceData, &ImageTexture);
    assert(SUCCEEDED(Result));
    
    free(ImageData);
    
    // Shader resource view.
    
    Result = ID3D11Device1_CreateShaderResourceView(Device, (ID3D11Resource *)ImageTexture, NULL, &Texture->ShaderResourceView);
    assert(SUCCEEDED(Result));
    
    // Sampler.
    
    D3D11_SAMPLER_DESC ImageSamplerDesc = { 0 };
    
    ImageSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    ImageSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    ImageSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    ImageSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    ImageSamplerDesc.MipLODBias = 0.0f;
    ImageSamplerDesc.MaxAnisotropy = 1;
    ImageSamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    ImageSamplerDesc.BorderColor[0] = 1.0f;
    ImageSamplerDesc.BorderColor[1] = 1.0f;
    ImageSamplerDesc.BorderColor[2] = 1.0f;
    ImageSamplerDesc.BorderColor[3] = 1.0f;
    ImageSamplerDesc.MinLOD = -FLT_MAX;
    ImageSamplerDesc.MaxLOD = FLT_MAX;
    
    Result = ID3D11Device1_CreateSamplerState(Device, &ImageSamplerDesc, &Texture->SamplerState);
    assert(SUCCEEDED(Result));
    
    return Index;
}

int CreateMesh(float *Vertices, size_t Size, int StrideInt, int Offset,
               int MeshIndex) {
    
    int Index = MeshIndex;
    if (Index == 0) Index = MeshCount++;
    mesh *Mesh = &Meshes[Index];
    
    Mesh->Stride = StrideInt * sizeof(float);
    Mesh->NumVertices = Size / Mesh->Stride;
    Mesh->Offset = Offset;
    Mesh->Vertices = MemoryAlloc(Size);
    memcpy(Mesh->Vertices, Vertices, Size);
    
    D3D11_BUFFER_DESC BufferDesc = { Size, D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0 };
    
    D3D11_SUBRESOURCE_DATA InitialData = { Mesh->Vertices };
    
    ID3D11Device1_CreateBuffer(Device, &BufferDesc, &InitialData, &Mesh->Buffer);
    
    return Index;
}

void CreateDefaultMeshes() {
    
    // Triangle.
    
    float TriangleVertexData[] = {
        -0.5f, -0.5f, 0.0f,
        0.0f, 0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
    };
    
    CreateMesh(TriangleVertexData, sizeof(TriangleVertexData), 3, 0, DEFAULT_MESH_TRIANGLE);
    
    // Rectangle.
    
    float RectangleVertexData[] = {
        -0.5f, -0.5f, 0.0f,
        -0.5f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
        0.5f, 0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
    };
    
    CreateMesh(RectangleVertexData, sizeof(RectangleVertexData), 3, 0, DEFAULT_MESH_RECTANGLE);
    
    // Rectangle, with uv.
    
    float RectangleUVVertexData[] = {
        // xyz              // uv
        -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
        -0.5f, 0.5f, 0.0f, 0.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
        -0.5f, -0.5f, 0.0f, 0.0f, 1.0f,
        0.5f, 0.5f, 0.0f, 1.0f, 0.0f,
        0.5f, -0.5f, 0.0f, 1.0f, 1.0f,
    };
    
    CreateMesh(RectangleUVVertexData, sizeof(RectangleUVVertexData), 5, 0,
               DEFAULT_MESH_RECTANGLE_UV);
    
    // Rectangle, with lines.
    
    float RectangleLinesVertexData[] = {
        -0.5f, -0.5f, 0.0f,
        -0.5f, 0.5f, 0.0f,
        -0.5f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.0f,
        0.5f, 0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        0.5f, -0.5f, 0.0f,
        -0.5f, -0.5f, 0.0f,
    };
    
    CreateMesh(RectangleLinesVertexData, sizeof(RectangleLinesVertexData), 3, 0,
               DEFAULT_MESH_RECTANGLE_LINES);
    
    // Line.
    
    float LineVertexData[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
    };
    
    CreateMesh(LineVertexData, sizeof(LineVertexData), 3, 0, DEFAULT_MESH_LINE);
    
    // Point.
    
    float PointVertexData[] = { 0.0f, 0.0f, 0.0f };
    
    CreateMesh(PointVertexData, sizeof(PointVertexData), 3, 0, DEFAULT_MESH_POINT);
}

void DrawEntityArgs(v3 Position, v3 Scale, float Rotation, color Color,
                    int Mesh, int Texture, int Shader, int ConstantBuffer,
                    int InputLayout, int PrimitiveTopology) {
    
    if (Texture) {
        ID3D11DeviceContext1_PSSetShaderResources(Context, 0, 1, &Textures[Texture].ShaderResourceView);
        ID3D11DeviceContext1_PSSetSamplers(Context, 0, 1, &Textures[Texture].SamplerState);
    }
    
    ID3D11DeviceContext1_IASetInputLayout(Context, InputLayouts[InputLayout]);
    ID3D11DeviceContext1_VSSetShader(Context, Shaders[Shader].VertexShader, 0, 0);
    ID3D11DeviceContext1_PSSetShader(Context, Shaders[Shader].PixelShader, 0, 0);
    ID3D11DeviceContext1_IASetPrimitiveTopology(Context, PrimitiveTopology);
    ID3D11DeviceContext1_IASetVertexBuffers(Context, 0, 1, &Meshes[Mesh].Buffer, &Meshes[Mesh].Stride, &Meshes[Mesh].Offset);
    
    D3D11_MAPPED_SUBRESOURCE MappedSubresource;
    
    ID3D11DeviceContext1_Map(Context,
                             (ID3D11Resource *)ConstantBuffers[ConstantBuffer], 0,
                             D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource);
    constants *Constants = (constants *)MappedSubresource.pData;
    
    Constants->Model = (matrix){
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        Position.X, Position.Y, Position.Z, 1.0f
    };
    
    float Theta = DegreesToRadians(Rotation);
    
    Constants->Rotation = (matrix){
        cos(Theta), sin(Theta), 0.0f, 0.0f,
        -sin(Theta), cos(Theta), 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    Constants->Scale = (matrix){
        Scale.X, 0.0f, 0.0f, 0.0f,
        0.0f, Scale.Y, 0.0f, 0.0f,
        0.0f, 0.0f, Scale.Z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    
    Constants->View = ViewMatrix;
    Constants->Projection = ProjectionMatrix;
    Constants->Color = Color;
    
    if (Texture) {
        Constants->UOffset = Textures[Texture].UOffset;
        Constants->VOffset = Textures[Texture].VOffset;
        Constants->USize = Textures[Texture].USize;
        Constants->VSize = Textures[Texture].VSize;
    }
    
    ID3D11DeviceContext1_Unmap(Context, (ID3D11Resource *)ConstantBuffers[ConstantBuffer], 0);
    ID3D11DeviceContext1_Draw(Context, Meshes[Mesh].NumVertices, 0);
}

void DrawEntity(entity *Entity) {
    if(!Entity->Alive) return;
    if (Entity->Texture) {
        ID3D11DeviceContext1_PSSetShaderResources(Context, 0, 1, &Textures[Entity->Texture].ShaderResourceView);
        ID3D11DeviceContext1_PSSetSamplers(Context, 0, 1, &Textures[Entity->Texture].SamplerState);
    }
    
    ID3D11DeviceContext1_IASetInputLayout(Context, InputLayouts[Entity->InputLayout]);
    ID3D11DeviceContext1_VSSetShader(Context, Shaders[Entity->Shader].VertexShader, 0, 0);
    ID3D11DeviceContext1_PSSetShader(Context, Shaders[Entity->Shader].PixelShader, 0, 0);
    ID3D11DeviceContext1_IASetPrimitiveTopology(Context, Entity->PrimitiveTopology);
    ID3D11DeviceContext1_IASetVertexBuffers(Context, 0, 1, &Meshes[Entity->Mesh].Buffer, &Meshes[Entity->Mesh].Stride, &Meshes[Entity->Mesh].Offset);
    
    D3D11_MAPPED_SUBRESOURCE MappedSubresource;
    
    ID3D11DeviceContext1_Map(Context, (ID3D11Resource *)ConstantBuffers[Entity->ConstantBuffer], 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource);
    constants *Constants = (constants *)MappedSubresource.pData;
    
    Constants->Model = (matrix){
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        Entity->Position.X, Entity->Position.Y, Entity->Position.Z, 1.0f
    };
    
    float Theta = DegreesToRadians(Entity->Rotation);
    
    Constants->Rotation = (matrix){
        cos(Theta), sin(Theta), 0.0f, 0.0f,
        -sin(Theta), cos(Theta), 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    
    Constants->Scale = (matrix){
        Entity->Scale.X, 0.0f, 0.0f, 0.0f,
        0.0f, Entity->Scale.Y, 0.0f, 0.0f,
        0.0f, 0.0f, Entity->Scale.Z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    
    Constants->View = ViewMatrix;
    Constants->Projection = ProjectionMatrix;
    Constants->Color = Entity->Color;
    
    if (Entity->Texture) {
        Constants->UOffset = Textures[Entity->Texture].UOffset;
        Constants->VOffset = Textures[Entity->Texture].VOffset;
        Constants->USize = Textures[Entity->Texture].USize;
        Constants->VSize = Textures[Entity->Texture].VSize;
    }
    
    ID3D11DeviceContext1_Unmap(Context, (ID3D11Resource *)ConstantBuffers[Entity->ConstantBuffer], 0);
    ID3D11DeviceContext1_Draw(Context, Meshes[Entity->Mesh].NumVertices, 0);
    
    if(Entity->ShowHealthBar) {
        DrawHealthBar(Entity);
    }
    
}

void CreateDefaultBlendStates() { CreateBlendState(); }

// TODO: params.

int CreateBlendState() {
    
    D3D11_BLEND_DESC BlendStateDesc = {
        .RenderTarget[0].BlendEnable = TRUE,
        .RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
        
        .RenderTarget[0].SrcBlend = D3D11_BLEND_ONE,
        .RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE,
        .RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
        .RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
        
        .RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD,
        .RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD,
    };
    
    ID3D11Device1_CreateBlendState(Device, &BlendStateDesc, &BlendStates[DEFAULT_BLEND_STATE]);
    return BlendStateCount - 1;
}

// This is used to create temp shaders for input layouts.

int CreateShaderDiscard(const wchar_t *Filename, shaderInfo *Info,
                        shader *Shader) {
    
    HRESULT Result = E_FAIL;
    
    if (Info == NULL) {
        Result = D3DCompileFromFile(Filename, NULL, NULL, "vs_main", "vs_5_0", NULL, NULL, &Shader->VSBlob, NULL);
        assert(SUCCEEDED(Result));
        
        Result = D3DCompileFromFile(Filename, NULL, NULL, "ps_main", "ps_5_0", NULL, NULL, &Shader->PSBlob, NULL);
        assert(SUCCEEDED(Result));
    } else {
        
        // TODO: handle all Info fields.
        
        Result = D3DCompileFromFile(Filename, Info->VSMacros, NULL, "vs_main", "vs_5_0", NULL, NULL, &Shader->VSBlob, NULL);
        assert(SUCCEEDED(Result));
        Result = D3DCompileFromFile(Filename, Info->PSMacros, NULL, "ps_main", "ps_5_0", NULL, NULL, &Shader->PSBlob, NULL);
        assert(SUCCEEDED(Result));
    }
    
    Result = ID3D11Device1_CreateVertexShader(Device, ID3D10Blob_GetBufferPointer(Shader->VSBlob), ID3D10Blob_GetBufferSize(Shader->VSBlob), 0, &Shader->VertexShader);
    assert(SUCCEEDED(Result));
    
    Result = ID3D11Device1_CreatePixelShader(Device, ID3D10Blob_GetBufferPointer(Shader->PSBlob), ID3D10Blob_GetBufferSize(Shader->PSBlob), 0, &Shader->PixelShader);
    assert(SUCCEEDED(Result));
    
    return Result;
}

int CreateShader(const wchar_t *Filename, shaderInfo *Info, int ShaderIndex) {
    
    int Index = ShaderIndex;
    if (ShaderIndex == 0) Index = ShaderCount++;
    shader *Shader = &Shaders[Index];
    
    HRESULT Result = E_FAIL;
    
    if (Info == NULL) {
        Result = D3DCompileFromFile(Filename, NULL, NULL, "vs_main", "vs_5_0", NULL, NULL, &Shader->VSBlob, NULL);
        assert(SUCCEEDED(Result));
        
        Result = D3DCompileFromFile(Filename, NULL, NULL, "ps_main", "ps_5_0", NULL, NULL, &Shader->PSBlob, NULL);
        assert(SUCCEEDED(Result));
    } else {
        
        // TODO: handle all Info fields.
        
        Result = D3DCompileFromFile(Filename, Info->VSMacros, NULL, "vs_main", "vs_5_0", NULL, NULL, &Shader->VSBlob, NULL);
        assert(SUCCEEDED(Result));
        Result = D3DCompileFromFile(Filename, Info->PSMacros, NULL, "ps_main", "ps_5_0", NULL, NULL, &Shader->PSBlob, NULL);
        assert(SUCCEEDED(Result));
    }
    
    Result = ID3D11Device1_CreateVertexShader(Device, ID3D10Blob_GetBufferPointer(Shader->VSBlob), ID3D10Blob_GetBufferSize(Shader->VSBlob), 0, &Shader->VertexShader);
    assert(SUCCEEDED(Result));
    
    Result = ID3D11Device1_CreatePixelShader(Device, ID3D10Blob_GetBufferPointer(Shader->PSBlob), ID3D10Blob_GetBufferSize(Shader->PSBlob), 0, &Shader->PixelShader);
    assert(SUCCEEDED(Result));
    
    return Index;
}

int CreateInputLayout(shader *Shader, D3D11_INPUT_ELEMENT_DESC *Desc,
                      size_t Size, int InputLayoutIndex) {
    
    int Index = InputLayoutIndex;
    
    if (Index == 0) { Index = InputLayoutCount++; }
    
    HRESULT Result = ID3D11Device1_CreateInputLayout(Device, Desc, Size, ID3D10Blob_GetBufferPointer(Shader->VSBlob), ID3D10Blob_GetBufferSize(Shader->VSBlob), &InputLayouts[Index]);
    assert(SUCCEEDED(Result));
    
    return Index;
}

// TODO: Broken. Fix.

void GridInit(grid *Grid) {
    
    // Shaders.
    
    HRESULT Result = D3DCompileFromFile(L"shaders_grid.hlsl", 0, 0, "vs_main", "vs_5_0", 0, 0, &Grid->VSBlob, 0);
    assert(SUCCEEDED(Result));
    
    Result = ID3D11Device1_CreateVertexShader(Device, ID3D10Blob_GetBufferPointer(Grid->VSBlob), ID3D10Blob_GetBufferSize(Grid->VSBlob), 0, &Grid->VertexShader);
    assert(SUCCEEDED(Result));
    
    Result = D3DCompileFromFile(L"shaders_grid.hlsl", 0, 0, "ps_main", "ps_5_0", 0, 0, &Grid->PSBlob, 0);
    assert(SUCCEEDED(Result));
    
    Result = ID3D11Device1_CreatePixelShader(Device, ID3D10Blob_GetBufferPointer(Grid->PSBlob), ID3D10Blob_GetBufferSize(Grid->PSBlob), 0, &Grid->PixelShader);
    assert(SUCCEEDED(Result));
    
    // Data layout.
    
    D3D11_INPUT_ELEMENT_DESC GridInputElementDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    
    Result = ID3D11Device1_CreateInputLayout(Device, GridInputElementDesc, ARRAYSIZE(GridInputElementDesc), ID3D10Blob_GetBufferPointer(Grid->VSBlob), ID3D10Blob_GetBufferSize(Grid->VSBlob), &Grid->InputLayout);
    assert(SUCCEEDED(Result));
    
    // Mesh.
    
    if (Grid->Size <= 0.0f) Grid->Size = 1.0f;
    
    int YLines = Grid->Height + 1;
    int XLines = Grid->Width + 1;
    float HalfSize = Grid->Size / 2.0f;
    float *Vertices = NULL;
    
    size_t Size = (YLines * 6 + XLines * 6) * sizeof(*Vertices);
    Vertices = MemoryAlloc(Size);
    
    int VerticesIndex = 0;
    
    for (int Line = 0; Line < YLines; ++Line) {
        Vertices[0 + Line * 6] = -HalfSize;
        Vertices[1 + Line * 6] = -HalfSize + (float)Line * Grid->Size;
        Vertices[2 + Line * 6] = 0.0f;
        Vertices[3 + Line * 6] = -HalfSize + (float)Grid->Width;
        Vertices[4 + Line * 6] = -HalfSize + (float)Line * Grid->Size;
        Vertices[5 + Line * 6] = 0.0f;
        VerticesIndex = 5 + Line * 6;
    }
    
    for (int Line = 0; Line < XLines; ++Line) {
        Vertices[VerticesIndex + 1 + 0 + Line * 6] =
            -HalfSize + (float)Line * Grid->Size;
        Vertices[VerticesIndex + 1 + 1 + Line * 6] = -HalfSize;
        Vertices[VerticesIndex + 1 + 2 + Line * 6] = 0.0f;
        Vertices[VerticesIndex + 1 + 3 + Line * 6] =
            -HalfSize + (float)Line * Grid->Size;
        Vertices[VerticesIndex + 1 + 4 + Line * 6] = -HalfSize + (float)Grid->Width;
        Vertices[VerticesIndex + 1 + 5 + Line * 6] = 0.0f;
    }
    
    Grid->Mesh = CreateMesh(Vertices, Size, 3, 0, 0);
}

void GridDraw(grid *Grid) {
    
    ID3D11DeviceContext1_IASetInputLayout(Context, Grid->InputLayout);
    ID3D11DeviceContext1_VSSetShader(Context, Grid->VertexShader, 0, 0);
    ID3D11DeviceContext1_PSSetShader(Context, Grid->PixelShader, 0, 0);
    
    ID3D11DeviceContext1_IASetPrimitiveTopology(
                                                Context, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ID3D11DeviceContext1_IASetVertexBuffers(
                                            Context, 0, 1, &Meshes[Grid->Mesh].Buffer, &Meshes[Grid->Mesh].Stride,
                                            &Meshes[Grid->Mesh].Offset);
    
    D3D11_MAPPED_SUBRESOURCE MappedSubresource;
    ID3D11DeviceContext1_Map(Context, (ID3D11Resource *)ConstantBuffers[0], 0,
                             D3D11_MAP_WRITE_DISCARD, 0, &MappedSubresource);
    constants *Constants = (constants *)MappedSubresource.pData;
    Constants->Model = (matrix){
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    
    Constants->View = ViewMatrix;
    Constants->Projection = ProjectionMatrix;
    Constants->Color = Grid->Color;
    ID3D11DeviceContext1_Unmap(Context, (ID3D11Resource *)ConstantBuffers[0], 0);
    ID3D11DeviceContext1_Draw(Context, Meshes[Grid->Mesh].NumVertices, 0);
}

// Camera.

void CameraUpdateByAcceleration(v3 Acceleration) {
    v3 CameraVelocity = { 0 };
    
    CameraVelocity = V3Add(
                           CameraVelocity, V3MultiplyScalar(Acceleration, DeltaTime * Camera.Speed));
    Camera.Position =
        V3Add(Camera.Position,
              V3MultiplyScalar(CameraVelocity, DeltaTime * Camera.Speed));
    
    ViewMatrix = (matrix){
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        -Camera.Position.X, -Camera.Position.Y, -Camera.Position.Z, 1.0f,
    };
}

void HandleCamera() {
    
    v3 CameraAcceleration = { 0 };
    
    if (KeyDown('W')) { CameraAcceleration.Y = 1.0f; }
    if (KeyDown('A')) { CameraAcceleration.X = -1.0f; }
    if (KeyDown('S')) { CameraAcceleration.Y = -1.0f; }
    if (KeyDown('D')) { CameraAcceleration.X = 1.0f; }
    if (KeyDown('Q')) { CameraAcceleration.Z = -1.0f; }
    if (KeyDown('E')) { CameraAcceleration.Z = 1.0f; }
    
    if (Mouse.WheelDown) {
        CameraAcceleration.Z = -10.0f;
        Mouse.WheelDown = 0;
    }
    if (Mouse.WheelUp) {
        CameraAcceleration.Z = 10.0f;
        Mouse.WheelUp = 0;
    }
    
    CameraUpdateByAcceleration(CameraAcceleration);
}

// Misc

// Note: creates a mesh every iteration, for testing.

/*
void DrawRectangle(rectangle* R, color Color) {

    float RectangleLinesVertexData[] = {
        R->Left, R->Bottom, 0.0f,
        R->Left, R->Top, 0.0f,
        R->Left, R->Top, 0.0f,
        R->Right, R->Top, 0.0f,
        R->Right, R->Top, 0.0f,
        R->Right, R->Bottom, 0.0f,
        R->Right, R->Bottom, 0.0f,
        R->Left, R->Bottom, 0.0f,
    };

    TestRectangleMesh = CreateMesh(RectangleLinesVertexData,
sizeof(RectangleLinesVertexData), 3, 0, 0);

    DrawEntity((v3){0.0f, 0.0f, 0.0f},
                        (v3){1.0f, 1.0f, 1.0f},
                        0.0f,
                        Color,
                        TestRectangleMesh,
                        0,
                        DEFAULT_SHADER_POSITION,
                        1,
                        DEFAULT_INPUT_LAYOUT_POSITION,
                        D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
}
*/

int PickMeshRectangle(int MouseX, int MouseY, v3 Position, mesh *Mesh) {
    
    int Result = 0;
    
    // Map mousecoords into [-1,1] range.
    float X = ((2.0f * (float)MouseX) / (float)ClientWidth) - 1.0f;
    float Y = (((2.0f * (float)MouseY) / (float)ClientHeight) - 1.0f) * -1.0f;
    
    X = X / ProjectionMatrix.M[0][0];
    Y = Y / ProjectionMatrix.M[1][1];
    
    matrix InverseViewMatrix = { 0 };
    MatrixInverse(&ViewMatrix, &InverseViewMatrix);
    
    v3 Direction = { 0 };
    Direction.X = X * InverseViewMatrix.M[0][0] + Y * InverseViewMatrix.M[1][0] +
        InverseViewMatrix.M[2][0];
    Direction.Y = X * InverseViewMatrix.M[0][1] + Y * InverseViewMatrix.M[1][1] +
        InverseViewMatrix.M[2][1];
    Direction.Z = X * InverseViewMatrix.M[0][2] + Y * InverseViewMatrix.M[1][2] +
        InverseViewMatrix.M[2][2];
    
    v3 Origin = Camera.Position;
    matrix TranslatedModel = MatrixTranslation(Position);
    
    matrix InverseModel = { 0 };
    MatrixInverse(&TranslatedModel, &InverseModel);
    
    v3 RayOrigin = V3TransformCoord(&Origin, &InverseModel);
    v3 RayDirection = V3TransformNormal(&Direction, &InverseModel);
    
    V3Normalize(&RayDirection);
    
    // TODO: This is mesh with UV.
    
    /*     
        triangle Triangle1 = {
            .A = {Mesh->Vertices[0], Mesh->Vertices[1], Mesh->Vertices[2]},
            .B = {Mesh->Vertices[5], Mesh->Vertices[6], Mesh->Vertices[7]},
            .C = {Mesh->Vertices[10], Mesh->Vertices[11], Mesh->Vertices[12]} };
        
        triangle Triangle2 = {
            .A = {Mesh->Vertices[15], Mesh->Vertices[16], Mesh->Vertices[17]},
            .B = {Mesh->Vertices[20], Mesh->Vertices[21], Mesh->Vertices[22]},
            .C = {Mesh->Vertices[25], Mesh->Vertices[26], Mesh->Vertices[27]} };
         */
    
    triangle Triangle1 = {
        .A = {Mesh->Vertices[0], Mesh->Vertices[1], Mesh->Vertices[2]},
        .B = {Mesh->Vertices[3], Mesh->Vertices[4], Mesh->Vertices[5]},
        .C = {Mesh->Vertices[6], Mesh->Vertices[7], Mesh->Vertices[8]} };
    
    triangle Triangle2 = {
        .A = {Mesh->Vertices[9], Mesh->Vertices[10], Mesh->Vertices[11]},
        .B = {Mesh->Vertices[12], Mesh->Vertices[13], Mesh->Vertices[14]},
        .C = {Mesh->Vertices[15], Mesh->Vertices[16], Mesh->Vertices[17]} };
    
    if (RayTriangleIntersect(RayOrigin, RayDirection, &Triangle1) ||
        RayTriangleIntersect(RayOrigin, RayDirection, &Triangle2)) {
        Result = 1;
    }
    
    return Result;
}

rectangle GetRectangleByPositionAndScale(v3 Position, v3 Scale) {
    return (rectangle) {
        .Left = Position.X - Scale.X / 2.0f,
        .Right = Position.X + Scale.X / 2.0f,
        .Top = Position.Y + Scale.Y / 2.0f,
        .Bottom = Position.Y - Scale.Y / 2.0f,
    };
}

int RotatedRectanglesCollide(v3 Scale, v3 Position, float Rotation, v3 Scale2,
                             v3 Position2, float Rotation2) {
    
    int HitCounter = 0;
    
    {
        v3 Corners[4] = { 0 };
        GetRectangleCorners(Corners, Scale, Position, Rotation);
        DrawRectangleCorners(Corners);
        
        lineODL Axes[2] = { 0 };
        GetRectangleAxes(Axes, Scale2, Position2, Rotation2);
        DrawRectangleAxes(Axes);
        
        v3 Projections[8] = { 0 };
        GetCornerToAxisProjections(Projections, Corners, Axes);
        DrawCornerToAxisProjections(Corners, Projections);
        
        line XSegment = { 0 };
        line YSegment = { 0 };
        GetSegments(&XSegment, &YSegment, Projections);
        DrawSegments(XSegment, YSegment, Position2, Scale2);
        
        if (AxisSegmentIntersectsRectangle(XSegment, Position2, Scale2, 0) &&
            AxisSegmentIntersectsRectangle(YSegment, Position2, Scale2, 1)) {
            HitCounter += 2;
        }
    }
    
    {
        v3 Corners[4] = { 0 };
        GetRectangleCorners(Corners, Scale2, Position2, Rotation2);
        DrawRectangleCorners(Corners);
        
        lineODL Axes[2] = { 0 };
        GetRectangleAxes(Axes, Scale, Position, Rotation);
        DrawRectangleAxes(Axes);
        
        v3 Projections[8] = { 0 };
        GetCornerToAxisProjections(Projections, Corners, Axes);
        DrawCornerToAxisProjections(Corners, Projections);
        
        line XSegment = { 0 };
        line YSegment = { 0 };
        GetSegments(&XSegment, &YSegment, Projections);
        DrawSegments(XSegment, YSegment, Position, Scale);
        
        if (AxisSegmentIntersectsRectangle(XSegment, Position, Scale, 0) &&
            AxisSegmentIntersectsRectangle(YSegment, Position, Scale, 1)) {
            HitCounter += 2;
        }
    }
    
    if (HitCounter == 4) {
        return 1;
    }
    return 0;
}

int RectangleOutOfBounds(rectangle Rectangle, rectangle Bounds) {
    if (Rectangle.Top > Bounds.Top) return 1;
    if (Rectangle.Bottom < Bounds.Bottom) return 1;
    if (Rectangle.Left < Bounds.Left) return 1;
    if (Rectangle.Right > Bounds.Right) return 1;
    return 0;
}

int RectangleBoundsCollision(rectangle Rectangle, rectangle Bounds,
                             v3 *Normal) {
    V3Zero(Normal);
    
    if (Rectangle.Top > Bounds.Top) {
        *Normal = (v3){ 0.0f, -1.0f, 0.0f };
    } else if (Rectangle.Bottom < Bounds.Bottom) {
        *Normal = (v3){ 0.0f, 1.0f, 0.0f };
    } else if (Rectangle.Left < Bounds.Left) {
        *Normal = (v3){ 1.0f, 0.0f, 0.0f };
    } else if (Rectangle.Right > Bounds.Right) {
        *Normal = (v3){ -1.0f, 0.0f, 0.0f };
    }
    
    if (!V3IsZero(*Normal))
        return 1;
    
    return 0;
}

void DrawRectangleCollisionData(v3 Scale, v3 Position, float Rotation,
                                v3 Scale2, v3 Position2, float Rotation2) {
    
    int HitCounter = 0;
    
    {
        v3 Corners[4] = { 0 };
        GetRectangleCorners(Corners, Scale, Position, Rotation);
        DrawRectangleCorners(Corners);
        
        lineODL Axes[2] = { 0 };
        GetRectangleAxes(Axes, Scale2, Position2, Rotation2);
        DrawRectangleAxes(Axes);
        
        v3 Projections[8] = { 0 };
        GetCornerToAxisProjections(Projections, Corners, Axes);
        DrawCornerToAxisProjections(Corners, Projections);
        
        line XSegment = { 0 };
        line YSegment = { 0 };
        GetSegments(&XSegment, &YSegment, Projections);
        DrawSegments(XSegment, YSegment, Position2, Scale2);
        
        if (AxisSegmentIntersectsRectangle(XSegment, Position2, Scale2, 0) &&
            AxisSegmentIntersectsRectangle(YSegment, Position2, Scale2, 1)) {
            HitCounter += 2;
        }
    }
    
    {
        v3 Corners[4] = { 0 };
        GetRectangleCorners(Corners, Scale2, Position2, Rotation2);
        DrawRectangleCorners(Corners);
        
        lineODL Axes[2] = { 0 };
        GetRectangleAxes(Axes, Scale, Position, Rotation);
        DrawRectangleAxes(Axes);
        
        v3 Projections[8] = { 0 };
        GetCornerToAxisProjections(Projections, Corners, Axes);
        DrawCornerToAxisProjections(Corners, Projections);
        
        line XSegment = { 0 };
        line YSegment = { 0 };
        GetSegments(&XSegment, &YSegment, Projections);
        DrawSegments(XSegment, YSegment, Position, Scale);
        
        if (AxisSegmentIntersectsRectangle(XSegment, Position, Scale, 0) &&
            AxisSegmentIntersectsRectangle(YSegment, Position, Scale, 1)) {
            HitCounter += 2;
        }
    }
}

// Draws a circle using the font file.
void DrawCircle(v3 Position, color Color, v3 Scale) {
    
    v3 NewPosition = Position;
    Textures[DEFAULT_TEXTURE_FONT].UOffset = 7;
    
    DrawEntityArgs(NewPosition, Scale, 0.0f, Color, DEFAULT_MESH_RECTANGLE_UV,
                   DEFAULT_TEXTURE_FONT, DEFAULT_SHADER_POSITION_UV_ATLAS, 0,
                   DEFAULT_INPUT_LAYOUT_POSITION_UV,
                   D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}


// TODO: Not very good. Looks ugly.
void DrawString(v3 Position, char *String, color Color, v3 Scale) {
    
    v3 NewPosition = Position;
    
    while (*String) {
        
        Textures[DEFAULT_TEXTURE_FONT].UOffset = *String % 16;
        Textures[DEFAULT_TEXTURE_FONT].VOffset = *String / 16;
        
        DrawEntityArgs(NewPosition, Scale, 0.0f, Color, DEFAULT_MESH_RECTANGLE_UV,
                       DEFAULT_TEXTURE_FONT, DEFAULT_SHADER_POSITION_UV_ATLAS, 0,
                       DEFAULT_INPUT_LAYOUT_POSITION_UV,
                       D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ++String;
        NewPosition.X += Scale.X;
    }
}

int IsRepeat(LPARAM LParam) { return (HIWORD(LParam) & KF_REPEAT); }

/*
TIMER
Usage:
timer Timer = {0};
if(TimeElapsed(5.0f, &Timer)) {};
*/

void StartTimer(timer *Timer) {
    QueryPerformanceCounter(&Timer->StartingCount);
}

// True if Seconds elapsed. Resets the timer.
int TimeElapsed(float Seconds, timer *Timer) {
    // Start timer if not started.
    if(Timer->StartingCount.QuadPart == 0) StartTimer(Timer);
    // Get current current count.
    QueryPerformanceCounter(&Timer->EndingCount); 
    // Get total count.
    Timer->TotalCount.QuadPart = Timer->EndingCount.QuadPart - Timer->StartingCount.QuadPart;
    // For precision.
    Timer->TotalCount.QuadPart *= 1000000;
    // Calculate elapsed milliseconds.
    Timer->ElapsedMilliSeconds = ((double)Timer->TotalCount.QuadPart / (double)PerformanceCounterFrequency.QuadPart) /
        1000.0f;
    
    if(Timer->ElapsedMilliSeconds > (Seconds * 1000.0f)) {
        // Reset.
        Timer->StartingCount = Timer->EndingCount;
        return 1;
    }
    
    return 0;
}

// Get non-diagonal surrounding tiles of (XCenter, YCenter) from a one dimensional int array.
int GetSurroundingTilesWithValue(int *Tiles, int Value, int XCenter, int YCenter, int Width, int Height, int **Result) {
    
    int Dirs[] = {0,-1, -1, 0, 1, 0, 0, 1};
    
    int Temp[4] = {0};
    int ResultSize = 0;
    
    for(int Index = 0; Index < 8; Index+=2) {
        int X = XCenter+Dirs[Index];
        int Y = YCenter+Dirs[Index+1];
        
        if((X < 0) || (X >= Width))  continue;
        if((Y < 0) || (Y >= Height)) continue;
        
        int ValueIndex = Y * Width + X;
        
        if(Tiles[ValueIndex] == Value) {
            Temp[ResultSize++] = ValueIndex;
        }
    }
    
    *Result = MemoryAlloc(ResultSize * sizeof(*Result));
    memcpy(*Result, Temp, ResultSize * sizeof(*Result));
    
    return ResultSize;
}

int ValueInIntArray(int *Array, int Size, int Value) {
    for(int Index = 0; Index < Size; ++Index) {
        if(Array[Index] == Value) return 1;
    }
    return 0;
}

// Get surrounding tiles (including diagonals) of (XCenter, YCenter) from a one dimensional int array.
int GetSurroundingTilesWithValueDiagonal(int *Tiles, int Value, int XCenter, int YCenter, int Width, int Height, int **Result) {
    
    int Dirs[] = {
        -1,-1,  0,-1,  1,-1,
        -1, 0,         1, 0,
        -1, 1,  0, 1,  1, 1,
    };
    
    int Temp[8] = {0};
    int ResultSize = 0;
    
    for(int Index = 0; Index < 16; Index+=2) {
        int X = XCenter+Dirs[Index];
        int Y = YCenter+Dirs[Index+1];
        
        if((X < 0) || (X >= Width))  continue;
        if((Y < 0) || (Y >= Height)) continue;
        
        int ValueIndex = Y * Width + X;
        
        if(Tiles[ValueIndex] == Value) {
            Temp[ResultSize++] = ValueIndex;
        }
    }
    
    *Result = MemoryAlloc(ResultSize * sizeof(*Result));
    memcpy(*Result, Temp, ResultSize * sizeof(*Result));
    
    return ResultSize;
}

/*
Möller–Trumbore intersection algorithm
https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
*/

int RayTriangleIntersect(v3 RayOrigin, v3 RayDirection, triangle *Triangle) {
    const float EPSILON = 0.0000001f;
    v3 Vertex0 = Triangle->A;
    v3 Vertex1 = Triangle->B;
    v3 Vertex2 = Triangle->C;
    v3 Edge1, Edge2, H, S, Q;
    float A, F, U, V;
    Edge1 = V3Subtract(Vertex1, Vertex0);
    Edge2 = V3Subtract(Vertex2, Vertex0);
    H = V3CrossProduct(RayDirection, Edge2);
    A = V3DotProduct(Edge1, H);
    if (A > -EPSILON && A < EPSILON)
        return 0;
    F = 1.0f / A;
    S = V3Subtract(RayOrigin, Vertex0);
    U = F * V3DotProduct(S, H);
    if (U < 0.0f || U > 1.0f)
        return 0;
    Q = V3CrossProduct(S, Edge1);
    V = F * V3DotProduct(RayDirection, Q);
    if (V < 0.0f || ((U + V) > 1.0f))
        return 0;
    float T = F * V3DotProduct(Edge2, Q);
    if (T > EPSILON)
        return 1;
    return 0;
}

// Intersection test for axis-aligned rectangles.
int RectanglesIntersect(rectangle A, rectangle B) {
    if (A.Left > B.Right) return 0;
    if (B.Left > A.Right) return 0;
    if (A.Bottom > B.Top) return 0;
    if (B.Bottom > A.Top) return 0;
    return 1;
}

int ColorIsZero(color Color) {
    if (Color.R == 0.0f && 
        Color.G == 0.0f && 
        Color.B == 0.0f &&
        Color.A == 0.0f) {
        return 1;
    }
    return 0;
}

float GetRandomZeroToOne() { return (float)rand() / (float)RAND_MAX; }

color GetRandomColor() {
    return (color) {
        GetRandomZeroToOne(),
        GetRandomZeroToOne(),
        GetRandomZeroToOne(),
    };
}

color GetRandomShadeOfGray() {
    float Shade = GetRandomZeroToOne();
    return (color) { Shade, Shade, Shade };
}

color GetColorByRGB(int R, int G, int B) {
    return (color) {
        (float)R / 255.0f,
        (float)G / 255.0f,
        (float)B / 255.0f,
        1.0f
    };
}

// Debug.

void Debug(char *Format, ...) {
    va_list Arguments;
    va_start(Arguments, Format);
    char String[1024] = { 0 };
    vsprintf(String, Format, Arguments);
    OutputDebugString(String);
    va_end(Arguments);
}

void DebugV3(char *Message, v3 *V) {
    OutputDebugString(Message);
    char String[24] = { 0 };
    sprintf(String, "\n%+06.2f %+06.2f %+06.2f\n", V->X, V->Y, V->Z);
    OutputDebugString(String);
}

void DebugMatrix(char *Message, matrix *M) {
    OutputDebugString(Message);
    char String[1024] = { 0 };
    sprintf(String,
            "\n%+06.2f %+06.2f %+06.2f %+06.2f\n"
            "%+06.2f %+06.2f %+06.2f %+06.2f\n"
            "%+06.2f %+06.2f %+06.2f %+06.2f\n"
            "%+06.2f %+06.2f %+06.2f %+06.2f\n",
            M->M[0][0], M->M[0][1], M->M[0][2], M->M[0][3], M->M[1][0],
            M->M[1][1], M->M[1][2], M->M[1][3], M->M[2][0], M->M[2][1],
            M->M[2][2], M->M[2][3], M->M[3][0], M->M[3][1], M->M[3][2],
            M->M[3][3]);
    OutputDebugString(String);
}

// Memory.

MemoryInit(size_t Size) {
    MemoryBackend = malloc(Size);
    Memory.Data = (unsigned char *)MemoryBackend;
    Memory.Length = Size;
    Memory.Offset = 0;
}

void *MemoryAlloc(size_t Size) {
    void *Pointer = NULL;
    if (Memory.Offset + Size <= Memory.Length) {
        Pointer = &Memory.Data[Memory.Offset];
        Memory.Offset += Size;
        memset(Pointer, 0, Size);
    }
    
    if (Pointer == NULL) {
        Debug("Game over, man. Game over!");
    }
    
    return Pointer;
}

// Maths.

float DegreesToRadians(float Degrees) { return Degrees * M_PI / 180.0f; }
float RadiansToDegrees(float Radians) { return Radians * 180.0f / M_PI; }

v3 V3RotateZ(v3 Vector, float Rotation) {
    matrix MatrixRotation = MatrixRotationZ(Rotation);
    return MatrixV3Multiply(MatrixRotation, Vector);
}

v3 V3ReflectNormal(v3 Vector, v3 Normal) {
    float Temp = 2 * V3DotProduct(Normal, Vector);
    v3 Result = V3MultiplyScalar(Normal, Temp);
    Result = V3Subtract(Result, Vector);
    Result = V3Inverse(Result);
    return Result;
}

// TODO: 3D distance.
float V3GetDistance(v3 A, v3 B) {
    return (sqrt((A.X - B.X) * (A.X - B.X) + (A.Y - B.Y) * (A.Y - B.Y)));
}

v3 V3GetDirection(v3 A, v3 B) {
    v3 Direction = V3Subtract(B, A);
    V3Normalize(&Direction);
    return Direction;
}

float V3GetAngle(v3 V) {
    float Radians = atan2(V.Y, V.X);
    return RadiansToDegrees(Radians);
}

v3 V3GetRandomV2Direction() {
    return (v3) { cos((float)(rand() % 361)), sin((float)(rand() % 361)), 0.0f };
}

v3 V3Inverse(v3 V) { return (v3) { -V.X, -V.Y, -V.Z }; }

v3 V3Add(v3 A, v3 B) {
    v3 Result = { 0 };
    Result.X += A.X + B.X;
    Result.Y += A.Y + B.Y;
    Result.Z += A.Z + B.Z;
    return Result;
}

v3 V3Subtract(v3 A, v3 B) {
    v3 Result = { 0 };
    Result.X += A.X - B.X;
    Result.Y += A.Y - B.Y;
    Result.Z += A.Z - B.Z;
    return Result;
}

v3 V3CrossProduct(v3 A, v3 B) {
    return (v3) {
        .X = A.Y * B.Z - A.Z * B.Y,
        .Y = A.Z * B.X - A.X * B.Z,
        .Z = A.X * B.Y - A.Y * B.X
    };
}

float V3DotProduct(v3 A, v3 B) {
    return (A.X * B.X + A.Y * B.Y + A.Z * B.Z);
}

v3 V3AddScalar(v3 A, float B) {
    v3 Result = { 0 };
    Result.X += A.X + B;
    Result.Y += A.Y + B;
    Result.Z += A.Z + B;
    return Result;
}

v3 V3MultiplyScalar(v3 A, float B) {
    v3 Result = { 0 };
    Result.X = A.X * B;
    Result.Y = A.Y * B;
    Result.Z = A.Z * B;
    return Result;
}

int V3IsZero(v3 Vector) {
    if (Vector.X == 0.0f && Vector.Y == 0.0f && Vector.Z == 0.0f) {
        return 1;
    }
    return 0;
}

void V3Zero(v3 *Vector) {
    Vector->X = 0.0f;
    Vector->Y = 0.0f;
    Vector->Z = 0.0f;
}

int V3Compare(v3 A, v3 B) {
    if (A.X == B.X && A.Y == B.Y && A.Z == B.Z) {
        return 1;
    }
    return 0;
}

float V3Length(v3 *V) { return sqrt(V->X * V->X + V->Y * V->Y + V->Z * V->Z); }

void V3Normalize(v3 *V) {
    float Length = V3Length(V);
    
    if (!Length) {
        V->X = 0.0f;
        V->Y = 0.0f;
        V->Z = 0.0f;
    } else {
        V->X = V->X / Length;
        V->Y = V->Y / Length;
        V->Z = V->Z / Length;
    }
}

v3 V3TransformCoord(v3 *V, matrix *M) {
    
    float Norm =
        M->M[0][3] * V->X + M->M[1][3] * V->Y + M->M[2][3] * V->Z + M->M[3][3];
    
    return (v3) {
        (M->M[0][0] * V->X + M->M[1][0] * V->Y + M->M[2][0] * V->Z + M->M[3][0]) /
            Norm,
        (M->M[0][1] * V->X + M->M[1][1] * V->Y + M->M[2][1] * V->Z + M->M[3][1]) /
            Norm,
        (M->M[0][2] * V->X + M->M[1][2] * V->Y + M->M[2][2] * V->Z + M->M[3][2]) /
            Norm,
    };
}

v3 V3TransformNormal(v3 *V, matrix *M) {
    
    return (v3) {
        M->M[0][0] * V->X + M->M[1][0] * V->Y + M->M[2][0] * V->Z,
        M->M[0][1] * V->X + M->M[1][1] * V->Y + M->M[2][1] * V->Z,
        M->M[0][2] * V->X + M->M[1][2] * V->Y + M->M[2][2] * V->Z
    };
}

matrix MatrixIdentity() {
    return (matrix) {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
}

matrix MatrixTranslation(v3 V) {
    matrix Result = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        V.X, V.Y, V.Z, 1.0f
    };
    return Result;
}

matrix MatrixRotationZ(float AngleDegrees) {
    float Theta = DegreesToRadians(AngleDegrees);
    return (matrix) {
        cos(Theta), sin(Theta), 0.0f, 0.0f,
        -sin(Theta), cos(Theta), 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
}

matrix MatrixScale(v3 V) {
    return (matrix) {
        V.X, 0.0f, 0.0f, 0.0f,
        0.0f, V.Y, 0.0f, 0.0f,
        0.0f, 0.0f, V.Z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
}

v3 MatrixV3Multiply(matrix M, v3 V) {
    float Temp[4] = { 0 };
    float VFloat[4] = { V.X, V.Y, V.Z };
    int i, j;
    for (j = 0; j < 4; ++j) {
        Temp[j] = 0.0f;
        for (i = 0; i < 4; ++i)
            Temp[j] += M.M[i][j] * VFloat[i];
    }
    return (v3) { Temp[0], Temp[1], Temp[2] };
}

matrix MatrixMultiply(matrix *A, matrix *B) {
    return (matrix) {
        A->M[0][0] * B->M[0][0] + A->M[0][1] * B->M[1][0] +
            A->M[0][2] * B->M[2][0] + A->M[0][3] * B->M[3][0],
        A->M[0][0] * B->M[0][1] + A->M[0][1] * B->M[1][1] +
            A->M[0][2] * B->M[2][1] + A->M[0][3] * B->M[3][1],
        A->M[0][0] * B->M[0][2] + A->M[0][1] * B->M[1][2] +
            A->M[0][2] * B->M[2][2] + A->M[0][3] * B->M[3][2],
        A->M[0][0] * B->M[0][3] + A->M[0][1] * B->M[1][3] +
            A->M[0][2] * B->M[2][3] + A->M[0][3] * B->M[3][3],
        A->M[1][0] * B->M[0][0] + A->M[1][1] * B->M[1][0] +
            A->M[1][2] * B->M[2][0] + A->M[1][3] * B->M[3][0],
        A->M[1][0] * B->M[0][1] + A->M[1][1] * B->M[1][1] +
            A->M[1][2] * B->M[2][1] + A->M[1][3] * B->M[3][1],
        A->M[1][0] * B->M[0][2] + A->M[1][1] * B->M[1][2] +
            A->M[1][2] * B->M[2][2] + A->M[1][3] * B->M[3][2],
        A->M[1][0] * B->M[0][3] + A->M[1][1] * B->M[1][3] +
            A->M[1][2] * B->M[2][3] + A->M[1][3] * B->M[3][3],
        A->M[2][0] * B->M[0][0] + A->M[2][1] * B->M[1][0] +
            A->M[2][2] * B->M[2][0] + A->M[2][3] * B->M[3][0],
        A->M[2][0] * B->M[0][1] + A->M[2][1] * B->M[1][1] +
            A->M[2][2] * B->M[2][1] + A->M[2][3] * B->M[3][1],
        A->M[2][0] * B->M[0][2] + A->M[2][1] * B->M[1][2] +
            A->M[2][2] * B->M[2][2] + A->M[2][3] * B->M[3][2],
        A->M[2][0] * B->M[0][3] + A->M[2][1] * B->M[1][3] +
            A->M[2][2] * B->M[2][3] + A->M[2][3] * B->M[3][3],
        A->M[3][0] * B->M[0][0] + A->M[3][1] * B->M[1][0] +
            A->M[3][2] * B->M[2][0] + A->M[3][3] * B->M[3][0],
        A->M[3][0] * B->M[0][1] + A->M[3][1] * B->M[1][1] +
            A->M[3][2] * B->M[2][1] + A->M[3][3] * B->M[3][1],
        A->M[3][0] * B->M[0][2] + A->M[3][1] * B->M[1][2] +
            A->M[3][2] * B->M[2][2] + A->M[3][3] * B->M[3][2],
        A->M[3][0] * B->M[0][3] + A->M[3][1] * B->M[1][3] +
            A->M[3][2] * B->M[2][3] + A->M[3][3] * B->M[3][3],
    };
}

void MatrixInverse(matrix *Source, matrix *Target) {
    
    float Determinant;
    
    Target->M[0][0] = +Source->M[1][1] * Source->M[2][2] * Source->M[3][3] -
        Source->M[1][1] * Source->M[2][3] * Source->M[3][2] -
        Source->M[2][1] * Source->M[1][2] * Source->M[3][3] +
        Source->M[2][1] * Source->M[1][3] * Source->M[3][2] +
        Source->M[3][1] * Source->M[1][2] * Source->M[2][3] -
        Source->M[3][1] * Source->M[1][3] * Source->M[2][2];
    
    Target->M[0][1] = -Source->M[0][1] * Source->M[2][2] * Source->M[3][3] +
        Source->M[0][1] * Source->M[2][3] * Source->M[3][2] +
        Source->M[2][1] * Source->M[0][2] * Source->M[3][3] -
        Source->M[2][1] * Source->M[0][3] * Source->M[3][2] -
        Source->M[3][1] * Source->M[0][2] * Source->M[2][3] +
        Source->M[3][1] * Source->M[0][3] * Source->M[2][2];
    
    Target->M[0][2] = +Source->M[0][1] * Source->M[1][2] * Source->M[3][3] -
        Source->M[0][1] * Source->M[1][3] * Source->M[3][2] -
        Source->M[1][1] * Source->M[0][2] * Source->M[3][3] +
        Source->M[1][1] * Source->M[0][3] * Source->M[3][2] +
        Source->M[3][1] * Source->M[0][2] * Source->M[1][3] -
        Source->M[3][1] * Source->M[0][3] * Source->M[1][2];
    
    Target->M[0][3] = -Source->M[0][1] * Source->M[1][2] * Source->M[2][3] +
        Source->M[0][1] * Source->M[1][3] * Source->M[2][2] +
        Source->M[1][1] * Source->M[0][2] * Source->M[2][3] -
        Source->M[1][1] * Source->M[0][3] * Source->M[2][2] -
        Source->M[2][1] * Source->M[0][2] * Source->M[1][3] +
        Source->M[2][1] * Source->M[0][3] * Source->M[1][2];
    
    Target->M[1][0] = -Source->M[1][0] * Source->M[2][2] * Source->M[3][3] +
        Source->M[1][0] * Source->M[2][3] * Source->M[3][2] +
        Source->M[2][0] * Source->M[1][2] * Source->M[3][3] -
        Source->M[2][0] * Source->M[1][3] * Source->M[3][2] -
        Source->M[3][0] * Source->M[1][2] * Source->M[2][3] +
        Source->M[3][0] * Source->M[1][3] * Source->M[2][2];
    
    Target->M[1][1] = +Source->M[0][0] * Source->M[2][2] * Source->M[3][3] -
        Source->M[0][0] * Source->M[2][3] * Source->M[3][2] -
        Source->M[2][0] * Source->M[0][2] * Source->M[3][3] +
        Source->M[2][0] * Source->M[0][3] * Source->M[3][2] +
        Source->M[3][0] * Source->M[0][2] * Source->M[2][3] -
        Source->M[3][0] * Source->M[0][3] * Source->M[2][2];
    
    Target->M[1][2] = -Source->M[0][0] * Source->M[1][2] * Source->M[3][3] +
        Source->M[0][0] * Source->M[1][3] * Source->M[3][2] +
        Source->M[1][0] * Source->M[0][2] * Source->M[3][3] -
        Source->M[1][0] * Source->M[0][3] * Source->M[3][2] -
        Source->M[3][0] * Source->M[0][2] * Source->M[1][3] +
        Source->M[3][0] * Source->M[0][3] * Source->M[1][2];
    
    Target->M[1][3] = +Source->M[0][0] * Source->M[1][2] * Source->M[2][3] -
        Source->M[0][0] * Source->M[1][3] * Source->M[2][2] -
        Source->M[1][0] * Source->M[0][2] * Source->M[2][3] +
        Source->M[1][0] * Source->M[0][3] * Source->M[2][2] +
        Source->M[2][0] * Source->M[0][2] * Source->M[1][3] -
        Source->M[2][0] * Source->M[0][3] * Source->M[1][2];
    
    Target->M[2][0] = +Source->M[1][0] * Source->M[2][1] * Source->M[3][3] -
        Source->M[1][0] * Source->M[2][3] * Source->M[3][1] -
        Source->M[2][0] * Source->M[1][1] * Source->M[3][3] +
        Source->M[2][0] * Source->M[1][3] * Source->M[3][1] +
        Source->M[3][0] * Source->M[1][1] * Source->M[2][3] -
        Source->M[3][0] * Source->M[1][3] * Source->M[2][1];
    
    Target->M[2][1] = -Source->M[0][0] * Source->M[2][1] * Source->M[3][3] +
        Source->M[0][0] * Source->M[2][3] * Source->M[3][1] +
        Source->M[2][0] * Source->M[0][1] * Source->M[3][3] -
        Source->M[2][0] * Source->M[0][3] * Source->M[3][1] -
        Source->M[3][0] * Source->M[0][1] * Source->M[2][3] +
        Source->M[3][0] * Source->M[0][3] * Source->M[2][1];
    
    Target->M[2][2] = +Source->M[0][0] * Source->M[1][1] * Source->M[3][3] -
        Source->M[0][0] * Source->M[1][3] * Source->M[3][1] -
        Source->M[1][0] * Source->M[0][1] * Source->M[3][3] +
        Source->M[1][0] * Source->M[0][3] * Source->M[3][1] +
        Source->M[3][0] * Source->M[0][1] * Source->M[1][3] -
        Source->M[3][0] * Source->M[0][3] * Source->M[1][1];
    
    Target->M[2][3] = -Source->M[0][0] * Source->M[1][1] * Source->M[2][3] +
        Source->M[0][0] * Source->M[1][3] * Source->M[2][1] +
        Source->M[1][0] * Source->M[0][1] * Source->M[2][3] -
        Source->M[1][0] * Source->M[0][3] * Source->M[2][1] -
        Source->M[2][0] * Source->M[0][1] * Source->M[1][3] +
        Source->M[2][0] * Source->M[0][3] * Source->M[1][1];
    
    Target->M[3][0] = -Source->M[1][0] * Source->M[2][1] * Source->M[3][2] +
        Source->M[1][0] * Source->M[2][2] * Source->M[3][1] +
        Source->M[2][0] * Source->M[1][1] * Source->M[3][2] -
        Source->M[2][0] * Source->M[1][2] * Source->M[3][1] -
        Source->M[3][0] * Source->M[1][1] * Source->M[2][2] +
        Source->M[3][0] * Source->M[1][2] * Source->M[2][1];
    
    Target->M[3][1] = +Source->M[0][0] * Source->M[2][1] * Source->M[3][2] -
        Source->M[0][0] * Source->M[2][2] * Source->M[3][1] -
        Source->M[2][0] * Source->M[0][1] * Source->M[3][2] +
        Source->M[2][0] * Source->M[0][2] * Source->M[3][1] +
        Source->M[3][0] * Source->M[0][1] * Source->M[2][2] -
        Source->M[3][0] * Source->M[0][2] * Source->M[2][1];
    
    Target->M[3][2] = -Source->M[0][0] * Source->M[1][1] * Source->M[3][2] +
        Source->M[0][0] * Source->M[1][2] * Source->M[3][1] +
        Source->M[1][0] * Source->M[0][1] * Source->M[3][2] -
        Source->M[1][0] * Source->M[0][2] * Source->M[3][1] -
        Source->M[3][0] * Source->M[0][1] * Source->M[1][2] +
        Source->M[3][0] * Source->M[0][2] * Source->M[1][1];
    
    Target->M[3][3] = +Source->M[0][0] * Source->M[1][1] * Source->M[2][2] -
        Source->M[0][0] * Source->M[1][2] * Source->M[2][1] -
        Source->M[1][0] * Source->M[0][1] * Source->M[2][2] +
        Source->M[1][0] * Source->M[0][2] * Source->M[2][1] +
        Source->M[2][0] * Source->M[0][1] * Source->M[1][2] -
        Source->M[2][0] * Source->M[0][2] * Source->M[1][1];
    
    Determinant =
        +Source->M[0][0] * Target->M[0][0] + Source->M[0][1] * Target->M[1][0] +
        Source->M[0][2] * Target->M[2][0] + Source->M[0][3] * Target->M[3][0];
    
    Determinant = 1.0f / Determinant;
    
    Target->M[0][0] *= Determinant;
    Target->M[0][1] *= Determinant;
    Target->M[0][2] *= Determinant;
    Target->M[0][3] *= Determinant;
    Target->M[1][0] *= Determinant;
    Target->M[1][1] *= Determinant;
    Target->M[1][2] *= Determinant;
    Target->M[1][3] *= Determinant;
    Target->M[2][0] *= Determinant;
    Target->M[2][1] *= Determinant;
    Target->M[2][2] *= Determinant;
    Target->M[2][3] *= Determinant;
    Target->M[3][0] *= Determinant;
    Target->M[3][1] *= Determinant;
    Target->M[3][2] *= Determinant;
    Target->M[3][3] *= Determinant;
}

// Entities.

void UpdateEntityPosition(entity *Entity) {
    // position += velocity * dt * speed
    Entity->Position =
        V3Add(Entity->Position,
              V3MultiplyScalar(Entity->Velocity, DeltaTime * Entity->Speed));
}

entityArray NewEntityArray(int Capacity) {
    entityArray Array = {
        .Items = MemoryAlloc(Capacity * sizeof(entity)),
        .Capacity = Capacity,
    };
    return Array;
}

// Note: Overrides elements starting from index 0 if overflowing.
void AddEntityToArray(entityArray *Array, entity *Entity) {
    Array->Items[Array->Index++] = *Entity;
    if (Array->Length < Array->Capacity) {
        ++Array->Length;
    }
    if (Array->Index >= Array->Capacity) {
        Array->Index = 0;
    }
}

void DrawEntityArray(entityArray *Array) {
    for (int Index = 0; Index < Array->Length; ++Index) {
        DrawEntity(&Array->Items[Index]);
    }
}

void KeepEntityWithinBounds(entity *Entity, rectangle Bounds) {
    
    // Assumes AABB.
    
    float HalfWidth = Entity->Scale.X / 2.0f;
    float HalfHeight = Entity->Scale.Y / 2.0f;
    
    if (Entity->Position.X + HalfWidth > Bounds.Right) {
        Entity->Position.X = Bounds.Right - HalfWidth;
    } else if (Entity->Position.X - HalfWidth < Bounds.Left) {
        Entity->Position.X = Bounds.Left + HalfWidth;
    } else if (Entity->Position.Y + HalfHeight > Bounds.Top) {
        Entity->Position.Y = Bounds.Top - HalfHeight;
    } else if (Entity->Position.Y - HalfHeight < Bounds.Bottom) {
        Entity->Position.Y = Bounds.Bottom + HalfHeight;
    }
}

// Misc.

int CompareFloatsForSorting(const void *First, const void *Second) {
    float A = *((float *)First);
    float B = *((float *)Second);
    if (A < B) return -1;
    if (A > B) return 1;
    return 0;
}

void SortFloats(float *Values, int NumElements) {
    qsort(Values, NumElements, sizeof(*Values), CompareFloatsForSorting);
}

int CompareIntsForSorting(const void *First, const void *Second) {
    int A = *((int *)First);
    int B = *((int *)Second);
    if (A < B) return -1;
    if (A > B) return 1;
    return 0;
}

void SortInts(int *Values, int NumElements) {
    qsort(Values, NumElements, sizeof(*Values), CompareIntsForSorting);
}
