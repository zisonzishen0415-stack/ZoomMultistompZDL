#!/usr/bin/env python3
"""Build Reverson.ZDL from reverson_zdl.c + manifest.json.

9-knob config cloned from K9Probe (the hardware-proven knob layout):
knobs 1..3 use the stock blobs (LineSel knob1/knob2 + AIR knob3), knobs
4..9 use the linker's synthesized LineSel clones. State lives in ctx[3].
TI compiler root: $TI_CGT_ROOT, or the local Windows default below.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent.parent
sys.path.insert(0, str(ROOT / "build"))
sys.path.insert(0, str(ROOT / "src" / "airwindows" / "common"))

from airwindows_image import make_airwindows_reverb_screen  # noqa: E402
from linker import LinkerConfig, link, params_from_manifest  # noqa: E402
from manifest_params import write_param_header  # noqa: E402

TI_ROOT = Path(os.environ.get(
    "TI_CGT_ROOT",
    "C:/Users/34723/Downloads/ti-cgt-c6000_8.5.0.LTS",
))
CL6X = TI_ROOT / "bin" / "cl6x"

CFLAGS = [
    "--c99",
    "--opt_level=2",
    "-mv6740",
    "--abi=eabi",
    "--mem_model:data=far",
    f"--include_path={TI_ROOT}/include",
]


def main() -> None:
    manifest = json.loads((HERE / "manifest.json").read_text())
    write_param_header(manifest, HERE / "reverson_params.h", "REVERSON")

    src_c = HERE / "reverson_zdl.c"
    obj = HERE / "reverson_zdl.obj"
    out_zdl = ROOT / "dist" / f"{manifest['effect_name']}.ZDL"
    out_zdl.parent.mkdir(exist_ok=True)

    print(f"[reverson] compiling {src_c.name} -> {obj.name} with {CL6X}")
    subprocess.run(
        [str(CL6X), *CFLAGS, "-c", str(src_c), f"--output_file={obj}"],
        check=True,
        cwd=HERE,
    )

    for junk in ("compiler.opt", "linker.cmd"):
        p = HERE / junk
        if p.exists():
            p.unlink()

    cfg = LinkerConfig(
        effect_name=manifest["effect_name"],
        audio_func_name=manifest.get("audio_func_name"),
        gid=manifest["gid"],
        fxid=manifest["fxid"],
        params=params_from_manifest(manifest["params"]),
        obj_path=obj,
        output_path=out_zdl,
        fxid_version=manifest.get("fxid_version", "1.00").encode("ascii"),
        flags_byte=manifest.get("flags_byte", 0x01),
        screen_image=make_airwindows_reverb_screen("Reverson"),
        # Knobs 1..3: proven stock blobs (LineSel knob1/knob2 + AIR knob3).
        handler_blob_path=ROOT / "build" / "linesel_handlers.bin",
        knob3_blob_path=ROOT / "build" / "air_knob3_edit.bin",
        # Knobs 4..9 (params index 3..8): LineSel-cloned synthetic handlers.
        synthesize_linesel_edit_handlers=True,
        synth_edit_start_index=3,
        use_object_edit_handlers=False,
        audio_nop=manifest.get("audio_nop", False),
    )
    link(cfg)

    print(f"\n[reverson] done -> {out_zdl}")


if __name__ == "__main__":
    main()
