#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "embed.h"
#define BUBBLES_NUM 30
#define BOUNCY_SPEED 4.0f
#define BUBBLE_SPEED 2.0f
#define BUBBLE_RADIUS 80.f

#define MAX_PIPES 80
#define PIPE_MAX_BENDS 20
#define PIPE_MAX_BEND_LENGTH 1.0f
#define PIPE_MIN_BEND_LENGTH 3.0f
#define PIPE_RADIUS 0.15f

const Color pipePalette[] = {
    GRAY,
    RED,
    GREEN,
    BLUE,
    ORANGE,
    YELLOW,
    PURPLE,
    BEIGE,
};

typedef enum {
    SS_MODE_DVD_LOGO = 0,
    SS_MODE_BUBBLE,
    SS_MODE_PIPES,
    SS_MODE_SIZE,
} SSMode;

static SSMode mode = SS_MODE_PIPES;

Color getBubbleColor() {
    int random = abs(rand()) % 5;
    switch (random) {
        case 0:  return GetColor(0x80D9FFFF); break;
        case 1:  return GetColor(0x86FF80FF); break;
        case 2:  return GetColor(0x8A80FFFF); break;
        case 3:  return GetColor(0xFF80E8FF); break;
        case 4:  return GetColor(0xFFDD80FF); break;
        default: return WHITE;
    }
}

Vector3 getRandomDir(void) {
    static const Vector3 dirs[6] = {
        {+1,0,0},{-1,0,0},{0,+1,0},{0,-1,0},{0,0,+1},{0,0,-1}
    };
    return dirs[rand() % 6];
}

typedef struct {
    Image orgImg;
    Image cpyImg;
    Texture2D texture;
    Vector2 pos;
    Vector2 vol;
    Color color;
    bool reload;
} SSBouncy;

typedef struct {
    Vector2 pos;
    Vector2 vol;
    Color color;
    float targetHue;
} Bubble;

typedef struct {
    Bubble items[BUBBLES_NUM];
    int count;
    float lastBubbleTime;
    Texture2D bubbleTexture;
} SSBubble;

typedef struct {
    Vector3 pos;
    float length;
    Vector3 dir;
    float progress;
} PipeBend;

typedef struct {
    PipeBend bends[PIPE_MAX_BENDS];
    int nBends;
    Color color;
} Pipe;

typedef struct {
    Camera3D camera;
    Pipe pipes[MAX_PIPES];
    int count;
} SSPipes;

void bouncy_logo_frame(SSBouncy *logo) {
    logo->pos.x += logo->vol.x * BOUNCY_SPEED;
    logo->pos.y += logo->vol.y * BOUNCY_SPEED;
    if (logo->pos.x < 0) {
        logo->reload = true;
        logo->vol.x = 1;
        logo->color = GREEN;
    } else if (logo->pos.x > GetScreenWidth() - logo->texture.width) {
        logo->reload = true;
        logo->vol.x = -1;
        logo->color = YELLOW;
    }
    if (logo->pos.y < 0) {
        logo->reload = true;
        logo->vol.y = 1;
        logo->color = BLUE;
    } else if (logo->pos.y > GetScreenHeight() - logo->texture.height) {
        logo->reload = true;
        logo->vol.y = -1;
        logo->color = RED;
    }

    if (logo->reload) {
        UnloadImage(logo->cpyImg);
        logo->cpyImg = ImageCopy(logo->orgImg);
        ImageColorTint(&logo->cpyImg, logo->color);
        Color *pixels = LoadImageColors(logo->cpyImg);
        UpdateTexture(logo->texture, pixels);
        UnloadImageColors(pixels);
        logo->reload = false;
    }

    BeginDrawing();
        ClearBackground(BLACK);
        DrawTextureEx(logo->texture, logo->pos, 0.0f, 1.f, WHITE);
    EndDrawing();
}

float random_vol() {
    int random = (rand() % 1600) - 800;
    if (random == 0) random = 1;
    return (float) random / 200.f;
}

void bubbles_frame(SSBubble *bubbles) {
    float dt = GetFrameTime();
    if (dt > 0.05f) dt = 0.05f;
    float W = (float)GetScreenWidth();
    float H = (float)GetScreenHeight();
    const float R = BUBBLE_RADIUS;
    const float D = R * 2.0f;

    bubbles->lastBubbleTime += dt;
    if (bubbles->lastBubbleTime > 1.0f && bubbles->count < BUBBLES_NUM) {
        bubbles->lastBubbleTime = 0.0f;
        Bubble *b = &bubbles->items[bubbles->count++];
        b->pos = (Vector2){
            R + (float)GetRandomValue(0, (int)(W - D)),
            H + R
        };
        b->vol = (Vector2){ random_vol(), -BUBBLE_SPEED };
        b->color = getBubbleColor();
    }

    for (int i=0; i < bubbles->count; ++i) {
        Bubble *bob = &bubbles->items[i];
        bob->pos.x += bob->vol.x;
        bob->pos.y += bob->vol.y;
        if (bob->pos.x < 0) {
            bob->vol.x = BUBBLE_SPEED;
        } else if (bob->pos.x > GetScreenWidth()) {
            bob->vol.x = -BUBBLE_SPEED;
        }
        if (bob->pos.y < 0) {
            bob->vol.y = BUBBLE_SPEED;
        } else if (bob->pos.y > GetScreenHeight()) {
            bob->vol.y = -BUBBLE_SPEED;
        }
    }

    for (int i = 0; i < bubbles->count; ++i) {
        for (int j = i + 1; j < bubbles->count; ++j) {
            Bubble *a = &bubbles->items[i];
            Bubble *b = &bubbles->items[j];

            float dist = Vector2Distance(a->pos, b->pos);
            if (dist >= D) continue;
            a->targetHue = ColorToHSV(getBubbleColor()).x;
            b->targetHue = ColorToHSV(getBubbleColor()).x;

            float dx = b->pos.x - a->pos.x;
            float dy = b->pos.y - a->pos.y;
            float nx = dx / dist;
            float ny = dy / dist;

            float overlap = D - dist;
            a->pos.x -= nx * overlap * 0.5f;
            a->pos.y -= ny * overlap * 0.5f;
            b->pos.x += nx * overlap * 0.5f;
            b->pos.y += ny * overlap * 0.5f;

            float rvx = b->vol.x - a->vol.x;
            float rvy = b->vol.y - a->vol.y;
            float velAlongNormal = rvx * nx + rvy * ny;

            if (velAlongNormal > 0.0f) continue;

            float jimp = -velAlongNormal;
            a->vol.x -= jimp * nx;
            a->vol.y -= jimp * ny;
            b->vol.x += jimp * nx;
            b->vol.y += jimp * ny;

            float tx = -ny, ty = nx;
            float jitter = random_vol() * 0.02f;
            a->vol.x += tx * jitter;  a->vol.y += ty * jitter;
            b->vol.x -= tx * jitter;  b->vol.y -= ty * jitter;
        }
    }


    float degreesPerSecond = 90.0f;
    float step = degreesPerSecond * dt;
    for (int i = 0; i < bubbles->count; ++i) {
        Bubble *bob = &bubbles->items[i];
        Vector3 hsv = ColorToHSV(bob->color);

        float diff = bob->targetHue - hsv.x;
        while (diff >  180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;

        if (fabsf(diff) <= step) {
            hsv.x = bob->targetHue;
        } else {
            hsv.x += (diff > 0.0f) ? step : -step;
        }

        hsv.x = fmodf(hsv.x + 360.0f, 360.0f);
        bob->color = ColorFromHSV(hsv.x, hsv.y, hsv.z);
    }
    BeginDrawing();
        ClearBackground(GetColor(0));
        for (int i=0; i < bubbles->count; ++i) {
            Vector2 pos = Vector2SubtractValue(bubbles->items[i].pos, BUBBLE_RADIUS);
            DrawTextureEx(bubbles->bubbleTexture, pos, 0.0f, 0.8f, ColorAlpha(bubbles->items[i].color, 0.8));
        }
    EndDrawing();
}

void pipes_frames(SSPipes *p) {
    float dt = GetFrameTime();
    float speed = 80.0f;

    if (p->count == MAX_PIPES) {
        p->count = 0;
        memset(p->pipes, 0, sizeof(Pipe) * MAX_PIPES);
    } else {
        Pipe *pipe = &p->pipes[p->count];
        if (pipe->nBends >= PIPE_MAX_BENDS) {
            p->count++;
        } else {
            if (pipe->nBends == 0)
                pipe->color = pipePalette[rand() % 8];

            PipeBend *bend = &pipe->bends[pipe->nBends];
            if (bend->length == 0.0f) {
                if (pipe->nBends == 0) {
                    bend->pos = (Vector3) {
                        .x = (float)(rand() % 20 - 10),
                        .y = (float)(rand() % 20 - 10),
                        .z = (float)(rand() % 20 - 10),
                    };
                } else {
                    PipeBend *last = &pipe->bends[pipe->nBends - 1];
                    bend->pos = Vector3Add(last->pos,
                                           Vector3Scale(last->dir, last->length));
                }
                bend->dir = getRandomDir();
                bend->length = PIPE_MIN_BEND_LENGTH +
                               (rand() / (float)RAND_MAX) *
                               (PIPE_MAX_BEND_LENGTH - PIPE_MIN_BEND_LENGTH);
                bend->progress = 0.0f;
            } else if (bend->progress < bend->length) {
                bend->progress += speed * dt;
                if (bend->progress > bend->length)
                    bend->progress = bend->length;
            } else {
                pipe->nBends++;
            }
        }
    }

    BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(p->camera);

            int numPipes = p->count + 1;
            if (numPipes > MAX_PIPES) numPipes = MAX_PIPES;

            for (int i = 0; i < numPipes; ++i) {
                Pipe *pipe = &p->pipes[i];

                int numBends = pipe->nBends + 1;
                if (numBends > PIPE_MAX_BENDS) numBends = PIPE_MAX_BENDS;

                for (int j = 0; j < numBends; ++j) {
                    PipeBend *bend = &pipe->bends[j];
                    Vector3 endPos = Vector3Add(bend->pos, Vector3Scale(bend->dir, bend->progress));
                    DrawCylinderEx(bend->pos, endPos, PIPE_RADIUS, PIPE_RADIUS, 20, pipe->color);
                    DrawCylinderWiresEx(bend->pos, endPos, PIPE_RADIUS, PIPE_RADIUS, 20, BLACK);
                    // DrawSphere(bend->pos, PIPE_RADIUS, pipe->color);
                }
            }

        EndMode3D();
    EndDrawing();
}

void help(char* program) {
    printf("ScreenSaver - Made by @mahanfr\n");
    printf("Usage: %s <command>\n", program);
    printf("Note: You can change between modes with up and down arrow but bubbles transparent background requires a restart.\n");
    printf("Commands:\n");
    printf("   %s pipes   : Show windows 95 pipes screensaver\n", program);
    printf("   %s bubbles : Show windows 7 and up bubbles screensaver\n", program);
    printf("   %s dvd     : Show bouncing dvd logo\n", program);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        help(argv[0]);
        mode = SS_MODE_PIPES;
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    } else {
        if (strcmp(argv[1], "pipes") == 0) {
            mode = SS_MODE_PIPES;
            SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
        } else if (strcmp(argv[1], "bubbles") == 0) {
            mode = SS_MODE_BUBBLE;
            SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_WINDOW_TRANSPARENT);
        } else if (strcmp(argv[1], "dvd") == 0) {
            mode = SS_MODE_DVD_LOGO;
            SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_WINDOW_TRANSPARENT);
        } else {
            help(argv[0]);
            return 1;
        }
    }
    time_t mytime = time(NULL);
    srand(mytime);
    SetTraceLogLevel(LOG_NONE);
    InitWindow(800, 600, "ScreenSaver");

    // DVD_LOGO
    SSBouncy logo = {0};
    logo.orgImg = LoadImageFromMemory(".png", DVDLogoPNG, DVD_LOGO_PNG_SIZE);
    ImageFormat(&logo.orgImg, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ImageResize(&logo.orgImg, 200, 100);
    logo.texture = LoadTextureFromImage(logo.orgImg);
    logo.cpyImg = ImageCopy(logo.orgImg);
    logo.pos = (Vector2) {0.0f, 0.0f};
    logo.vol = (Vector2) {1.f, 1.f};
    logo.color = BLUE;
    logo.reload = true;

    // BUBBLES
    SSBubble bubbles = {0};
    Image bubbleImage = LoadImageFromMemory(".png", BubblePNG, BUBBLE_PNG_SIZE);
    bubbles.bubbleTexture = LoadTextureFromImage(bubbleImage);

    // PIPES
    SSPipes pipes = {0};
    pipes.camera = (Camera3D) {0};
    pipes.camera.fovy = 60.f;
    pipes.camera.position = (Vector3) {0.0f, 0.0f, 18.0f};
    pipes.camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    pipes.camera.up = (Vector3) {0.0f, 1.0f, 0.0f};
    pipes.camera.projection = CAMERA_PERSPECTIVE;

    SetTargetFPS(60);
    DisableCursor();
    ToggleBorderlessWindowed();
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_UP))   mode = (mode + 1) % SS_MODE_SIZE;
        if (IsKeyPressed(KEY_DOWN)) mode = (mode + SS_MODE_SIZE - 1) % SS_MODE_SIZE;
        switch (mode) {
            case SS_MODE_DVD_LOGO:
                bouncy_logo_frame(&logo);
                break;
            case SS_MODE_BUBBLE:
                bubbles_frame(&bubbles);
                break;
            case SS_MODE_PIPES:
                pipes_frames(&pipes);
                break;
            default:
                bouncy_logo_frame(&logo);
                break;
        }
    }
    UnloadTexture(bubbles.bubbleTexture);
    UnloadTexture(logo.texture);
    UnloadImage(bubbleImage);
    UnloadImage(logo.orgImg);
    UnloadImage(logo.cpyImg);
    return 0;
}
