#include "VehicleMover.h"

// Original 0x5fbbe0..0x5fbc56, before collider/world queries. This is a recovered
// basic-block prefix, not a recovery of the complete 0x5fbbe0 routine.
void MoverModeOwner::SnapshotFrameKinematics(float duration) {
    if (duration > 0.0f) {
        DrivetrainAuxiliary *state = State();
        previousPosition510 = state->position20;
        previousVelocity520 = state->velocity30;
        previousAngular530 = state->angularD0;
    }
}

// Original 0x604740..0x60482a, before race/world/player post-update. Its caller
// 0x609ce0 snapshots before body advancement and produces acceleration after
// collision resolution. Preserve the one reciprocal and the observed spills.
void MoverModeOwner::UpdateFrameAcceleration(float duration) {
    if (duration > 0.0f) {
        DrivetrainAuxiliary *state = State();
        double reciprocal = 1.0 / (double)duration;
        volatile float deltaY = state->velocity30.y - previousVelocity520.y;
        volatile float deltaZ = state->velocity30.z - previousVelocity520.z;
        volatile float angularX = state->angularD0.x - previousAngular530.x;
        volatile float angularY = state->angularD0.y - previousAngular530.y;
        volatile float angularZ = state->angularD0.z - previousAngular530.z;
        state->value40.z = (float)((double)deltaZ * reciprocal);
        // Car +0x120/+0x124/+0x128: state +0xe0, not the angular velocity +0xd0.
        state->valueE0.x = (float)((double)angularX * reciprocal);
        state->valueE0.y = (float)((double)angularY * reciprocal);
        state->valueE0.z = (float)((double)angularZ * reciprocal);
        state->value40.x = (float)(((double)state->velocity30.x - previousVelocity520.x) * reciprocal);
        state->value40.y = (float)((double)deltaY * reciprocal);
    }
}
