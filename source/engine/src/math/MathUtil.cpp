#include "Math/MathUtil.h"

float Loops::Math::lerp(float x, float y, float t)
{
    return x * (1.f - t) + y * t;
}