#include "VehicleIntegrator.h"

// PC v1.2 0x5a0d20. Derivatives are supplied by the original block callbacks.
void VehicleIntegrator::CollectDerivatives(float *output) {
    int offset = 0;
    IntegrationStateBlock *block = IntegrationStateBlock::FromLink(blocks.next);
    while (block != IntegrationStateBlock::FromLink(&blocks)) {
        block->WriteDerivatives(output + offset);
        offset += block->GetStateCount();
        block = IntegrationStateBlock::FromLink(block->link.next);
    }
}
