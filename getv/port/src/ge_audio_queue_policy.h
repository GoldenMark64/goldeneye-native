#ifndef GE_AUDIO_QUEUE_POLICY_H
#define GE_AUDIO_QUEUE_POLICY_H

static inline int geAudioQueuedFramesFromBytes(unsigned queued_bytes)
{
    return (int)(queued_bytes / 4U);
}

static inline int geAudioWantForQueued(int queued, int target, int min_frame, int max_frame)
{
    int want = target - queued;

    if (want > max_frame) {
        want = max_frame;
    }

    want &= ~0xf;

    if (want < min_frame) {
        if (queued >= target) {
            return 0;
        }
        want = min_frame;
    }

    return want;
}

#endif
