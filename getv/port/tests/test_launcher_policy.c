/* Windows launcher startup policy without a ROM, SDL, ImGui or a built game. */
#include <stdio.h>

#include "ge_launcher_policy.h"

static int failures;

static void check_i(const char *what, int got, int want)
{
    if (got == want) { printf("  ok    %s\n", what); }
    else { printf("  FAIL  %s: got %d want %d\n", what, got, want); failures++; }
}

int main(void)
{
    char *plain[] = { "goldeneye.exe", NULL };
    char *direct[] = { "goldeneye.exe", "--stage", "dam", NULL };
    char *explicit_launcher[] = { "goldeneye.exe", "--launcher", NULL };

    check_i("Windows double-click opens launcher",
            geLauncherPolicyWantsWindow(1, plain, 0, 0, 0, 1), 1);
    check_i("explicit GETV_LAUNCHER=0 bypasses launcher",
            geLauncherPolicyWantsWindow(1, plain, 1, 0, 0, 1), 0);
    check_i("launcher child does not reopen launcher",
            geLauncherPolicyWantsWindow(1, plain, 1, 0, 0, 1), 0);
    check_i("command-line gameplay remains direct",
            geLauncherPolicyWantsWindow(3, direct, 0, 0, 0, 1), 0);
    check_i("--launcher remains explicit",
            geLauncherPolicyWantsWindow(2, explicit_launcher, 1, 0, 0, 1), 1);
    check_i("GETV_LAUNCHER=1 opens launcher",
            geLauncherPolicyWantsWindow(1, plain, 1, 1, 0, 0), 1);
    check_i("autoplay still takes launcher path",
            geLauncherPolicyWantsWindow(1, plain, 0, 0, 1, 0), 1);
    check_i("other desktop platforms keep opt-in behavior",
            geLauncherPolicyWantsWindow(1, plain, 0, 0, 0, 0), 0);

    return failures ? 1 : 0;
}
