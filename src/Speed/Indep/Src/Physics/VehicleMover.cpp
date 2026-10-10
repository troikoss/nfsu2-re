#include "VehicleMover.h"

// Original PC v1.2 0x5abdf0. Preserve the preparation/five-pass/finishing order.
void VehicleMover::CorrectWheelCoupling() {
    drivetrain4C->ApplyInitialDriveValue();
    int mode = owner5C->mode3C->value18;
    for (int i = 0; i < 4; ++i) {
        wheels38[i]->PrepareForces(0.0f, mode);
    }
    WheelMatrix inverse;
    WheelTranspose(&inverse, &body20->matrix30);
    WheelVector local;
    WheelTransform(&local, &inverse, &body20->vector110);
    for (int pass = 0; pass < 5; ++pass) {
        DrivetrainWheel *wheel = wheels38[0];
        // First wheel components and every accumulated sum cross binary32 stores.
        volatile float firstX = wheel->basisC0.x * wheel->value128 + wheel->basisB0.x * wheel->value124;
        volatile float firstY = wheel->basisC0.y * wheel->value128 + wheel->basisB0.y * wheel->value124;
        volatile float sumX = local.x + firstX;
        volatile float sumY = local.y + firstY;
        double torque = (double)firstY * arms28[0]->value90 - (double)firstX * arms28[0]->value94;
        for (int i = 1; i < 4; ++i) {
            wheel = wheels38[i];
            double x = (double)wheel->basisC0.x * wheel->value128 + (double)wheel->basisB0.x * wheel->value124;
            double y = (double)wheel->basisC0.y * wheel->value128 + (double)wheel->basisB0.y * wheel->value124;
            sumX = sumX + x;
            sumY = sumY + y;
            torque = torque + y * arms28[i]->value90;
            torque = torque - x * arms28[i]->value94;
        }
        double reciprocal = 1.0 / body20->divisor164;
        volatile float xRate = sumX * reciprocal;
        volatile float yRate = reciprocal * sumY;
        WheelMatrix *selected = body20->flag158 ? &body20->matrix1B0 : &body20->matrix170;
        volatile float angular = torque / (1.0 / selected->elements[10]);
        for (int i = 0; i < 4; ++i) {
            wheel = wheels38[i];
            if (!wheel->flag160 && !wheel->flag10C) {
                double x = xRate - (double)angular * arms28[i]->value94;
                double y = (double)angular * arms28[i]->value90 + yRate;
                double radius = wheel->spec14->value10;
                double response = (wheel->value108 - radius * wheel->value124) / wheel->divisor2C * radius;
                double projected = y * wheel->basisC0.x + x * wheel->basisB0.x;
                wheel->value124 = (response - projected) / radius * wheel->divisor2C + wheel->value124;
            }
        }
        drivetrain4C->ApplyDriveCorrection();
    }
    for (int i = 0; i < 4; ++i) {
        union { int integer; float real; } argument;
        argument.integer = i; // Original passes the raw loop-index bits.
        wheels38[i]->CorrectForces(argument.real);
    }
    drivetrain4C->FinishDriveCoupling();
}
