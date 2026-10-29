# Web instrument

`docs/play.html` is the playable page, served by GitHub Pages. It runs `meikyu_core.c` and `meikyu_screen.c`, `toro.c` and `toro_screen.c`, and `theseus.c` and `theseus_screen.c`, all unchanged, compiled to WebAssembly together with `glue.c`.

The glue stands in for the OP-1 parts:
- one synth slot that plays toro or theseus
- six voices, round robin
- a simple envelope for each engine
- a soft clip on the output

The page draws each plugin's static SVG and then every line its screen code emits on a canvas. It feeds toro's moving nodes (rig, rod, shadow) as SVG transforms.

To rebuild, run this from `hundekraft-3.0.3-source/ext/kennel/synth_sdk` with clang 18 and wasm-ld. Set `D` to this folder and `PL` to the folder that holds `meikyu/`, `toro/` and `theseus/`:

```sh
P=$PWD
CF="--target=wasm32 -std=c99 -O2 -ffreestanding -nostdlib -DOP1_HOST -I$D/inc -I$PL/toro/host -I$P -include $D/proto_host.h"
clang $CF -I$PL/toro    -c $D/tu_toro.c    -o tu_toro.o
clang $CF -I$PL/theseus -c $D/tu_theseus.c -o tu_theseus.o
clang $CF -I$PL/meikyu  -c $D/tu_meikyu.c  -o tu_meikyu.o
clang $CF -I$PL/meikyu  -c $PL/meikyu/meikyu_core.c -o core.o
clang $CF -I$PL/meikyu  -c $D/glue.c -o glue.o
wasm-ld --no-entry --export-all --allow-undefined -o device.wasm glue.o tu_toro.o tu_theseus.o tu_meikyu.o core.o
```

Then fill the `%%WASM%%` placeholder in `page.html` with the base64 of `device.wasm`, and each `%%SVG_*%%` placeholder with that plugin's SVG (without the XML declaration).
