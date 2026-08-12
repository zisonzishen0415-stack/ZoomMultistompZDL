#!/usr/bin/env python3
"""Build K9Probe.ZDL.

Hardware probe for the LineSel-cloned synthesized edit-handler path on
knobs 4..9 (pages 2/3). Knobs 1..3 use the hardware-proven stock blobs
(LineSel knob1/knob2 from linesel_handlers.bin, AIR knob3 from
air_knob3_edit.bin); knobs 4..9 use linker.py's synthesized LineSel
clones (synthesize_linesel_edit_handlers=True, synth_edit_start_index=3).

TI compiler root: use $TI_CGT_ROOT if set, otherwise the macOS default
used by the rest of this repo.
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

from airwindows_image import make_airwindows_tape_screen  # noqa: E402
from linker import LinkerConfig, link, params_from_manifest  # noqa: E402
from manifest_params import write_param_header  # noqa: E402

TI_ROOT = Path(os.environ.get(
    "TI_CGT_ROOT",
    "/Applications/ti/ccs2050/ccs/tools/compiler/ti-cgt-c6000_8.5.0.LTS",
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
    write_param_header(manifest, HERE / "k9probe_params.h", "K9PROBE")

    src_c = HERE / "k9probe.c"
    obj = HERE / "k9probe.obj"
    out_zdl = ROOT / "build" / "probes" / f"{manifest['effect_name']}.ZDL"
    out_zdl.parent.mkdir(exist_ok=True)

    print(f"[k9probe] compiling {src_c.name} -> {obj.name} with {CL6X}")
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
        screen_image=make_airwindows_tape_screen("K9", "Probe"),
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

    print(f"\n[k9probe] done -> {out_zdl}")


if __name__ == "__main__":
    main()
