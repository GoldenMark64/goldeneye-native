/* ROM-free fixtures invented for issue #30, not copied from a stage setup.
 * Setup words have already been converted to host endian before the game reads
 * these fields. Copy host-value words, not a byte sequence or field initializer:
 * either of those could accidentally test a different serialization contract.
 */
#include <stdio.h>
#include "bondtypes.h"

int main(void)
{
    const u32 words[] = {0xffff0000u, 0x01230007u, 0x02460000u, 0x00000009u};
    const u16 models[] = {0xffff, 0x0123, 0x0246, 0};
    const u16 quantities[] = {0, 7, 0, 9};
    struct multiammocrateslot slots[4];
    int failures = 0;

    if (sizeof(slots[0]) != 4 || sizeof(slots) != sizeof(words) ||
        (char *)&slots[1] - (char *)&slots[0] != 4) {
        printf("FAIL slot size/stride: expected 4 bytes, got %u/%u\n",
            (unsigned)sizeof(slots[0]),
            (unsigned)((char *)&slots[1] - (char *)&slots[0]));
        return 1; /* Do not copy words into an incorrectly sized destination. */
    }
    printf("PASS four-byte slot size and array stride\n");
    __builtin_memcpy(slots, words, sizeof(words));
    for (unsigned i = 0; i < 4; i++) {
        if (slots[i].modelnum != models[i] || slots[i].quantity != quantities[i]) {
            printf("FAIL slot %u: expected model=%u quantity=%u, got model=%u quantity=%u\n",
                i, (unsigned)models[i], (unsigned)quantities[i],
                (unsigned)slots[i].modelnum, (unsigned)slots[i].quantity);
            failures++;
        } else {
            printf("PASS slot %u model=%u quantity=%u\n", i,
                (unsigned)slots[i].modelnum, (unsigned)slots[i].quantity);
        }
    }
    return failures != 0;
}
