#include "VehicleMover.h"
#include "Engine.h"
#include "../../Libs/bWare/Strings.h"
#include "../World/WCollision.h"

extern float engineSubstepDuration;
extern float engineHalfPi;
extern float wheelMissingGroundHeight;
extern float wheelSteepGroundTravel;
extern float wheelMinimumUpDot;
extern float wheelSpinDragScale;
extern float wheelLockDragScale;
extern float wheelMinimumSubstep;
extern float wheelLoadDotScale;
extern float wheelLoadDotOffset;
extern float wheelLoadMinimum;
extern float wheelAngleWrap;
extern float wheelAngleNegativeWrap;
extern float wheelAngleReset;
extern float wheelAngleNegativeReset;
extern float wheelSpinEpsilon;
extern float wheelNegativeSpinEpsilon;
extern unsigned char gameFlag89E7D8;
extern int gameFlag890108;

// Original surface lattice noise 0x5d3ca0 over the 16-entry table at 0x800bf8.
float RoadSurfaceBump(float *position, int surface) {
    RoadSurface *record = &roadSurfaces[surface];
    float x = record->bumpFrequency18 * position[0];
    float y = position[1] * record->bumpFrequency18;
    volatile float floorX = (float)(int)x;
    if (floorX > x) {
        floorX = floorX - 1.0f;
    }
    float floorY = (float)(int)y;
    if (floorY > y) {
        floorY = floorY - 1.0f;
    }
    int cellX = (int)floorX;
    int cellY = (int)floorY;
    volatile float fractionX = x - floorX;
    float fractionY = y - floorY;
    float inverseY = 1.0f - fractionY;
    float inverseX = 1.0f - fractionX;
    int h00 = (cellY * cellX * -5 - (cellY + 1) * (cellX + 1) * 3) & 0xf;
    int h10 = ((cellX + 1) * cellY * -5 - (cellX + 2) * (cellY + 1) * 3) & 0xf;
    int h01 = ((cellY + 1) * cellX * -5 - (cellY + 2) * (cellX + 1) * 3) & 0xf;
    int h11 = ((cellX + 1) * (cellY + 1) * -5 - (cellX + 2) * (cellY + 2) * 3) & 0xf;
    float sum = roadNoiseTable[h00] * inverseX * inverseY;
    sum = sum + roadNoiseTable[h10] * inverseY * fractionX;
    sum = sum + roadNoiseTable[h01] * inverseX * fractionY;
    sum = sum + roadNoiseTable[h11] * fractionY * fractionX;
    return sum * record->bumpAmplitude1C;
}

// Constructor body of original 0x5a8160 ("Wheel"); vtable 0x7a20e8 is installed by the caller.
void DrivetrainWheel::Construct(DrivetrainWheelSpec *spec, WheelCorrectionSpec *correction,
                                WheelPreparationSpec *preparation, DrivetrainAuxiliary *auxiliary) {
    value198 = 0;
    auxiliary20 = auxiliary;
    spec14 = spec;
    value162 = 0;
    value16C = 0.0f;
    value168 = 0.0f;
    value120 = 0.0f;
    offset18C = 0.0f;
    correctionSpec18 = correction;
    state110 = 1;
    preparationSpec1C = preparation;
    coefficient180 = 1.0f;
    coefficient184 = 1.0f;
    coefficient190 = 1.0f;
    coefficient194 = 1.0f;
    coefficient188 = 1.0f;
    nfs_strcpy(name0C, "Wheel");
}

// Original 0x59d250: distance from a world mount point to the ground along the body's up axis.
float DrivetrainWheel::QueryGround(WheelVector *normal, int *surface, WheelVector *point,
                                   WheelMatrix *matrix) {
    char found;
    float height = auxiliary20->collider4->QueryGround(point, surface, normal, &found) + offset18C;
    if (!found) {
        height = wheelMissingGroundHeight;
    }
    float ground[3];
    ground[2] = height;
    ground[0] = point->x;
    ground[1] = point->y;
    float bump = RoadSurfaceBump(ground, *surface);
    value168 = bump;
    ground[2] = bump + ground[2];
    if (gameFlag89E7D8 || gameFlag890108) {
        *surface = 1;
    }
    float up = matrix->elements[9] * normal->y + matrix->elements[10] * normal->z
             + matrix->elements[8] * normal->x;
    if (!(up >= wheelMinimumUpDot)) {
        *surface = 0;
        return wheelSteepGroundTravel;
    }
    return ((ground[1] - point->y) * normal->y + (ground[0] - point->x) * normal->x
            + (ground[2] - point->z) * normal->z) / up;
}

// Original 0x59d160: contact flag, contact point and load scaled by camber alignment.
float DrivetrainWheel::SetContact(int contact, float load) {
    flag11C = (unsigned char)contact;
    if (contact) {
        WheelScaleAdd(&contactPoint140, reinterpret_cast<WheelVector *>(&matrix70.elements[12]),
                      &vector150, -spec14->value10);
    }
    if (load > 0.0f) {
        WheelVector *axis = reinterpret_cast<WheelVector *>(&matrix70.elements[4]);
        WheelVector lateral;
        WheelCross(&lateral, axis, &vector150);
        WheelCross(&lateral, &lateral, axis);
        float alignment = (lateral.y * vector150.y + lateral.z * vector150.z
                           + lateral.x * vector150.x) * wheelLoadDotScale - wheelLoadDotOffset;
        if (!(alignment >= wheelLoadMinimum)) {
            alignment = wheelLoadMinimum;
        }
        value164 = alignment * load;
        return 0.0f;
    }
    value164 = 0.0f;
    return 0.0f;
}

// Original 0x59d350, vtable slot 0x18: wheel angle and spin derivatives.
int DrivetrainWheel::WriteBlockDerivative(float *output) {
    output[0] = value28;
    float response = ((value108 - spec14->value10 * value124) + value120) * 0.5f;
    value120 = response;
    output[1] = (response - wheelSpinDragScale * roadSurfaces[surface114].spinDrag2C * value28
                 - value174 * wheelLockDragScale * value28) / divisor2C;
    if (flag10C) {
        output[1] = 0.0f;
    }
    if (!flag160 && engineSubstepDuration > wheelMinimumSubstep) {
        output[1] = output[1] - value118 / (spec14->value10 * engineSubstepDuration);
    }
    return 0;
}

// Original 0x59d030, vtable slot 0x1c: spin frame, accumulator reset and direction latch.
void DrivetrainWheel::UpdateAfterIntegration() {
    WheelIdentity(&matrix30);
    float cosine = EngineSine(angle24 + engineHalfPi);
    float sine = EngineSine(angle24);
    matrix30.elements[8] = sine;
    matrix30.elements[2] = -sine;
    matrix30.elements[0] = cosine;
    matrix30.elements[10] = cosine;
    value124 = 0.0f;
    value128 = 0.0f;
    value108 = 0.0f;
    value118 = 0.0f;
    if (angle24 > wheelAngleWrap) {
        angle24 = angle24 - wheelAngleWrap;
        if (angle24 > wheelAngleReset) {
            angle24 = 0.0f;
        }
    } else if (!(angle24 >= wheelAngleNegativeWrap)) {
        angle24 = angle24 + wheelAngleWrap;
        if (!(angle24 >= wheelAngleNegativeReset)) {
            angle24 = 0.0f;
        }
    }
    if (state110 == 0) {
        if (!(value28 >= 0.0f)) {
            value28 = 0.0f;
        }
    } else if (state110 == 2 && value28 > 0.0f) {
        value28 = 0.0f;
    }
    if (value28 > wheelSpinEpsilon) {
        state110 = 0;
    } else if (value28 >= wheelNegativeSpinEpsilon) {
        state110 = 1;
    } else {
        state110 = 2;
    }
}

// Original 0x591480: copy the steered local frame into rows 0x0b0..0x0ef.
void DrivetrainWheel::SetLocalMatrix(WheelMatrix *matrix) {
    WheelMatrix *target = reinterpret_cast<WheelMatrix *>(&basisB0);
    *target = *matrix;
}

// Original 0x5913d0: copy the world frame into matrix 0x70.
void DrivetrainWheel::SetWorldMatrix(WheelMatrix *matrix) {
    matrix70 = *matrix;
}
