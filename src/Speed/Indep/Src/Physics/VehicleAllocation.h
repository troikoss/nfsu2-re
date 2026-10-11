#ifndef UG2_VEHICLE_ALLOCATION_H
#define UG2_VEHICLE_ALLOCATION_H

#include "VehicleMover.h"

struct Engine;
struct EngineSpec;
struct EngineAttributeOwner;

// `new` expressions of original 0x5b5cc0. Each allocates from a bWare slot pool
// (0x440e50) and runs a constructor that installs the original vtable:
// body 0x86b2e0/0x7a20c4, steering 0x86b2e4/0x7a2084, wheel 0x86b2e8/0x7a20e8,
// arm 0x86b2f8/0x7a210c, engine 0x86b2ec/0x7a235c, drivetrain 0x86b2f0/0x7a2508,
// aero 0x86b2f4. Vtable layout is compiler-specific, so a build target supplies these
// and then calls the recovered Construct bodies.
MoverBody *NewMoverBody(MoverBodySpec *spec);
AckermannSteering *NewAckermannSteering(CarPhysicsRecord *record);
DrivetrainWheel *NewDrivetrainWheel(DrivetrainWheelSpec *spec, WheelCorrectionSpec *correction,
                                    WheelPreparationSpec *preparation, DrivetrainAuxiliary *auxiliary);
MoverArm *NewMoverArm(int index, MoverBody *body, DrivetrainWheel *wheel,
                      AckermannSteering *steering, SuspensionSpec *spec);
Engine *NewEngine(EngineSpec *spec, EngineAttributeOwner *owner);
Drivetrain *NewDrivetrain(DrivetrainSpec *spec, DrivetrainTuning *tuning, DrivetrainComponent *engine,
                          DrivetrainWheel **wheels, DrivetrainAuxiliary *auxiliary);
MoverAero *NewMoverAero(MoverAeroSpec *spec, MoverBody *body);

#endif
