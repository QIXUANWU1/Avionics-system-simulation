#ifndef IMA_CABIN_FIXTURE_H
#define IMA_CABIN_FIXTURE_H
#include <stdint.h>

enum { CAMERA_WIDTH=128, CAMERA_HEIGHT=96 };
enum CabinScene { CABIN_CLEAR, CABIN_DIM, CABIN_OCCLUDED, CABIN_GLARE };
typedef struct { uint8_t pixels[CAMERA_WIDTH*CAMERA_HEIGHT]; } CabinFrame;
void cabin_fixture(CabinFrame *, unsigned scene, double lux, uint32_t seed);
#endif
