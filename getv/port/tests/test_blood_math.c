#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../src/ge_blood_math.h"

int main(void)
{
    GeBloodPoint triangle[3]={{0,0},{10,0},{0,10}};
    GeBloodPoint reversed[3]={{0,10},{10,0},{0,0}};
    GeBloodPoint degenerate[3]={{0,0},{1,1},{2,2}};
    GeBloodPoint polygon[GE_BLOOD_POLY_MAX];
    int winding,i,n;
    for(winding=0;winding<2;winding++) {
        polygon[0]=(GeBloodPoint){-5,-5}; polygon[1]=(GeBloodPoint){15,-5};
        polygon[2]=(GeBloodPoint){15,15}; polygon[3]=(GeBloodPoint){-5,15};
        n=geBloodClip(polygon,4,winding?reversed:triangle);
        assert(n>=3 && n<=GE_BLOOD_POLY_MAX);
        for(i=0;i<n;i++) {
            assert(isfinite(polygon[i].x) && isfinite(polygon[i].y));
            assert(polygon[i].x>=-0.001f && polygon[i].y>=-0.001f);
            assert(polygon[i].x+polygon[i].y<=10.001f);
        }
    }
    polygon[0]=(GeBloodPoint){20,20}; polygon[1]=(GeBloodPoint){21,20};
    polygon[2]=(GeBloodPoint){20,21};
    assert(geBloodClip(polygon,3,triangle)==0);
    assert(geBloodClip(polygon,3,degenerate)==0);
    /* Exercise clipping near mesh diagonals and tiny corners with translated,
     * irregular convex octagons. Every output must remain on the source face. */
    for(winding=0;winding<1000;winding++) {
        float x=(winding%37)*0.8f-10.0f;
        float y=(winding%53)*0.6f-10.0f;
        for(i=0;i<8;i++) {
            float a=i*0.78539816339f;
            polygon[i]=(GeBloodPoint){x+cosf(a)*6.0f,y+sinf(a)*4.0f};
        }
        n=geBloodClip(polygon,8,triangle);
        assert(n==0 || (n>=3 && n<=GE_BLOOD_POLY_MAX));
        for(i=0;i<n;i++) {
            assert(isfinite(polygon[i].x) && isfinite(polygon[i].y));
            assert(polygon[i].x>=-0.001f && polygon[i].y>=-0.001f);
            assert(polygon[i].x+polygon[i].y<=10.001f);
        }
    }
    /* Broad, rotated lobes must retain real area without creating ink outside
     * the clipped mesh face; zero-area tangencies must not consume stain slots. */
    for(winding=0;winding<720;winding++) {
        float angle=winding*0.01745329252f;
        float original_area;
        for(i=0;i<8;i++) {
            float a=i*0.78539816339f;
            float x=cosf(a)*110.0f, y=sinf(a)*35.0f;
            polygon[i]=(GeBloodPoint){5.0f+x*cosf(angle)-y*sinf(angle),
                                     5.0f+x*sinf(angle)+y*cosf(angle)};
        }
        original_area=geBloodPolygonArea(polygon,8);
        n=geBloodClip(polygon,8,triangle);
        assert(n>=3);
        assert(geBloodPolygonArea(polygon,n)<=50.001f);
        assert(geBloodPolygonArea(polygon,n)<=original_area);
    }
    polygon[0]=(GeBloodPoint){0,0}; polygon[1]=(GeBloodPoint){1,1};
    polygon[2]=(GeBloodPoint){2,2};
    assert(geBloodPolygonArea(polygon,3)==0.0f);
    {
        unsigned char mask[GE_BLOOD_MASK_SIZE*GE_BLOOD_MASK_SIZE];
        unsigned char repeat[GE_BLOOD_MASK_SIZE*GE_BLOOD_MASK_SIZE];
        int covered=0,soft=0,outer=0,holes=0;
        geBloodMakeMask(mask); geBloodMakeMask(repeat);
        for(i=0;i<GE_BLOOD_MASK_SIZE*GE_BLOOD_MASK_SIZE;i++) {
            int x=i%GE_BLOOD_MASK_SIZE,y=i/GE_BLOOD_MASK_SIZE;
            int alpha=mask[i]&15;
            float px=(x+0.5f)*(2.0f/GE_BLOOD_MASK_SIZE)-1.0f;
            float py=(y+0.5f)*(2.0f/GE_BLOOD_MASK_SIZE)-1.0f;
            float r2=px*px+py*py;
            assert(mask[i]==repeat[i]);
            assert((mask[i]&240)==240);
            if(x==0 || y==0 || x==GE_BLOOD_MASK_SIZE-1 || y==GE_BLOOD_MASK_SIZE-1) assert(alpha==0);
            if(alpha) covered++;
            if(alpha>0 && alpha<15) soft++;
            if(alpha && r2>0.36f) outer++;
            if(!alpha && r2<0.16f) holes++;
        }
        assert(covered>600 && covered<2200);
        assert(soft>40 && outer>10 && holes>0);
    }
    puts("PASS blood geometry: clipping, winding, outside, degenerate 1000 edge cases, broad lobes and procedural mask passed");
    return 0;
}
