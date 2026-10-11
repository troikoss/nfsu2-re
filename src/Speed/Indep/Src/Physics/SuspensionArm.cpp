#include "VehicleMover.h"
#include "Engine.h"
#include "../../Libs/bWare/Strings.h"

extern float engineSubstepDuration;
extern float drivetrainComponentEpsilon;
extern float suspensionSteerScale;
extern float suspensionBumpScale;
extern int moverImpulsesDisabled;

// Constructor body of original 0x5a86c0 ("Suspnsn"); vtable 0x7a210c is installed by the caller.
void MoverArm::Construct(int index, MoverBody *body, DrivetrainWheel *wheel,
                         AckermannSteering *steering, SuspensionSpec *spec) {
    bodyA4 = body;
    indexAC = index;
    steeringA8 = steering;
    wheelA0 = wheel;
    spec14 = spec;
    pair18 = 0;
    nfs_strcpy(name0C, "Suspnsn");
}

// Original 0x59e0c0: mount position relative to the body centre, lowered by offset.
void MoverArm::SetMount(float extra, float offset) {
    extra88 = extra;
    WheelVector *wheelPosition = &wheelA0->spec14->position0;
    MoverBody *body = bodyA4;
    float y = wheelPosition->y;
    float bodyY = body->centre1F0.y;
    float z = wheelPosition->z;
    float bodyZ = body->centre1F0.z;
    value90 = wheelPosition->x - body->centre1F0.x;
    value98 = z - bodyZ;
    value94 = y - bodyY;
    value98 = value98 - offset;
}

// Original 0x59e130, vtable slot 0x18: suspension states have zero derivatives.
void MoverArm::WriteDerivative(float *output) {
    output[0] = 0.0f;
    output[1] = 0.0f;
}

// Original 0x5a8790, vtable slot 0x1c: ground travel, wheel frames and spring force.
void MoverArm::UpdateAfterIntegration() {
    WheelVector world;
    WheelTransform(&world, &bodyA4->matrix30, reinterpret_cast<WheelVector *>(&value90));
    volatile float worldX = world.x + bodyA4->position20.x;
    volatile float worldY = bodyA4->position20.y + world.y;
    volatile float worldZ = bodyA4->position20.z + world.z;
    world.x = worldX;
    world.y = worldY;
    world.z = worldZ;
    float candidate = wheelA0->QueryGround(&wheelA0->vector150, &wheelA0->surface114, &world,
                                           &bodyA4->matrix30)
                    + wheelA0->spec14->value10;
    SuspensionSpec *spec = spec14;
    float previous = travel1C;
    if (candidate > spec->upper14) {
        travel1C = spec->upper14;
        contact80 = 1;
    } else if (!(candidate >= spec->lower18)) {
        travel1C = spec->lower18;
        contact80 = 0;
    } else {
        travel1C = candidate;
        contact80 = 1;
    }
    if (engineSubstepDuration > drivetrainComponentEpsilon) {
        rate20 = (travel1C - previous) / engineSubstepDuration;
    }
    position30.x = value90;
    position30.y = value94;
    position30.z = value98;
    position30.z = travel1C + position30.z;
    WheelMatrix local;
    WheelIdentity(&local);
    float steer = wheelA0->value174;
    if (steer != 0.0f) {
        WheelMatrix rotation;
        WheelIdentity(&rotation);
        volatile float angle = EngineSine(wheelA0->angle24) * suspensionSteerScale * steer;
        float cosine = EngineSine(angle + engineHalfPi);
        float sine = EngineSine(angle);
        rotation.elements[4] = -sine;
        rotation.elements[0] = cosine;
        rotation.elements[1] = sine;
        rotation.elements[5] = cosine;
        WheelMultiply(&local, &local, &rotation);
    }
    local.elements[13] = position30.y;
    local.elements[14] = position30.z;
    local.elements[12] = position30.x;
    local.elements[15] = 1.0f;
    BodyMatrixCopy(&matrix40, &local);
    if (steeringA8) {
        WheelMultiply(&local, &local, &steeringA8->matrices8[indexAC]);
    }
    wheelA0->SetLocalMatrix(&local);
    WheelVector offset, velocity;
    WheelTransform(&offset, &bodyA4->matrix30, &position30);
    WheelCross(&velocity, &bodyA4->vectorC0, &offset);
    wheelA0->vectorF0.x = velocity.x + bodyA4->vectorA0.x;
    wheelA0->vectorF0.y = bodyA4->vectorA0.y + velocity.y;
    wheelA0->vectorF0.z = bodyA4->vectorA0.z + velocity.z;
    WheelMultiply(&local, &bodyA4->matrixD0, &local);
    wheelA0->SetWorldMatrix(&local);
    if (contact80) {
        float compression = travel1C > 0.0f ? travel1C : 0.0f;
        int damper = rate20 <= 0.0f ? 1 : 0;
        float force = spec->damping8[damper] * rate20
                    + (compression * spec->progression0 + 1.0f) * spec->stiffness4 * travel1C
                    + extra88;
        force24 = force;
        force24 = (travel1C - pair18->travel1C) * spec->antiRoll10 + force;
        if (force24 >= 0.0f) {
            return;
        }
    }
    force24 = 0.0f;
}

// Original 0x59e1a0: bottomed-out impulse along the contact normal.
float MoverArm::ApplyBumpStop() {
    WheelVector offset, spin;
    WheelTransform(&offset, &bodyA4->matrix30, &position30);
    MoverBody *body = bodyA4;
    WheelCross(&spin, &body->vectorC0, &offset);
    volatile float velocityX = spin.x + body->vectorA0.x;
    float velocityY = body->vectorA0.y + spin.y;
    volatile float velocityZ = body->vectorA0.z + spin.z;
    WheelVector *normal = &wheelA0->vector150;
    volatile float normalX = normal->x;
    volatile float normalY = normal->y;
    volatile float normalZ = normal->z;
    float closing = -(normalZ * velocityZ + normalY * velocityY + normalX * velocityX);
    if (!(closing >= 0.0f)) {
        return 0.0f;
    }
    volatile float impulse = suspensionBumpScale * closing * bodyA4->divisor164;
    WheelVector applied;
    applied.x = normalX * impulse;
    applied.y = normalY * impulse;
    applied.z = normalZ * impulse;
    if (!moverImpulsesDisabled) {
        bodyA4->AddLimitedImpulse(&applied, &offset);
    }
    return impulse;
}
