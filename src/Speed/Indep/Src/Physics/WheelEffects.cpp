#include "VehicleMover.h"
#include "../World/WCollision.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)

extern float skidLoadThreshold;
extern float skidMinimumAlignment;
extern float skidRollingIntensity;
extern float skidMinimumIntensity;

void WheelSetLength(WheelVector *output, WheelVector *input, float length);

// Original 0x59db50: rotate a vector by the 3x3 part of a matrix.
void WheelRotate(WheelVector *output, WheelMatrix *matrix, WheelVector *input) {
    float *m = matrix->elements;
    output->x = m[0] * input->x + m[4] * input->y + m[8] * input->z;
    output->y = m[5] * input->y + m[1] * input->x + m[9] * input->z;
    output->z = m[6] * input->y + m[2] * input->x + m[10] * input->z;
}

// Original 0x59dbb0: lay a skid-mark segment while sliding on the ground.
void DrivetrainWheel::UpdateEffects(float duration, MoverModeOwner *owner) {
    (void)duration;
    bool loaded = value170 * value164 > skidLoadThreshold;
    SkidMarkTrail *trail = reinterpret_cast<SkidMarkTrail *>(&value198);
    if ((!flag160 && !loaded) || !flag11C || ((1 << spec14->value18) & owner->mask95C)
        || !((float)sqrt(vectorF0.x * vectorF0.x + vectorF0.y * vectorF0.y) > 1.0f)) {
        trail->End();
        return;
    }
    WheelVector across;
    across.y = vectorF0.x;
    across.x = -vectorF0.y;
    across.z = 0.0f;
    WheelNormalize(&across, &across);
    WheelVector unitY = {0.0f, 1.0f, 0.0f};
    WheelVector axis;
    WheelRotate(&axis, &matrix70, &unitY);
    volatile float alignment = axis.z * across.z + axis.y * across.y + axis.x * across.x;
    alignment = alignment < 0.0f ? -alignment : alignment;
    volatile float surfaceWidth = spec14->value14 * roadSurfaces[surface114].skidWidth14;
    float clamped = alignment;
    if (!(clamped >= skidMinimumAlignment)) {
        clamped = skidMinimumAlignment;
    }
    volatile float width = surfaceWidth * clamped * 0.5f;
    WheelSetLength(&across, &across, width);
    WheelVector left, right;
    WheelVector *position = reinterpret_cast<WheelVector *>(&matrix70.elements[12]);
    left.x = across.x + position->x;
    left.y = position->y + across.y;
    left.z = position->z + across.z;
    right.x = position->x - across.x;
    right.y = position->y - across.y;
    right.z = position->z - across.z;
    volatile float leftHeight = auxiliary20->collider4->QueryGround(&left, 0, 0, 0) + offset18C;
    float rightHeight = auxiliary20->collider4->QueryGround(&right, 0, 0, 0) + offset18C;
    WheelVector centre;
    centre.y = position->y;
    centre.x = position->x;
    centre.z = (leftHeight + rightHeight) * 0.5f;
    across.z = leftHeight * 0.5f - rightHeight * 0.5f;
    WheelSetLength(&across, &across, width);
    float intensity = alignment;
    if (!flag160) {
        intensity = alignment * skidRollingIntensity;
    }
    if (surface114 == 0xd) {
        intensity = 0.0f;
    } else if (!(intensity >= skidMinimumIntensity)) {
        intensity = skidMinimumIntensity;
    }
    trail->AddSegment(owner, &centre, &across, surface114, intensity);
}
