# FSM compiler per 39F010A

Aquest directori inclou un compilador declaratiu per generar la ROM de control d'una FSM implementada amb `SST39SF010A` + `74HC374`.

## Fitxers

- `compile_fsm.py`: converteix una especificació JSON en imatge ROM (`.bin`, opcional `.hex` i format Logisim).
- `sim_trace.py`: simula la FSM amb una traça d'entrades i imprimeix estat/regla/sortides.
- `examples/ft245_msx_bridge.json`: esquelet inicial per bridge `MSX I/O` <-> `FT245`.
- `examples/demo_trace.json`: traça curta per provar el simulador.

## Model

- Adreça de ROM = entrada de la funció de transició+sortida
  - `A0..A2`: `state` (retornats pel latch `74HC374`)
  - `A3..A7`: senyals assíncrones (`FT_RXF_L`, `FT_TXE`, `WR_L`, `RD_L`, `IORQ_L`)
  - `A8..A15`: `msx_a0..msx_a7` (decodificació de port)
- Dades de ROM
  - `D0..D2`: `next_state`
  - `D3..D6`: `FT_RD_L`, `FT_WR`, `DIR_L`, `EBUS_L` (`D7` reservat)

## Compilar la ROM

```bash
python3 scripts/fsm/compile_fsm.py \
  scripts/fsm/examples/ft245_msx_bridge.json \
  --out-bin build/fsm/ft245_fsm.bin \
  --out-hex build/fsm/ft245_fsm.hex \
  --out-logisim build/fsm/ft245_fsm_logisim.mem \
  --mirror-39sf010a \
  --out-bin-128k build/fsm/ft245_fsm_39sf010a.bin
```

Per `Logisim-evolution`, carrega `build/fsm/ft245_fsm_logisim.mem` des de `ROM -> Load Image...`.

## Simular una traça

```bash
python3 scripts/fsm/sim_trace.py \
  scripts/fsm/examples/ft245_msx_bridge.json \
  scripts/fsm/examples/demo_trace.json \
  --initial-state IDLE
```

## DSL de regles

Cada regla pot contenir:

- `name`: etiqueta humana
- `state`: estat únic o llista d'estats on aplica
- `when`: expressió booleana
- `next`: estat següent
- `controls`: assignacions de sortida
- `stop`: si és `true` talla l'avaluació de regles (per defecte `true`)

Variables disponibles a `when`:

- `state`: id numèric d'estat actual
- `addr`: adreça actual de ROM (0..65535)
- `port`: valor de `msx_a0..msx_a7`
- totes les senyals declarades a `address_layout.input_signals`

Operadors admesos a `when`:

- lògics: `and`, `or`, `not`
- comparació: `==`, `!=`, `<`, `<=`, `>`, `>=`
- bits/aritmètica: `&`, `|`, `^`, `<<`, `>>`, `+`, `-`, `*`, `//`, `%`, `~`

## Notes de maquinari

- El compilador genera la taula de veritat, però no substitueix validació temporal (`RD#/WR#` pulse width, hold/setup).
- La sortida per defecte ha de ser segura (`RD#=1`, `WR#=1`, etc.) per evitar glitches destructius.
