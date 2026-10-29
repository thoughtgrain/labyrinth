# Theseus web prototype

`theseus_phalanx.html` is a playable page. It runs the real `meikyu_core.c`, `theseus.c` and `theseus_screen.c`, compiled to WebAssembly together with `proto.c`. The page embeds that wasm module and draws every line the screen code emits onto a canvas over `theseus.svg`.

`proto.c` stands in for the OP-1 parts the plugins lean on:
- six voices, round robin
- an instant-attack, long-release envelope
- a soft clip on the output

To rebuild the module, run this from `hundekraft-3.0.3-source/ext/kennel/synth_sdk` with clang 18 (it has the wasm32 target built in):

```sh
P=$PWD; cd /path/to/web-prototype
clang --target=wasm32 -std=c99 -O2 -ffreestanding -nostdlib -DOP1_HOST -Iinc -I$P/plugins/theseus/host -I$P \
  -I$P/plugins/theseus -I$P/plugins/meikyu -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
  -o proto.wasm proto.c $P/plugins/meikyu/meikyu_core.c
base64 -w0 proto.wasm   # paste over WASM_B64 in theseus_phalanx.html
```
