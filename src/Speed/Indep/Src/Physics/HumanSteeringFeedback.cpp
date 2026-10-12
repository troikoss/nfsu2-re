#include "HumanSteering.h"
#include "Engine.h"
#include "ForceFeedback.h"
#include "GameModeFlags.h"

extern "C" double __cdecl fabs(double);
#pragma intrinsic(fabs)

float WheelLength(WheelVector *input); // 0x440f70
extern int *gameFlow8900D8;
extern unsigned char forceFeedbackTable83AA68[];
extern float forceFeedbackLastSpeed827AAC[];
extern float forceFeedbackJoltTime827AA4[];
extern int drivetrainClock;
extern float drivetrainClockScale;
extern float forceFeedbackSpeedScale;      // 1/13
extern float forceFeedbackSteeringScale;   // 10000
extern float forceFeedbackSpringScale;     // 7058.82
extern float forceFeedbackJoltThreshold;   // 4
extern float forceFeedbackLoadThreshold;   // 0.05
extern float forceFeedbackJoltInterval;    // 0.15
extern float forceFeedbackJoltScale;       // 1/15
extern float forceFeedbackLoadJoltScale;   // 6666.67
extern float forceFeedbackSurfaceScale;    // 39.2157
extern float forceFeedbackRumbleSpeed;     // 23
extern float forceFeedbackLoadDivisor;     // 4

// 0x59daf0: basisC0 * value128 + basisB0 * value124.
void DrivetrainWheel::GetBlendedBasis(WheelVector *output) {
    float y = (float)((double)basisC0.y * value128 + (double)basisB0.y * value124);
    float z = (float)((double)basisC0.z * value128 + (double)basisB0.z * value124);
    output->x = (float)((double)basisC0.x * value128 + (double)value124 * basisB0.x);
    output->y = y;
    output->z = z;
}

// 0x5eb420: first byte of the 0x28-byte row selected by +0x10.
int MoverRaceEntry::QueryFlag5EB420() {
    return forceFeedbackTable83AA68[index10 * 0x28];
}

// 0x4018f0.
bool GameSession89E7A0::Query4018F0() {
    return flag38 != 0 || gameFlag890108 != 0;
}

// 0x406460: steering force feedback for a human car (+0x28 == 1) with an
// FF device, while the game flow state is 4. Outside the 0x5eb420/0x4018f0
// conditions all effects stop. Otherwise it sets the centring and steering
// torque from the front wheels' lateral force (scaled by valueC = 7), a spring
// that weakens with speed, jolts on sudden speed loss or suspension load,
// a surface effect from the wheel surfaces +0x394/+0x398, and a rumble on
// surface 13 above 23; the per-device last speed is kept at 0x827aac.
void HumanSteering::UpdateForceFeedback(float input, int device) {
    MoverModeOwner *car = car0;
    if (car->moverType28 != 1 || car->raceEntry18 == 0 || forceFeedbackManager870760 == 0) return;
    if (!forceFeedbackManager870760->HasDevice5BFD70(device)) return;
    if (gameFlow8900D8 == 0 || *gameFlow8900D8 != 4) return;
    if (car0->raceEntry18->QueryFlag5EB420() == 0 || reinterpret_cast<GameSession89E7A0 *>(0x89e7a0)->Query4018F0()) {
        forceFeedbackManager870760->Stop5CD850(device);
        forceFeedbackManager870760->Stop5CD9D0(device);
        forceFeedbackManager870760->Stop5BFE70(device);
        forceFeedbackManager870760->Stop5CDBB0(device);
        forceFeedbackManager870760->Stop5CD910(device);
        if (fabs(input) == 1.0f) {
            if (forceFeedbackManager870760->IsEffectActive5BFD90(device, 2)) forceFeedbackManager870760->Stop5BFE00(device);
            else forceFeedbackManager870760->Play5C9150(device, 0, 10000, 10000);
        } else if (forceFeedbackManager870760->IsEffectActive5BFD90(device, 2)) {
            forceFeedbackManager870760->Stop5BFE00(device);
        } else {
            forceFeedbackManager870760->Play5C9150(device, 0, 0x1b92, 0x1b92);
        }
        return;
    }
    if (fabs(input) == 1.0f) forceFeedbackManager870760->Play5C9150(device, 0, 10000, 10000);
    else forceFeedbackManager870760->Stop5BFE00(device);
    car = car0;
    double length = WheelLength(&car->State()->velocity30);
    float speed = (float)length;
    float spring = EngineMinimum(1.0f, (float)(length * forceFeedbackSpeedScale));
    WheelVector force;
    car->mover34->wheels38[0]->GetBlendedBasis(&force);
    float front = force.y;
    car->mover34->wheels38[1]->GetBlendedBasis(&force);
    double lateral = (double)force.y + front;
    float total = (float)lateral;
    if (lateral != 0.0) {
        int torque;
        if (!(fabs(speed) > 1.0f)) torque = 0;
        else {
            torque = (int)(((double)total / valueC) * forceFeedbackSteeringScale);
            if (torque > 10000) torque = 10000;
            else if (torque < -10000) torque = -10000;
        }
        if (!forceFeedbackManager870760->IsEffectActive5BFD90(device, 2))
            forceFeedbackManager870760->SetConstant5C9550(device, torque, 0x10e);
    }
    forceFeedbackManager870760->SetSpring5C9590(device, (int)((1.0f - (double)spring) * forceFeedbackSpringScale));
    unsigned char *carBytes = reinterpret_cast<unsigned char *>(car0);
    if (carBytes[0x390] == 0 && carBytes[0x391] == 0) {
        if (!forceFeedbackManager870760->IsEffectActive5C9040(device, 9)) forceFeedbackManager870760->Start5CDA10(device);
    } else if (forceFeedbackManager870760->IsEffectActive5C9040(device, 9)) {
        forceFeedbackManager870760->Stop5CDBB0(device);
    }
    float *last = &forceFeedbackLastSpeed827AAC[device];
    double drop = (double)*last - speed;
    float loss = (float)drop;
    if (fabs(drop) > forceFeedbackJoltThreshold) {
        double now = (double)drivetrainClock * drivetrainClockScale;
        double since = now - forceFeedbackJoltTime827AA4[device];
        if (since < 0.0) {
            *reinterpret_cast<int *>(&forceFeedbackJoltTime827AA4[device]) = 0;
        } else if (!(since < forceFeedbackJoltInterval)) {
            forceFeedbackJoltTime827AA4[device] = (float)now;
            float jolt = EngineMinimum(1.0f, (float)(((double)loss - forceFeedbackJoltThreshold) * forceFeedbackJoltScale));
            forceFeedbackManager870760->PlayJolt5C9760(device, (int)((double)jolt * forceFeedbackSteeringScale));
        }
    }
    float *load = reinterpret_cast<float *>(carBytes + 0x370);
    double sum = (((double)load[0] + load[1]) + load[2]) + load[3];
    if (sum > forceFeedbackLoadThreshold) {
        double strength = EngineMinimum(1.0f, (float)(sum / forceFeedbackLoadDivisor));
        double now = (double)drivetrainClock * drivetrainClockScale;
        double since = now - forceFeedbackJoltTime827AA4[device];
        bool play = true;
        if (since < 0.0) *reinterpret_cast<int *>(&forceFeedbackJoltTime827AA4[device]) = 0;
        else if (since < forceFeedbackJoltInterval) play = false;
        else forceFeedbackJoltTime827AA4[device] = (float)now;
        if (play) forceFeedbackManager870760->PlayJolt5C9760(device, (int)(strength * forceFeedbackLoadJoltScale));
    }
    int first = *reinterpret_cast<int *>(carBytes + 0x394);
    int second = *reinterpret_cast<int *>(carBytes + 0x398);
    int level = 0;
    int parameter = 0;
    switch (first) {
    case 8: level = 0x82; parameter = 0x50; break;
    case 9: level = 0x6e; parameter = 0x50; break;
    case 10: level = 0x46; parameter = 0x5a; break;
    case 11: level = 0x64; parameter = 0x64; break;
    case 15: level = 0x64; parameter = 0x28; break;
    case 16: level = 0x50; parameter = 0x64; break;
    case 17: level = 0x50; parameter = 0x8c; break;
    }
    int kind;
    switch (second) {
    case 8: kind = 0; level += 0x82; parameter += 0x50; break;
    case 9: kind = 0; level += 0x6e; parameter += 0x50; break;
    case 10: kind = 1; level += 0x46; parameter += 0x5a; break;
    case 11: kind = 1; level += 0x64; parameter += 0x64; break;
    case 15: kind = 2; level += 0x64; parameter += 0x28; break;
    case 16: kind = 1; level = 0x50; parameter = 0x64; break;
    case 17: kind = 1; level = 0x50; parameter = 0x8c; break;
    default: kind = 0; break;
    }
    int halfLevel = level / 2;
    forceFeedbackManager870760->SetSurface5C9FD0(device, kind, (int)(((double)halfLevel * spring) * forceFeedbackSurfaceScale),
                                                 parameter / 2);
    if ((first == 13 || second == 13) && speed > forceFeedbackRumbleSpeed)
        forceFeedbackManager870760->SetRumble5C9D90(device, 10000);
    else
        forceFeedbackManager870760->Stop5CD910(device);
    *last = speed;
}
