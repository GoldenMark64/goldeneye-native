#ifndef GE_LAUNCHER_POLICY_H
#define GE_LAUNCHER_POLICY_H

#include <string.h>

/* Decide whether this process should show the settings launcher before gameplay.
 *
 * Kept independent of SDL/ImGui so the player-facing startup policy has a ROM-free unit test.
 * `launcher_env_present` matters separately from its value: GETV_LAUNCHER=0 is the explicit
 * escape hatch for scripts and for the child process created after the user presses Start.
 */
static int geLauncherPolicyWantsWindow(int argc, char *const argv[],
                                       int launcher_env_present,
                                       int launcher_env_enabled,
                                       int autoplay_enabled,
                                       int default_on_plain_start)
{
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--launcher") == 0) return 1;
    }
    if (launcher_env_enabled || autoplay_enabled) return 1;

    return default_on_plain_start && !launcher_env_present && argc == 1;
}

#endif
