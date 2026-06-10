// render.c - all drawing. GDI+ flat API (C-callable) into premultiplied-ARGB
// DIB sections, so overlays get true per-pixel alpha (soft shadows, antialiased
// glyphs) and tray icons are generated at runtime instead of shipping bitmaps.
#include "micmute.h"

// ---- GDI+ flat API (minimal C declarations; gdiplus.dll exports these) ----
typedef struct { UINT32 GdiplusVersion; void* DebugEventCallback; BOOL SuppressBackgroundThread; BOOL SuppressExternalCodecs; } GpStartupInput;
typedef struct { float X, Y; } GpPointF;

#define PixelFormat32bppPARGB 0xE200B
#define SmoothAntiAlias 4
#define UnitPixel 2
#define CapRound 2
#define CompositingSourceCopy 1
#define CompositingSourceOver 0
#define WrapTileFlipXY 3

int  WINAPI GdiplusStartup(ULONG_PTR* token, const GpStartupInput* input, void* output);
void WINAPI GdiplusShutdown(ULONG_PTR token);
int  WINAPI GdipCreateBitmapFromScan0(INT w, INT h, INT stride, INT format, BYTE* scan0, void** bitmap);
int  WINAPI GdipDisposeImage(void* image);
int  WINAPI GdipGetImageGraphicsContext(void* image, void** graphics);
int  WINAPI GdipDeleteGraphics(void* graphics);
int  WINAPI GdipSetSmoothingMode(void* graphics, int mode);
int  WINAPI GdipSetCompositingMode(void* graphics, int mode);
int  WINAPI GdipCreatePath(int fillMode, void** path);
int  WINAPI GdipDeletePath(void* path);
int  WINAPI GdipAddPathArc(void* path, float x, float y, float w, float h, float start, float sweep);
int  WINAPI GdipClosePathFigure(void* path);
int  WINAPI GdipCreateSolidFill(DWORD argb, void** brush);
int  WINAPI GdipCreateLineBrush(const GpPointF* p1, const GpPointF* p2, DWORD c1, DWORD c2, int wrap, void** brush);
int  WINAPI GdipDeleteBrush(void* brush);
int  WINAPI GdipFillPath(void* graphics, void* brush, void* path);
int  WINAPI GdipFillEllipse(void* graphics, void* brush, float x, float y, float w, float h);
int  WINAPI GdipCreatePen1(DWORD argb, float width, int unit, void** pen);
int  WINAPI GdipDeletePen(void* pen);
int  WINAPI GdipSetPenLineCap197819(void* pen, int startCap, int endCap, int dashCap);
int  WINAPI GdipDrawPath(void* graphics, void* pen, void* path);
int  WINAPI GdipDrawLine(void* graphics, void* pen, float x1, float y1, float x2, float y2);
int  WINAPI GdipDrawArc(void* graphics, void* pen, float x, float y, float w, float h, float start, float sweep);

// ---- palette ----
#define COL_BADGE_BG    0xEE15171FU   // near-opaque charcoal
#define COL_BORDER      0x2EFFFFFFU
#define COL_MIC_WHITE   0xFFF2F3F7U
#define COL_MIC_DARK    0xFF1B1B1FU
#define COL_RED         0xFFFF4750U
#define COL_RED_BADGE   0xFFE0353FU
#define COL_GREEN       0xFF3DD68CU
#define COL_GREEN_DOT   0xFF23C268U

static ULONG_PTR g_gdipToken;

// Scale a color's alpha channel (for the background melt-away animation).
static DWORD ScaleA(DWORD argb, float f)
{
    return ((DWORD)((BYTE)((argb >> 24) * f)) << 24) | (argb & 0xFFFFFF);
}

BOOL Render_Init(void)
{
    GpStartupInput in = {1, NULL, FALSE, FALSE};
    return GdiplusStartup(&g_gdipToken, &in, NULL) == 0;
}

void Render_Shutdown(void)
{
    if (g_gdipToken) { GdiplusShutdown(g_gdipToken); g_gdipToken = 0; }
}

static HBITMAP CreatePargbDib(int w, int h, void** bits)
{
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;   // top-down so Scan0 maps directly
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    return CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, bits, NULL, 0);
}

static void* BeginDraw(int w, int h, void* bits, void** outBmp)
{
    void *bmp = NULL, *g = NULL;
    GdipCreateBitmapFromScan0(w, h, w * 4, PixelFormat32bppPARGB, (BYTE*)bits, &bmp);
    if (bmp) GdipGetImageGraphicsContext(bmp, &g);
    if (g) GdipSetSmoothingMode(g, SmoothAntiAlias);
    *outBmp = bmp;
    return g;
}

static void EndDraw(void* g, void* bmp)
{
    if (g) GdipDeleteGraphics(g);
    if (bmp) GdipDisposeImage(bmp);
}

static void* RoundRectPath(float x, float y, float w, float h, float r)
{
    void* p = NULL;
    float d = r * 2;
    GdipCreatePath(0, &p);
    GdipAddPathArc(p, x, y, d, d, 180, 90);
    GdipAddPathArc(p, x + w - d, y, d, d, 270, 90);
    GdipAddPathArc(p, x + w - d, y + h - d, d, d, 0, 90);
    GdipAddPathArc(p, x, y + h - d, d, d, 90, 90);
    GdipClosePathFigure(p);
    return p;
}

static void FillRoundRect(void* g, DWORD argb, float x, float y, float w, float h, float r)
{
    void *p = RoundRectPath(x, y, w, h, r), *b = NULL;
    GdipCreateSolidFill(argb, &b);
    GdipFillPath(g, b, p);
    GdipDeleteBrush(b);
    GdipDeletePath(p);
}

static void* MakeRoundPen(DWORD argb, float width)
{
    void* pen = NULL;
    GdipCreatePen1(argb, width, UnitPixel, &pen);
    GdipSetPenLineCap197819(pen, CapRound, CapRound, CapRound);
    return pen;
}

// Classic mic silhouette (capsule, cradle arc, stem, base) inside box (gx,gy,u).
static void DrawMic(void* g, float gx, float gy, float u, DWORD color)
{
    FillRoundRect(g, color, gx + 0.33f * u, gy + 0.04f * u, 0.34f * u, 0.56f * u, 0.17f * u);
    void* pen = MakeRoundPen(color, 0.085f * u);
    GdipDrawArc(g, pen, gx + 0.155f * u, gy + 0.10f * u, 0.69f * u, 0.62f * u, 0, 180);
    GdipDrawLine(g, pen, gx + 0.5f * u, gy + 0.72f * u, gx + 0.5f * u, gy + 0.88f * u);
    GdipDrawLine(g, pen, gx + 0.34f * u, gy + 0.92f * u, gx + 0.66f * u, gy + 0.92f * u);
    GdipDeletePen(pen);
}

// Diagonal "blocked" slash: first a notch cut in the badge color (SourceCopy
// replaces pixels, so the mic appears severed), then the accent stroke on top.
static void DrawSlash(void* g, float gx, float gy, float u, DWORD notchColor, DWORD accentColor)
{
    float x1 = gx + 0.13f * u, y1 = gy + 0.08f * u;
    float x2 = gx + 0.87f * u, y2 = gy + 0.92f * u;
    void* pen = MakeRoundPen(notchColor, 0.19f * u);
    GdipSetCompositingMode(g, CompositingSourceCopy);
    GdipDrawLine(g, pen, x1, y1, x2, y2);
    GdipSetCompositingMode(g, CompositingSourceOver);
    GdipDeletePen(pen);
    pen = MakeRoundPen(accentColor, 0.10f * u);
    GdipDrawLine(g, pen, x1, y1, x2, y2);
    GdipDeletePen(pen);
}

// Soft drop shadow: stacked translucent rounded rects, slightly sunk.
static void DrawShadow(void* g, float x, float y, float w, float h, float r, float spread, float opacity)
{
    for (int j = 0; j < 6; j++) {
        float grow = spread * (6 - j) / 6.0f;
        DWORD a = (DWORD)((5 + j * 6) * opacity);
        FillRoundRect(g, a << 24, x - grow, y - grow + spread * 0.35f,
                      w + 2 * grow, h + 2 * grow, r + grow);
    }
}

// On-screen overlay badge. windowPx is the full DIB (badge + shadow margin),
// badgePx the badge itself. muted=TRUE: white mic, red slash. FALSE: green
// "live" mic used for the unmute confirmation flash. bg scales the badge
// background (shadow/plate/highlight/border/glow) from 1 (full) to 0 (gone),
// driving the melt-away a few seconds after muting; the glyph always stays.
HBITMAP RenderBadge(int windowPx, int badgePx, BOOL muted, float bg)
{
    void* bits = NULL;
    HBITMAP dib = CreatePargbDib(windowPx, windowPx, &bits);
    if (!dib) return NULL;

    if (bg < 0) bg = 0;
    if (bg > 1) bg = 1;

    void* bmp = NULL;
    void* g = BeginDraw(windowPx, windowPx, bits, &bmp);
    if (g) {
        float m = (windowPx - badgePx) / 2.0f;
        float s = (float)badgePx;
        float r = s * 0.24f;

        if (bg > 0) {
            DrawShadow(g, m, m, s, s, r, m * 0.8f, bg);
            FillRoundRect(g, ScaleA(COL_BADGE_BG, bg), m, m, s, s, r);

            // glassy top highlight
            GpPointF p1 = {m, m}, p2 = {m, m + s * 0.6f};
            void* grad = NULL;
            GdipCreateLineBrush(&p1, &p2, ScaleA(0x17FFFFFFU, bg), 0x00FFFFFFU, WrapTileFlipXY, &grad);
            if (grad) {
                void* clip = RoundRectPath(m + 1, m + 1, s - 2, s * 0.6f - 1, r - 1);
                GdipFillPath(g, grad, clip);
                GdipDeletePath(clip);
                GdipDeleteBrush(grad);
            }

            // hairline border
            void* border = RoundRectPath(m + 0.5f, m + 0.5f, s - 1, s - 1, r);
            void* pen = MakeRoundPen(ScaleA(COL_BORDER, bg), max(1.0f, s * 0.02f));
            GdipDrawPath(g, pen, border);
            GdipDeletePen(pen);
            GdipDeletePath(border);
        }

        // soft state-colored glow behind the glyph
        float u = s * 0.60f;
        float gx = m + (s - u) / 2, gy = m + (s - u) / 2;
        void* glow = NULL;
        GdipCreateSolidFill(ScaleA(muted ? 0x2090202AU /*red tint*/ : 0x203DD68CU, bg), &glow);
        GdipFillEllipse(g, glow, gx - 0.10f * u, gy - 0.10f * u, 1.2f * u, 1.2f * u);
        GdipDeleteBrush(glow);

        // soft dark under-shadow keeps the glyph readable once the badge is gone
        DrawMic(g, gx + 0.05f * u, gy + 0.05f * u, u, 0x59101218U);
        DrawMic(g, gx, gy, u, muted ? COL_MIC_WHITE : COL_GREEN);
        if (muted) DrawSlash(g, gx, gy, u, ScaleA(COL_BADGE_BG, bg), COL_RED);
    }
    EndDraw(g, bmp);
    return dib;
}

// Tray icon, rendered at runtime. Muted: red rounded badge + white severed
// mic. Live: bare theme-colored mic with a green status dot.
HICON RenderTrayIcon(int px, BOOL muted, BOOL lightTaskbar)
{
    void* bits = NULL;
    HBITMAP dib = CreatePargbDib(px, px, &bits);
    if (!dib) return NULL;

    void* bmp = NULL;
    void* g = BeginDraw(px, px, bits, &bmp);
    if (g) {
        float s = (float)px;
        if (muted) {
            FillRoundRect(g, COL_RED_BADGE, 0.5f, 0.5f, s - 1, s - 1, s * 0.28f);
            float u = s * 0.72f, off = (s - u) / 2;
            DrawMic(g, off, off, u, COL_MIC_WHITE);
            DrawSlash(g, off, off, u, COL_RED_BADGE, COL_MIC_WHITE);
        } else {
            float u = s * 0.92f, off = (s - u) / 2;
            DrawMic(g, off, off, u, lightTaskbar ? COL_MIC_DARK : COL_MIC_WHITE);
            float d = s * 0.42f;
            void* ring = NULL;   // contrast ring so the dot reads on any tray
            GdipCreateSolidFill(lightTaskbar ? 0x66FFFFFFU : 0x66000000U, &ring);
            GdipFillEllipse(g, ring, s - d - 1, s - d - 1, d + 1, d + 1);
            GdipDeleteBrush(ring);
            void* dot = NULL;
            GdipCreateSolidFill(COL_GREEN_DOT, &dot);
            GdipFillEllipse(g, dot, s - d, s - d, d, d);
            GdipDeleteBrush(dot);
        }
    }
    EndDraw(g, bmp);

    HBITMAP mask = CreateBitmap(px, px, 1, 1, NULL);
    ICONINFO ii = {TRUE, 0, 0, mask, dib};
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(mask);
    DeleteObject(dib);
    return icon;
}
