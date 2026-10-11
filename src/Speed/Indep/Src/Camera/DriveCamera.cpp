#include "DriveCamera.h"
#include "../Physics/VehicleMover.h"
#include "../Physics/Engine.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)

// Original globals (bound to the mapped retail image by OriginalSymbols.ld).
extern CameraDynamicsCurve cameraDynamicsCurves7EC610[6];
extern unsigned short cameraFieldOfView7EB5B4[6];
extern float cameraSampleParameter787154;
extern float cameraMinimumDuration7845A0;
extern float cameraDefaultNear784260;
extern float cameraDriverNear7EB5B0;
extern float cameraLimitDuration787150;
extern unsigned short cameraOverrideAngle82A4D0;
extern unsigned short cameraReverseAngle82A4DC;
extern unsigned short cameraSmallAngle8286E8;
extern float cameraZoom78721C;
extern CameraPairCurve cameraHeadingCurve82A4E0;
extern float cameraHeadingCurvePoints7EC420[];
extern int cameraCut883E34;

// Original 0x401020 / 0x43c370 are recovered in the physics sources.
float EngineCosineUnits(unsigned int units);

// Original 0x5749e0: advance one cubic follower channel by `step` seconds.
void CameraCubicChannel::Update(float step, float maximumVelocity, float maximumAcceleration) {
    if (state2C == 0) {
        return;
    }
    if (state2C != 1) {
        if (state2C != 2) {
            return;
        }
        time20 = 0.0f;
        if (mode2E == 0) {
            state2C = 1;
        }
        if (0.0f < maximumVelocity) {
            LimitVelocity(maximumVelocity);
        }
        float delta = target8 - value0;
        cubicD1C = value0;
        cubicC18 = velocity4;
        cubicA10 = (targetVelocityC + velocity4) - (delta + delta);
        cubicB14 = (delta * 3.0f - targetVelocityC) - (velocity4 + velocity4);
        if (0.0f < maximumAcceleration) {
            LimitAcceleration(maximumAcceleration);
        }
    }
    if (duration24 <= cameraMinimumDuration7845A0) {
        time20 = 1.0f;
    } else {
        time20 = step / duration24 + time20;
    }
    if (time20 < limit28) {
        float t = time20;
        value0 = ((t * cubicA10 + cubicB14) * t + cubicC18) * t + cubicD1C;
        velocity4 = (cubicB14 + cubicB14 + t * cubicA10 * 3.0f) * t + cubicC18;
    } else {
        state2C = 0;
        if (1.0f <= limit28) {
            value0 = target8;
            velocity4 = targetVelocityC;
        }
    }
}

// Original 0x5748c0: limit the end velocity of the restarted curve.
void CameraCubicChannel::LimitVelocity(float maximum) {
    float inverse = 1.0f / duration24;
    float scaled = inverse * duration24;
    float velocity = ((cubicB14 + cubicB14 + scaled * cubicA10 * 3.0f) * scaled + cubicC18) * inverse;
    float magnitude = velocity < 0.0f ? -velocity : velocity;
    if (maximum < magnitude) {
        targetVelocityC = (magnitude / velocity) * maximum * duration24;
    }
}

// Original 0x574910: limit the end acceleration of the restarted curve.
void CameraCubicChannel::LimitAcceleration(float maximum) {
    float inverse = 1.0f / duration24;
    float twiceB = cubicB14 + cubicB14;
    float start = (0.0f * inverse * cubicA10 * 6.0f + twiceB) * inverse * inverse;
    float magnitude = start < 0.0f ? -start : start;
    float end = (inverse * duration24 * cubicA10 * 6.0f + twiceB) * inverse * inverse;
    if (maximum < magnitude) {
        start = (magnitude / start) * maximum;
    }
    float endMagnitude = end < 0.0f ? -end : end;
    if (endMagnitude <= maximum) {
        if (magnitude <= maximum) {
            return;
        }
    } else {
        end = (endMagnitude / end) * maximum;
    }
    float squared = duration24 * duration24;
    start = start * squared;
    cubicB14 = 0.5f * start;
    cubicA10 = (squared * end - start) * 0.16666667f;
}

// Value = target, velocity = target velocity, follower idle (the reset blocks of 0x44de90).
void CameraCubicChannel::Snap() {
    value0 = target8;
    velocity4 = targetVelocityC;
    state2C = 0;
}

// Original 0x4419a0: all three channels share one duration.
void CameraCubicVector::Construct(short mode, float duration, float limit) {
    for (int i = 0; i < 3; ++i) {
        CameraCubicChannel *channel = &channels0[i];
        channel->value0 = 0.0f;
        channel->velocity4 = 0.0f;
        channel->target8 = 0.0f;
        channel->targetVelocityC = 0.0f;
        channel->state2C = 0;
        channel->mode2E = mode;
        channel->duration24 = duration;
        channel->limit28 = limit;
    }
}

// Original 0x441a20: one duration per axis.
void CameraCubicVector::Construct(short mode, const float *durations, float limit) {
    for (int i = 0; i < 3; ++i) {
        CameraCubicChannel *channel = &channels0[i];
        channel->value0 = 0.0f;
        channel->velocity4 = 0.0f;
        channel->target8 = 0.0f;
        channel->targetVelocityC = 0.0f;
        channel->state2C = 0;
        channel->mode2E = mode;
        channel->duration24 = durations[i];
        channel->limit28 = limit;
    }
}

// Original 0x574df0.
void CameraCubicVector::Update(float step) {
    channels0[0].Update(step, 0.0f, 0.0f);
    channels0[1].Update(step, 0.0f, 0.0f);
    channels0[2].Update(step, 0.0f, 0.0f);
}

// Original 0x570ae0: set the values, restarting a channel whose target differs.
void CameraCubicVector::SetValue(float x, float y, float z) {
    channels0[0].value0 = x;
    if (x != channels0[0].target8) {
        channels0[0].state2C = 2;
    }
    channels0[1].value0 = y;
    if (y != channels0[1].target8) {
        channels0[1].state2C = 2;
    }
    channels0[2].value0 = z;
    if (z != channels0[2].target8) {
        channels0[2].state2C = 2;
    }
}

// Original 0x570ba0 (reached through 0x574cd0): set the targets.
void CameraCubicVector::SetTarget(float x, float y, float z) {
    channels0[0].target8 = x;
    if (x != channels0[0].value0) {
        channels0[0].state2C = 2;
    }
    channels0[1].target8 = y;
    if (y != channels0[1].value0) {
        channels0[1].state2C = 2;
    }
    channels0[2].target8 = z;
    if (z != channels0[2].value0) {
        channels0[2].state2C = 2;
    }
}

// Original 0x574d10.
void CameraCubicVector::SetDurations(const float *durations) {
    channels0[0].duration24 = durations[0];
    channels0[1].duration24 = durations[1];
    channels0[2].duration24 = durations[2];
}

void CameraCubicVector::SetDuration(float duration) {
    channels0[0].duration24 = duration;
    channels0[1].duration24 = duration;
    channels0[2].duration24 = duration;
}

// The three reset blocks at the end of 0x44de90.
void CameraCubicVector::Snap() {
    channels0[0].Snap();
    channels0[1].Snap();
    channels0[2].Snap();
}

// Original 0x446e70 with the base constructor 0x576850.
void CameraAverager::Construct(int capacity) {
    vtable0 = 0;
    stride4 = 0x10;
    capacity5 = (unsigned char)capacity;
    count6 = 0;
    head7 = 0;
    buffer8 = new float[capacity * 4];
    for (int i = 0; i < capacity * 4; ++i) {
        buffer8[i] = 0.0f;
    }
    sum10[0] = sum10[1] = sum10[2] = 0.0f;
    average20[0] = average20[1] = average20[2] = 0.0f;
}

// Original 0x446f10.
void CameraAverager::Push(const float *vector) {
    if (count6 < capacity5) {
        count6 = count6 + 1;
    }
    float *old = buffer8 + head7 * 4;
    float oldX = old[0];
    float oldY = old[1];
    float oldZ = old[2];
    sum10[0] = sum10[0] - oldX;
    sum10[1] = sum10[1] - oldY;
    sum10[2] = sum10[2] - oldZ;
    sum10[0] = vector[0] + sum10[0];
    sum10[1] = vector[1] + sum10[1];
    sum10[2] = vector[2] + sum10[2];
    old[0] = vector[0];
    old[1] = vector[1];
    old[2] = vector[2];
    float scale = 1.0f / (float)count6;
    average20[0] = scale * sum10[0];
    average20[1] = sum10[1] * scale;
    average20[2] = sum10[2] * scale;
    head7 = head7 + 1;
    if (capacity5 <= head7) {
        head7 = 0;
    }
}

// Original 0x441140. The first table serves one active view, the second (+0x70) two.
const CameraSetting *CameraSettingsTable::Find(int type, int viewState) const {
    int count;
    const CameraSetting *entries;
    if (viewState == 3) {
        count = count4;
        entries = entries70;
    } else {
        count = count0;
        entries = entries10;
    }
    for (int i = 0; i < count; ++i) {
        if (type == entries[i].type0) {
            return &entries[i];
        }
    }
    return 0;
}

// Original 0x5ecca0: the setting for a type, else the one for type 0.
const CameraSetting *CameraSettingsTable::FindOrDefault(int type, int viewState) const {
    const CameraSetting *setting = Find(type, viewState);
    if (!setting) {
        setting = Find(0, viewState);
    }
    return setting;
}

// Original 0x445750: blend two keys, `fraction` of `high` and the rest of `low`.
static void BlendDynamicsKeys(CameraDynamicsKey *output, const CameraDynamicsKey *high,
                              const CameraDynamicsKey *low, float fraction) {
    float rest = 1.0f - fraction;
    const float *h = &high->durations0[0];
    const float *l = &low->durations0[0];
    float *o = &output->durations0[0];
    o[3] = fraction * h[3] + rest * l[3];
    o[0] = rest * l[0] + fraction * h[0];
    o[1] = fraction * h[1] + rest * l[1];
    o[2] = fraction * h[2] + rest * l[2];
    for (int block = 1; block < 11; ++block) {
        float *out = o + block * 4;
        const float *hi = h + block * 4;
        const float *lo = l + block * 4;
        out[0] = rest * lo[0] + fraction * hi[0];
        out[1] = rest * lo[1] + fraction * hi[1];
        out[2] = rest * lo[2] + fraction * hi[2];
    }
}

// Original 0x446dc0.
void CameraDynamicsCurve::Sample(CameraDynamicsKey *output, float position) {
    int count = count0;
    float index = (position - start4) * inverseSpacingC;
    int whole = (int)index;
    if (whole < 0) {
        *output = keys10[0];
        return;
    }
    if (count - 1 <= whole) {
        *output = keys10[count - 1];
        return;
    }
    float floorValue = (float)whole;
    if (index < floorValue) {
        floorValue = floorValue - 1.0f;
    }
    BlendDynamicsKeys(output, &keys10[whole + 1], &keys10[whole], index - floorValue);
}

// Original 0x576790.
float CameraPairCurve::Lookup(float input) {
    if (1 < count4) {
        float *p = points0;
        if (input < p[0] != (input == p[0])) {
            return p[1];
        }
        if (p[count4 * 2 - 2] <= input) {
            return p[count4 * 2 - 1];
        }
        for (int i = 0; i < count4 - 1; ++i) {
            if (p[i * 2] <= input && input < p[i * 2 + 2]) {
                float span = p[i * 2 + 2] - p[i * 2];
                if ((span < 0.0f ? -span : span) <= 1.0e-6f) {
                    return (p[i * 2 + 3] - p[i * 2 + 1]) * 0.5f + p[i * 2 + 1];
                }
                return ((input - p[i * 2]) / span) * (p[i * 2 + 3] - p[i * 2 + 1]) + p[i * 2 + 1];
            }
        }
    }
    return points0[1];
}

// Original 0x5c7ec0 for view type 1: both half angles are the field of view scaled by the
// pixel-aspect constants (0x7a27dc, 0x7a27e0, 0x7a27d8) and passed through the quantised sine.
extern float cameraAspectScaleX7A27DC;
extern float cameraAspectScaleY7A27E0;
extern float cameraAspectDivisor7A27D8;

void ComputeCameraProjection(CameraProjection *output, unsigned short fov, float aspect,
                             float nearPlane, float farPlane) {
    float scaleY = aspect * (cameraAspectScaleY7A27E0 / cameraAspectDivisor7A27D8);
    float scaleX = cameraAspectScaleX7A27DC;
    unsigned int halfX = (unsigned int)(int)((float)fov * scaleX * 0.5f);
    unsigned int halfY = (unsigned int)(int)((float)fov * scaleY * 0.5f);
    output->tangentX = EngineSineUnits(halfX) / EngineCosineUnits(halfX);
    output->tangentY = EngineSineUnits(halfY) / EngineCosineUnits(halfY);
    output->nearPlane = nearPlane;
    output->farPlane = farPlane;
}

// Original 0x502500.
int DriveCameraTypeForOption(int option) {
    switch (option) {
    case 0:
        return DRIVE_CAMERA_BUMPER;
    case 1:
        return DRIVE_CAMERA_HOOD;
    case 2:
        return DRIVE_CAMERA_CLOSE;
    case 4:
        return DRIVE_CAMERA_SUPER_FAR;
    default:
        return DRIVE_CAMERA_FAR;
    }
}

// Original static initialisers: 0x77be00 builds the heading curve from the points at 0x7ec420,
// 0x77be70 and 0x77be80 store the override angles, 0x77be60 a small angle.
void InitialiseDriveCameraStatics() {
    cameraHeadingCurve82A4E0.points0 = cameraHeadingCurvePoints7EC420;
    cameraHeadingCurve82A4E0.count4 = 2;
    cameraOverrideAngle82A4D0 = 0xbbb;
    cameraReverseAngle82A4DC = 0x38e;
    cameraSmallAngle8286E8 = 0x38e;
}

// Original 0x445e00 for a mover whose slot 0x24 is false: the car matrix with the car origin as
// translation (0x401340 on car +0x90 and +0x50).
void DriveCameraMover::GetCarFrame(WheelMatrix *output) {
    DrivetrainAuxiliary *state = car2C->State();
    WheelMatrix *source = &state->matrix50;
    for (int i = 0; i < 12; ++i) {
        output->elements[i] = source->elements[i];
    }
    output->elements[12] = state->origin10.x;
    output->elements[13] = state->origin10.y;
    output->elements[14] = state->origin10.z;
    output->elements[15] = 1.0f;
}

// Original 0x445eb0: the camera offset (forward, lateral, height) and pitch angle of a setting.
// The drift flags (0x89e7e2/0x890100), the rear-view table (0x7ec400, player +0x2b clear) and the
// non-human input override (input slot 0x54) are not reachable in free drive: the drift flags and
// slot are false, and the player's +0x2b byte is set.
void DriveCameraMover::ReadSetting(float *offset, unsigned short *angle, const CameraSetting *setting) {
    offset[0] = setting->x4;
    offset[1] = setting->zC;
    offset[2] = setting->y8;
    *angle = setting->angle2;
}

// Original 0x44de90: eye, look-at and up targets from the settings and the car's motion.
void DriveCameraMover::ComputePose(WheelMatrix *carFrame, const CameraSetting *setting,
                                   const CameraDynamicsKey *key, int clock) {
    // 0x445c20 steers the heading follower, which only 0x445e00 of the chase subclass reads.
    GetCarFrame(carFrame);
    float accel[3] = {0.0f, 0.0f, 0.0f};
    if (swing50) {
        // 0x5baf90 inverts the car frame (transposed axes, negated origin) and 0x486f50 applies
        // it with w = 0: the averaged world acceleration expressed in the car axes.
        float x = averager5C->average20[0];
        float y = averager5C->average20[1];
        float z = averager5C->average20[2];
        float *m = carFrame->elements;
        accel[0] = m[0] * x + m[1] * y + m[2] * z;
        accel[1] = m[4] * x + m[5] * y + m[6] * z;
        accel[2] = m[8] * x + m[9] * y + m[10] * z;
    }
    if (car2C->colliderPack5CC != 0 && 1 < car2C->mode424) {
        groundTime58 = clock;
    }
    float offset[3];
    unsigned short angle;
    ReadSetting(offset, &angle, setting);
    float sine = EngineSineUnits(angle);
    float cosine = EngineCosineUnits(angle);
    float tangent = sine / cosine;
    float lookX;
    float lookZ;
    short type = setting->type0;
    if (type == 1 || type == 0 || type == 5) {
        lookX = 0.0f;
        lookZ = offset[0] * tangent + offset[2];
    } else {
        lookZ = offset[2] - tangent;
        lookX = offset[0] + 1.0f;
    }
    float position[3];
    position[0] = accel[0] * key->eyeScale40[0];
    position[1] = accel[1] * key->eyeScale40[1];
    position[2] = accel[2] * key->eyeScale40[2];
    for (int i = 0; i < 3; ++i) {
        if (position[i] < key->eyeMinimum50[i]) position[i] = key->eyeMinimum50[i];
        if (key->eyeMaximum60[i] < position[i]) position[i] = key->eyeMaximum60[i];
        position[i] = position[i] + offset[i];
    }
    eye38->SetTarget(position[0], position[1], position[2]);
    float look[3];
    look[0] = accel[0] * key->targetScale70[0];
    look[1] = accel[1] * key->targetScale70[1];
    look[2] = accel[2] * key->targetScale70[2];
    for (int i = 0; i < 3; ++i) {
        if (look[i] < key->targetMinimum80[i]) look[i] = key->targetMinimum80[i];
        if (key->targetMaximum90[i] < look[i]) look[i] = key->targetMaximum90[i];
    }
    look[0] = look[0] + lookX;
    look[2] = look[2] + lookZ;
    target3C->SetTarget(look[0], look[1], look[2]);
    float up[3];
    up[0] = key->upScale10[0] * averager5C->average20[0];
    up[1] = key->upScale10[1] * averager5C->average20[1];
    up[2] = key->upScale10[2] * averager5C->average20[2] + 1.0f;
    for (int i = 0; i < 3; ++i) {
        if (up[i] < key->upMinimum20[i]) up[i] = key->upMinimum20[i];
        if (key->upMaximum30[i] < up[i]) up[i] = key->upMaximum30[i];
    }
    up44->SetTarget(up[0], up[1], up[2]);
    // Field of view target for the camera type of the setting.
    float fovTarget = (float)(int)cameraFieldOfView7EB5B4[type];
    fov34->target8 = fovTarget;
    if (fovTarget != fov34->value0) {
        fov34->state2C = 2;
    }
    // The reset blocks test the member at +0x52 (the parameter only reaches 0x445c20) and the
    // cockpit types 2 and 3 always snap.
    if (snap52 || type == 2 || type == 3) {
        up44->Snap();
        fov34->Snap();
        eye38->Snap();
        target3C->Snap();
    }
    if (snap52) {
        cameraCut883E34 = 1;
    }
}

// Original 0x445b10 (slot 0x20): change the camera type and snap on the next update.
void DriveCameraMover::SetType(int type) {
    snap52 = 1;
    type48 = type;
}

// Original 0x445a30 (slot 0x14).
void DriveCameraMover::SetLookBack(bool lookBack) {
    if (lookBack51 != (unsigned char)lookBack) {
        cameraCut883E34 = 1;
    }
    lookBack51 = (unsigned char)lookBack;
}

// Original constructor 0x451660 (base 0x44ab90), reduced to the state the update consumes. The
// original also seeds the followers from the previous Camera state (0x444910, 0x444980,
// 0x444a70) and snaps when that is far from the new targets; with a fresh Camera the pose pass
// below with snap set produces the same starting state.
void DriveCameraMover::Construct(MoverModeOwner *car, const CameraSettingsTable *table, int state,
                                 int type, bool disableSwing, bool lookBack, bool noDefaults) {
    car2C = car;
    settings = table;
    viewState = state;
    unknown30 = 0;
    type48 = type;
    previousType4C = type;
    swing50 = !disableSwing;
    lookBack51 = lookBack;
    snap52 = 0;
    flag53 = noDefaults;
    active54 = 1;
    groundTime58 = 0;
    CameraDynamicsKey key;
    cameraDynamicsCurves7EC610[type].Sample(&key, cameraSampleParameter787154);
    fov34 = new CameraCubicChannel;
    fov34->value0 = fov34->velocity4 = fov34->target8 = fov34->targetVelocityC = 0.0f;
    fov34->state2C = 0;
    fov34->mode2E = 1;
    fov34->duration24 = key.durations0[2];
    fov34->limit28 = cameraLimitDuration787150;
    eye38 = new CameraCubicVector;
    eye38->Construct(1, key.durations0[0], cameraLimitDuration787150);
    target3C = new CameraCubicVector;
    target3C->Construct(1, key.durations0[1], cameraLimitDuration787150);
    heading40 = new CameraCubicVector;
    heading40->Construct(1, key.headingDurationsA0, cameraLimitDuration787150);
    up44 = new CameraCubicVector;
    up44->Construct(1, key.headingDurationsA0, cameraLimitDuration787150);
    averager5C = new CameraAverager;
    averager5C->Construct(4);
    WheelMatrix carFrame;
    const CameraSetting *setting = settings->FindOrDefault(type48, viewState);
    snap52 = 1;
    ComputePose(&carFrame, setting, &key, 0);
    snap52 = 0;
}

void DriveCameraMover::Destroy() {
    delete fov34;
    delete eye38;
    delete target3C;
    delete heading40;
    delete up44;
    delete[] averager5C->buffer8;
    delete averager5C;
    fov34 = 0;
    eye38 = target3C = heading40 = up44 = 0;
    averager5C = 0;
}

// Original 0x453570 (vtable slot 0x04).
void DriveCameraMover::Update(float step, int clock, DriveCameraView *view) {
    active54 = 0;
    int type = type48;
    if (type == DRIVE_CAMERA_HOOD || type == DRIVE_CAMERA_BUMPER) {
        // The cockpit types fall back to the far camera when the input is not human (slot 0x54),
        // when 0x445fc0 reports the view blocked or when the 0x5d39f0 world ray from the car to
        // the camera hits geometry. Those world queries are not recovered and report clear.
        snap52 = snap52 | (previousType4C == 0);
    }
    previousType4C = type;
    CameraDynamicsKey key;
    cameraDynamicsCurves7EC610[type].Sample(&key, cameraSampleParameter787154);
    DrivetrainAuxiliary *state = car2C->State();
    averager5C->Push(&state->value40.x);
    up44->channels0[0].duration24 = key.durations0[3];
    up44->channels0[1].duration24 = key.durations0[3];
    up44->channels0[2].duration24 = key.durations0[3];
    fov34->duration24 = key.durations0[2];
    target3C->SetDuration(key.durations0[1]);
    heading40->SetDurations(key.headingDurationsA0);
    eye38->SetDuration(key.durations0[0]);
    up44->Update(step);
    fov34->Update(step, 0.0f, 0.0f);
    eye38->Update(step);
    target3C->Update(step);
    heading40->Update(step);

    const CameraSetting *setting = settings->FindOrDefault(type, viewState);
    short settingType = setting->type0;
    bool chase = settingType == 1 || settingType == 0 || settingType == 5;
    bool attach = false;
    if (!chase && (lookBack51 || type != DRIVE_CAMERA_HOOD)) {
        attach = true;
    }
    view->attachedToCar = attach;
    view->chase = chase;
    view->nearPlane = settingType == 3 ? cameraDriverNear7EB5B0 : cameraDefaultNear784260;
    view->farPlane = 10000.0f;     // immediate 0x461c4000 stored by the base constructor 0x44ab90

    WheelMatrix carFrame;
    ComputePose(&carFrame, setting, &key, clock);
    snap52 = 0;

    float scale = 1.0f;
    if (lookBack51) {
        scale = -1.0f;
    }
    float up[3];
    up[0] = up44->channels0[0].value0;
    up[1] = up44->channels0[1].value0;
    up[2] = 1.0f;
    if (!chase) {
        up[0] = state->matrix90.elements[8];
        up[1] = state->matrix90.elements[9];
        up[2] = state->matrix90.elements[10];
    }
    WheelVector eye;
    eye.x = scale * eye38->channels0[0].value0;
    eye.y = scale * eye38->channels0[1].value0;
    eye.z = eye38->channels0[2].value0;
    WheelVector target;
    target.x = scale * target3C->channels0[0].value0;
    target.y = scale * target3C->channels0[1].value0;
    target.z = target3C->channels0[2].value0;
    WheelTransform(&eye, &carFrame, &eye);
    WheelTransform(&target, &carFrame, &target);
    float deltaZ = eye.z - target.z;
    float deltaX = (eye.x - target.x) * cameraZoom78721C;
    float deltaY = (eye.y - target.y) * cameraZoom78721C;
    eye.x = deltaX + target.x;
    eye.y = deltaY + target.y;
    eye.z = deltaZ * cameraZoom78721C + target.z;
    // The field of view goes through 0x43e6d0 with the zoom; the half angle round trip returns the
    // follower value to within one unit.
    unsigned int half = ((unsigned int)(int)fov34->value0 & 0xffffu) >> 1;
    view->fov = (unsigned short)(half << 1);
    view->eye[0] = eye.x;
    view->eye[1] = eye.y;
    view->eye[2] = eye.z;
    view->target[0] = target.x;
    view->target[1] = target.y;
    view->target[2] = target.z;
    view->up[0] = up[0];
    view->up[1] = up[1];
    view->up[2] = up[2];
}
