"""PlatformIO post-build hook: copy firmware.bin -> HP12C.bin.

Keeps PIO's default `firmware.bin` (so internal tooling and OTA paths still
work) and produces a side-by-side `HP12C.bin` ready to upload to the
Launcher's SD `/apps/` folder.
"""
import shutil
from pathlib import Path

Import("env")  # type: ignore  # provided by PlatformIO at script load

APP_BIN_NAME = "HP12C.bin"


def _rename(target, source, env):
    build_dir = Path(env.subst("$BUILD_DIR"))
    src = build_dir / "firmware.bin"
    dst = build_dir / APP_BIN_NAME
    if src.exists():
        shutil.copy2(src, dst)
        print(f"post_build: wrote {dst}")


env.AddPostAction("$BUILD_DIR/firmware.bin", _rename)
