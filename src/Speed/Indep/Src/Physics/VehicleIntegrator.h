#ifndef UG2_VEHICLE_INTEGRATOR_H
#define UG2_VEHICLE_INTEGRATOR_H

struct IntegrationLink {
    IntegrationLink *next;
    IntegrationLink *previous;
};

// Only virtual slots observed at 0x5a0d20 and 0x5aa480 are named.
class IntegrationStateBlock {
public:
    virtual void UnknownSlot0() = 0;
    virtual int GetStateCount() = 0;
    virtual void UnknownSlot8() = 0;
    virtual float *GetStateValues() = 0;
    virtual void UnknownSlot16() = 0;
    virtual void UnknownSlot20() = 0;
    virtual void WriteDerivatives(float *output) = 0;
    virtual void UnknownSlot28() = 0;
    virtual void BeforeIntegrate() = 0;
    IntegrationLink link;

    static IntegrationStateBlock *FromLink(IntegrationLink *value) {
        return value ? reinterpret_cast<IntegrationStateBlock *>(
            reinterpret_cast<char *>(value) - sizeof(void *)) : 0;
    }
};

class VehicleIntegrator {
public:
    virtual void UnknownSlot0() = 0;
    virtual void AfterIntegrate() = 0;
    char unidentified[8];
    IntegrationLink blocks;
    void CollectDerivatives(float *output);
    void Integrate(float duration);
};

#endif
