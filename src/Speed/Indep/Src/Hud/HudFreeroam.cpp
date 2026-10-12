#include "HudFreeroam.h"
#include "HudPlayerMasks.h"

extern HudFlagHolder hudHolder85B940;
extern CareerBank861E74 careerBank861E74;

extern unsigned char hudDrag89E7D8;
extern int hudDragOverride890108;
extern int hudPlayerCount89E7C0;
extern float engineSpeedOverride;        // 0x7fbed8
extern float engineSpeedOverrideUnset;   // 0x784258
extern float hudSpeedScaleMetric7904FC;
extern float hudSpeedScaleImperial78426C;
extern float hudTachSegments788218;      // -7
extern signed char hudGearCharacters7F66A4[20];
extern unsigned char units83AA1A;
extern "C" double __cdecl fabs(double);
#pragma intrinsic(fabs)
extern float hudOne784250;
extern float hudHalf784260;
extern float hudZero7D7D3C;
extern float hudNegativeOne7842A0;
extern float hudFullTurn78450C;
extern float hudTurboCentre78E058;
extern float hudTurboSweep78E05C;
extern float hudDragTurboCentre78E060;
extern float hudDragTurboSweep78E064;
extern float hudNosEnd795AEC;
extern float hudNosSweep795AF0;
extern float hudNosRotation8364B0;
extern float gearboxShiftWeights[];
extern float gearboxMatchWindow;
extern unsigned char hudRewardPending83649C;
extern int hudRewardBase8364A0;
extern int hudRewardExtra8364A4;
extern int hudRewardType8364A8;
extern int hudRewardDelay8364AC;
extern void *hudGpsReceiver82B884;
extern int hudClock86518C;

// Original 0x4abf20; preserve the upper bits of the default unsigned result.
unsigned int HudNotificationPriority(int type) {
    switch (type) {
    case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:
    case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: return 4;
    case 9: case 10: return 3;
    case 19: return 2;
    case 20: return 1;
    default: return (static_cast<unsigned int>(type) - 1) & 0xffffff00u;
    }
}

// Original 0x4c90b0: first slot wins ties. Both priority switches are inline
// in the original; no priority function call is introduced here.
HudNotificationEntry *HudNotificationNext(void *value) {
    HudNotificationFeature *feature = static_cast<HudNotificationFeature *>(value);
    unsigned char highest = 0;
    HudNotificationEntry *best = 0;
    for (int i = 0; i < 8; ++i) {
        HudNotificationEntry *entry = &feature->entries60[i];
        if (!entry->state0) continue;
        unsigned char priority;
        switch (entry->type1) {
        case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:
        case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: priority = 4; break;
        case 9: case 10: priority = 3; break;
        case 19: priority = 2; break;
        case 20: priority = 1; break;
        default: continue;
        }
        if (priority > highest) { highest = priority; best = entry; }
    }
    return best;
}

// Original 0x4c9030, retaining entry presentation at its original boundaries.
void HudNotificationUpdate(HudNotificationEntry *entry, HudPlayerSlot *player) {
    if (!entry->type1) { entry->state0 = 0; return; }
    if (entry->state0 == 1 || !entry->state0) return;
    HudNotificationPrepare(entry);
    if (entry->type1 == 0x11) HudNotificationType11(entry, player);
    else if (entry->type1 == 0x13) HudNotificationType13(entry, player);
    else if (entry->type1 == 0x14) HudNotificationType14(entry, player);
    else HudNotificationNormal(entry, player);
}

static void HideNotifications(HudNotificationFeature *feature) {
    // Original uses 0x50c990 here; 0x50c900 is the GPS texture call.
    HudNotificationTexture(feature->object20, 0xea570722);
    HudHideObject(feature->object34);
    HudHideObject(feature->object38);
    HudHideObject(feature->object3C);
    HudHideObject(feature->object2C);
    HudHideObject(feature->object28);
}

static void StartNotification(HudNotificationEntry *entry) {
    entry->state0 = 2;
    entry->started4 = hudClock86518C;
    if (entry->type1 != 0x14) HudNotificationSound(0xb, entry->type1 == 0x13 ? 0xf : 0xd);
}

// Original 0x4da9a0. Entry rendering, queue selection and priority stay at
// their original boundaries; this recovers the outer eight-slot scheduler.
void HudNotificationFeature::Update(HudPlayerSlot *player) {
    HideNotifications(this);
    if (!(current18 & mask10) && !(current1C & mask14)) return;
    for (int i = 0; i < 8; ++i) HudNotificationUpdate(&entries60[i], player);
    HudNotificationEntry *active = 0;
    for (int i = 0; i < 8; ++i) {
        if (entries60[i].state0 == 2 || entries60[i].state0 == 3) {
            active = &entries60[i];
            break;
        }
    }
    HudNotificationEntry *next = HudNotificationNext(this);
    if (!active) {
        if (!next) { HideNotifications(this); return; }
        if (next->state0 != 4) StartNotification(next);
        HudNotificationUpdate(next, player);
        return;
    }
    unsigned char priority = active->state0 ? HudNotificationPriority(active->type1) : 0;
    // The original active path assumes the selector returns a slot.
    if (!next->state0 || HudNotificationPriority(next->type1) <= priority) {
        HudNotificationUpdate(active, player);
        return;
    }
    if (active->type1 == 0x13) {
        active->state0 = 4;
        HudNotificationUpdate(active, 0);
    } else {
        active->state0 = 6;
        HudNotificationUpdate(active, 0);
        active->state0 = 0;
        active->type1 = 0;
    }
    StartNotification(next);
}

// Original 0x5eccd0.
float HudReadHeat(MoverModeOwner *car) {
    if (car->moverType28 != 1) return hudZero7D7D3C;
    float value = car->mover34->engine48->adjustment8C;
    if (value <= hudZero7D7D3C) return hudZero7D7D3C;
    if (hudOne784250 < value) return hudOne784250;
    return value;
}

// Original 0x4c7aa0.
void HudHeatFeature::Update(HudPlayerSlot *player) {
    if (!(current18 & mask10) && !(current1C & mask14)) return;
    if (hudPlayerCount89E7C0 == 1)
        HudVerticalBar(bar20, HudReadHeat(player->car4), 1.0f, 105.0f, 5.0f, 1);
    else HudVerticalBar(bar20, HudReadHeat(player->car4), 1.0f, 25.0f, 25.0f, 1);
}

// Original 0x4c8410. Repeated route queries retain the original call order.
void HudGpsFeature::Update(HudPlayerSlot *player) {
    if (HudGpsMode() == 1) {
        HudShowObject(object2C);
        HudObjectTexture(object2C, 0x155409e8);
    } else if (HudGpsMode() == 2) {
        HudShowObject(object2C);
        HudObjectTexture(object2C, 0x31139b65);
    } else HudHideObject(object2C);
    unsigned char arrived = 0;
    if (HudGpsMode() == 0 || HudGpsMode() == 1 || HudGpsMode() == 2) {
        HudShowObject(object24);
        HudShowObject(object30);
    }
    if (HudGpsMode() == 3) {
        arrived = 1;
        HudHideObject(object24);
        HudShowObject(object30);
    }
    bool cameraFlag = true;
    char *car = reinterpret_cast<char *>(player->car4);
    if (*reinterpret_cast<int *>(car + 0x24) == 1) {
        char *value2C = *reinterpret_cast<char **>(car + 0x2c);
        int value3E4 = *reinterpret_cast<int *>(value2C + 0x3e4);
        cameraFlag = value3E4 == 1 || value3E4 == 2;
    }
    if (!(HudGpsMode() == 0 || HudGpsMode() == 1 || HudGpsMode() == 2 || HudGpsMode() == 3) || cameraFlag) {
        HudHideObject(object24);
        HudShowObject(object30);
    }
    float x, y;
    HudGpsPosition(&x, &y);
    HudObjectPosition(object30, x, y);
    if (!arrived35 && arrived && hudGpsReceiver82B884) HudGpsEvent(hudGpsReceiver82B884, 0x12);
    arrived35 = arrived;
}

// Original 0x4aa160. Keep extended intermediates until the caller's float store.
float HudTurboAngle(float boost, float start, float end) {
    float angle = (end - start) * (boost + hudOne784250) * hudHalf784260 + start;
    if (hudFullTurn78450C < angle) angle = angle - hudFullTurn78450C;
    if (angle < hudZero7D7D3C) angle = hudFullTurn78450C - angle;
    return angle;
}

// Original 0x5ecec0: reads the recovered Engine, including capacity test.
float HudReadBoost(MoverModeOwner *car, unsigned char *installed) {
    if (installed) *installed = 0;
    if (car->moverType28 == 1) {
        Engine *engine = car->mover34->engine48;
        if (installed) *installed = hudZero7D7D3C < engine->capacity98;
        return engine->value9C;
    }
    return hudZero7D7D3C;
}

// Original 0x594f00.
float HudReadShiftWindow(Drivetrain *drivetrain, int gear) {
    return gearboxShiftWeights[gear + drivetrain->indexF0 * 9] * drivetrain->coefficientE4;
}

// Original 0x5ecd30, including the alternate mover's offset-based layout.
float HudReadShiftPoint(MoverModeOwner *car, int gear) {
    if (car->moverType28 == 1) return car->mover34->drivetrain4C->upshift84[gear];
    if (car->moverType28 == 3) {
        char *mover = reinterpret_cast<char *>(car->mover34);
        char *value24 = *reinterpret_cast<char **>(mover + 0x24);
        return *reinterpret_cast<float *>(value24 + 0x19c + gear * 4);
    }
    return hudZero7D7D3C;
}

// Original 0x5ea8b0.
bool UseMetric5EA8B0() {
    return units83AA1A == 1;
}

// Original 0x4c68d0: speed, units, needle (or the multi-player tach bars), gear and shift light.
void HudGaugeFeature::Update(HudPlayerSlot *player) {
    MoverModeOwner *car = player->car4;
    MoverContext *context = car->context14;
    const char *unused = car->valueC + 0x7f0;
    const EngineSpec *engine = reinterpret_cast<const EngineSpec *>(car->record20->engine1F0);
    float rpm;
    if (context->value4 == 1 && !context->flag900 && car->replay968) {
        rpm = car->replay968->rpm58;
    } else if (engineSpeedOverride == engineSpeedOverrideUnset) {
        rpm = car->engineValue440;
    } else {
        rpm = engineSpeedOverride;
    }
    float speed = (float)fabs(car->speed430);
    if ((current18 & mask10) || (current1C & mask14)) {
        const char *units;
        if (UseMetric5EA8B0()) {
            speed = speed * hudSpeedScaleMetric7904FC;
            units = HudLocalisedText(0x8569a25fu);
        } else {
            speed = speed * hudSpeedScaleImperial78426C;
            units = HudLocalisedText(0x8569ab44u);
        }
        HudTextPrint(speedUnits5C, units);
        HudTextPrint(speed58, "%3d", (int)speed);
        if (hudPlayerCount89E7C0 == 1) {
            HudObjectRotation(needle30, HudNeedleAngle(rpm, sweepC4, engine, unused));
        } else {
            int lit = 1 - (int)(rpm / engine->value4 * hudTachSegments788218);
            for (int i = 0; i < 7; ++i) {
                unsigned int colour;
                if (i < lit) colour = i + 1 >= 6 ? 0xffc80b0bu : 0xffccf48au;
                else colour = i + 1 >= 6 ? 0x7dc80b0bu : 0x64ccf48au;
                HudObjectColour(segments34[i], colour);
            }
        }
        if (gear64) {
            int gear = car->gear434;
            int shifting = car->value448;
            char text = 'X';
            for (unsigned int i = 0; i < 10; ++i) {
                if (gear == hudGearCharacters7F66A4[i * 2]) {
                    text = hudGearCharacters7F66A4[i * 2 + 1];
                    break;
                }
            }
            HudTextPrint(gear64, "%c", (int)text);
            HudObjectColour(gear64, shifting ? 0xffc3c3c3u : 0xffff8040u);
        }
        int gear = car->gear434;
        float window = gearboxShiftWeights[gear + car->physicsInfo10->parameters10.GetClassIndex() * 9];
        if (car->moverType28 == 1) window = HudReadShiftWindow(car->mover34->drivetrain4C, car->gear434);
        if (HudReadShiftPoint(car, car->gear434) - gearboxMatchWindow * window < rpm) {
            HudShowObject(shiftLight78);
        } else {
            HudHideObject(shiftLight78);
        }
    }
    if (noSkinC0) {
        HudHideObject(rpmNumbers50);
        HudHideObject(redline7C);
    }
}

// Original 0x4c6b80. Boost is read even when the feature is inactive.
void HudTurboFeature::Update(HudPlayerSlot *player) {
    unsigned char installed = 0;
    float boost = HudReadBoost(player->car4, &installed);
    if ((current18 & mask10) || (current1C & mask14)) {
        HudVisibilityObject *objects[] = {fill20, lines24, dial28, red2C, red30, backing34};
        for (int i = 0; i < 6; ++i) {
            if (installed) HudShowObject(objects[i]);
            else HudHideObject(objects[i]);
        }
        float start, end;
        if (!hudDrag89E7D8 && !hudDragOverride890108) {
            end = hudTurboCentre78E058 + hudTurboSweep78E05C;
            start = hudTurboCentre78E058 - hudTurboSweep78E05C;
        } else {
            end = hudDragTurboCentre78E060 - hudDragTurboSweep78E064;
            start = hudDragTurboSweep78E064 + hudDragTurboCentre78E060;
        }
        HudObjectRotation(dial28, HudTurboAngle(boost, start, end));
    }
}

// Original 0x4c7b20. Fractions above one are the gamebreaker range;
// do not clamp them to one. The bar helpers remain separate FEng calls.
void HudNitrousFeature::Update(HudPlayerSlot *player) {
    if (!(current18 & mask10) && !(current1C & mask14)) return;
    float fraction = player->car4->State()->GetNitrousElapsed();
    if (fraction <= hudZero7D7D3C) {
        HudHideObject(line20);
        HudHideObject(gamebreaker24);
        return;
    }
    if (hudOne784250 < fraction) {
        HudShowObject(line20);
        HudShowObject(gamebreaker24);
        if (hudPlayerCount89E7C0 == 1) {
            if (!hudDrag89E7D8 && !hudDragOverride890108) {
                float angle = hudNosEnd795AEC - (hudNosSweep795AF0 - (fraction - hudOne784250) * hudNosSweep795AF0);
                HudObjectRotation(mask28, angle);
                HudObjectColour(gamebreaker24, colour30);
                hudNosRotation8364B0 = angle * hudNegativeOne7842A0;
                return;
            }
            HudVerticalBar(line20, 1.0f, 1.0f, 45.0f, -9.0f, 1);
            HudVerticalBar(gamebreaker24, fraction - hudOne784250, 1.0f, 45.0f, -9.0f, 1);
        } else if (!hudDrag89E7D8 && !hudDragOverride890108) {
            HudHorizontalBar(line20, 1.0f, 1.0f, 65.0f, 65.0f, 1);
            HudHorizontalBar(gamebreaker24, fraction - hudOne784250, 1.0f, 65.0f, 65.0f, 1);
        } else {
            HudVerticalBar(line20, 1.0f, 1.0f, 64.0f, 64.0f, 1);
            HudVerticalBar(gamebreaker24, fraction - hudOne784250, 1.0f, 64.0f, 64.0f, 1);
        }
        return;
    }
    if (hudPlayerCount89E7C0 != 1) {
        HudShowObject(line20);
        HudHideObject(gamebreaker24);
        if (!hudDrag89E7D8 && !hudDragOverride890108)
            HudHorizontalBar(line20, fraction, 1.0f, 65.0f, 65.0f, 1);
        else HudVerticalBar(line20, fraction, 1.0f, 64.0f, 64.0f, 1);
    } else if (!hudDrag89E7D8 && !hudDragOverride890108) {
        HudHideObject(line20);
        HudShowObject(gamebreaker24);
        float angle = hudNosEnd795AEC - (hudNosSweep795AF0 - fraction * hudNosSweep795AF0);
        HudObjectRotation(mask28, angle);
        HudObjectColour(gamebreaker24, colour2C);
        hudNosRotation8364B0 = angle * hudNegativeOne7842A0;
    } else {
        HudShowObject(line20);
        HudHideObject(gamebreaker24);
        HudVerticalBar(line20, fraction, 1.0f, 45.0f, -9.0f, 1);
    }
}

// Original 0x4a9e70: a new, nonzero region hash starts ZoomIn once.
void HudRegionFeature::Update(HudRegionPlayer *player) {
    if (!(current18 & mask10) && !(current1C & mask14)) {
        HudHideObject(message20);
    } else if (player->region9B8 && player->region9B8 != region24) {
        HudTextPrint(message20, "%s", HudLocalisedText(player->region9B8));
        HudShowObject(message20);
        HudObjectScript(message20, "ZoomIn", 1);
        region24 = player->region9B8;
    }
}

// Original 0x4a9ef0. Cash producers and FEng animation stay at call boundaries.
void HudCashFeature::Update(HudPlayerSlot *) {
    if (((current18 & mask10) || (current1C & mask14)) && !hudHolder85B940.IsCashSuppressed()) {
        HudShowObject(bank20);
        HudShowObject(data24);
        HudShowObject(static_cast<HudVisibilityObject *>(HudLookupObject(package8, HudObjectHash("Cash_header"))));
        hidden2D = 0;
        if (reward28) {
            HudTextPrint(bank20, "%s %$d", HudLocalisedText(0x5d10f6afu), reward28);
            HudObjectScript(bank20, "Cash_Animate", 1);
            reward28 = 0;
            animated2C = 1;
        }
        HudTextPrint(data24, "%$d", careerBank861E74.GetBalance());
    } else {
        HudHideObject(bank20);
        HudHideObject(data24);
        HudHideObject(static_cast<HudVisibilityObject *>(HudLookupObject(package8, HudObjectHash("Cash_header"))));
        hidden2D = 1;
    }
}

// Original 0x4a9ff0: delayed one-shot reward text. Countdown only runs active.
void HudRewardFeature::Update(HudPlayerSlot *) {
    if (!(current18 & mask10) && !(current1C & mask14)) {
        HudHideObject(reward20);
        return;
    }
    if (0 < hudRewardDelay8364AC) --hudRewardDelay8364AC;
    if (!hudRewardPending83649C || hudRewardDelay8364AC) return;
    const char *separator = GetLanguage4FF970() == 1 ? " " : "";
    if (hudRewardType8364A8 == 3 || hudRewardType8364A8 == 1) {
        HudTextPrint(reward20, "%s%s: %$d", HudLocalisedText(0xaba1e56fu), separator,
                     hudRewardExtra8364A4 + hudRewardBase8364A0);
    } else if (hudRewardType8364A8 == 4 || hudRewardType8364A8 == 2) {
        HudTextPrint(reward20, "%s%s: %$d", HudLocalisedText(0x2ab10d61u), separator,
                     hudRewardExtra8364A4 + hudRewardBase8364A0);
    }
    HudShowObject(reward20);
    HudObjectScript(reward20, "ZoomIn", 1);
    hudRewardPending83649C = 0;
}
