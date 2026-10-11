#include "VehicleMover.h"
#include "Engine.h"

extern float nitrousTicksPerSecond;
extern float drivetrainClockScale;
extern float wheelRenderAngleScale;

// Statically linked D3DX helper reached through the dispatch pointer at 0x803e74.
extern "C" WheelMatrix *__stdcall D3DXMatrixMultiply(WheelMatrix *output, WheelMatrix *left, WheelMatrix *right);

// Original 0x5bac90: rotation about Z by a 16-bit angle.
void MakeRotationZ(WheelMatrix *output, unsigned int angle) {
    volatile float cosine = EngineSineUnits(angle + 0x4000);
    float sine = EngineSineUnits(angle);
    float *m = output->elements;
    m[0] = cosine;
    m[1] = sine;
    m[5] = cosine;
    m[4] = -sine;
    m[2] = 0.0f;
    m[3] = 0.0f;
    m[6] = 0.0f;
    m[7] = 0.0f;
    m[8] = 0.0f;
    m[9] = 0.0f;
    m[10] = 1.0f;
    m[11] = 0.0f;
    m[12] = 0.0f;
    m[13] = 0.0f;
    m[14] = 0.0f;
    m[15] = 1.0f;
}

// Original 0x5bada0: right-multiply by a rotation about Y.
void RotateMatrixY(WheelMatrix *output, WheelMatrix *input, unsigned int angle) {
    volatile float cosine = EngineSineUnits(angle + 0x4000);
    float sine = EngineSineUnits(angle);
    WheelMatrix rotation;
    float *m = rotation.elements;
    m[2] = -sine;
    m[8] = sine;
    m[0] = cosine;
    m[10] = cosine;
    m[1] = 0.0f;
    m[3] = 0.0f;
    m[4] = 0.0f;
    m[5] = 1.0f;
    m[6] = 0.0f;
    m[7] = 0.0f;
    m[9] = 0.0f;
    m[11] = 0.0f;
    m[12] = 0.0f;
    m[13] = 0.0f;
    m[14] = 0.0f;
    m[15] = 1.0f;
    D3DXMatrixMultiply(output, input, &rotation);
}

// Original 0x5bae60: right-multiply by a rotation about Z.
void RotateMatrixZ(WheelMatrix *output, WheelMatrix *input, unsigned int angle) {
    WheelMatrix rotation;
    MakeRotationZ(&rotation, angle);
    D3DXMatrixMultiply(output, input, &rotation);
}

// Original 0x5921b0: render frames for a wheel from spin, steering and suspension.
void DrivetrainAuxiliary::UpdateWheelRenderMatrices(int wheel) {
    if (!renderFlags3B4[wheel]) {
        return;
    }
    WheelMatrix spun, steered;
    WheelIdentity(&spun);
    WheelIdentity(&steered);
    RotateMatrixY(&spun, &spun, (int)(wheelAngle300[wheel] * wheelRenderAngleScale));
    if (wheel < 2) {
        RotateMatrixZ(&spun, &spun, steering3CE);
        RotateMatrixZ(&steered, &steered, steering3CE);
    }
    WheelVector *position = &record0->wheels10[wheel].position0;
    steered.elements[13] = position->y;
    steered.elements[14] = position->z;
    steered.elements[12] = position->x;
    steered.elements[15] = 1.0f;
    float z = position->z;
    spun.elements[13] = position->y;
    spun.elements[12] = position->x;
    spun.elements[15] = 1.0f;
    steered.elements[14] = z + suspension340[wheel];
    spun.elements[14] = position->z + suspension340[wheel];
    BodyMatrixCopy(&wheelMatricesF0[wheel], &spun);
    BodyMatrixCopy(&wheelMatrices1F0[wheel], &steered);
}

// Original 0x591cd0: nitrous capacity timers from the car record.
void DrivetrainAuxiliary::InitialiseNitrous() {
    CarPhysicsRecord *record = record0;
    timer414 = (int)(record->nitrousCapacity2A0 * nitrousTicksPerSecond + 0.5f);
    timer41C = (int)(record->nitrousCapacity2A0 * nitrousTicksPerSecond + 0.5f);
    int bottles = record->nitrousBottles2A4;
    if (bottles > 0) {
        timer418 = (int)(record->nitrousCapacity2A0 / bottles * nitrousTicksPerSecond + 0.5f);
        timer420 = 0;
        return;
    }
    timer418 = 0;
    timer420 = 0;
}

// Original 0x59f880: reset the shared vehicle state to rest on four wheels.
void DrivetrainAuxiliary::Reset() {
    velocity30.x = velocity30.y = velocity30.z = 0.0f;
    value40.x = value40.y = value40.z = 0.0f;
    WheelIdentity(&matrix50);
    heading3CC = 0;
    steering3CE = 0;
    angularD0.x = angularD0.y = angularD0.z = 0.0f;
    valueE0.x = valueE0.y = valueE0.z = 0.0f;
    for (int i = 0; i < 4; ++i) {
        wheelSpin2F0[i] = 0.0f;
        wheelAngle300[i] = 0.0f;
        wheelLoad310[i] = 0.0f;
        wheelValue320[i] = 0.0f;
        accumulated330[i] = 0.0f;
        contacts350[i] = 1;
        renderFlags3B4[i] = 1;
        surfaces354[i] = 1;
        maximumSurfaces364[i] = 1;
        wheelPairs374[i][0] = 0.0f;
        wheelPairs374[i][1] = 0.0f;
        wheelValue394[i] = 0.0f;
        suspension340[i] = 0.0f;
        values3A4[i] = 0.0f;
    }
    value3E4 = 4;
    value3E8 = 4;
    for (int i = 0; i < 4; ++i) {
        UpdateWheelRenderMatrices(i);
    }
    averageWheel3D8 = 0.0f;
    slip3DC = 0.0f;
    slip3E0 = 0.0f;
    value3EC = 0.0f;
    value3F0 = 0.0f;
    gear3F4 = 1;
    control3F8 = 0.0f;
    value3FC = 0.0f;
    engineValue400 = 1000.0f;
    value404 = -1.0f;
    value408 = 1;
    if (timer414 == 0 || timer414 == 0x7fffffff || (float)timer414 * drivetrainClockScale == 0.0f) {
        InitialiseNitrous();
    }
    value428 = 1.0f;
    value434 = 1.0f;
    selector42C = 0;
    value430 = 0.0f;
    for (int i = 0; i < 16; ++i) {
        matrix440.elements[i] = 0.0f;
    }
    flag480 = 0;
}
