#ifndef GE_BLOOD_MATH_H
#define GE_BLOOD_MATH_H

#include <math.h>

/* Original cosmetic geometry. Convex clipping keeps every stain inside the actual
 * background triangle, including at doorways, corners and thin ledges. */
typedef struct GeBloodPoint { float x, y; } GeBloodPoint;
#define GE_BLOOD_POLY_MAX 12

static float geBloodSide(GeBloodPoint a, GeBloodPoint b, GeBloodPoint p)
{
    return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);
}

static float geBloodPolygonArea(const GeBloodPoint *polygon, int count)
{
    float twice_area=0.0f;
    int i;
    for(i=0;i<count;i++) {
        int next=(i+1)%count;
        twice_area+=polygon[i].x*polygon[next].y-polygon[next].x*polygon[i].y;
    }
    return twice_area<0.0f ? -twice_area*0.5f:twice_area*0.5f;
}

static int geBloodClip(GeBloodPoint *polygon, int count, const GeBloodPoint triangle[3])
{
    GeBloodPoint output[GE_BLOOD_POLY_MAX];
    float area = geBloodSide(triangle[0], triangle[1], triangle[2]);
    float sign = area < 0.0f ? -1.0f : 1.0f;
    int edge, i, out;
    if (area > -0.0001f && area < 0.0001f) return 0;
    for (edge=0; edge<3 && count; edge++) {
        GeBloodPoint a=triangle[edge], b=triangle[(edge+1)%3];
        GeBloodPoint previous=polygon[count-1];
        float previous_side=sign*geBloodSide(a,b,previous);
        out=0;
        for (i=0; i<count; i++) {
            GeBloodPoint current=polygon[i];
            float side=sign*geBloodSide(a,b,current);
            if ((side>=0.0f)!=(previous_side>=0.0f)) {
                float t=previous_side/(previous_side-side);
                if (out>=GE_BLOOD_POLY_MAX) return 0;
                output[out].x=previous.x+(current.x-previous.x)*t;
                output[out++].y=previous.y+(current.y-previous.y)*t;
            }
            if (side>=0.0f) {
                if (out>=GE_BLOOD_POLY_MAX) return 0;
                output[out++]=current;
            }
            previous=current;
            previous_side=side;
        }
        count=out;
        for (i=0; i<count; i++) polygon[i]=output[i];
    }
    return count>=3 ? count : 0;
}
/* Original procedural ink, independent of game textures and gameplay RNG.
 * IA8 keeps the 64x64 mask within the standard 4KB texture memory footprint. */
#define GE_BLOOD_MASK_SIZE 64
static unsigned int geBloodMaskRandom(unsigned int *state)
{
    unsigned int x=*state;
    x^=x<<13; x^=x>>17; x^=x<<5;
    *state=x;
    return x;
}
static float geBloodMaskUnit(unsigned int *state)
{
    return (geBloodMaskRandom(state)&65535U)*(1.0f/65535.0f);
}
static void geBloodMakeMask(unsigned char *mask)
{
    struct { float x,y,rx,ry; } lobes[50],holes[9];
    unsigned int seed=0x71b10d5U;
    int i,x,y;
    for(i=0;i<50;i++) {
        float angle=geBloodMaskUnit(&seed)*6.28318530718f;
        float radius=i<18 ? 0.45f+geBloodMaskUnit(&seed)*0.10f
                          : 0.62f+geBloodMaskUnit(&seed)*0.15f;
        lobes[i].x=cosf(angle)*radius; lobes[i].y=sinf(angle)*radius;
        lobes[i].rx=i<18 ? 0.09f+geBloodMaskUnit(&seed)*0.08f
                        : 0.012f+geBloodMaskUnit(&seed)*0.027f;
        lobes[i].ry=lobes[i].rx*(0.55f+geBloodMaskUnit(&seed)*0.7f);
    }
    for(i=0;i<9;i++) {
        float angle=geBloodMaskUnit(&seed)*6.28318530718f;
        float radius=0.15f+geBloodMaskUnit(&seed)*0.38f;
        holes[i].x=cosf(angle)*radius; holes[i].y=sinf(angle)*radius;
        holes[i].rx=0.018f+geBloodMaskUnit(&seed)*0.028f;
        holes[i].ry=holes[i].rx*(0.7f+geBloodMaskUnit(&seed)*0.7f);
    }
    for(y=0;y<GE_BLOOD_MASK_SIZE;y++) for(x=0;x<GE_BLOOD_MASK_SIZE;x++) {
        float px=(x+0.5f)*(2.0f/GE_BLOOD_MASK_SIZE)-1.0f;
        float py=(y+0.5f)*(2.0f/GE_BLOOD_MASK_SIZE)-1.0f;
        float angle=atan2f(py,px),radius=sqrtf(px*px+py*py);
        float boundary=0.51f+0.045f*sinf(angle*3.0f+0.8f)+0.033f*sinf(angle*7.0f)+0.018f*sinf(angle*13.0f+1.1f);
        float ink=(boundary-radius)*35.0f+0.5f;
        if(ink<0.0f) ink=0.0f;
        if(ink>1.0f) ink=1.0f;
        for(i=0;i<50;i++) {
            float dx=(px-lobes[i].x)/lobes[i].rx;
            float dy=(py-lobes[i].y)/lobes[i].ry;
            float dab=(1.0f-dx*dx-dy*dy)*4.0f;
            if(dab>1.0f) dab=1.0f;
            if(dab>ink) ink=dab;
        }
        for(i=0;i<9;i++) {
            float dx=(px-holes[i].x)/holes[i].rx;
            float dy=(py-holes[i].y)/holes[i].ry;
            float gap=(1.0f-dx*dx-dy*dy)*4.0f;
            if(gap>1.0f) gap=1.0f;
            if(gap>0.0f) ink*=1.0f-gap;
        }
        /* White intensity multiplies the vertex blood colour; the low nibble
         * supplies only coverage, with bilinear filtering smoothing its edge. */
        mask[y*GE_BLOOD_MASK_SIZE+x]=(unsigned char)(0xf0U | (unsigned int)(ink*15.0f+0.5f));
    }
}
#endif
