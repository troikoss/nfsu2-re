// Collision notifications to the audio system and the game message queue.
#include "../../Audio/AudioSystem.h"
#include "../../../Libs/bWare/bSlotPool.h"

extern int audioEnabled7FAAA8;
extern void *gameMessagePool883E38;
extern int gameMessageCount876DC8;

// Game message (0x24 bytes) queued by 0x5e0360.
struct GameMessage {
    GameMessage *next0;
    GameMessage *previous4;
    int value8;
    int typeC;
    int unidentified10[5];
};

struct GameMessageList {
    GameMessage *head0;
    GameMessage *tail4;
};
extern GameMessageList *gameMessages800ECC;

// Original 0x483e20: hand a new collision event to the collision sound set.
void Audio82B884::RegisterCollision(CollisionEvent *event) {
    if (audioEnabled7FAAA8) collisions3C->Add481CD0(event);
}

// Original 0x483e40.
void Audio82B884::UnregisterCollision(CollisionEvent *event) {
    if (audioEnabled7FAAA8) collisions3C->Remove481E50(event);
}

// Original 0x5e0360: allocate a cleared message of `type` and append it to the queue.
GameMessage *PostGameMessage(int type) {
    GameMessage *message = static_cast<GameMessage *>(bOMalloc(gameMessagePool883E38));
    if (!message) return 0;
    int *words = reinterpret_cast<int *>(message);
    for (int i = 0; i < 9; ++i) words[i] = 0;
    message->value8 = 0;
    message->typeC = type;
    GameMessageList *list = gameMessages800ECC;
    GameMessage *tail = list->tail4;
    tail->next0 = message;
    list->tail4 = message;
    message->previous4 = tail;
    message->next0 = reinterpret_cast<GameMessage *>(list);
    ++gameMessageCount876DC8;
    return message;
}
