#include "VehicleIntegrator.h"

// PC v1.2 0x5aa480. The fixed buffer and update order are observed in the binary.
// This is the recovered integration operation, not a newly designed force model.
void VehicleIntegrator::Integrate(float duration) {
    float derivatives[140];
    CollectDerivatives(derivatives);
    int offset = 0;
    IntegrationStateBlock *block = IntegrationStateBlock::FromLink(blocks.next);
    while (block != IntegrationStateBlock::FromLink(&blocks)) {
        int count = block->GetStateCount();
        float *values = block->GetStateValues();
        float *rates = derivatives + offset;
        block->BeforeIntegrate();
        for (int i = 0; i < count; ++i) {
            *values++ += duration * *rates++;
        }
        offset += count;
        block = IntegrationStateBlock::FromLink(block->link.next);
    }
    AfterIntegrate();
}
