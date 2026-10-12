#include "HumanDriverInput.h"
#include "ForceFeedback.h"
#include "GameModeFlags.h"
#include "../World/WCollision.h"

extern unsigned char inputDeviceSlots864788[];
extern unsigned char wheelControlEnabled7E94A0;
extern float wheelControlLimitSecond;      // 10
extern float wheelControlLimitFirst;       // 30
extern float wheelControlUnlimited;        // -1
extern float wheelControlSpeedScale;       // 2.237 (m/s to mph)
extern float wheelControlDelay;            // 0.22
extern float wheelControlThreshold;        // 0.8
extern float handbrakeLockThreshold;       // 0.6
extern int drivetrainClock;
extern float drivetrainClockScale;
void UpdateColliderFaceRecords(WCollider *collider, WheelVector *position); // 0x5dc410

// 0x4022c0 is empty in this build.
void HumanInputHook4022C0(MoverModeOwner *) {}

// 0x571a60: device slots map to 0xff when unassigned.
int IsInputDeviceAssigned(int device) {
    return inputDeviceSlots864788[device] != 0xff;
}

// 0x4076c0 clears the action state written by the input event handlers.
void HumanDriverInput::ClearActions() {
    action2C4[0] = 0x80;
    action2C4[1] = 0x80;
    action2D0[0] = 0;
    action2D0[1] = 0;
    action2D0[2] = 0;
    action2D8[0] = 0;
    action2D8[1] = 0;
    action2D8[2] = 0;
    action2E0[0] = 0;
    action2E0[1] = 0;
    action2E8[0] = 0;
    action2E8[1] = 0;
    *reinterpret_cast<int *>(&steering2C8) = 0;
    *reinterpret_cast<int *>(&throttle2D4) = 0;
    *reinterpret_cast<int *>(&brake2DC) = 0;
    *reinterpret_cast<int *>(&value2E4) = 0;
    *reinterpret_cast<int *>(&handbrake2EC) = 0;
    wheelStamp300 = 0;
    wheelMode2FB = 0;
    *reinterpret_cast<int *>(&wheelInput304) = 0;
    *reinterpret_cast<int *>(&wheelInput308) = 0;
    *reinterpret_cast<int *>(&wheelInput30C) = 0;
    *reinterpret_cast<int *>(&wheelInput310) = 0;
}

// 0x407370: per-wheel control is allowed when enabled (0x7e94a0), below the
// speed limit in mph (30, or 10 for the second query; -1 means unlimited),
// and outside groups 89E7D9/890104/89E7E3/89010C.
bool HumanDriverInput::CanUseWheelControl(bool second) {
    float limit = second ? wheelControlLimitSecond : wheelControlLimitFirst;
    bool allowed = true;
    if (wheelControlEnabled7E94A0 == 0 || limit == 0.0f) allowed = false;
    if (limit != wheelControlUnlimited && (double)car2A8->speed430 * wheelControlSpeedScale > limit) allowed = false;
    if (gameFlag89E7D9 || gameFlag890104 || gameFlag89E7E3 || gameFlag89010C) return false;
    return allowed;
}

// 0x407400 maps the per-wheel action axes (+0x304..+0x310) onto wheel
// inputs (+0x1ec) in mode 2 or wheel targets (+0x1fc) in mode 1; the mode is
// requested by +0x2fa after a 0.22 s stamp at +0x300. The argument is unused.
void HumanDriverInput::UpdateWheelControl(float) {
    bool allowed = CanUseWheelControl(false);
    if (!allowed && wheelControl2F9) {
        for (int i = 0; i < 4; ++i) *reinterpret_cast<int *>(&wheelInputs1EC[i]) = 0;
        for (int i = 0; i < 4; ++i) *reinterpret_cast<int *>(&wheelTargets1FC[i]) = 0;
        wheelMode2FB = 0;
        *reinterpret_cast<int *>(&wheelInput304) = 0;
        *reinterpret_cast<int *>(&wheelInput308) = 0;
        *reinterpret_cast<int *>(&wheelInput310) = 0;
    }
    wheelControl2F9 = allowed;
    if (wheelStamp300 != 0 && wheelStamp300 != 0x7fffffff
            && (double)(drivetrainClock - wheelStamp300) * drivetrainClockScale > wheelControlDelay) {
        if (CanUseWheelControl(true) && wheelRequest2FA) {
            wheelMode2FB = (wheelRequest2FA != 0xaa) + 1;
            wheelStamp300 = 0;
        }
        wheelStamp300 = 0;
    }
    if (wheelMode2FB == 2) {
        float lower = -wheelControlThreshold;
        for (int i = 0; i < 4; ++i) *reinterpret_cast<int *>(&wheelInputs1EC[i]) = 0;
        if (wheelInput308 < lower) {
            wheelInputs1EC[2] = -wheelInput308;
            wheelInputs1EC[3] = -wheelInput308;
        } else if (wheelInput308 > wheelControlThreshold) {
            wheelInputs1EC[1] = wheelInput308;
            wheelInputs1EC[0] = wheelInput308;
        } else if (wheelInput304 < lower) {
            wheelInputs1EC[1] = -wheelInput304;
            wheelInputs1EC[2] = -wheelInput304;
        } else if (wheelInput304 > wheelControlThreshold) {
            wheelInputs1EC[0] = wheelInput304;
            wheelInputs1EC[3] = wheelInput304;
        }
    }
    if (wheelMode2FB == 1) {
        float lower = -wheelControlThreshold;
        signed char mask = 0;
        if (wheelInput310 < lower) mask = 0xc;
        else if (wheelInput310 > wheelControlThreshold) mask = 3;
        else if (wheelInput30C < lower) mask = 6;
        else if (wheelInput30C > wheelControlThreshold) mask = 9;
        if (wheelState2FC == 0) {
            for (int wheel = 0; wheel < 4; ++wheel) {
                if (!(mask & (1 << wheel))) continue;
                if (car2A8->GetWheelState(wheel) == 0) wheelState2FC = 1;
                else if (car2A8->GetWheelState(wheel) == 1 && wheelState2FC == 0) wheelState2FC = -1;
            }
            if (wheelState2FC == 0) return;
        }
        for (int wheel = 0; wheel < 4; ++wheel)
            if (mask & (1 << wheel)) wheelTargets1FC[wheel] = wheelState2FC > 0 ? 1.0f : 0.0f;
    }
}

// 0x41e2c0: per-frame human input update for an assigned device: collider
// face records, action reset for unassigned slots, pedals (0x407a30),
// steering (0x41dfc0) unless disabled, handbrake/transmission handling for a
// human car's drivetrain and per-wheel control (0x407400).
void HumanDriverInput::Update(float duration) {
    if (device2AC == -1) return;
    HumanInputHook4022C0(car2A8);
    UpdateColliderFaceRecords(car2A8->Collider(), &car2A8->State()->position20);
    if (!IsInputDeviceAssigned(device2AC)) ClearActions();
    if (value2B8) {
        throttle2D4 = 0.0f;
        brake2DC = 1.0f;
    }
    UpdatePedals(duration);
    if (steeringEnabled2F8) {
        float steer;
        if (wheelMode2FB) steer = 0.0f;
        else steer = steering2C8;
        steering1E0 = steering220.Update(steer, duration, device2AC);
    }
    if (flag2B4) {
        gearRequest1E4 = 1;
        SetBrake(1.0f);
    }
    MoverModeOwner *car = car2A8;
    if (car->moverType28 == 1) {
        Drivetrain *drive = car->mover34->drivetrain4C;
        if (transmission2C0 == 3) {
            int gear = car->gear434;
            int request = gearRequest1E4;
            if (gear != request && (gear <= 1 || request <= 1)) {
                transmission2C0 = 4;
                drive->status60 = 0;
            }
        } else if (transmission2C0 == 4 && !car->HasPendingShift()) {
            transmission2C0 = 3;
            drive->status60 = 3;
        }
        if (transmission2C0 == 3) {
            drive->status60 = 3;
            drive->SetClutchInput(clutch2F4);
            UpdateWheelControl(duration);
            return;
        }
        if (reinterpret_cast<GameSession89E7A0 *>(0x89e7a0)->Query401950())
            drive->flag74 = handbrake210 > handbrakeLockThreshold;
    }
    UpdateWheelControl(duration);
}

// 0x401950: group-3 test on the session object (+0x38 and +0x39).
bool GameSession89E7A0::Query401950() {
    if (flag38 || gameFlag890108) return false;
    return gameFlag890114 || flag39 || gameFlag890104;
}
