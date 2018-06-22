/*
 * MIT License
 *
 * Copyright (c) 2016-2017 Patrick Rudolph <siro@das-labor.org>
 * Copyright (c) 2018 Mohamed Anwar
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/
/*
 * ==============================================================
 *
 *                              API
 *
 * ===============================================================
 */
#ifndef NK_RAWFB_H_
#define NK_RAWFB_H_

#include <libnk/nuklear.h>

struct rawfb_context;

struct rawfb_image {
    void *pixels;
    int w, h, pitch;
    enum nk_font_atlas_format format;
};
struct rawfb_context {
    struct nk_context ctx;
    struct nk_rect scissors;
    struct rawfb_image fb;
    struct rawfb_image font_tex;
    struct nk_font_atlas atlas;
};

/* All functions are thread-safe */
NK_API struct rawfb_context *nk_rawfb_init(void *fb, void *tex_mem, const unsigned int w, const unsigned int h, const unsigned int pitch);
NK_API void                  nk_rawfb_render(const struct rawfb_context *rawfb, const struct nk_color clear, const unsigned char enable_clear);
NK_API void                  nk_rawfb_shutdown(struct rawfb_context *rawfb);
NK_API void                  nk_rawfb_resize_fb(struct rawfb_context *rawfb, void *fb, const unsigned int w, const unsigned int h, const unsigned int pitch);

#endif
/*
 * ==============================================================
 *
 *                          IMPLEMENTATION
 *
 * ===============================================================
 */
#ifdef NK_RAWFB_IMPLEMENTATION

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif

static unsigned int
nk_color_from_byte(const nk_byte *c)
{
    unsigned int res = 0;
#if defined(RAWFB_RGBX_8888)
    res |= (unsigned int)c[0] << 16;
    res |= (unsigned int)c[1] << 8;
    res |= (unsigned int)c[2] << 0;
#elif defined(RAWFB_XRGB_8888)
    res = ((unsigned int *)c)[0];
#else
#error Define one of RAWFB_RGBX_8888 , RAWFB_XRGB_8888
#endif
    return (res);
}

static void
nk_rawfb_setpixel(const struct rawfb_context *rawfb,
    const short x0, const short y0, const struct nk_color col)
{
    unsigned int c = nk_color_from_byte(&col.r);
    unsigned char *pixels = rawfb->fb.pixels;
    unsigned int *ptr;

    pixels += y0 * rawfb->fb.pitch;
    ptr = (unsigned int *)pixels;
    ptr += x0;

    if (y0 < rawfb->scissors.h && y0 >= rawfb->scissors.y &&
        x0 >= rawfb->scissors.x && x0 < rawfb->scissors.w)
        *ptr = c;
}

static void
nk_rawfb_line_horizontal(const struct rawfb_context *rawfb,
    const short x0, const short y, const short x1, const struct nk_color col)
{
    /* This function is called the most. Try to optimize it a bit...
     * It does not check for scissors or image borders.
     * The caller has to make sure it does no exceed bounds. */
    unsigned int i, n;
    unsigned int c[16];
    unsigned char *pixels = rawfb->fb.pixels;
    unsigned int *ptr;

    pixels += y * rawfb->fb.pitch;
    ptr = (unsigned int *)pixels;
    ptr += x0;

    n = x1 - x0;
    for (i = 0; i < sizeof(c) / sizeof(c[0]); i++)
        c[i] = nk_color_from_byte(&col.r);

    while (n > 16) {
        memcpy((void *)ptr, c, sizeof(c));
        n -= 16; ptr += 16;
    } for (i = 0; i < n; i++)
        ptr[i] = c[i];
}

static void
nk_rawfb_imagesetpixel(const struct rawfb_image *img,
    const int x0, const int y0, const struct nk_color col)
{
    unsigned char *ptr;
    //NK_ASSERT(img);
    if (y0 < img->h && y0 > 0 && x0 > 0 && x0 < img->w) {
        ptr = img->pixels;
        if (img->format == NK_FONT_ATLAS_ALPHA8) {
            ptr += img->pitch * y0;
            ptr[x0] = col.a;
        } else {
            ptr += img->pitch * y0;
            ((struct nk_color *)ptr)[x0] = col;
        }
    }
}

static struct nk_color
nk_image_getpixel(const struct rawfb_image *img, const int x0, const int y0)
{
    struct nk_color col = {0, 0, 0, 0};
    unsigned char *ptr;
    //NK_ASSERT(img);
    if (y0 < img->h && y0 > 0 && x0 > 0 && x0 < img->w) {
        ptr = img->pixels;
        if (img->format == NK_FONT_ATLAS_ALPHA8) {
            ptr += img->pitch * y0;
            col.a = ptr[x0];
            col.b = col.g = col.r = 0xff;
        } else {
            ptr += img->pitch * y0;
            col = ((struct nk_color *)ptr)[x0];
        }
    } return col;
}

static void
nk_image_blendpixel(const struct rawfb_image *img,
    const int x0, const int y0, struct nk_color col)
{
    struct nk_color col2;
    unsigned char inv_a;
    if (col.a == 0)
        return;

    inv_a = 0xff - col.a;
    col2 = nk_image_getpixel(img, x0, y0);
    col.r = (col.r * col.a + col2.r * inv_a) >> 8;
    col.g = (col.g * col.a + col2.g * inv_a) >> 8;
    col.b = (col.b * col.a + col2.b * inv_a) >> 8;
    nk_rawfb_imagesetpixel(img, x0, y0, col);
}

static void
nk_rawfb_scissor(struct rawfb_context *rawfb,
                 const float x,
                 const float y,
                 const float w,
                 const float h)
{
    rawfb->scissors.x = MIN(MAX(x, 0), rawfb->fb.w);
    rawfb->scissors.y = MIN(MAX(y, 0), rawfb->fb.h);
    rawfb->scissors.w = MIN(MAX(w + x, 0), rawfb->fb.w);
    rawfb->scissors.h = MIN(MAX(h + y, 0), rawfb->fb.h);
}

static void
nk_rawfb_stroke_line(const struct rawfb_context *rawfb,
    short x0, short y0, short x1, short y1,
    const unsigned int line_thickness, const struct nk_color col)
{
    short tmp;
    int dy, dx, stepx, stepy;

    dy = y1 - y0;
    dx = x1 - x0;

    /* fast path */
    if (dy == 0) {
        if (dx == 0 || y0 >= rawfb->scissors.h || y0 < rawfb->scissors.y)
            return;

        if (dx < 0) {
            /* swap x0 and x1 */
            tmp = x1;
            x1 = x0;
            x0 = tmp;
        }
        x1 = MIN(rawfb->scissors.w - 1, x1);
        x0 = MIN(rawfb->scissors.w - 1, x0);
        x1 = MAX(rawfb->scissors.x, x1);
        x0 = MAX(rawfb->scissors.x, x0);
        nk_rawfb_line_horizontal(rawfb, x0, y0, x1, col);
        return;
    }
    if (dy < 0) {
        dy = -dy;
        stepy = -1;
    } else stepy = 1;

    if (dx < 0) {
        dx = -dx;
        stepx = -1;
    } else stepx = 1;

    dy <<= 1;
    dx <<= 1;

    nk_rawfb_setpixel(rawfb, x0, y0, col);
    if (dx > dy) {
        int fraction = dy - (dx >> 1);
        while (x0 != x1) {
            if (fraction >= 0) {
                y0 += stepy;
                fraction -= dx;
            }
            x0 += stepx;
            fraction += dy;
            nk_rawfb_setpixel(rawfb, x0, y0, col);
        }
    } else {
        int fraction = dx - (dy >> 1);
        while (y0 != y1) {
            if (fraction >= 0) {
                x0 += stepx;
                fraction -= dy;
            }
            y0 += stepy;
            fraction += dx;
            nk_rawfb_setpixel(rawfb, x0, y0, col);
        }
    }
}

static void
nk_rawfb_fill_polygon(const struct rawfb_context *rawfb,
    const struct nk_vec2i *pnts, int count, const struct nk_color col)
{
    int i = 0;
    #define MAX_POINTS 64
    int left = 10000, top = 10000, bottom = 0, right = 0;
    int nodes, nodeX[MAX_POINTS], pixelX, pixelY, j, swap ;

    if (count == 0) return;
    if (count > MAX_POINTS)
        count = MAX_POINTS;

    /* Get polygon dimensions */
    for (i = 0; i < count; i++) {
        if (left > pnts[i].x)
            left = pnts[i].x;
        if (right < pnts[i].x)
            right = pnts[i].x;
        if (top > pnts[i].y)
            top = pnts[i].y;
        if (bottom < pnts[i].y)
            bottom = pnts[i].y;
    } bottom++; right++;

    /* Polygon scanline algorithm released under public-domain by Darel Rex Finley, 2007 */
    /*  Loop through the rows of the image. */
    for (pixelY = top; pixelY < bottom; pixelY ++) {
        nodes = 0; /*  Build a list of nodes. */
        j = count - 1;
        for (i = 0; i < count; i++) {
            if (((pnts[i].y < pixelY) && (pnts[j].y >= pixelY)) ||
                ((pnts[j].y < pixelY) && (pnts[i].y >= pixelY))) {
                nodeX[nodes++]= (int)((float)pnts[i].x
                     + ((float)pixelY - (float)pnts[i].y) / ((float)pnts[j].y - (float)pnts[i].y)
                     * ((float)pnts[j].x - (float)pnts[i].x));
            } j = i;
        }

        /*  Sort the nodes, via a simple “Bubble” sort. */
        i = 0;
        while (i < nodes - 1) {
            if (nodeX[i] > nodeX[i+1]) {
                swap = nodeX[i];
                nodeX[i] = nodeX[i+1];
                nodeX[i+1] = swap;
                if (i) i--;
            } else i++;
        }
        /*  Fill the pixels between node pairs. */
        for (i = 0; i < nodes; i += 2) {
            if (nodeX[i+0] >= right) break;
            if (nodeX[i+1] > left) {
                if (nodeX[i+0] < left) nodeX[i+0] = left ;
                if (nodeX[i+1] > right) nodeX[i+1] = right;
                for (pixelX = nodeX[i]; pixelX < nodeX[i + 1]; pixelX++)
                    nk_rawfb_setpixel(rawfb, pixelX, pixelY, col);
            }
        }
    }
    #undef MAX_POINTS
}

static void
nk_rawfb_stroke_arc(const struct rawfb_context *rawfb,
    short x0, short y0, short w, short h, const short s,
    const short line_thickness, const struct nk_color col)
{
    /* Bresenham's ellipses - modified to draw one quarter */
    const int a2 = (w * w) / 4;
    const int b2 = (h * h) / 4;
    const int fa2 = 4 * a2, fb2 = 4 * b2;
    int x, y, sigma;

    if (s != 0 && s != 90 && s != 180 && s != 270) return;
    if (w < 1 || h < 1) return;

    /* Convert upper left to center */
    h = (h + 1) / 2;
    w = (w + 1) / 2;
    x0 += w; y0 += h;

    /* First half */
    for (x = 0, y = h, sigma = 2*b2+a2*(1-2*h); b2*x <= a2*y; x++) {
        if (s == 180)
            nk_rawfb_setpixel(rawfb, x0 + x, y0 + y, col);
        else if (s == 270)
            nk_rawfb_setpixel(rawfb, x0 - x, y0 + y, col);
        else if (s == 0)
            nk_rawfb_setpixel(rawfb, x0 + x, y0 - y, col);
        else if (s == 90)
            nk_rawfb_setpixel(rawfb, x0 - x, y0 - y, col);
        if (sigma >= 0) {
            sigma += fa2 * (1 - y);
            y--;
        } sigma += b2 * ((4 * x) + 6);
    }

    /* Second half */
    for (x = w, y = 0, sigma = 2*a2+b2*(1-2*w); a2*y <= b2*x; y++) {
        if (s == 180)
            nk_rawfb_setpixel(rawfb, x0 + x, y0 + y, col);
        else if (s == 270)
            nk_rawfb_setpixel(rawfb, x0 - x, y0 + y, col);
        else if (s == 0)
            nk_rawfb_setpixel(rawfb, x0 + x, y0 - y, col);
        else if (s == 90)
            nk_rawfb_setpixel(rawfb, x0 - x, y0 - y, col);
        if (sigma >= 0) {
            sigma += fb2 * (1 - x);
            x--;
        } sigma += a2 * ((4 * y) + 6);
    }
}

static void
nk_rawfb_fill_arc(const struct rawfb_context *rawfb, short x0, short y0,
    short w, short h, const short s, const struct nk_color col)
{
    /* Bresenham's ellipses - modified to fill one quarter */
    const int a2 = (w * w) / 4;
    const int b2 = (h * h) / 4;
    const int fa2 = 4 * a2, fb2 = 4 * b2;
    int x, y, sigma;
    struct nk_vec2i pnts[3];
    if (w < 1 || h < 1) return;
    if (s != 0 && s != 90 && s != 180 && s != 270)
        return;

    /* Convert upper left to center */
    h = (h + 1) / 2;
    w = (w + 1) / 2;
    x0 += w;
    y0 += h;

    pnts[0].x = x0;
    pnts[0].y = y0;
    pnts[2].x = x0;
    pnts[2].y = y0;

    /* First half */
    for (x = 0, y = h, sigma = 2*b2+a2*(1-2*h); b2*x <= a2*y; x++) {
        if (s == 180) {
            pnts[1].x = x0 + x; pnts[1].y = y0 + y;
        } else if (s == 270) {
            pnts[1].x = x0 - x; pnts[1].y = y0 + y;
        } else if (s == 0) {
            pnts[1].x = x0 + x; pnts[1].y = y0 - y;
        } else if (s == 90) {
            pnts[1].x = x0 - x; pnts[1].y = y0 - y;
        }
        nk_rawfb_fill_polygon(rawfb, pnts, 3, col);
        pnts[2] = pnts[1];
        if (sigma >= 0) {
            sigma += fa2 * (1 - y);
            y--;
        } sigma += b2 * ((4 * x) + 6);
    }

    /* Second half */
    for (x = w, y = 0, sigma = 2*a2+b2*(1-2*w); a2*y <= b2*x; y++) {
        if (s == 180) {
            pnts[1].x = x0 + x; pnts[1].y = y0 + y;
        } else if (s == 270) {
            pnts[1].x = x0 - x; pnts[1].y = y0 + y;
        } else if (s == 0) {
            pnts[1].x = x0 + x; pnts[1].y = y0 - y;
        } else if (s == 90) {
            pnts[1].x = x0 - x; pnts[1].y = y0 - y;
        }
        nk_rawfb_fill_polygon(rawfb, pnts, 3, col);
        pnts[2] = pnts[1];
        if (sigma >= 0) {
            sigma += fb2 * (1 - x);
            x--;
        } sigma += a2 * ((4 * y) + 6);
    }
}

static void
nk_rawfb_stroke_rect(const struct rawfb_context *rawfb,
    const short x, const short y, const short w, const short h,
    const short r, const short line_thickness, const struct nk_color col)
{
    if (r == 0) {
        nk_rawfb_stroke_line(rawfb, x, y, x + w, y, line_thickness, col);
        nk_rawfb_stroke_line(rawfb, x, y + h, x + w, y + h, line_thickness, col);
        nk_rawfb_stroke_line(rawfb, x, y, x, y + h, line_thickness, col);
        nk_rawfb_stroke_line(rawfb, x + w, y, x + w, y + h, line_thickness, col);
    } else {
        const short xc = x + r;
        const short yc = y + r;
        const short wc = (short)(w - 2 * r);
        const short hc = (short)(h - 2 * r);

        nk_rawfb_stroke_line(rawfb, xc, y, xc + wc, y, line_thickness, col);
        nk_rawfb_stroke_line(rawfb, x + w, yc, x + w, yc + hc, line_thickness, col);
        nk_rawfb_stroke_line(rawfb, xc, y + h, xc + wc, y + h, line_thickness, col);
        nk_rawfb_stroke_line(rawfb, x, yc, x, yc + hc, line_thickness, col);

        nk_rawfb_stroke_arc(rawfb, xc + wc - r, y,
                (unsigned)r*2, (unsigned)r*2, 0 , line_thickness, col);
        nk_rawfb_stroke_arc(rawfb, x, y,
                (unsigned)r*2, (unsigned)r*2, 90 , line_thickness, col);
        nk_rawfb_stroke_arc(rawfb, x, yc + hc - r,
                (unsigned)r*2, (unsigned)r*2, 270 , line_thickness, col);
        nk_rawfb_stroke_arc(rawfb, xc + wc - r, yc + hc - r,
                (unsigned)r*2, (unsigned)r*2, 180 , line_thickness, col);
    }
}

static void
nk_rawfb_fill_rect(const struct rawfb_context *rawfb,
    const short x, const short y, const short w, const short h,
    const short r, const struct nk_color col)
{
    int i;
    if (r == 0) {
        for (i = 0; i < h; i++)
            nk_rawfb_stroke_line(rawfb, x, y + i, x + w, y + i, 1, col);
    } else {
        const short xc = x + r;
        const short yc = y + r;
        const short wc = (short)(w - 2 * r);
        const short hc = (short)(h - 2 * r);

        struct nk_vec2i pnts[12];
        pnts[0].x = x;
        pnts[0].y = yc;
        pnts[1].x = xc;
        pnts[1].y = yc;
        pnts[2].x = xc;
        pnts[2].y = y;

        pnts[3].x = xc + wc;
        pnts[3].y = y;
        pnts[4].x = xc + wc;
        pnts[4].y = yc;
        pnts[5].x = x + w;
        pnts[5].y = yc;

        pnts[6].x = x + w;
        pnts[6].y = yc + hc;
        pnts[7].x = xc + wc;
        pnts[7].y = yc + hc;
        pnts[8].x = xc + wc;
        pnts[8].y = y + h;

        pnts[9].x = xc;
        pnts[9].y = y + h;
        pnts[10].x = xc;
        pnts[10].y = yc + hc;
        pnts[11].x = x;
        pnts[11].y = yc + hc;

        nk_rawfb_fill_polygon(rawfb, pnts, 12, col);

        nk_rawfb_fill_arc(rawfb, xc + wc - r, y,
                (unsigned)r*2, (unsigned)r*2, 0 , col);
        nk_rawfb_fill_arc(rawfb, x, y,
                (unsigned)r*2, (unsigned)r*2, 90 , col);
        nk_rawfb_fill_arc(rawfb, x, yc + hc - r,
                (unsigned)r*2, (unsigned)r*2, 270 , col);
        nk_rawfb_fill_arc(rawfb, xc + wc - r, yc + hc - r,
                (unsigned)r*2, (unsigned)r*2, 180 , col);
    }
}

static void
nk_rawfb_fill_triangle(const struct rawfb_context *rawfb,
    const short x0, const short y0, const short x1, const short y1,
    const short x2, const short y2, const struct nk_color col)
{
    struct nk_vec2i pnts[3];
    pnts[0].x = x0;
    pnts[0].y = y0;
    pnts[1].x = x1;
    pnts[1].y = y1;
    pnts[2].x = x2;
    pnts[2].y = y2;
    nk_rawfb_fill_polygon(rawfb, pnts, 3, col);
}

static void
nk_rawfb_stroke_triangle(const struct rawfb_context *rawfb,
    const short x0, const short y0, const short x1, const short y1,
    const short x2, const short y2, const unsigned short line_thickness,
    const struct nk_color col)
{
    nk_rawfb_stroke_line(rawfb, x0, y0, x1, y1, line_thickness, col);
    nk_rawfb_stroke_line(rawfb, x1, y1, x2, y2, line_thickness, col);
    nk_rawfb_stroke_line(rawfb, x2, y2, x0, y0, line_thickness, col);
}

static void
nk_rawfb_stroke_polygon(const struct rawfb_context *rawfb,
    const struct nk_vec2i *pnts, const int count,
    const unsigned short line_thickness, const struct nk_color col)
{
    int i;
    for (i = 1; i < count; ++i)
        nk_rawfb_stroke_line(rawfb, pnts[i-1].x, pnts[i-1].y, pnts[i].x,
                pnts[i].y, line_thickness, col);
    nk_rawfb_stroke_line(rawfb, pnts[count-1].x, pnts[count-1].y,
            pnts[0].x, pnts[0].y, line_thickness, col);
}

static void
nk_rawfb_stroke_polyline(const struct rawfb_context *rawfb,
    const struct nk_vec2i *pnts, const int count,
    const unsigned short line_thickness, const struct nk_color col)
{
    int i;
    for (i = 0; i < count-1; ++i)
        nk_rawfb_stroke_line(rawfb, pnts[i].x, pnts[i].y,
                 pnts[i+1].x, pnts[i+1].y, line_thickness, col);
}

static void
nk_rawfb_fill_circle(const struct rawfb_context *rawfb,
    short x0, short y0, short w, short h, const struct nk_color col)
{
    /* Bresenham's ellipses */
    const int a2 = (w * w) / 4;
    const int b2 = (h * h) / 4;
    const int fa2 = 4 * a2, fb2 = 4 * b2;
    int x, y, sigma;

    /* Convert upper left to center */
    h = (h + 1) / 2;
    w = (w + 1) / 2;
    x0 += w;
    y0 += h;

    /* First half */
    for (x = 0, y = h, sigma = 2*b2+a2*(1-2*h); b2*x <= a2*y; x++) {
        nk_rawfb_stroke_line(rawfb, x0 - x, y0 + y, x0 + x, y0 + y, 1, col);
        nk_rawfb_stroke_line(rawfb, x0 - x, y0 - y, x0 + x, y0 - y, 1, col);
        if (sigma >= 0) {
            sigma += fa2 * (1 - y);
            y--;
        } sigma += b2 * ((4 * x) + 6);
    }
    /* Second half */
    for (x = w, y = 0, sigma = 2*a2+b2*(1-2*w); a2*y <= b2*x; y++) {
        nk_rawfb_stroke_line(rawfb, x0 - x, y0 + y, x0 + x, y0 + y, 1, col);
        nk_rawfb_stroke_line(rawfb, x0 - x, y0 - y, x0 + x, y0 - y, 1, col);
        if (sigma >= 0) {
            sigma += fb2 * (1 - x);
            x--;
        } sigma += a2 * ((4 * y) + 6);
    }
}

static void
nk_rawfb_stroke_circle(const struct rawfb_context *rawfb,
    short x0, short y0, short w, short h, const short line_thickness,
    const struct nk_color col)
{
    /* Bresenham's ellipses */
    const int a2 = (w * w) / 4;
    const int b2 = (h * h) / 4;
    const int fa2 = 4 * a2, fb2 = 4 * b2;
    int x, y, sigma;

    /* Convert upper left to center */
    h = (h + 1) / 2;
    w = (w + 1) / 2;
    x0 += w;
    y0 += h;

    /* First half */
    for (x = 0, y = h, sigma = 2*b2+a2*(1-2*h); b2*x <= a2*y; x++) {
        nk_rawfb_setpixel(rawfb, x0 + x, y0 + y, col);
        nk_rawfb_setpixel(rawfb, x0 - x, y0 + y, col);
        nk_rawfb_setpixel(rawfb, x0 + x, y0 - y, col);
        nk_rawfb_setpixel(rawfb, x0 - x, y0 - y, col);
        if (sigma >= 0) {
            sigma += fa2 * (1 - y);
            y--;
        } sigma += b2 * ((4 * x) + 6);
    }
    /* Second half */
    for (x = w, y = 0, sigma = 2*a2+b2*(1-2*w); a2*y <= b2*x; y++) {
        nk_rawfb_setpixel(rawfb, x0 + x, y0 + y, col);
        nk_rawfb_setpixel(rawfb, x0 - x, y0 + y, col);
        nk_rawfb_setpixel(rawfb, x0 + x, y0 - y, col);
        nk_rawfb_setpixel(rawfb, x0 - x, y0 - y, col);
        if (sigma >= 0) {
            sigma += fb2 * (1 - x);
            x--;
        } sigma += a2 * ((4 * y) + 6);
    }
}

static void
nk_rawfb_stroke_curve(const struct rawfb_context *rawfb,
    const struct nk_vec2i p1, const struct nk_vec2i p2,
    const struct nk_vec2i p3, const struct nk_vec2i p4,
    const unsigned int num_segments, const unsigned short line_thickness,
    const struct nk_color col)
{
    unsigned int i_step, segments;
    float t_step;
    struct nk_vec2i last = p1;

    segments = MAX(num_segments, 1);
    t_step = 1.0f/(float)segments;
    for (i_step = 1; i_step <= segments; ++i_step) {
        float t = t_step * (float)i_step;
        float u = 1.0f - t;
        float w1 = u*u*u;
        float w2 = 3*u*u*t;
        float w3 = 3*u*t*t;
        float w4 = t * t *t;
        float x = w1 * p1.x + w2 * p2.x + w3 * p3.x + w4 * p4.x;
        float y = w1 * p1.y + w2 * p2.y + w3 * p3.y + w4 * p4.y;
        nk_rawfb_stroke_line(rawfb, last.x, last.y,
                (short)x, (short)y, line_thickness,col);
        last.x = (short)x; last.y = (short)y;
    }
}

static void
nk_rawfb_clear(const struct rawfb_context *rawfb, const struct nk_color col)
{
    nk_rawfb_fill_rect(rawfb, 0, 0, rawfb->fb.w, rawfb->fb.h, 0, col);
}

NK_API struct rawfb_context*
nk_rawfb_init(void *fb, void *tex_mem, const unsigned int w, const unsigned int h,
    const unsigned int pitch)
{
    const void *tex;
    struct rawfb_context *rawfb;
    rawfb = malloc(sizeof(struct rawfb_context));
    if (!rawfb)
        return NULL;

    memset(rawfb, 0, sizeof(struct rawfb_context));
    rawfb->font_tex.pixels = tex_mem;
    //rawfb->font_tex.format = NK_FONT_ATLAS_ALPHA8;
    rawfb->font_tex.format = NK_FONT_ATLAS_RGBA32;
    rawfb->font_tex.w = rawfb->font_tex.h = 0;

    rawfb->fb.pixels = fb;
    rawfb->fb.w = w;
    rawfb->fb.h = h;

#if defined(RAWFB_XRGB_8888) || defined(RAWFB_RGBX_8888)
    rawfb->fb.format = NK_FONT_ATLAS_RGBA32;
    rawfb->fb.pitch = pitch;
#else
    #error Fixme
#endif

    //nk_init_default(&rawfb->ctx, 0);
    nk_font_atlas_init_default(&rawfb->atlas);
    nk_font_atlas_begin(&rawfb->atlas);

    struct nk_font *font = nk_font_atlas_add_from_file(&rawfb->atlas, "/usr/share/fonts/ubuntu.ttf", 15.0f, NULL);

    //tex = nk_font_atlas_bake(&rawfb->atlas, &rawfb->font_tex.w, &rawfb->font_tex.h, rawfb->font_tex.format);
    tex = nk_font_atlas_bake(&rawfb->atlas, &rawfb->font_tex.w, &rawfb->font_tex.h, NK_FONT_ATLAS_RGBA32);
    if (!tex) return 0;

#if 0
    switch(rawfb->font_tex.format) {
    case NK_FONT_ATLAS_ALPHA8:
        rawfb->font_tex.pitch = rawfb->font_tex.w * 1;
        break;
    case NK_FONT_ATLAS_RGBA32:
#endif
        rawfb->font_tex.pitch = rawfb->font_tex.w * 4;
#if 0
        break;
    };
#endif
    /* Store the font texture in tex scratch memory */
    memcpy(rawfb->font_tex.pixels, tex, rawfb->font_tex.pitch * rawfb->font_tex.h);
    //nk_font_atlas_end(&rawfb->atlas, nk_handle_ptr(NULL), NULL);
    nk_font_atlas_end(&rawfb->atlas, nk_handle_ptr(&rawfb->font_tex), NULL);
    nk_init_default(&rawfb->ctx, &font->handle);

#if 0
    if (rawfb->atlas.default_font)
        nk_style_set_font(&rawfb->ctx, &rawfb->atlas.default_font->handle);
#endif

    nk_style_load_all_cursors(&rawfb->ctx, rawfb->atlas.cursors);
    nk_rawfb_scissor(rawfb, 0, 0, rawfb->fb.w, rawfb->fb.h);
    return rawfb;
}

static void
nk_rawfb_stretch_image(const struct rawfb_image *dst,
    const struct rawfb_image *src, const struct nk_rect *dst_rect,
    const struct nk_rect *src_rect, const struct nk_rect *dst_scissors)
{
    short i, j;
    struct nk_color col;
    float xinc = src_rect->w / dst_rect->w;
    float yinc = src_rect->h / dst_rect->h;
    float xoff = src_rect->x, yoff = src_rect->y;

    /* Simple nearest filtering rescaling */
    /* TODO: use bilinear filter */
    for (j = 0; j < (short)dst_rect->h; j++) {
        for (i = 0; i < (short)dst_rect->w; i++) {
            if (dst_scissors) {
                if (i + (int)(dst_rect->x + 0.5f) < dst_scissors->x || i + (int)(dst_rect->x + 0.5f) >= dst_scissors->w)
                    continue;
                if (j + (int)(dst_rect->y + 0.5f) < dst_scissors->y || j + (int)(dst_rect->y + 0.5f) >= dst_scissors->h)
                    continue;
            }
            col = nk_image_getpixel(src, (int)xoff, (int) yoff);
            nk_image_blendpixel(dst, i + (int)(dst_rect->x + 0.5f), j + (int)(dst_rect->y + 0.5f), col);
            xoff += xinc;
        }
        xoff = src_rect->x;
        yoff += yinc;
    }
}

static void
nk_rawfb_font_query_font_glyph(nk_handle handle, const float height,
    struct nk_user_font_glyph *glyph, const nk_rune codepoint,
    const nk_rune next_codepoint)
{
    float scale;
    const struct nk_font_glyph *g;
    struct nk_font *font;
    //NK_ASSERT(glyph);
    NK_UNUSED(next_codepoint);

    font = (struct nk_font*)handle.ptr;
    //NK_ASSERT(font);
    //NK_ASSERT(font->glyphs);
    if (!font || !glyph)
        return;

    scale = height/font->info.height;
    g = nk_font_find_glyph(font, codepoint);
    glyph->width = (g->x1 - g->x0) * scale;
    glyph->height = (g->y1 - g->y0) * scale;
    glyph->offset = nk_vec2(g->x0 * scale, g->y0 * scale);
    glyph->xadvance = (g->xadvance * scale);
    glyph->uv[0] = nk_vec2(g->u0, g->v0);
    glyph->uv[1] = nk_vec2(g->u1, g->v1);
}

NK_API void
nk_rawfb_draw_text(const struct rawfb_context *rawfb,
    const struct nk_user_font *font, const struct nk_rect rect,
    const char *text, const int len, const float font_height,
    const struct nk_color fg)
{
    float x = 0;
    int text_len = 0;
    nk_rune unicode = 0;
    nk_rune next = 0;
    int glyph_len = 0;
    int next_glyph_len = 0;
    struct nk_user_font_glyph g;
    if (!len || !text) return;

    x = 0;
    glyph_len = nk_utf_decode(text, &unicode, len);
    if (!glyph_len) return;

    /* draw every glyph image */
    while (text_len < len && glyph_len) {
        struct nk_rect src_rect;
        struct nk_rect dst_rect;
        float char_width = 0;
        if (unicode == NK_UTF_INVALID) break;

        /* query currently drawn glyph information */
        next_glyph_len = nk_utf_decode(text + text_len + glyph_len, &next, (int)len - text_len);
        nk_rawfb_font_query_font_glyph(font->userdata, font_height, &g, unicode,
                    (next == NK_UTF_INVALID) ? '\0' : next);

        /* calculate and draw glyph drawing rectangle and image */
        char_width = g.xadvance;
        src_rect.x = g.uv[0].x * rawfb->font_tex.w;
        src_rect.y = g.uv[0].y * rawfb->font_tex.h;
        src_rect.w = g.uv[1].x * rawfb->font_tex.w - g.uv[0].x * rawfb->font_tex.w;
        src_rect.h = g.uv[1].y * rawfb->font_tex.h - g.uv[0].y * rawfb->font_tex.h;

        dst_rect.x = x + g.offset.x + rect.x;
        dst_rect.y = g.offset.y + rect.y;
        dst_rect.w = ceilf(g.width);
        dst_rect.h = ceilf(g.height);

        /* TODO: account fg */
        /* Use software rescaling to blit glyph from font_text to framebuffer */
        nk_rawfb_stretch_image(&rawfb->fb, &rawfb->font_tex, &dst_rect, &src_rect, &rawfb->scissors);

        /* offset next glyph */
        text_len += glyph_len;
        x += char_width;
        glyph_len = next_glyph_len;
        unicode = next;
    }
}

NK_API void
nk_rawfb_drawimage(const struct rawfb_context *rawfb,
    const int x, const int y, const int w, const int h,
    const struct nk_image *img, const struct nk_color *col)
{
    struct nk_rect src_rect;
    struct nk_rect dst_rect;

    src_rect.x = img->region[0];
    src_rect.y = img->region[1];
    src_rect.w = img->region[2];
    src_rect.h = img->region[3];

    dst_rect.x = x;
    dst_rect.y = y;
    dst_rect.w = w;
    dst_rect.h = h;
    //nk_rawfb_stretch_image(&rawfb->fb, &rawfb->font_tex, &dst_rect, &src_rect, &rawfb->scissors);
    nk_rawfb_stretch_image(&rawfb->fb, img->handle.ptr, &dst_rect, &src_rect, &rawfb->scissors);
}

NK_API void
nk_rawfb_imgcpy(const struct rawfb_context *rawfb,
    const int x, const int y, const int w, const int h,
    const struct nk_image *img)
{
    //fprintf(stderr, "nk_rawfb_imgcpy(%p, %d, %d, %d, %d, %p)\n", rawfb, x, y, w, h, img);
    struct rawfb_image *handle = img->handle.ptr;
    char *fb = rawfb->fb.pixels;
    int fb_pitch = rawfb->fb.pitch;

    char *img_buf = handle->pixels;
    int img_pitch = handle->pitch;

    for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; ++j) {
            fb[(y+i) * fb_pitch + 4*(x+j) + 0] = img_buf[i * img_pitch + 4*j + 0];
            fb[(y+i) * fb_pitch + 4*(x+j) + 1] = img_buf[i * img_pitch + 4*j + 1];
            fb[(y+i) * fb_pitch + 4*(x+j) + 2] = img_buf[i * img_pitch + 4*j + 2];
        }
    }
}

NK_API void
nk_rawfb_shutdown(struct rawfb_context *rawfb)
{
    nk_free(&rawfb->ctx);
    memset(rawfb, 0, sizeof(struct rawfb_context));
    free(rawfb);
}

NK_API void
nk_rawfb_resize_fb(struct rawfb_context *rawfb,
                   void *fb,
                   const unsigned int w,
                   const unsigned int h,
                   const unsigned int pitch)
{
    rawfb->fb.w = w;
    rawfb->fb.h = h;
    rawfb->fb.pixels = fb;
    rawfb->fb.pitch = pitch;
}

NK_API void
nk_rawfb_render(const struct rawfb_context *rawfb,
                const struct nk_color clear,
                const unsigned char enable_clear)
{
    const struct nk_command *cmd;
    if (enable_clear)
        nk_rawfb_clear(rawfb, clear);

    nk_foreach(cmd, (struct nk_context*)&rawfb->ctx) {
        //fprintf(stderr, "cmd->type %d\n", cmd->type);
        switch (cmd->type) {
        case NK_COMMAND_NOP: break;
        case NK_COMMAND_SCISSOR: {
            const struct nk_command_scissor *s =(const struct nk_command_scissor*)cmd;
            nk_rawfb_scissor((struct rawfb_context *)rawfb, s->x, s->y, s->w, s->h);
        } break;
        case NK_COMMAND_LINE: {
            const struct nk_command_line *l = (const struct nk_command_line *)cmd;
            nk_rawfb_stroke_line(rawfb, l->begin.x, l->begin.y, l->end.x,
                l->end.y, l->line_thickness, l->color);
        } break;
        case NK_COMMAND_RECT: {
            const struct nk_command_rect *r = (const struct nk_command_rect *)cmd;
            nk_rawfb_stroke_rect(rawfb, r->x, r->y, r->w, r->h,
                (unsigned short)r->rounding, r->line_thickness, r->color);
        } break;
        case NK_COMMAND_RECT_FILLED: {
            const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled *)cmd;
            nk_rawfb_fill_rect(rawfb, r->x, r->y, r->w, r->h,
                (unsigned short)r->rounding, r->color);
        } break;
        case NK_COMMAND_CIRCLE: {
            const struct nk_command_circle *c = (const struct nk_command_circle *)cmd;
            nk_rawfb_stroke_circle(rawfb, c->x, c->y, c->w, c->h, c->line_thickness, c->color);
        } break;
        case NK_COMMAND_CIRCLE_FILLED: {
            const struct nk_command_circle_filled *c = (const struct nk_command_circle_filled *)cmd;
            nk_rawfb_fill_circle(rawfb, c->x, c->y, c->w, c->h, c->color);
        } break;
        case NK_COMMAND_TRIANGLE: {
            const struct nk_command_triangle*t = (const struct nk_command_triangle*)cmd;
            nk_rawfb_stroke_triangle(rawfb, t->a.x, t->a.y, t->b.x, t->b.y,
                t->c.x, t->c.y, t->line_thickness, t->color);
        } break;
        case NK_COMMAND_TRIANGLE_FILLED: {
            const struct nk_command_triangle_filled *t = (const struct nk_command_triangle_filled *)cmd;
            nk_rawfb_fill_triangle(rawfb, t->a.x, t->a.y, t->b.x, t->b.y,
                t->c.x, t->c.y, t->color);
        } break;
        case NK_COMMAND_POLYGON: {
            const struct nk_command_polygon *p =(const struct nk_command_polygon*)cmd;
            nk_rawfb_stroke_polygon(rawfb, p->points, p->point_count, p->line_thickness,p->color);
        } break;
        case NK_COMMAND_POLYGON_FILLED: {
            const struct nk_command_polygon_filled *p = (const struct nk_command_polygon_filled *)cmd;
            nk_rawfb_fill_polygon(rawfb, p->points, p->point_count, p->color);
        } break;
        case NK_COMMAND_POLYLINE: {
            const struct nk_command_polyline *p = (const struct nk_command_polyline *)cmd;
            nk_rawfb_stroke_polyline(rawfb, p->points, p->point_count, p->line_thickness, p->color);
        } break;
        case NK_COMMAND_TEXT: {
            const struct nk_command_text *t = (const struct nk_command_text*)cmd;
            nk_rawfb_draw_text(rawfb, t->font, nk_rect(t->x, t->y, t->w, t->h),
                t->string, t->length, t->height, t->foreground);
        } break;
        case NK_COMMAND_CURVE: {
            const struct nk_command_curve *q = (const struct nk_command_curve *)cmd;
            nk_rawfb_stroke_curve(rawfb, q->begin, q->ctrl[0], q->ctrl[1],
                q->end, 22, q->line_thickness, q->color);
        } break;
        case NK_COMMAND_RECT_MULTI_COLOR:
        case NK_COMMAND_IMAGE: {
            const struct nk_command_image *q = (const struct nk_command_image *)cmd;
            nk_rawfb_drawimage(rawfb, q->x, q->y, q->w, q->h, &q->img, &q->col);
        } break;
        case NK_COMMAND_ARC: {
            assert(0 && "NK_COMMAND_ARC not implemented\n");
        } break;
        case NK_COMMAND_ARC_FILLED: {
            assert(0 && "NK_COMMAND_ARC_FILLED not implemented\n");
        } break;
        case NK_COMMAND_CUSTOM: {
            const struct nk_command_custom *q = (const struct nk_command_custom *)cmd;
            nk_rawfb_imgcpy(rawfb, q->x, q->y, q->w, q->h, q->callback_data.ptr);
        } break;
        default: break;
        }
    } nk_clear((struct nk_context*)&rawfb->ctx);
}
#endif

