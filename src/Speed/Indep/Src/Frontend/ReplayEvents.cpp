#include "PauseOptions.h"
#include "../Hud/HudPlayerMasks.h"

extern int collisionFrame7FB724;
extern PlayerRecord *playerRecords8900AC[2]; // 0x8900ac
extern RaceStatus890118 *raceStatus890118;

// Original 0x4a7b30: apply a game event (also replayed from the recording).
int ApplyReplayEvent(Replay8650B8::ReplayEvent *event) {
    PlayerRecord *player = playerRecords8900AC[event->player8];
    int type = event->type0;
    if (type == 0) {
        raceStatus890118->eventValueBEC = event->valueC;
        raceStatus890118->eventPlayerBF0 = event->player8;
        return 1;
    }
    if (type == 1) {
        player->transmission20 = (unsigned char)event->valueC;
        player->car4->input30->SetTransmission(event->valueC);
        return type;
    }
    if (type == 2) {
        player->stability21 = (unsigned char)event->valueC;
        player->car4->stability464 = event->valueC;
        return 1;
    }
    if (type == 3) {
        player->jumpCamera24 = (float)event->valueC;
        return 1;
    }
    return 0;
}

// Original 0x571830: a full event table stops the recording.
void Replay8650B8::Overflow571830() {
    if (value80 == 0) value80 = 1;
}

// Original 0x578ef0: record a game event for the replay (an event repeating the last one's
// type, frame and player replaces it) and apply it.
void Replay8650B8::Value578EF0(int type, int playerIndex, int value, bool record) {
    ReplayEvent event;
    event.type0 = type;
    event.frame4 = collisionFrame7FB724;
    event.player8 = playerIndex;
    event.valueC = value;
    if (value78 != -1 && value80 == 0 && value8C != 0) {
        if (record && eventCount40E4 > 0) {
            ReplayEvent *last = &events40EC[eventCount40E4 - 1];
            if (last->frame4 == event.frame4 && last->type0 == type && last->player8 == playerIndex)
                --eventCount40E4;
        }
        if (eventCount40E4 < 0x100) {
            ReplayEvent *entry = &events40EC[eventCount40E4];
            entry->type0 = type;
            entry->frame4 = event.frame4;
            entry->player8 = event.player8;
            entry->valueC = event.valueC;
            ++eventCount40E4;
        } else {
            Overflow571830();
        }
    }
    ApplyReplayEvent(&event);
}
