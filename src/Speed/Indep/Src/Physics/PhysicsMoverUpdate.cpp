#include "VehicleMover.h"
#include "VehicleIntegrator.h"
#include "GameModeFlags.h"
#include "Engine.h"
#include "DriveFeedbackCurve.h"

extern float engineSubstepDuration;
extern float engineAngleUnit;
extern float wheelForcePi;
extern float bodyGravity;
extern float drivetrainFinishingInputThreshold;
extern float drivetrainSelectorUpperThreshold;
extern float drivetrainSynchronizationScale;
extern int drivetrainClock;
extern float drivetrainClockScale;
extern float engineValueScale;
extern float engineSpeedOverride;
extern float engineSpeedOverrideUnset;
extern WheelVector zeroVector;
extern float moverSubstepLength;
extern int moverCentreFromRecord;
extern float moverWheelInertia;
extern float moverCentreBlend;
extern float moverOwnerGravity;
extern float moverDriftCap;
extern float moverDriftThrottle;
extern float moverStabilityThrottle;
extern float moverStabilityTilt;
extern float moverStabilitySlip;
extern float moverStabilityLimit;
extern float moverStabilitySpeed;
extern float moverAwdSpeed;
extern int moverResponseEnabled;
extern float moverResponseMaskScale;
extern float moverLowGripScale;
extern float moverHandbrakeRise;
extern float moverHandbrakeFall;
extern float moverHandbrakeMinimum;
extern float moverDraftGrip;
extern float moverLaunchThrottle;
extern float moverLaunchBrake;
extern float moverLaunchSpeed;
extern float moverLaunchBrakeScale;
extern float moverHandbrakeTable[4];
extern float moverHandbrakeBoost;
extern float moverHandbrakeSlip;
extern float moverHandbrakeSpeed;
extern float moverNitrousGrip[];
extern float moverTractionHandbrake;
extern float moverTractionSlip;
extern float moverTractionSpeedLow;
extern float moverTractionSpeedHigh;
extern float moverTractionScaleHigh;
extern float moverTractionScaleLow;
extern float moverTractionCap;
extern float moverThrottleDeadZone;
extern float moverLaunchRpm;
extern float moverQuarterScale;
extern int effectsSequence89E7A0;
extern DriveFeedbackCurve moverEventCurve;
extern float moverEventDuration;
extern ReplayRecorder replayRecorder;
extern DrivetrainTuning drivetrainTuningRows[];
extern float launchSlipLimit;
extern float launchWheelLimit;
extern float launchSettleTime;
extern float launchPulseTime;
extern float launchSpeedLimit;
extern float launchLoadDivisor;
extern DriveFeedbackCurve launchCurve;
extern float launchCountBase;
extern int launchCountScale;
extern float launchPulseShare;
extern float launchGripScale;
extern float frameDuration865154;
extern float angleDegreesToRadians;

unsigned int bGetTicker();

// Original 0x591d90: velocity heading relative to the stored body heading.
float DrivetrainAuxiliary::GetSlipAngle() {
    unsigned short heading = WheelAngle(velocity30.x, velocity30.y);
    float angle = (float)(unsigned short)(heading3CC - heading) * engineAngleUnit;
    if (angle > wheelForcePi) {
        angle = angle - (wheelForcePi + wheelForcePi);
    }
    return angle;
}

// Original 0x5ec790: event curve sampled for four seconds after mode +0x140 is stamped.
float MoverModeOwner::GetEventCurveValue() {
    int stamp = collisionBody3C->tick140;
    if (stamp != 0 && stamp != 0x7fffffff) {
        volatile float elapsed = (float)(drivetrainClock - stamp) * drivetrainClockScale;
        if (elapsed < moverEventDuration) {
            return moverEventCurve.Lookup(elapsed);
        }
    }
    return 0.0f;
}

// Original 0x5990c0: rear grip factor while sliding.
float ComputeTractionFactor(float slip, float handbrake, float speed, float base, float extra,
                            int grounded, float *tilt, float event) {
    *tilt = *tilt < 0.0f ? -*tilt : *tilt;
    float result = *tilt + 1.0f;
    float magnitude = slip < 0.0f ? -slip : slip;
    float coefficient = grounded ? extra * event + base : base + extra;
    if (handbrake > moverTractionHandbrake && !(magnitude >= moverTractionSlip * angleDegreesToRadians)) {
        return result;
    }
    float scale = (speed - moverTractionSpeedLow) / (moverTractionSpeedHigh - moverTractionSpeedLow)
                * (moverTractionScaleHigh - moverTractionScaleLow) + moverTractionScaleLow;
    float bonus = coefficient * (magnitude * scale);
    if (bonus > moverTractionCap) {
        bonus = moverTractionCap;
    }
    return result + bonus;
}

// Original 0x5a60f0: centre-of-mass target from mode state or the car record.
int UpdateMoverCentre(MoverModeOwner *owner, MoverBody *body, unsigned char *pending) {
    WheelVector centre;
    if (owner->state38 == 1) {
        CarCollisionBody *collision = owner->collisionBody3C;
        float recordZ = owner->record20->centre0.z;
        float target = (collision->upperD4 - collision->lowerD0) * moverCentreBlend + collision->lowerD0;
        centre.y = 0.0f;
        centre.x = 0.0f;
        volatile float z = target - (target - recordZ) * ((float)owner->mode424 * moverQuarterScale);
        centre.z = z;
        if (centre.z == body->centre1F0.z) {
            return 0;
        }
        *pending = 1;
    } else {
        if (!*pending) {
            return 0;
        }
        centre = owner->record20->centre0;
        *pending = 0;
    }
    body->MoveCentre(&centre);
    return 1;
}

// Original 0x599170: response scale shared by engine, body, aero and wheels.
void VehicleMover::SetResponseScale(float scale) {
    if (!moverResponseEnabled) {
        scale = 1.0f;
    }
    float engineScale = scale;
    if (owner5C->mask95C && owner5C->collisionBody3C->events18) {
        engineScale = moverResponseMaskScale * scale;
    }
    engine48->coefficient90 = engineScale;
    body20->value208 = scale;
    aero50->scale8 = scale;
    aero50->valueC = 1.0f - owner5C->value4DC;
    wheels38[0]->coefficient184 = scale;
    wheels38[1]->coefficient184 = scale;
    wheels38[2]->coefficient184 = scale;
    wheels38[3]->coefficient184 = scale;
}

// Original 0x5ab990: centre/mass refresh and static axle preload.
void VehicleMover::RefreshLoads() {
    UpdateMoverCentre(owner5C, body20, &flag64);
    body20->RefreshParameters();
    float centre = body20->centre1F0.x;
    if (!moverCentreFromRecord) {
        centre = 0.0f;
    }
    CarPhysicsRecord *record = record54;
    float front = record->wheels10[0].position0.x - centre;
    volatile float rearShare = front / (front - (record->wheels10[3].position0.x - centre)) * 0.5f;
    float frontShare = 0.5f - rearShare;
    float weight = body20->divisor164 * bodyGravity;
    float speed = owner5C->speed42C;
    if (!(0.0f <= speed)) {
        speed = 0.0f;
    }
    MoverAeroSpec *aero = aero50->spec0;
    volatile float total = (aero->coefficient8 + aero->coefficient4) * aero50->value30 * speed
                         * record->value26C + weight;
    volatile float frontLoad = total * frontShare;
    arms28[0]->SetMount(frontLoad, state58->rideOffsets3B8[0]);
    arms28[1]->SetMount(frontLoad, state58->rideOffsets3B8[1]);
    volatile float rearLoad = total * rearShare;
    arms28[2]->SetMount(rearLoad, state58->rideOffsets3B8[2]);
    arms28[3]->SetMount(rearLoad, state58->rideOffsets3B8[3]);
    wheels38[0]->divisor2C = moverWheelInertia;
    wheels38[1]->divisor2C = moverWheelInertia;
    wheels38[2]->divisor2C = moverWheelInertia;
    wheels38[3]->divisor2C = moverWheelInertia;
}

// Original 0x5a49a0: surface grip, owner state modes, stability limits and drive bias.
void VehicleMover::RefreshModes() {
    DrivetrainAuxiliary *state = state58;
    float grip = 1.0f;
    int grounded = state->value3E4;
    if (grounded > 0) {
        float a = state->contacts350[0] ? roadSurfaces[state->surfaces354[0]].brakeScale30 : 0.0f;
        float b = state->contacts350[1] ? roadSurfaces[state->surfaces354[1]].brakeScale30 : 0.0f;
        float sum = a + b;
        float c = state->contacts350[2] ? roadSurfaces[state->surfaces354[2]].brakeScale30 : 0.0f;
        sum = sum + c;
        float d = state->contacts350[3] ? roadSurfaces[state->surfaces354[3]].brakeScale30 : 0.0f;
        grip = (sum + d) / grounded;
    }
    int mode = 0;
    switch (owner5C->state38) {
    case 1: {
        body20->flag150 = 0;
        body20->flag154 = 0;
        body20->value160 = moverOwnerGravity;
        aero50->value30 = 0.0f;
        aero50->value34 = 1.0f;
        float throttle = input4->throttle20C;
        if (moverDriftCap >= throttle) {
            input4->SetThrottle(throttle);
        } else {
            input4->SetThrottle(moverDriftCap);
        }
        mode = 0;
        break;
    }
    case 0:
    case 2:
        body20->flag150 = 1;
        body20->flag154 = 1;
        body20->value160 = 9.8f;
        aero50->value30 = flag84 ? 0.0f : 1.0f;
        mode = 1;
        aero50->value34 = grip;
        break;
    }
    if (owner5C->mode424 == 0) {
        mode = 0;
    }
    MoverBody *body = body20;
    if (body->flag158 != mode) {
        if (mode == 0) {
            body->vector80.z = body->vector80.z / body->spec14->inertiaScale50;
        }
        body->flag158 = mode;
    }
    if (*raceState890118 == 4) {
        Engine *engine = engine48;
        float adjustment;
        if (IsModeGroup2()) {
            adjustment = engine->adjustment8C;
            if (!(0.0f <= adjustment)) {
                adjustment = 0.0f;
            }
        } else {
            adjustment = -1.0f;
        }
        engine->adjustment8C = adjustment;
        if (input4->throttle20C > moverStabilityThrottle) {
            body20->flag150 = 0;
        } else if (state58->value3E4 == 4) {
            float tilt = state58->matrix50.elements[2];
            if ((tilt < 0.0f ? -tilt : tilt) > moverStabilityTilt) {
                body20->flag150 = 0;
            }
        }
    }
    float scale = owner5C->value4E4;
    if (scale == 0.0f || owner5C->mask95C) {
        scale = owner5C->value468;
    }
    SetResponseScale(scale);
    if (flag84) {
        body20->limit200 = 1.0e-14f;
    } else if (state58->mode424 == 0) {
        body20->limit200 = record54->value288;
    } else {
        float limit = record54->value28C;
        if (limit == 0.0f) {
            MoverBody *target = body20;
            float slip = state58->value3EC >= 1.0f ? state58->GetSlipAngle() : 0.0f;
            if (!(slip * target->vector80.z >= 0.0f)) {
                if ((slip < 0.0f ? -slip : slip) > moverStabilitySlip) {
                    limit = moverStabilityLimit;
                }
            }
        }
        body20->limit200 = limit;
    }
    if (flag84) {
        body20->value204 = 0.0f;
    } else {
        body20->value204 = record54->value290;
    }
    if (record54->drivetrain1B0.selector10 > drivetrainSelectorUpperThreshold) {
        if (owner5C->speed430 * drivetrainSynchronizationScale < moverAwdSpeed
            && owner5C->collisionBody3C->wallContacts120 > 0) {
            drivetrain4C->coefficientEC = 0.65f;
            return;
        }
        drivetrain4C->coefficientEC = record54->drivetrain1B0.selector10;
    }
}

// Original 0x5abbf0: driver controls into steering, engine and gearbox.
void VehicleMover::TransferControls() {
    if (owner5C->gear434 != 1 && !IsModeGroupE2()) {
        MoverInput *input = input4;
        if (input->throttle20C > input->brake214 && input->handbrake210 < input->throttle20C) {
            if (input->brake214 < 0.5f) {
                input->SetBrake(0.0f);
            }
            if (input4->handbrake210 < 0.5f) {
                input4->SetHandbrake(0.0f);
            }
        } else if (input->throttle20C < 0.5f) {
            input->SetThrottle(0.0f);
        }
    }
    MoverInput *input = input4;
    float throttle = input->throttle20C;
    if (!(throttle > moverThrottleDeadZone)) {
        throttle = 0.0f;
    } else if (throttle > 0.0f && drivetrain4C->ratioIndex > 1
               && engine48->state14 * engineValueScale < moverLaunchRpm && !(0.5f <= throttle)) {
        throttle = 0.5f;
    }
    steering24->SetAngle(input->steering1E0);
    engine48->input78 = throttle;
    int direct = input4->GetDirectGear();
    Drivetrain *drivetrain = drivetrain4C;
    if (direct > 1 && direct <= drivetrain->spec->unidentified18 + 1) {
        if (drivetrain->value54 == 1) {
            drivetrain->value50 = 0.0f;
        }
        drivetrain->value54 = direct;
    }
    engine48->index84 = state58->GetNitrousIndex();
    float limit = engineSpeedOverride;
    if (engineSpeedOverride == engineSpeedOverrideUnset) {
        limit = owner5C->engineValue440;
    }
    MoverInput *controls = input4;
    int request = controls->GetGearRequest();
    controls->SetSelectedGear(drivetrain4C->SelectGear(request, limit));
    input4->SetCurrentGear(drivetrain4C->value54);
    aero50->control10 = throttle;
    aero50->grounded14 = (float)state58->value3E4 * moverQuarterScale;
}

// Original 0x5a4d80: per-wheel brake, handbrake and grip coefficients.
void VehicleMover::DistributeBrakes(float duration) {
    volatile float grip = value6C;
    volatile float speed = state58->value3EC * drivetrainSynchronizationScale;
    if (state58->mode424 == 0 && !IsModeGroup3()) {
        grip = moverLowGripScale * grip;
    }
    float ramp;
    if (input4->handbrake210 < 0.5f) {
        ramp = moverHandbrakeRise * duration + value70;
        if (ramp > 1.0f) {
            ramp = 1.0f;
        }
    } else {
        ramp = value70 - moverHandbrakeFall * duration;
        if (!(ramp >= moverHandbrakeMinimum)) {
            ramp = moverHandbrakeMinimum;
        }
    }
    value70 = ramp;
    float draft = owner5C->GetDraftValue();
    grip = (1.0f - (1.0f - moverDraftGrip) * draft) * grip;
    for (int i = 0; i < 4; ++i) {
        DrivetrainWheel *wheel = wheels38[i];
        MoverInput *input = input4;
        if (input->throttle20C > moverLaunchThrottle && input->brake214 > moverLaunchBrake
            && speed < moverLaunchSpeed && i >= 2) {
            wheel->coefficient178 = speed * moverLaunchBrakeScale;
        } else {
            float bias = record54->brakeBias27C;
            if (i < 2) {
                bias = 1.0f - bias;
            }
            wheel->coefficient178 = (bias + bias) * input->brake214;
        }
        volatile float handbrake = moverHandbrakeTable[i] * input4->handbrake210;
        float applied = handbrake;
        if (state58->mode424 == 0) {
            if (applied > drivetrainFinishingInputThreshold) {
                applied = applied + 0.5f;
            }
        } else if (applied > drivetrainFinishingInputThreshold) {
            float slip = state58->value3EC >= 1.0f ? state58->GetSlipAngle() : 0.0f;
            applied = handbrake;
            if ((slip < 0.0f ? -slip : slip) > moverHandbrakeSlip && speed < moverHandbrakeSpeed) {
                applied = applied + 0.5f;
            }
        }
        wheel->coefficient17C = applied;
        float wheelGrip = grip;
        if (!IsModeGroup3() && !IsModeGroupE2() && i >= 2) {
            float tilt = body20->matrix30.elements[2];
            float vehicleSpeed = state58->value3F0;
            int groundedMode = state58->mode424;
            CarPhysicsRecord *record = record54;
            float wheelHandbrake = wheel->coefficient17C;
            volatile float slip = state58->value3EC >= 1.0f ? state58->GetSlipAngle() : 0.0f;
            float event = owner5C->GetEventCurveValue();
            float factor = ComputeTractionFactor(slip, wheelHandbrake, vehicleSpeed, record->value280,
                                                 record->value284, groundedMode, &tilt, event);
            wheelGrip = ((factor - 1.0f) * ramp + 1.0f) * grip;
        }
        wheel->coefficient180 = wheelGrip;
        if ((1 << i) & owner5C->mask95C) {
            wheel->value174 = 1.0f;
        } else {
            wheel->value174 = 0.0f;
        }
        int nitrous = state58->GetNitrousIndex();
        if (nitrous > 0) {
            wheel->coefficient180 = moverNitrousGrip[nitrous] * wheel->coefficient180;
        }
        if (flag84) {
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
            if (!driven && input4->throttle20C == 1.0f) {
                wheel->coefficient180 = wheel->coefficient180 * 0.0f;
            }
        }
    }
}

// Original PhysicsMover vtable slot 4 at 0x5b72d0: pre-steps, fixed substeps and post-step.
float VehicleMover::Update(float duration) {
    bGetTicker();
    DrivetrainAuxiliary *state = state58;
    state->assisted3C8[0] = 0;
    state58->assisted3C8[1] = 0;
    state58->assisted3C8[2] = 0;
    state58->assisted3C8[3] = 0;
    float *accumulated = state58->accumulated330;
    accumulated[0] = 0.0f;
    accumulated[1] = 0.0f;
    accumulated[2] = 0.0f;
    accumulated[3] = 0.0f;
    RefreshLoads();
    RefreshModes();
    TransferControls();
    DistributeBrakes(duration);
    ApplyLaunchAssist();
    int count = (int)(duration / moverSubstepLength) + 1;
    volatile float steps = (float)count;
    volatile float step = duration / steps;
    sum100 = zeroVector;
    sum110 = zeroVector;
    for (int i = count; i > 0; --i) {
        engineSubstepDuration = step;
        Integrator()->Integrate(step);
        MoverBody *body = body20;
        sum100.x = sum100.x + body->saved130.x;
        sum100.y = body->saved130.y + sum100.y;
        sum100.z = body->saved130.z + sum100.z;
        body = body20;
        sum110.x = body->saved140.x + sum110.x;
        sum110.y = body->saved140.y + sum110.y;
        sum110.z = body->saved140.z + sum110.z;
        WheelOrthonormalize(&body20->matrix30, &body20->matrix30);
    }
    float scale = 1.0f / steps;
    sum100.x = scale * sum100.x;
    sum100.y = sum100.y * scale;
    sum100.z = sum100.z * scale;
    sum110.x = scale * sum110.x;
    sum110.y = sum110.y * scale;
    sum110.z = sum110.z * scale;
    ExportState(duration);
    if ((effectsSequence89E7A0 < 0x1130 || effectsSequence89E7A0 >= 0x1194) && !flag84
        && owner5C->HasWheelEffects()) {
        for (int i = 0; i < 4; ++i) {
            wheels38[i]->UpdateEffects(duration, owner5C);
        }
    }
    MoverContext *context = owner5C->context14;
    if (context->value4 == 1 && !context->flag900 && replayRecorder.QueryState5EFE70()) {
        replayRecorder.QueryState5F00F0();
    }
    return duration;
}

// Original 0x5b6300: shift-adjustment grip boost and launch traction pulses.
void VehicleMover::ApplyLaunchAssist() {
    int value = owner5C->value24;
    bool active = (value == 1 || value == 2 || value == 3) && !owner5C->IsResetPending()
               && owner5C->speed42C > 0.0f && !IsModeGroupE2();
    CarPhysicsRecord *record = record54;
    int variant = drivetrainFinishingInputThreshold <= record->drivetrain1B0.selector10;
    if (record->drivetrain1B0.selector10 > drivetrainSelectorUpperThreshold) {
        variant = 2;
    }
    PhysicsInfo *info = owner5C->physicsInfo10;
    if (info->index14 == -1) {
        info->Refresh(1, 1, 1);
    }
    volatile float rate = reinterpret_cast<float *>(&drivetrainTuningRows[0])[variant * 0x51 + info->index14];
    if (drivetrain4C->adjustment68 > 0.0f) {
        float inverse = 1.0f / rate;
        for (int i = 0; i < 4; ++i) {
            float boost = inverse * drivetrain4C->adjustment68;
            if (!(1.0f >= boost)) {
                boost = 1.0f;
            }
            wheels38[i]->coefficient180 = (boost + 1.0f) * wheels38[i]->coefficient180;
        }
    }
    if (!active) {
        return;
    }
    DrivetrainAuxiliary *state = state58;
    float speed = state->value3EC * drivetrainSynchronizationScale;
    if (state->selector42C != 0) {
        float slip = state->value3EC >= 1.0f ? state->GetSlipAngle() : 0.0f;
        if ((slip < 0.0f ? -slip : slip) > launchSlipLimit) {
            state58->selector42C = 0;
        } else if (wheels38[0]->value118 < launchWheelLimit && wheels38[1]->value118 < launchWheelLimit
                   && wheels38[2]->value118 < launchWheelLimit && wheels38[3]->value118 < launchWheelLimit) {
            float timer = frameDuration865154 + state58->value438;
            state58->value438 = timer;
            if (timer > launchSettleTime) {
                state58->selector42C = 0;
            }
        } else {
            state58->value438 = 0.0f;
            float timer = state58->value430 - frameDuration865154;
            state58->value430 = timer;
            if (!(timer >= 0.0f)) {
                state58->selector42C = state58->selector42C - 1;
                state58->value430 = launchPulseTime;
            }
        }
    } else if (*raceState890118 == 4 && speed < launchSpeedLimit) {
        volatile float scale = 1.0f / launchLoadDivisor;
        volatile float best = 0.0f;
        int index = 0;
        for (int i = 0; i < 4; ++i) {
            DrivetrainWheel *wheel = wheels38[i];
            volatile float scaled = wheel->value164 * scale * wheel->value118;
            float candidate = wheel->value118 < scaled ? wheel->value118 : scaled;
            if (i == 0 ? candidate > 0.0f : candidate > best) {
                best = candidate;
                index = i;
            }
        }
        if (best > 0.5f) {
            float sample;
            launchCurve.Evaluate(&sample, best);
            state58->value434 = 1.0f / rate * sample;
            int parity = index & 1;
            state58->selector42C = (int)((launchCountBase - sample) * launchCountScale + parity);
            state58->value430 = launchPulseTime;
            state58->value438 = 0.0f;
        }
    }
    if (state58->selector42C == 0) {
        return;
    }
    for (int i = 0; i < 4; ++i) {
        DrivetrainWheel *wheel = wheels38[i];
        wheel->coefficient180 = launchGripScale * wheel->coefficient180;
        float share = 1.0f - state58->value434;
        int remainder = state58->selector42C % 2;
        bool low = (i == 0 || i == 3) ? remainder == 0 : remainder == 1;
        share = low ? share * launchPulseShare : (1.0f - launchPulseShare) * share;
        wheel->coefficient180 = (1.0f - share) * wheel->coefficient180;
    }
}
