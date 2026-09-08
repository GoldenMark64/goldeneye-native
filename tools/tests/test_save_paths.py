#!/usr/bin/env python3
"""Compile the production save-path initializer with synthetic host services.

No SDL, decompilation, ROM, real HOME access, or save I/O is needed. Only the
initializer is extracted; save loading/flushing cannot execute in this harness.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HARNESS = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#define ge_errno errno
static const char *save_dir, *home_dir;
static int default_exists, legacy_exists, override_exists, mkdir_fails;
static int default_calls, stat_calls, mkdir_calls;
static char made_dir[1024], ge_eeprom_path[1024];
#define DEFAULT "/synthetic/home/Library/Application Support/Goldeneye-Native"
#define LEGACY "/synthetic/home/Library/Application Support/GoldenEyeTV"
#define OVERRIDE "/synthetic/isolated"
static char *fake_getenv(const char *key) {
    if (!strcmp(key, "HOME")) return (char *)home_dir;
    if (!strcmp(key, "GETV_SAVEDIR")) return (char *)save_dir;
    abort();
}
static int fake_stat(const char *path, struct stat *st) {
    (void)st;
    stat_calls++;
    if (!strcmp(path, DEFAULT "/eeprom.bin")) return default_exists ? 0 : -1;
    if (!strcmp(path, LEGACY "/eeprom.bin")) return legacy_exists ? 0 : -1;
    if (!strcmp(path, OVERRIDE "/eeprom.bin")) return override_exists ? 0 : -1;
    abort();
}
static int gePortUserDataDir(const char *org, const char *app, char *out, size_t size) {
    (void)org; (void)app;
    default_calls++;
    if (!home_dir || !*home_dir) return -1;
    snprintf(out, size, "%s", DEFAULT);
    return 0;
}
static int gePortMakeDir(const char *path, unsigned mode) {
    if (mode != 0755) abort();
    mkdir_calls++;
    snprintf(made_dir, sizeof(made_dir), "%s", path);
    errno = EACCES;
    return mkdir_fails ? -1 : 0;
}
#define getenv fake_getenv
#define stat(...) fake_stat(__VA_ARGS__)
/* PRODUCTION_INITIALIZER */
static int run_case(const char *name, const char *override, const char *home,
                    int current, int legacy, int existing_override, int fail_mkdir,
                    const char *expected, int expected_defaults, int expected_stats) {
    save_dir = override; home_dir = home;
    default_exists = current; legacy_exists = legacy;
    override_exists = existing_override; mkdir_fails = fail_mkdir;
    default_calls = stat_calls = mkdir_calls = 0;
    ge_eeprom_path[0] = made_dir[0] = '\0';
    int result = geSavePathInit();
    char expected_file[1024] = "";
    if (expected) snprintf(expected_file, sizeof(expected_file), "%s/eeprom.bin", expected);
    if (result != (expected ? 0 : -1) || strcmp(ge_eeprom_path, expected_file) ||
        default_calls != expected_defaults || stat_calls != expected_stats ||
        mkdir_calls != (expected || fail_mkdir ? 1 : 0) ||
        (expected && strcmp(made_dir, expected)) ||
        (fail_mkdir && strcmp(made_dir, OVERRIDE))) {
        fprintf(stderr, "FAIL %s: result=%d path=%s defaults=%d stats=%d mkdir=%s\n",
                name, result, ge_eeprom_path, default_calls, stat_calls, made_dir);
        return 1;
    }
    printf("PASS %s\n", name);
    return 0;
}
int main(void) {
    int failures = 0;
    failures += run_case("empty override directory ignores legacy", OVERRIDE, "/synthetic/home", 0, 1, 0, 0, OVERRIDE, 0, 0);
    failures += run_case("existing override remains selected", OVERRIDE, "/synthetic/home", 1, 1, 1, 0, OVERRIDE, 0, 0);
    failures += run_case("override without HOME", OVERRIDE, NULL, 0, 1, 0, 0, OVERRIDE, 0, 0);
    failures += run_case("override with empty HOME", OVERRIDE, "", 0, 1, 0, 0, OVERRIDE, 0, 0);
    failures += run_case("legacy fallback", NULL, "/synthetic/home", 0, 1, 0, 0, LEGACY, 1, 2);
    failures += run_case("default wins over legacy", NULL, "/synthetic/home", 1, 1, 0, 0, DEFAULT, 1, 1);
    failures += run_case("no existing saves", NULL, "/synthetic/home", 0, 0, 0, 0, DEFAULT, 1, 2);
    failures += run_case("empty override value retains migration", "", "/synthetic/home", 0, 1, 0, 0, LEGACY, 1, 2);
    failures += run_case("missing HOME disables default", NULL, NULL, 0, 1, 0, 0, NULL, 1, 0);
    failures += run_case("empty HOME disables default", NULL, "", 0, 1, 0, 0, NULL, 1, 0);
    failures += run_case("unwritable override does not fall back", OVERRIDE, "/synthetic/home", 0, 1, 0, 1, NULL, 0, 0);
    return failures ? 1 : 0;
}
'''


class SavePathTests(unittest.TestCase):
    def test_production_initializer(self):
        source = (ROOT / "getv/port/src/port_save.c").read_text()
        start = source.index("static int geSavePathInit(void)")
        end = source.index("static void geSaveLoad(void)", start)
        initializer = source[start:end]
        with tempfile.TemporaryDirectory(prefix="ge-save-path-test-") as temporary:
            directory = Path(temporary)
            harness = directory / "save_paths.c"
            harness.write_text(HARNESS.replace("/* PRODUCTION_INITIALIZER */", initializer))
            for mac in (False, True):
                with self.subTest(mac_diagnostic=mac):
                    executable = directory / "save_paths"
                    command = [os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror=return-type"]
                    if mac:
                        command.append("-DGE_PLATFORM_MAC")
                    subprocess.run(command + [str(harness), "-o", str(executable)], check=True)
                    result = subprocess.run([str(executable)], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual(sum(line.startswith("PASS ") for line in result.stdout.splitlines()), 11)
                    print(f"PASS save paths: 11 scenarios (GE_PLATFORM_MAC={mac})", flush=True)


if __name__ == "__main__":
    unittest.main()
