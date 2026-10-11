#include "VehicleMover.h"
#include "VehicleIntegrator.h"
#include "DriveFeedbackCurve.h"
#include "Engine.h"
#include "GameModeFlags.h"

extern "C" double __cdecl sqrt(double);
#pragma intrinsic(sqrt)

extern float engineSubstepDuration;
extern float engineAngleUnit;
extern float wheelForcePi;
extern float bodyGravity;
extern int drivetrainClock;
extern float drivetrainClockScale;
extern float drivetrainFinishingInputThreshold;
extern float drivetrainSelectorUpperThreshold;
extern int moverImpulsesDisabled;
extern int moverResetPulse;
extern float moverResetImpulse;
extern float moverOwnerGravity;
extern float restSlowDamping;
extern float restMovingSpeed;
extern float restLinearLimit;
extern float restAngularLimit;
extern float restTiltLimit;
extern float restSettledLimit;
extern float restSettledAngularLimit;
extern float restStuckTime;
extern float restUpsideDown;
extern float restThrottle;
extern int raceModeIndex89E7F4;
extern float yawAssistRates[];
extern float yawAssistBlend[];
extern float yawAssistScales[];
extern DriveFeedbackCurve yawAssistCurveB;
extern DriveFeedbackCurve yawAssistCurveA;
extern EngineCurve yawResponseCurve;
extern float guideSpeedMinimum;
extern float guideSpeedMaximum;
extern float guideMinimumUp;
extern float moverNetworkControl;
extern float engineCosineC4;
extern float engineCosineC6;
extern float engineCosineC8;
extern float engineCosineC10;
extern DriveFeedbackCurve guideCurve;
extern float guideAlignScale;
extern float liftInputScale;
extern float liftMinimum;
extern float liftRate;
extern float liftTravel;
extern float liftBase;
extern float liftSpeedScale;
extern float networkPositionGain;
extern float networkVelocityGain;
extern double networkSpinThreshold;
extern WheelVector zeroVector;
extern int timerReference89E7B0;

// Original 0x440f70.
float WheelLength(WheelVector *input) {
    return (float)sqrt(input->x * input->x + input->y * input->y + input->z * input->z);
}

// Original 0x43cf90: rescale to a length; a zero vector becomes (length, 0, 0).
void WheelSetLength(WheelVector *output, WheelVector *input, float length) {
    float scale = (float)sqrt(input->x * input->x + input->y * input->y + input->z * input->z) / length;
    if (scale == 0.0f) {
        output->x = length;
        output->y = 0.0f;
        output->z = 0.0f;
        return;
    }
    float inverse = 1.0f / scale;
    float z = input->z;
    output->x = inverse * input->x;
    output->y = input->y * inverse;
    output->z = z * inverse;
}

// Original 0x43c390: polynomial sine and cosine of a 16-bit angle.
void WheelSineCosine(float *sine, float *cosine, unsigned short angle) {
    float sign = 1.0f;
    float x = (float)(int)angle * engineAngleUnit;
    if (!(x < engineThreeHalfPi)) {
        x = x - engineTwoPi;
    } else if (!(x < engineHalfPi)) {
        x = x - enginePi;
        sign = -1.0f;
    }
    float x2 = x * x;
    float x3 = x2 * x;
    volatile float x4 = x2 * x2;
    float x5 = x3 * x2;
    volatile float x6 = x4 * x2;
    float x7 = x5 * x2;
    volatile float x8 = x6 * x2;
    *sine = ((((x - x3 * engineSineC3) + x5 * engineSineC5) - x7 * engineSineC7) + x7 * x2 * engineSineC9) * sign;
    *cosine = ((((1.0f - 0.5f * x2) + x4 * engineCosineC4) - x6 * engineCosineC6) + x8 * engineCosineC8
               - x8 * x2 * engineCosineC10) * sign;
}

// Original car-object methods used by the mover.
bool MoverModeOwner::HasWheelInputs() {
    for (int i = 0; i < 4; ++i) {
        if (input30->wheelInputs1EC[i] > 0.0f) {
            return true;
        }
    }
    return false;
}

int MoverModeOwner::GetMoverWheelState(int wheel) {
    if (moverType28 == 1) {
        return mover34->value74[wheel];
    }
    return 0;
}

bool MoverModeOwner::IsResetPending() {
    if (resetTick974 == 0 || resetTick974 == 0x7fffffff) {
        return false;
    }
    return (float)(drivetrainClock - resetTick974) * drivetrainClockScale < resetDuration978;
}

int MoverModeOwner::HasContactBodies() {
    if (mode424 > 0) {
        return 1;
    }
    MoverMode *mode = mode3C;
    int count = mode->value18;
    for (int i = 0; i < count; ++i) {
        if (mode->GetBody(i)->IsTouching()) {
            return 1;
        }
    }
    return 0;
}

int MoverModeOwner::AreWheelsSettled() {
    for (int i = 0; i < 4; ++i) {
        if (State()->contacts350[i] && State()->IsWheelDriven(i)) {
            return 0;
        }
    }
    return 1;
}

void MoverModeOwner::RequestReset() {
    resetRequested96C = 1;
}

bool MoverOwnerObject::IsRunning() {
    return value10 != 0 && timerReference89E7B0 != 0;
}

// Original 0x5992b0: 0 moving, 1 slowing, 2 at rest.
int ClassifyMoverRest(MoverModeOwner *owner, MoverBody *body, int previous) {
    if (owner->HasWheelInputs()) {
        return 0;
    }
    for (int i = 0; i < 4; ++i) {
        if (owner->GetMoverWheelState(i) == 2 || owner->GetMoverWheelState(i) == 3) {
            return 0;
        }
    }
    int grounded = owner->mode424;
    if (owner->input30->throttle20C > restThrottle && owner->gear434 != 1 && grounded > 0
        && owner->value428 > 2) {
        return 0;
    }
    volatile float linear = WheelLength(&body->vectorA0);
    volatile float angular = WheelLength(&body->vectorC0);
    if (linear > restLinearLimit || angular > restAngularLimit) {
        return 0;
    }
    float tiltA = body->matrix30.elements[2];
    float tiltB = body->matrix30.elements[6];
    if (((tiltA < 0.0f ? -tiltA : tiltA) > restTiltLimit || (tiltB < 0.0f ? -tiltB : tiltB) > restTiltLimit)
        && grounded > 0) {
        return 0;
    }
    if (owner->IsResetPending()) {
        return 0;
    }
    if (owner->HasContactBodies() == 0 && previous != 2) {
        return 0;
    }
    if (linear > restSettledLimit || angular > restSettledAngularLimit) {
        return 1;
    }
    return 2;
}

// Original 0x5a5e20: damp or stop momentum while resting and request stuck resets.
int ApplyMoverRest(MoverModeOwner *owner, MoverBody *body, int *timer, int *state, VehicleMover *mover) {
    int rest = ClassifyMoverRest(owner, body, *state);
    *state = rest;
    if (rest == 0) {
        if (WheelLength(&body->vectorA0) > drivetrainSelectorUpperThreshold) {
            *timer = 0;
        }
        owner->mode3C->value19 = 1;
        body->value160 = owner->state38 == 1 ? moverOwnerGravity : bodyGravity;
        return 0;
    }
    if (rest == 1) {
        float x = restSlowDamping * body->vector70.x;
        float y = body->vector70.y * restSlowDamping;
        body->vector70.z = body->vector70.z * restSlowDamping;
        body->vector70.x = x;
        body->vector70.y = y;
        x = restSlowDamping * body->vector80.x;
        y = body->vector80.y * restSlowDamping;
        body->vector80.z = body->vector80.z * restSlowDamping;
        body->vector80.x = x;
        body->vector80.y = y;
        if (mover) {
            for (int i = 0; i < 4; ++i) {
                mover->wheels38[i]->value28 = restSlowDamping * mover->wheels38[i]->value28;
            }
        }
        if (WheelLength(&body->vectorA0) > drivetrainSelectorUpperThreshold) {
            *timer = 0;
        }
        owner->mode3C->value19 = 1;
        body->value160 = owner->state38 == 1 ? moverOwnerGravity : bodyGravity;
        return 1;
    }
    if (rest != 2) {
        return 0;
    }
    body->vector70.x = body->vector70.y = body->vector70.z = 0.0f;
    body->vector80.x = body->vector80.y = body->vector80.z = 0.0f;
    if (mover && owner->mode424 > 0) {
        for (int i = 0; i < 4; ++i) {
            mover->wheels38[i]->value28 = 0.0f;
        }
    }
    if (body->matrix30.elements[10] < restUpsideDown
        || (owner->context14->value4 == 1 && owner->AreWheelsSettled())) {
        int start = *timer;
        if (start == 0 || start == 0x7fffffff) {
            *timer = drivetrainClock;
        } else {
            int elapsed = drivetrainClock - start;
            bool blocked = owner->object1C && owner->object1C->IsRunning();
            if (mover && (float)elapsed * drivetrainClockScale > restStuckTime && !blocked) {
                owner->RequestReset();
            }
        }
    } else {
        *timer = 0;
    }
    owner->mode3C->value19 = 0;
    body->value160 = 0.0f;
    return 1;
}

// Original 0x5abb00: rest handling, bump stops and the scripted reset impulse.
void VehicleMover::ResolveRestAndBumpStops() {
    int changed = ApplyMoverRest(owner5C, body20, &value68, &value60, this);
    if (changed == 0) {
        for (int i = 0; i < 4; ++i) {
            MoverArm *arm = arms28[i];
            if (arm->travel1C == arm->spec14->upper14) {
                float impulse = arm->ApplyBumpStop();
                changed = 1;
                state58->accumulated330[i] = impulse + state58->accumulated330[i];
            }
        }
    }
    if (moverResetPulse != 0 && owner5C->value24 == 1) {
        moverResetPulse = 0;
        WheelVector point = {0.0f, 0.0f, 0.0f};
        WheelVector impulse = {0.0f, 0.0f, moverResetImpulse};
        body20->AddLimitedImpulse(&impulse, &point);
    } else if (changed == 0) {
        return;
    }
    engineSubstepDuration = 0.0f;
    Integrator()->UpdateBlocksAfterIntegration();
}

// Original 0x5a0d80, integrator base slot: run every block's post-integration callback.
void VehicleIntegrator::UpdateBlocksAfterIntegration() {
    IntegrationStateBlock *block = IntegrationStateBlock::FromLink(blocks.next);
    while (block != IntegrationStateBlock::FromLink(&blocks)) {
        block->UpdateAfterIntegration();
        block = IntegrationStateBlock::FromLink(block->link.next);
    }
}

// Original 0x5b04d0, PhysicsMover integrator slot 4: post-updates, rest pass, then forces.
void VehicleMover::FinishIntegration() {
    Integrator()->UpdateBlocksAfterIntegration();
    ResolveRestAndBumpStops();
    ApplyControllerForces();
}

// Original 0x5ac260: yaw assist torque and the rear wheel response coefficient.
void VehicleMover::ApplyYawAssist() {
    DrivetrainAuxiliary *state = state58;
    unsigned short heading = WheelAngle(state->velocity30.x, state->velocity30.y);
    volatile float slip = (float)(unsigned short)(state->heading3CC - heading) * engineAngleUnit;
    if (slip > wheelForcePi) {
        slip = slip - (wheelForcePi + wheelForcePi);
    }
    volatile float yaw = state58->angularD0.z;
    CarPhysicsRecord *record = record54;
    int variant = drivetrainFinishingInputThreshold <= record->drivetrain1B0.selector10;
    if (record->drivetrain1B0.selector10 > drivetrainSelectorUpperThreshold) {
        variant = 2;
    }
    volatile float rate = yawAssistRates[variant * 0x51 + raceModeIndex89E7F4];
    if (slip * yaw > 0.0f && owner5C->mode424 >= 2) {
        float magnitude = slip < 0.0f ? -slip : slip;
        float sample;
        if (owner5C->context14->value4 == 2) {
            yawAssistCurveB.Evaluate(&sample, magnitude);
        } else {
            yawAssistCurveA.Evaluate(&sample, magnitude);
        }
        float torque = sample * rate * yaw;
        WheelVector *axis = reinterpret_cast<WheelVector *>(&owner5C->State()->matrix50.elements[8]);
        volatile float y = axis->y * torque;
        volatile float z = axis->z * torque;
        body20->force120.x = torque * axis->x + body20->force120.x;
        body20->force120.y = y + body20->force120.y;
        body20->force120.z = z + body20->force120.z;
    }
    record = record54;
    variant = drivetrainFinishingInputThreshold <= record->drivetrain1B0.selector10;
    if (record->drivetrain1B0.selector10 > drivetrainSelectorUpperThreshold) {
        variant = 2;
    }
    volatile float blend = yawAssistBlend[variant * 0x51];
    float slipMagnitude = slip < 0.0f ? -slip : slip;
    float yawMagnitude = yaw < 0.0f ? -yaw : yaw;
    volatile float input = slipMagnitude * (1.0f - blend) + yawMagnitude * blend;
    variant = drivetrainFinishingInputThreshold <= record->drivetrain1B0.selector10;
    if (record->drivetrain1B0.selector10 > drivetrainSelectorUpperThreshold) {
        variant = 2;
    }
    float response = yawResponseCurve.Lookup(input) * yawAssistScales[variant * 0x51];
    wheels38[2]->coefficient188 = response;
    wheels38[3]->coefficient188 = response;
}

// Original 0x5a51b0: per-wheel lift impulses from the controls' four wheel inputs.
void VehicleMover::ApplyWheelLift() {
    if (owner5C->mode3C->value18 > 0) {
        return;
    }
    float *inputs = input4->wheelInputs1EC;
    int mask = 0;
    if (inputs[1] != 0.0f && inputs[0] != 0.0f) {
        mask = 0xc;
    } else if (inputs[2] != 0.0f && inputs[3] != 0.0f) {
        mask = 3;
    } else if ((inputs[2] != 0.0f && inputs[1] != 0.0f) || (inputs[3] != 0.0f && inputs[0] != 0.0f)) {
        mask = 0x20;
    }
    bool active = mask != 0;
    for (int i = 0; i < 4; ++i) {
        float target = input4->wheelTargets1FC[i] * liftInputScale;
        value74[i] = 0;
        int masked = (1 << i) & mask;
        if (masked && !(target > liftMinimum)) {
            target = liftMinimum;
        }
        float *offset = &state58->rideOffsets3B8[i];
        if (target > *offset) {
            volatile float next = engineSubstepDuration * liftRate + *offset;
            if (!(target < next)) {
                target = next;
            }
            *offset = target;
            value74[i] = 2;
        } else if (!(target >= *offset)) {
            float next = *offset - engineSubstepDuration * liftRate;
            if (!(0.0f <= next)) {
                next = 0.0f;
            }
            *offset = next;
            value74[i] = 3;
        } else if (target > 0.0f) {
            value74[i] = 1;
        }
        if (masked && !(state58->rideOffsets3B8[i] >= liftMinimum)) {
            active = false;
        }
    }
    if (!active) {
        return;
    }
    for (int i = 0; i < 4; ++i) {
        if (input4->wheelInputs1EC[i] > 0.0f && arms28[i]->travel1C < liftTravel) {
            return;
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (!(input4->wheelInputs1EC[i] > 0.0f)) {
            continue;
        }
        WheelVector *normal = &wheels38[i]->vector150;
        WheelVector offset, spin;
        WheelTransform(&offset, &body20->matrix30, &arms28[i]->position30);
        MoverBody *body = body20;
        WheelCross(&spin, &body->vectorC0, &offset);
        float vx = spin.x + body->vectorA0.x;
        volatile float vy = body->vectorA0.y + spin.y;
        volatile float vz = body->vectorA0.z + spin.z;
        float closing = vx * normal->x + vz * normal->z + vy * normal->y;
        if (closing < 0.0f) {
            continue;
        }
        float impulse = (liftBase - closing * liftSpeedScale) * input4->wheelInputs1EC[i];
        if (!(impulse > 0.0f)) {
            continue;
        }
        impulse = impulse * body20->divisor164;
        WheelVector applied;
        applied.x = impulse * normal->x;
        applied.y = impulse * normal->y;
        applied.z = impulse * normal->z;
        body20->AddLimitedImpulse(&applied, &offset);
        state58->assisted3C8[i] = 1;
    }
}

// Original 0x5a5c20: remote-state correction used only when flag +0x84 is set.
void VehicleMover::ApplyNetworkCorrection() {
    MoverBody *body = body20;
    WheelVector velocity = zeroVector;
    if (body->saved130.x * body->vectorA0.x + body->saved130.y * body->vectorA0.y
        + body->saved130.z * body->vectorA0.z > 0.0f) {
        velocity = body->saved130;
    }
    WheelVector error;
    error.x = vector90.x - vectorE0.x;
    error.y = vector90.y - vectorE0.y;
    error.z = vector90.z - vectorE0.z;
    WheelScaleAdd(&velocity, &velocity, &error, networkPositionGain);
    WheelScaleAdd(&velocity, &velocity, &vectorF0, -networkVelocityGain);
    WheelScaleAdd(&vectorF0, &vectorF0, &velocity, body20->value168 * engineSubstepDuration);
    WheelScaleAdd(&vectorE0, &vectorE0, &vectorF0, engineSubstepDuration);
    body = body20;
    vectorE0.z = body->position20.z;
    body->position20.x = vectorE0.x;
    body->position20.y = vectorE0.y;
    body->position20.z = vectorE0.z;
    for (int i = 0; i < 4; ++i) {
        DrivetrainWheel *wheel = wheels38[i];
        int axle = wheel->spec14->value18;
        float selector = wheel->auxiliary20->record0->drivetrain1B0.selector10;
        bool driven;
        if (!(selector >= drivetrainFinishingInputThreshold)) {
            driven = axle == 1 || axle == 0;
        } else if (selector > drivetrainSelectorUpperThreshold) {
            driven = axle == 2 || axle == 3;
        } else {
            driven = true;
        }
        if (driven) {
            continue;
        }
        MoverBody *target = body20;
        float speed = target->matrix30.elements[0] * vectorF0.x + target->matrix30.elements[1] * vectorF0.y
                    + target->matrix30.elements[2] * vectorF0.z;
        if ((speed < 0.0f ? -speed : speed) < networkSpinThreshold) {
            speed = 0.0f;
        }
        wheel->value28 = speed / wheel->spec14->value10;
    }
}

// Original 0x5ecf70: guide force toward the heading requested by the controls.
void MoverModeOwner::ApplyGuideForce() {
    if (!mover34 || !input30 || !object1C) {
        return;
    }
    MoverBody *body = mover34->GetBody();
    if (!body) {
        return;
    }
    DrivetrainAuxiliary *state = State();
    float speed = state->value3F0;
    if (!(EngineClamp(speed, guideSpeedMinimum, guideSpeedMaximum) == speed)) {
        return;
    }
    if (state->matrix50.elements[10] < guideMinimumUp || mode424 < 3) {
        return;
    }
    unsigned short heading = input30->GetGuideHeading();
    if (heading == 0) {
        return;
    }
    volatile float spin = WheelLength(&body->vectorC0);
    volatile float mass = body->divisor164;
    WheelVector damping;
    WheelScale(&damping, &body->vectorC0, -(guideCurve.Lookup(spin) * mass));
    body->AddForce(&damping);
    float sine, cosine;
    WheelSineCosine(&sine, &cosine, heading);
    float *m = state->matrix50.elements;
    WheelVector current, target, axis;
    current.x = state->velocity30.x;
    current.z = 0.0f;
    target.x = sine * m[4] + cosine * m[0] + m[8] * 0.0f + m[12];
    target.y = sine * m[5] + cosine * m[1] + m[9] * 0.0f + m[13];
    volatile float z = sine * m[6] + cosine * m[2] + m[10] * 0.0f + m[14];
    target.z = z;
    current.y = state->velocity30.y;
    WheelNormalize(&current, &current);
    WheelCross(&axis, &current, &target);
    float alignment = current.z * target.z + current.x * target.x + current.y * target.y;
    WheelSetLength(&axis, &axis, (alignment - 1.0f) * body->divisor164 * guideAlignScale);
    body->AddForce(&axis);
}

// Original 0x5b0520, PhysicsMover integrator slot 8: contacts, coupling and body forces.
void VehicleMover::ApplyControllerForces() {
    if (flag84 && owner5C->object1C->link1E0->value4 == 2) {
        engine48->control80 = moverNetworkControl;
    }
    reinterpret_cast<IntegrationStateBlock *>(body20)->UnknownSlot20();
    float drag;
    aero50->Apply(&drag);
    for (int i = 0; i < 4; ++i) {
        MoverArm *arm = arms28[i];
        arm->valueB0 = arm->wheelA0->SetContact(arm->contact80, arm->force24);
    }
    CorrectWheelCoupling();
    for (int i = 0; i < 4; ++i) {
        MoverArm *arm = arms28[i];
        if (arm->contact80) {
            DrivetrainWheel *wheel = arm->wheelA0;
            MoverBody *body = arm->bodyA4;
            WheelVector point;
            volatile float y = wheel->contactPoint140.y - body->position20.y;
            volatile float z = wheel->contactPoint140.z - body->position20.z;
            point.x = wheel->contactPoint140.x - body->position20.x;
            point.y = y;
            point.z = z;
            if (!moverImpulsesDisabled) {
                body->ApplyForceAtPoint(&wheel->vector130, &point);
            }
        }
    }
    ApplyOpposingBodyForce();
    if (owner5C) {
        owner5C->ApplyGuideForce();
    }
    if (IsModeGroup3()) {
        ApplyYawAssist();
    }
    ApplyWheelLift();
    if (flag84) {
        ApplyNetworkCorrection();
    }
}
