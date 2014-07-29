#include <stdint.h>
#include <string.h>
#include <math.h>
#include "game_shared.h"

#define PI 3.14159

// out back bicubic
// from http://www.timotheegroleau.com/Flash/experiments/easing_function_generator.htm
float bump_out(float v0, float v1, float t)
{
   t /= 1;// intensity (d)

   float ts = t  * t;
   float tc = ts * t;
   return v0 + v1 * (4*tc + -9*ts + 6*t);
}

// interpolation functions
float lerp(float v0, float v1, float t)
{
   return v0 * (1 - t) + v1 * t;
}

float cos_interp(float v0,float v1, float t)
{
   float t2;

   t2 = (1-cos(t*PI))/2;
   return(v0*(1-t2)+v1*t2);
}
