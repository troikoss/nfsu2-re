#include "VehicleMover.h"
#include "VehicleIntegrator.h"
#include "VehicleAllocation.h"
#include "GameModeFlags.h"
#include "Engine.h"
#include "../../Libs/bWare/Strings.h"

extern float engineSubstepDuration;
extern float drivetrainFinishingInputThreshold;
extern float drivetrainSelectorUpperThreshold;
extern DrivetrainTuning drivetrainTuningRows[];
extern float drivetrainModeCoefficientsE4[];
extern float drivetrainModeCoefficientsE8[];
extern int raceModeIndex89E7F4;
extern int raceSetting850078;
extern char *raceTuningData81CB60;
extern float engineValueScale;
extern float engineSpeedOverride;
extern float engineSpeedOverrideUnset;

// Inline list append used by original 0x5b5cc0 for each integration block.
static void AppendIntegrationBlock(VehicleMover *mover, IntegrationStateBlock *block) {
    IntegrationLink *head = reinterpret_cast<IntegrationLink *>(mover->integrator8 + 0xc);
    IntegrationLink *link = block ? &block->link : 0;
    IntegrationLink *tail = head->previous;
    tail->next = link;
    head->previous = link;
    link->previous = tail;
    link->next = head;
    mover->stateCount1C += block->GetStateCount();
}

// Constructor body of original PhysicsMover 0x5b5cc0 ("PhysMov").
void VehicleMover::Construct(CarPhysicsRecord *record, MoverCarInfo *carInfo, MoverInput *input,
                             MoverModeOwner *owner) {
    IntegrationLink *head = reinterpret_cast<IntegrationLink *>(integrator8 + 0xc);
    head->next = head;
    head->previous = head;
    stateCount1C = 0;
    value68 = 0;
    record54 = record;
    owner5C = owner;
    input4 = input;
    state58 = reinterpret_cast<DrivetrainAuxiliary *>(reinterpret_cast<char *>(owner) + 0x40);
    value6C = 1.0f;
    flag84 = 0;
    nfs_strcpy(integrator8 + 4, "PhysMov");

    body20 = NewMoverBody(&record54->body110);
    AppendIntegrationBlock(this, reinterpret_cast<IntegrationStateBlock *>(body20));
    body20->value204 = record54->value290;
    body20->flag15C = carInfo->valueD8;
    float *centre = &record54->centre0.x;
    MoverBody *body = body20;
    body->centre1F0.z = centre[2];
    body->centre1F0.x = centre[0];
    body->centre1F0.y = centre[1];

    steering24 = NewAckermannSteering(record54);

    for (int i = 0, offset = 0; offset < 0xc0; ++i, offset += 0x30) {
        CarPhysicsRecord *source = record54;
        wheels38[i] = NewDrivetrainWheel(&source->wheels10[i], &source->correctionD0[i / 2].spec,
                                         &source->preparation270, state58);
        AckermannSteering *steering = offset < 0x60 ? steering24 : 0;
        arms28[i] = NewMoverArm(i, body20, wheels38[i], steering,
                                &record54->suspension170[i / 2]);
        AppendIntegrationBlock(this, reinterpret_cast<IntegrationStateBlock *>(wheels38[i]));
        AppendIntegrationBlock(this, reinterpret_cast<IntegrationStateBlock *>(arms28[i]));
        value74[i] = 0;
    }
    arms28[0]->pair18 = arms28[1];
    arms28[1]->pair18 = arms28[0];
    arms28[2]->pair18 = arms28[3];
    arms28[3]->pair18 = arms28[2];

    engine48 = NewEngine(reinterpret_cast<EngineSpec *>(record54->engine1F0),
                         reinterpret_cast<EngineAttributeOwner *>(state58));
    AppendIntegrationBlock(this, reinterpret_cast<IntegrationStateBlock *>(engine48));
    engine48->flag74 = carInfo->valueD8;

    int variant = drivetrainFinishingInputThreshold <= record54->drivetrain1B0.selector10;
    if (record54->drivetrain1B0.selector10 > drivetrainSelectorUpperThreshold) {
        variant = 2;
    }
    drivetrain4C = NewDrivetrain(&record54->drivetrain1B0, &drivetrainTuningRows[variant],
                                 reinterpret_cast<DrivetrainComponent *>(engine48), wheels38, state58);
    AppendIntegrationBlock(this, reinterpret_cast<IntegrationStateBlock *>(drivetrain4C));

    PhysicsInfo *info = owner->physicsInfo10;
    if (info->parameters10.index4 == -1 && info->parameters10.value0 != -1) {
        int mode = 0;
        if (IsModeGroup3()) {
            mode = 3;
        } else if (gameFlag89E7E3 || gameFlag89010C) {
            mode = 10;
        } else if (IsModeGroup2()) {
            mode = 2;
        }
        info->parameters10.Compose(mode, 1, 1);
    }
    drivetrain4C->indexF0 = info->parameters10.index4;
    engine48->drivetrain20 = drivetrain4C;
    if (owner->context14->value4 == 1 && IsModeGroup2() && raceSetting850078 != 1) {
        drivetrain4C->coefficientE4 = drivetrainModeCoefficientsE4[raceModeIndex89E7F4];
    }
    if (owner->context14->value4 == 2 && IsModeGroup2()
        && (owner->value24 == 3 || owner->value24 == 2 || owner->value24 == 4)) {
        if (raceSetting850078 != 1) {
            drivetrain4C->coefficientE8 = drivetrainModeCoefficientsE8[raceModeIndex89E7F4];
        }
        int level = owner->participant2C->GetTuningLevel();
        int clamped = level < 1 ? 0 : (level > 9 ? 9 : level);
        if (level == clamped) {
            float *row = reinterpret_cast<float *>(raceTuningData81CB60 + level * 0x68 + 0x524);
            Drivetrain *drivetrain = drivetrain4C;
            drivetrain->rangesCC[0] = row[0x34 / 4 + 0];
            drivetrain->rangesCC[1] = row[0x38 / 4];
            drivetrain->rangesCC[4] = row[0x3c / 4];
            drivetrain->rangesCC[5] = row[0x40 / 4];
            drivetrain4C->rangesCC[2] = row[0x2c / 4];
            drivetrain4C->rangesCC[3] = row[0x30 / 4];
        }
    }

    aero50 = NewMoverAero(&record54->aero260, body20);
    engineSubstepDuration = 0.0f;
    value70 = 1.0f;
    value60 = 0;
    flag64 = 0;
    RefreshLoads();
    InitialiseFromState();
    FinishIntegration();
    value124 = 0;
}

// Original 0x5a48a0: place body and wheels from the shared vehicle state.
void VehicleMover::InitialiseFromState() {
    DrivetrainAuxiliary *state = state58;
    body20->SetPose(&state->position20, &state->matrix50, &state->velocity30, &state->angularD0, 0);
    for (int i = 0; i < 4; ++i) {
        MoverArm *arm = arms28[i];
        arm->travel1C = 0.0f;
        arm->rate20 = 0.0f;
        float spin = state58->wheelSpin2F0[i];
        DrivetrainWheel *wheel = wheels38[i];
        wheel->angle24 = state58->wheelAngle300[i];
        wheel->value28 = spin;
    }
    float speed = engineSpeedOverride;
    if (engineSpeedOverride == engineSpeedOverrideUnset) {
        speed = state58->engineValue400;
    }
    engine48->state14 = speed * engineScalarScale;
    Drivetrain *drivetrain = drivetrain4C;
    int gear = state58->gear3F4;
    drivetrain->previous58 = 0.0f;
    drivetrain->ratioIndex = gear;
    drivetrain->value38 = gear;
    drivetrain->mode44 = 3;
    drivetrain->state14 = 1.0f;
}
