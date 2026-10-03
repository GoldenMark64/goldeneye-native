#include <stdio.h>

#include "../src/ge_audio_queue_policy.h"

static int checks;
static int failures;

static void check(const char *name, int got, int want)
{
    checks++;
    if (got == want) {
        printf("PASS %s got=%d\n", name, got);
    } else {
        failures++;
        printf("FAIL %s got=%d want=%d\n", name, got, want);
    }
}

int main(void)
{
    enum {
        frame_samples = 736,
        min_frame = frame_samples - 16,
        max_frame = frame_samples + 0x25 + 16,
        target = frame_samples * 4,
    };

    check("zero bytes means empty queue",
          geAudioQueuedFramesFromBytes(0), 0);
    check("stereo S16 byte count converts to frames",
          geAudioQueuedFramesFromBytes(11776), 2944);
    check("partial bytes truncate to complete frames",
          geAudioQueuedFramesFromBytes(7), 1);

    check("empty queue refills by aligned maximum",
          geAudioWantForQueued(0, target, min_frame, max_frame), 784);
    check("normal 736-frame deficit stays 736",
          geAudioWantForQueued(target - 736, target, min_frame, max_frame), 736);
    check("small deficit is raised to minimum synth frame",
          geAudioWantForQueued(target - 128, target, min_frame, max_frame), min_frame);
    check("target queue produces no audio",
          geAudioWantForQueued(target, target, min_frame, max_frame), 0);
    check("queue ahead of target produces no audio",
          geAudioWantForQueued(target + 512, target, min_frame, max_frame), 0);

    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
