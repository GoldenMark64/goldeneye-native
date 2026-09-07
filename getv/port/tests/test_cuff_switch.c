/* Native cuff selection consumes six consecutive entries from ModelFileHeader.Switches.
 * The decomp converts that field into a real pointer array, so its byte stride must follow
 * sizeof(void *) rather than the N64's fixed four-byte pointer width. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static int fails;

static void check(int condition, const char *what)
{
    if (condition) {
        printf("  ok    %s\n", what);
    } else {
        printf("  FAIL  %s\n", what);
        fails++;
    }
}

/* Mirrors the GE_PORT_NATIVE boundary in bondviewSelectCuff(). */
static int cuff_switch_offset(int num_switches, int switch_index, size_t *offset)
{
    if (switch_index < 0 || num_switches < 6 || switch_index > num_switches - 6) {
        return 0;
    }

    *offset = (size_t)switch_index * sizeof(void *);
    return 1;
}

int main(void)
{
    void *switches[36];
    void **base;
    size_t offset = 0;
    size_t i;

    printf("native cuff switch window\n");
    for (i = 0; i < 36; i++) {
        switches[i] = (void *)(uintptr_t)(0x1000 + i * 0x10);
    }

    check(cuff_switch_offset(36, 29, &offset),
          "Throwing Knife/Trigger/Watch Laser six-entry window is accepted");
    check(offset == 29 * sizeof(*switches), "offset uses native pointer stride");
    base = (void **)((unsigned char *)switches + offset);
    check(base == &switches[29], "computed base is Switches[29]");
    check(base[5] == switches[34], "sixth access remains inside the array");

    check(cuff_switch_offset(35, 29, &offset), "exact final six-entry window is accepted");
    check(!cuff_switch_offset(34, 29, &offset), "short Throwing Knife window is rejected");
    check(!cuff_switch_offset(36, 31, &offset), "window extending past the array is rejected");
    check(!cuff_switch_offset(36, -1, &offset), "negative switch index is rejected");
    check(!cuff_switch_offset(5, 0, &offset), "array shorter than six entries is rejected");

    check(cuff_switch_offset(10, 4, &offset), "opening-watch six-entry window is accepted");
    base = (void **)((unsigned char *)switches + offset);
    check(base == &switches[4] && base[5] == switches[9],
          "opening-watch window selects entries 4 through 9");

    if (sizeof(void *) > 4) {
        check(29 * sizeof(*switches) != (29u << 2),
              "native stride differs from N64 four-byte arithmetic");
    }

    printf("\n%s\n", fails ? "FAILURES" : "all checks passed");
    return fails != 0;
}
