// Recovered PC keyboard binding items. Screen/list ownership is still a platform seam.
#include "PCControls.h"

#define DEFINE_PC_BINDING(Name, Action, Slot) \
void Name::React(const char *, unsigned int) { \
    controlDevice874C40.BeginBinding(Action, Slot); \
    SetLabels(); \
} \
void Name::SetLabels() { SetPCControlBindingLabel(name2C, Action, Slot, 0); }

// Original React 0x4acfe0; SetLabels 0x4ad000; vtable 0x007926a8.
DEFINE_PC_BINDING(PCControl7926A8, 3, 0)
// Original React 0x4ad020; SetLabels 0x4ad040; vtable 0x007926e8.
DEFINE_PC_BINDING(PCControl7926E8, 3, 1)
// Original React 0x4ad080; SetLabels 0x4ad0a0; vtable 0x00792768.
DEFINE_PC_BINDING(PCControl792768, 4, 0)
// Original React 0x4ad0c0; SetLabels 0x4ad0e0; vtable 0x007927a8.
DEFINE_PC_BINDING(PCControl7927A8, 4, 1)
// Original React 0x4ad120; SetLabels 0x4ad140; vtable 0x00792828.
DEFINE_PC_BINDING(PCControl792828, 1, 0)
// Original React 0x4ad160; SetLabels 0x4ad180; vtable 0x00792868.
DEFINE_PC_BINDING(PCControl792868, 1, 1)
// Original React 0x4ad1c0; SetLabels 0x4ad1e0; vtable 0x007928e8.
DEFINE_PC_BINDING(PCControl7928E8, 2, 0)
// Original React 0x4ad200; SetLabels 0x4ad220; vtable 0x00792928.
DEFINE_PC_BINDING(PCControl792928, 2, 1)
// Original React 0x4ad260; SetLabels 0x4ad280; vtable 0x007929a8.
DEFINE_PC_BINDING(PCControl7929A8, 7, 0)
// Original React 0x4ad2a0; SetLabels 0x4ad2c0; vtable 0x007929e8.
DEFINE_PC_BINDING(PCControl7929E8, 7, 1)
// Original React 0x4ad300; SetLabels 0x4ad320; vtable 0x00792a28.
DEFINE_PC_BINDING(PCControl792A28, 8, 0)
// Original React 0x4ad340; SetLabels 0x4ad360; vtable 0x00792a68.
DEFINE_PC_BINDING(PCControl792A68, 8, 1)
// Original React 0x4ad3a0; SetLabels 0x4ad3c0; vtable 0x00792aa8.
DEFINE_PC_BINDING(PCControl792AA8, 5, 0)
// Original React 0x4ad3e0; SetLabels 0x4ad400; vtable 0x00792ae8.
DEFINE_PC_BINDING(PCControl792AE8, 5, 1)
// Original React 0x4ad440; SetLabels 0x4ad460; vtable 0x00792b28.
DEFINE_PC_BINDING(PCControl792B28, 6, 0)
// Original React 0x4ad480; SetLabels 0x4ad4a0; vtable 0x00792b68.
DEFINE_PC_BINDING(PCControl792B68, 6, 1)
// Original React 0x4ad4e0; SetLabels 0x4ad500; vtable 0x00792ba8.
DEFINE_PC_BINDING(PCControl792BA8, 10, 0)
// Original React 0x4ad520; SetLabels 0x4ad540; vtable 0x00792be8.
DEFINE_PC_BINDING(PCControl792BE8, 10, 1)
// Original React 0x4ad580; SetLabels 0x4ad5a0; vtable 0x00792c28.
DEFINE_PC_BINDING(PCControl792C28, 9, 0)
// Original React 0x4ad5c0; SetLabels 0x4ad5e0; vtable 0x00792c68.
DEFINE_PC_BINDING(PCControl792C68, 9, 1)
// Original React 0x4ad620; SetLabels 0x4ad640; vtable 0x00792ca8.
DEFINE_PC_BINDING(PCControl792CA8, 15, 0)
// Original React 0x4ad660; SetLabels 0x4ad680; vtable 0x00792ce8.
DEFINE_PC_BINDING(PCControl792CE8, 15, 1)
// Original React 0x4ad6c0; SetLabels 0x4ad6e0; vtable 0x00792da8.
DEFINE_PC_BINDING(PCControl792DA8, 16, 0)
// Original React 0x4ad700; SetLabels 0x4ad720; vtable 0x00792de8.
DEFINE_PC_BINDING(PCControl792DE8, 16, 1)
// Original React 0x4ad760; SetLabels 0x4ad780; vtable 0x00792e28.
DEFINE_PC_BINDING(PCControl792E28, 17, 0)
// Original React 0x4ad7a0; SetLabels 0x4ad7c0; vtable 0x00792e68.
DEFINE_PC_BINDING(PCControl792E68, 17, 1)
// Original React 0x4ad800; SetLabels 0x4ad820; vtable 0x00792d28.
DEFINE_PC_BINDING(PCControl792D28, 18, 0)
// Original React 0x4ad840; SetLabels 0x4ad860; vtable 0x00792d68.
DEFINE_PC_BINDING(PCControl792D68, 18, 1)
// Original React 0x4ada80; SetLabels 0x4adaa0; vtable 0x00792ea8.
DEFINE_PC_BINDING(PCControl792EA8, 14, 0)
// Original React 0x4adac0; SetLabels 0x4adae0; vtable 0x00792ee8.
DEFINE_PC_BINDING(PCControl792EE8, 14, 1)
// Original React 0x4ad8a0; SetLabels 0x4ad8c0; vtable 0x00792f28.
DEFINE_PC_BINDING(PCControl792F28, 22, 0)
// Original React 0x4ad8e0; SetLabels 0x4ad900; vtable 0x00792f68.
DEFINE_PC_BINDING(PCControl792F68, 22, 1)
// Original React 0x4ad940; SetLabels 0x4ad960; vtable 0x00792fa8.
DEFINE_PC_BINDING(PCControl792FA8, 19, 0)
// Original React 0x4ad980; SetLabels 0x4ad9a0; vtable 0x00792fe8.
DEFINE_PC_BINDING(PCControl792FE8, 19, 1)
// Original React 0x4ad9e0; SetLabels 0x4ada00; vtable 0x00793028.
DEFINE_PC_BINDING(PCControl793028, 12, 0)
// Original React 0x4ada20; SetLabels 0x4ada40; vtable 0x00793068.
DEFINE_PC_BINDING(PCControl793068, 12, 1)
#undef DEFINE_PC_BINDING
