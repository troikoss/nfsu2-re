#ifndef UG2_HUD_FREEROAM_H
#define UG2_HUD_FREEROAM_H

#include "HudMinimap.h"
#include "HudVisibility.h"

// FEng/career interfaces at the original call boundaries. Native implementations
// live in Platform/Native; verification records these calls separately.
void HudObjectRotation(void *object, float angle); // 0x51ced0
void HudObjectColour(void *object, unsigned int colour); // 0x51d7f0
void HudTextPrint(void *object, const char *format, ...); // 0x537be0
void *HudLookupObject(void *package, unsigned int hash); // 0x537a10
unsigned int HudObjectHash(const char *name); // 0x505450
const char *HudLocalisedText(unsigned int hash); // 0x4ffa80
void HudObjectScript(void *object, const char *name, int restart); // 0x51cf70
// Career bank at 0x861e74.
struct CareerBank861E74 {
    int balance0;
    int GetBalance(); // 0x500f90
};
int GetLanguage4FF970(); // 0x4ff970, reads 0x7f70d0
void HudVerticalBar(void *object, float value, float maximum, float start, float end, int flag); // 0x4c66f0
void HudHorizontalBar(void *object, float value, float maximum, float start, float end, int flag); // 0x4c67e0
int HudGpsMode(); // 0x501100, route manager 0x838590
void HudGpsPosition(float *x, float *y); // 0x5010c0, route manager 0x838590
void HudObjectTexture(void *object, unsigned int hash); // 0x50c900
void HudObjectPosition(void *object, float x, float y); // 0x51d4d0
void HudGpsEvent(void *receiver, int event); // 0x458520
struct HudNotificationEntry {
    unsigned char state0;
    signed char type1;
    char unidentified2[2];
    int started4;
    char unidentified8[12];
};
void HudNotificationUpdate(HudNotificationEntry *entry, HudPlayerSlot *player); // 0x4c9030
HudNotificationEntry *HudNotificationNext(void *feature); // 0x4c90b0
unsigned int HudNotificationPriority(int type); // 0x4abf20
void HudNotificationPrepare(HudNotificationEntry *entry); // 0x4c8ac0
void HudNotificationNormal(HudNotificationEntry *entry, HudPlayerSlot *player); // 0x4c8b80
void HudNotificationType11(HudNotificationEntry *entry, HudPlayerSlot *player); // 0x4c8dd0
void HudNotificationType13(HudNotificationEntry *entry, HudPlayerSlot *player); // 0x4abaf0
void HudNotificationType14(HudNotificationEntry *entry, HudPlayerSlot *player); // 0x4aba30
void HudNotificationSound(int first, int second); // 0x51de30
void HudNotificationTexture(void *object, unsigned int hash); // 0x50c990

struct HudTurboFeature : HudFeatureVisibility {
    HudVisibilityObject *fill20;
    HudVisibilityObject *lines24;
    HudVisibilityObject *dial28;
    HudVisibilityObject *red2C;
    HudVisibilityObject *red30;
    HudVisibilityObject *backing34;
    void Update(HudPlayerSlot *player); // 0x4c6b80, constructor 0x4f5a70
};
struct HudNitrousFeature : HudFeatureVisibility {
    HudVisibilityObject *line20;
    HudVisibilityObject *gamebreaker24;
    HudVisibilityObject *mask28;
    unsigned int colour2C;
    unsigned int colour30;
    void Update(HudPlayerSlot *player); // 0x4c7b20, constructor 0x4f6a90
};
struct HudRegionPlayer {
    char unidentified0[0x9b8];
    unsigned int region9B8;
};
struct HudHeatFeature : HudFeatureVisibility {
    HudVisibilityObject *bar20;
    unsigned char value24;
    unsigned char value25;
    char unidentified26[2];
    void Update(HudPlayerSlot *player); // 0x4c7aa0, constructor 0x4f69e0
};
struct HudGpsFeature : HudFeatureVisibility {
    void *value20;
    HudVisibilityObject *object24;
    void *value28;
    HudVisibilityObject *object2C;
    HudVisibilityObject *object30;
    unsigned char value34;
    unsigned char arrived35;
    char unidentified36[2];
    void Update(HudPlayerSlot *player); // 0x4c8410
};
struct HudRegionFeature : HudFeatureVisibility {
    HudVisibilityObject *message20;
    unsigned int region24;
    void Update(HudRegionPlayer *player); // 0x4a9e70, constructor 0x4f4e70
};
struct HudCashFeature : HudFeatureVisibility {
    HudVisibilityObject *bank20;
    HudVisibilityObject *data24;
    int reward28;
    unsigned char animated2C;
    unsigned char hidden2D;
    char unidentified2E[2];
    void Update(HudPlayerSlot *player); // 0x4a9ef0, constructor 0x4f4f10
};
struct HudRewardFeature : HudFeatureVisibility {
    HudVisibilityObject *reward20;
    unsigned int value24;
    void Update(HudPlayerSlot *player); // 0x4a9ff0, constructor 0x4f5060
};
struct HudNotificationFeature : HudFeatureVisibility {
    HudVisibilityObject *object20;
    void *value24;
    HudVisibilityObject *object28;
    HudVisibilityObject *object2C;
    void *value30;
    HudVisibilityObject *object34;
    HudVisibilityObject *object38;
    HudVisibilityObject *object3C;
    char unidentified40[0x20];
    HudNotificationEntry entries60[8];
    void Update(HudPlayerSlot *player); // 0x4da9a0
};

UG2_CHECK_SIZE(HudTurboFeature, 0x38);
UG2_CHECK_SIZE(HudNitrousFeature, 0x34);
UG2_CHECK_SIZE(HudHeatFeature, 0x28);
UG2_CHECK_SIZE(HudGpsFeature, 0x38);
UG2_CHECK_SIZE(HudNotificationEntry, 0x14);
UG2_CHECK_OFFSET(HudNotificationFeature, entries60, 0x60);
UG2_CHECK_SIZE(HudNotificationFeature, 0x100);
UG2_CHECK_SIZE(HudRegionFeature, 0x28);
UG2_CHECK_SIZE(HudCashFeature, 0x30);
UG2_CHECK_SIZE(HudRewardFeature, 0x28);
UG2_CHECK_OFFSET(HudRegionPlayer, region9B8, 0x9b8);

float HudTurboAngle(float boost, float start, float end); // 0x4aa160
float HudReadBoost(MoverModeOwner *car, unsigned char *installed); // 0x5ecec0
float HudReadHeat(MoverModeOwner *car); // 0x5eccd0
float HudReadShiftPoint(MoverModeOwner *car, int gear); // 0x5ecd30 / 0x5947b0
float HudReadShiftWindow(Drivetrain *drivetrain, int gear); // 0x594f00
// 0x4c68d0 tail, retaining 0x594f00/0x5ecd30 calls. Caller supplies the
// already selected RPM and 0x5b5330 tuning index, as the original gauge does.
void HudUpdateShiftLight(MoverModeOwner *car, int tuningIndex, float rpm, HudVisibilityObject *light);

#endif
