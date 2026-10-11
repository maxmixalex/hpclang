#ifndef HPCLANG_STORAGE_MEDIA_EVENTS_H
#define HPCLANG_STORAGE_MEDIA_EVENTS_H

#include <stdint.h>

struct hpclang_storage_media_events
{
    uint32_t pin, trigger, mask, route;
    unsigned owned;
};

/* Private SD-controller lease. Before acquisition, GPIO queries stay read-only.
 * While owned, EINT3 records removal edges even between software observations.
 * Card-event pending bits belong to the lease and are acknowledged; unrelated
 * pending sources are preserved. The namespace must run Stock ResetSD after
 * releasing this lease so Stock reconciles the current card, not stale events. */
int hpclang_storage_media_events_acquire(struct hpclang_storage_media_events *events);
int hpclang_storage_media_events_changed(struct hpclang_storage_media_events *events);
void hpclang_storage_media_events_release(struct hpclang_storage_media_events *events);

#endif
