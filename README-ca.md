# Introducció
Els sistemes MSX van aparèixer el 1983 i estaven basats en un processador Zilog Z80 i estaven basats en un maquinari amb una arquitectura estàndard. Aquests sistemes eren relativament senzills i estaven pensats per arrencar software de consum, bàsicament jocs.

A meitat de camí entre un experiment excèntric i un desafiament personal, volem dur a terme un projecte que faci que sigui possible, en última instància, establir una plataforma per avaluar problemes de maquinari del propi MSX, executar, monitoritzar i transferir dades amb sistemes externs moderns.

* L'execució de processos executats per multiplexació per temps basats en el senyal d'interrupció nadiu de MSX (VDP) cada 20 ms (50 Hz): un sistema operatiu de temps real (RTOS)
* L'establiment d'un canal de comunicació amb sistemes externs, que permet la transmissió de dades entre el port d'expansió MSX i un port USB
* Permetre l'ampliació de nous processos i funcionalitats, monitoritzats des de sistemes moderns.

Dividim el projecte en dues fases de desenvolupament que poden dur-se a terme de manera independent. D'una banda, el programari que permetrà les característiques plantejades i de l'altra, el desenvolupament de la interfície de maquinari (i el seu programari associat) que haurà de permetre la pretesa comunicació. La secció `zbridge` conté informació sobre el disseny de la interfície de hardware desenvolupat i el seu programari.

## Arquitectura del MSX
En primer lloc, cal conèixer els principis bàsics de funcionament d'un sistema MSX. Una mirada superficial apreciarà els aspectes més evidents, com ara la simplicitat de funcionament. Tanmateix, la seva aparent simplicitat imposa fortes restriccions a l'hora d'assolir els objectius previstos.

Aquests sistemes estaven pensats per executar programes de manera directa i de forma ràpida. Després d'un _power on_, el MSX executa una fase de _startup_ de codi ubicat en una ROM. Aquest processador sempre llegeix la primera instrucció a l'adreça `0x0000`. És a dir, el primer codi que llegeix és a les primeres adreces. Aquest conté rutines que només s'executen en el moment del _startup_ i també d'altres que poden executar-se en una fase posterior per altres programes. Les tasques que es duen a terme durant aquesta fase inicial són bàsicament la inicialització del maquinari i tests de memòria RAM.

El processador Z80 té un bus de dades de 8 bits i un d'adreces de 16 bits. En conseqüència, pot accedir a 65536 adreces de memòria. Els sistemes MSX venien equipats amb entre 16KB i 64KB de memòria RAM, però podien ser ampliats en recursos i per tant, accedir a més de 64KB de memòria. Els enginyers que van crear el MSX varen incorporar una funció que permet exposar rangs d'adreces de memòria _visibles_ pel processador a maquinari extern. El que van fer és crear els conceptes d'_slots_ i _pages_. La idea és presentar al Z80 un **espai d'adreces** com interfície única d'intercanvi d'informació (instruccions o dades) amb l'exterior. No hem de confondre aquest espai d'adreces amb una memòria física concreta, sinó més aviat com una capa d'abstracció. Aquest espai està dividit en 4 subespais anomenats _pages_, que consten d'un interval d'adreces consecutives de 16KB cadascun:

| Pàgina | Interval d'adreces |
|:---|:---|
| 0 | `0x0000-0x3FFF` |
| 1 | `0x4000-0x7FFF` |
| 2 | `0x8000-0xBFFF` |
| 3 | `0xC000-0xFFFF` |

[L'estàndard MSX](https://map.grauw.nl/resources/system/msxtech.pdf) no defineix amb precisió el concepte de _slot_. En aquest document el definim com **un conjunt de recursos de maquinari que poden ser accessibles pel Z80 a través d'una adreça de 16 bits**. [L'estàndard MSX](https://map.grauw.nl/resources/system/msxtech.pdf) permet fins a 4 _slots_, indexats del 0 al 3. Els _slots_ també poden ser dividits en subespais adreçables de 16KB (pàgines). A través dels _slots_ el processador pot dur a terme:

* Processos d'accés a memòria en qualsevol dels _slots_
* Processos I/O a dispositius externs, vinculats a un _slot_

A través d'un procediment anomenat _Memory Switching_, és possible vincular les pàgines amb els _slots_, a través de crides I/O a un dispositiu també nadiu del MSX anomenat PPI. Aquesta part del maquinari és la que estableix els canals de comunicació entre les pàgines de l'espai adreçable i les pàgines de recursos. Podem imaginar el _Memory Switching_ com l'establiment de canals entre elles. **Els canals només es poden establir entre pàgines amb el mateix índex**. Quan acaba, el processador pot continuar amb les instruccions (_Instruction Fetch_).

Tant el procés que estableix un _Memory Switching_ com el que executa tasques orientades a usuari han de satisfer certes condicions. Per exemple, si s'executa un _Memory Switching_ tal que els registres PC o SP del Z80 apunten a una adreça d'una pàgina commutada, el Z80 no serà capaç de continuar l'execució del programa original. **És responsabilitat del programador executar el _Memory Switching_ sobre pàgines no referenciades pels registres de context PC i SP.**

L'estàndard preveu l'existència d'un _slot 0_, que incorpora certs recursos de manera interna o integrada en el sistema. La seva configuració concreta no s'especifica, però sembla que com a mínim ha d'incorporar 32KB de ROM. Els _slots_ diferents del 0 poden tenir una projecció física en forma de port d'expansió. En aquest aspecte, l'estàndard era flexible i cada fabricant podria habilitar configuracions de maquinari diferents. Per fixar idees, mostrem tot seguit un exemple de procediment de _Memory Switching_.

Si el Z80 executa:
```
out (0xAB), 0x82
```
Aquesta instrucció estableix el mode d'operació del subsistema PPI. Aquest compta amb tres ports interns connectats a diferents agrupacions de maquinari intern: A, B i C. Els ports poden transferir dades entre el maquinari i el Z80. El port rellevant durant el _Memory Switching_ és l'A, i està vinculat a qualsevol dispositiu diferent del teclat i la unitat de _cassette_. En concret, el que fem aquí és configurar el port A en mode _normal_, amb el sentit de les dades del Z80 a l'_exterior_ (output).

Tot i que les configuracions dels ports B i C no són rellevants en aquest projecte, les descriurem breument. El port B, està vinculat al maquinari de teclat. En concret, li diem que es posi en mode _normal_, i en sentit del teclat cap al Z80. El port C està vinculat a la unitat de _cassette_ i al LED de la tecla de majúscules. De fet, la meitat del bus del port està dedicat al _cassette_ i l'altra al LED. També es configurarà en mode _normal_ en el sentit Z80 cap a l'_exterior_.

Si el Z80 executa:

```
out (0xA8), 0x00
```
es realitza el _Memory Switching_. L'establiment dels canals entre pàgines es fa a través del _Primary Slot Register_ (PSR), és a dir, un valor de 8 bits dividit en 4 grups de 2 bits. Cada parella de 2 bits (4 possibles valors) representa el número de _slot_ al qual van connectades les pàgines: `espai d'adreces <-> _slot_`. La posició dels 2 bits representa el número de pàgina. En aquest cas, el _Primary Slot Register_ val `0x00`. Això vol dir:

```
Ordre del bit: 7  6  5  4  3  2  1  0
          PSR: 0  0  0  0  0  0  0  0
          ---------------------------
         Slot: 0     0     0     0
         Page: 3     2     1     0
```
El _Memory Switching_ estableix doncs totes les pàgines de l'espai d'adreces al mateix _slot_. Aquesta és de fet, la configuració per defecte que adquireix el maquinari en el moment de l'inici del _power on_.

És important notar que la configuració de maquinari dels _slots_ depèn del model de MSX concret. Per exemple, els MSX HB-55P de Sony incorporava aquesta configuració de _slot 0_:
| Pàgina | Interval d'adreces | Recurs |
|:---|:---|:---|
| 0 | `0x0000-0x3FFF` | ROM |
| 1 | `0x4000-0x7FFF` | ROM Basic |
| 2 | `0x8000-0xBFFF` | ROM Personal Data Bank |
| 3 | `0xC000-0xFFFF` | RAM |

És a dir, el HB-55P té 16KB de RAM nadiua instal·lada a _slot 0_. El MSX HB-75P en canvi, tenia aquesta configuració de _slot 0_:

| Pàgina | Interval d'adreces | Recurs |
|:---|:---|:---|
| 0 | `0x0000-0x3FFF` | ROM |
| 1 | `0x4000-0x7FFF` | ROM Basic |
| 2 | `0x8000-0xBFFF` | ROM Personal Data Bank |
| 3 | `0xC000-0xFFFF` | unassigned |

En aquest cas, Sony va reservar un slot intern complet (_slot 2_) per la RAM i assolir els 64KB:

| Pàgina | Interval d'adreces | Recurs |
|:---|:---|:---|
| 0 | `0x0000-0x3FFF` | RAM |
| 1 | `0x4000-0x7FFF` | RAM |
| 2 | `0x8000-0xBFFF` | RAM |
| 3 | `0xC000-0xFFFF` | RAM |

El Philips VG-8010 conté un únic _slot_ intern. Els _slots_ 1 i 2 són extern (expansió). El _slot 0_ té el següent mapa:

| Pàgina | Interval d'adreces | Recurs |
|:---|:---|:---|
| 0 | `0x0000-0x3FFF` | ROM |
| 1 | `0x4000-0x7FFF` | ROM |
| 2 | `0x8000-0xBFFF` | RAM |
| 3 | `0xC000-0xFFFF` | RAM |

# Disseny del Sistema Operatiu
Una de les primeres decisions que incorpora el projecte és la substitució de la memòria ROM original per una de nova que incorpori les rutines bàsiques de _boot_. Per tal de facilitar els cicles de desenvolupament sobre hardware original, dissenyem una nova placa d'expansió que incorpori una memòria flaix i una de RAM, atès que les memòries EPROM o EEPROM són força més cares que les flaix i permeten emmagatzemar menys dades. Aquesta placa anirà instal·lada un _slot_ de _cartridge_. Però si volem que aquesta flaix proporcioni al Z80, el codi de _startup_ haurem d'extraure la ROM original i interceptar el senyal de sel·lecció de ROM, atès que el _slot 0_ és el que està activat quan en el MSX experimenta un _power on_. El codi que conté la pròpia flaix pot dur a terme un _memory switching_ per fer accessibles pàgines de RAM sobre la mateixa placa. És a dir, seria possible executar tot el programari sense necessitar la RAM integrada.

Aquesta nova targeta està basada en la memòria flaix multi propòsit [39SF010A](https://ww1.microchip.com/downloads/aemDocuments/documents/MPD/ProductDocuments/DataSheets/SST39SF010A-SST39SF020A-SST39SF040-Data-Sheet-DS20005022.pdf) de tipus CMOS. Té una capacitat de 1048576 bits amb bus d'accés de dades de 8 bits. L'espai adreçable és de 17 bits, que permet reservar dos espais adjacents de 65536 bits (64KB). Concretament:

* De `0x00000` a `0x0FFFF` : `bootloader` seleccionat quan el senyal `ROM_OE` és generat
* De `0x10000` a `0x1FFFF` : `startup`, RTOS i processos d'usuari

Al nucli, `ztick` implementa un RTOS preventiu de mida molt reduïda, pensat pels recursos limitats d'un Z80. El _tick_ del planificador prové directament de la interrupció nativa del VDP del MSX que s'esdevé cada 20 ms (50 Hz). A cada interrupció, el nucli desa el context de la tasca en execució, decideix quina tasca ha d'executar-se a continuació i continua la seva execució. L'algorisme és un _round-robin_ amb pesos: cada task té un `weight` (`1..3`) que determina quants _ticks_ consecutius manté la CPU abans de cedir el torn a la següent task preparada. Les tasques també poden bloquejar-se voluntàriament esperant un semàfor o una cua de l'IPC; mentre estan bloquejades, el planificador les salta sense consumir-hi temps de CPU. El nombre màxim de tasques és fix i es defineix en temps de compilació, i cadascuna té reservats per endavant el seu propi _stack_ i _heap_ de mida fixa. No hi ha reserva dinàmica de recursos, la qual cosa fa que el comportament del sistema sigui predictible i evita fragmentació. Les tasques es defineixen en un registre estàtic i s'arrenquen automàticament en el _boot_ segons el manifest del target (`BOOT_AUTOSTART`), o manualment des de la shell `xsh` (`start`/`stop`). La comunicació amb sistemes externs (el _host_) es gestiona als nivells `zlink`/`zbus`, descrits a continuació.

## `zlink`
El sistema MSX presenta limitacions a l'hora de transferir i rebre dades per I/O amb Z80. No incorpora cap interfície de maquinari que permeti detectar l'arribada de dades noves i activar interrupcions especialitzades. Per resoldre-ho, fem servir un protocol de capa d'enllaç anomenat `zlink`, situat sota `zbus`, per resoldre RX sense senyals de tipus `DATA_READY`.

Per tal de detectar la disponibilitat d'una trama nova respecte una ja processada, `zlink` fa servir número de seqüència (`SEQ`). També permet multiplexar diversos canals tipus `TTY`, en concret del `TTY0` al `TTY15`.

El _header_ de `zlink` està format per aquest _bytes_:

- `B0 = SOF5|TYPE3`
  - `SOF5` (bits `7..3`) = `0b10101`
  - `TYPE` (bits `2..0`):
    - `0` `POLL`
    - `1` `EMPTY`
    - `2` `DATA`
    - `3` `ACK`
    - `4` `NACK`
    - `5..7` reservat
- `B1 = TTY8`
  - `TTY` (`0..15`; bits alts reservats)
- `B2 = LEN8`
  - `LEN` (`0..64`)
- `B3 = SEQ` (`0..255`, mòdul 256)
- `PAYLOAD = LEN bytes`
- `CRC8` (1 byte) sobre `B0..B(3+LEN)`, polinomi `0x07`, init `0x00`, xorout `0x00`

La longitud total de la trama es troba entre 5 i 69 _bytes_. El MSX inicia cada cicle de recepció enviant una trama `POLL` sense payload. El _host_ respon immediatament amb `EMPTY` si no hi ha dades pendents, o amb `DATA` si n'hi ha. Quan el MSX rep una trama `DATA` vàlida, entrega el payload al `TTY` indicat i retorna `ACK` amb el mateix `SEQ`. Si arriba una `DATA` duplicada (`SEQ` idèntic a l'últim ja acceptat per aquell `TTY`), el MSX no la torna a processar i respon `ACK` per evitar una reexecució. El `NACK` només s'emet en el camí de recepció de `DATA` quan la trama és llegible però els camps no són acceptables per entrega (p. ex. `TTY` fora de rang o `LEN` invàlid); en altres errors de lectura/validació de trama la dada es descarta. `zlink` només transporta trames i multiplexa `TTY`; l'assignació de cada `TTY` a tasques/processos la fa `zbus`.

`zlink` és deliberadament asimètric. El costat MSX no pot observar directament l'estat de disponibilitat del bridge (`RXF#`/`TXE#`) ni disposa d'un senyal de tipus `DATA_READY`, de manera que la recepció `host -> MSX` es basa en `POLL` periòdic del MSX (`POLL -> DATA|EMPTY`). Tot i que el host sí que pot tenir aquests senyals, aquesta informació no és visible pel MSX i, per tant, el protocol prioritza simplicitat i robustesa al costat MSX en lloc de forçar una simetria completa.

## `zbus`
`zbus` és la capa superior a `zlink`: gestiona la semàntica de canals, l'aïllament entre tasques i les cues RX/TX. Mentre `zlink` només transporta trames, `zbus` decideix a qui pertany cada `TTY`, quan s'hi poden afegir dades a la cua i com es lliuren al consumidor.

Funcionament explícit de la capa:

- **Assignació de canals (`TTY`)**
  - `zbus` manté una taula de `TTY` (fins a `ZBUS_MAX_TTY=10`, de `TTY0` a `TTY9`) associats a una task propietària.
  - Cada `TTY` es pot `attach`/`detach`, i les operacions de lectura/escriptura només són vàlides per la task propietària.
- **Camí TX (MSX -> host)**
  - `zbus_write_tty()` encola trames de fins a `64` bytes en una cua per `TTY` (`ZBUS_TX_QUEUE_SIZE=8`).
  - Si la cua és plena, la trama es descarta i s'incrementa el comptador `tx_drop`.
  - `zbus_tick()` envia dades en *round-robin* entre `TTY` actius, amb límit de `ZBUS_TX_CHUNK=2` trames per tick, usant `zlink_send_data()`.
- **Camí RX (host -> MSX)**
  - A cada `tick`, `zbus` fa `poll` de `zlink` (`zlink_poll_once()`), amb límit `ZBUS_RX_CHUNK=1` trama per tick.
  - Si la trama és d'un `TTY` d'usuari vàlid i amb `rx_polling_enabled`, el payload s'afegeix al buffer circular RX (`ZBUS_BUFFER_SIZE=96`).
  - Si el buffer RX és ple, la resta de bytes es descarten i s'incrementa `rx_overflow`.
  - Les tasques recuperen bytes amb `zbus_read_tty()` (o `zbus_read()` en mode legacy).
- **Canal de control del kernel (`tty=15`)**
  - `TTY15` està reservat i no es mapeja a cap task d'usuari.
  - Les trames rebudes a `tty=15` activen comandes de control (`GET_STATS`, `GET_TASK_INFO`, `GET_TASK_LIST`, `GET_STACK_WM`) i `zbus` respon pel mateix canal amb `RSP_*`.
- **Integritat i estadístiques**
  - `zbus` agrega comptadors propis (`tx_drop`, `rx_overflow`, `attach_fail`) i els de diagnòstic de `zlink` (`rx_crc_err`, `rx_dup`, etc.).
  - Les seccions crítiques s'executen amb `CPU_DI()/CPU_EI()` per protegir taules i cues compartides.

La `tty=15` està reservada pel _kernel_. Les consultes via `tty=15` són el canal de control del _kernel_ (control-plane). A diferència dels `TTY` d'usuari, aquestes trames no s'entreguen a cap una tasca d'aplicació: el _kernel_ les interpreta com a comandes de diagnòstic/inspecció i retorna una resposta estructurada (`RSP_*`) pel mateix `tty=15`. Això permet monitoratge i automatització (stats, estat de tasks, stack watermark) sense barrejar aquest tràfic amb la shell o les dades normals dels processos.

Les següents seccions donen detalls dels missatges de control del _kernel_ intercanviats via `tty=15` sobre `zbus`/`zlink`.

## Estadístiques
Permet obtenir comptadors de salut del transport (`zbus`/`zlink`) per detectar pèrdues, errors i saturació.

- Request host->MSX (`DATA`, `tty=15`): payload `01` (`GET_STATS`)
- Response MSX->host (`DATA`, `tty=15`): payload
  - `81` (`RSP_STATS`)
  - `status` (`00=OK`, `01=BAD_CMD`, `02=BAD_LEN`)
  - 8 comptadors `uint16 little-endian` (si `status=00`):
    - `tx_drop`
    - `rx_overflow`
    - `attach_fail`
    - `zlink_rx_frames_ok`
    - `zlink_rx_crc_err`
    - `zlink_rx_dup`
    - `zlink_rx_type_err`
    - `zlink_rx_len_err`
- A `openmsx/zlink.tcl`, usa `zlink_dev::get_stats` (o alias `zlink::get_stats`) per enviar la request i veure la resposta decodificada.
- Per output JSON orientat a scripts, usa `zlink_dev::get_stats_json` (o `zlink::get_stats_json`).
  - Format JSON:
    - `type` = `"kernel_stats"`
    - `status`
    - `tx_drop`, `rx_overflow`, `attach_fail`
    - `zlink_rx_frames_ok`, `zlink_rx_crc_err`, `zlink_rx_dup`, `zlink_rx_type_err`, `zlink_rx_len_err`
    - en error: `len`, `payload_hex`

## Dades de tasca
Permet inspeccionar una task concreta (actual o per `task_id`) per conèixer estat, `tty`, `SP` i nom.

- Request host->MSX (`DATA`, `tty=15`):
  - payload `02` (`GET_TASK_INFO`, task actual), o
  - payload `02 <task_id>` (`GET_TASK_INFO` d'un task concret)
- Response MSX->host (`DATA`, `tty=15`): payload
  - `82` (`RSP_TASK_INFO`)
  - `status` (`00=OK`, `02=BAD_LEN`, `03=BAD_TASK`)
  - `task_id`
  - `task_state`
  - `task_tty`
  - `task_sp_lo task_sp_hi` (`uint16 little-endian`)
  - `task_name_len`
  - `task_name[8]`
- A `openmsx/zlink.tcl`, usa:
  - `zlink_dev::get_task_info` / `zlink::get_task_info`
  - `zlink_dev::get_task_info_json` / `zlink::get_task_info_json`
  - Format JSON:
    - `type` = `"kernel_task_info"`
    - `status`
    - `id`, `state`, `sp` (hexadecimal, p.ex. `"0x1234"`), `name_len`, `name`
    - en error: `len`, `payload_hex`

## Llista de tasques
Permet obtenir un resum de totes les tasks actives amb identificador, `tty` i nom.

- Request host->MSX (`DATA`, `tty=15`):
  - payload `03` (`GET_TASK_LIST`)
- Response MSX->host (`DATA`, `tty=15`): un o més frames `RSP_TASK_LIST`
  - `83` (`RSP_TASK_LIST`)
  - `status` (`00=OK_FINAL`, `80=OK_MORE`, `02=BAD_LEN`)
  - `count` entrades en aquest frame
  - `count` entrades de 11 bytes:
    - `task_id`
    - `task_tty`
    - `task_name_len`
    - `task_name[8]`
- `openmsx/zlink.tcl` reuneix tots els fragments transparentment i el helper JSON retorna la llista completa agregada.
- A `openmsx/zlink.tcl`, usa:
  - `zlink_dev::get_task_list` / `zlink::get_task_list`
  - `zlink_dev::get_task_list_json` / `zlink::get_task_list_json`
  - Format JSON:
    - `type` = `"kernel_task_list"`
    - `status`
    - `count`
    - `tasks`: llista d'objectes amb `id`, `tty`, `name_len`, `name`
    - en error: `len`, `payload_hex`

## Watermark de stacks
Permet mesurar ús de pila (actual i de pic) per dimensionar les piles de memòria i prevenir `stack overflow`.
Només s'accepta l'opcode `07`; l'opcode antic `04` ja no existeix.

- Request host->MSX (`DATA`, `tty=15`):
  - payload `07` (`GET_STACK_WM`, task actual), o
  - payload `07 <task_id>` (`GET_STACK_WM` d'un task concret)
- Response MSX->host (`DATA`, `tty=15`): payload
  - payload fix de 10 bytes: `87 status task_id task_state stack_size_lo stack_size_hi peak_used_lo peak_used_hi current_used_lo current_used_hi`
  - `87` (`RSP_STACK_WM`)
  - `status` (`00=OK`, `02=BAD_LEN`, `03=BAD_TASK`)
  - `task_id`
  - `task_state`
  - `stack_size_lo stack_size_hi` (`uint16 little-endian`)
  - `peak_used_lo peak_used_hi` (`uint16 little-endian`, watermark)
  - `current_used_lo current_used_hi` (`uint16 little-endian`)
- A `openmsx/zlink.tcl`, usa:
  - `zlink_dev::get_stack_wm` / `zlink::get_stack_wm`
  - `zlink_dev::get_stack_wm <task_id>` / `zlink::get_stack_wm <task_id>`

## Shell `xsh`
La shell (`xsh`) s'executa al task registrat com `xsh` sobre el seu `tty` (`zbus`). El seu punt d'entrada és `_main_xsh`. En arrencar, mostra el prompt:

```text
Z-Tick xsh
ztick> 
```

Funcionalitat implementada (estat actual del codi):

* Auto-adjunta el `tty` actual via `zbus_tty_get_current()`.
* Entrada interactiva amb eco:
  * caràcters ASCII imprimibles (`32..126`)
  * `Backspace`/`Delete` amb esborrat visual
  * `CR`/`LF` per executar comanda
* Línia màxima d'entrada: `55` caràcters útils (`XSH_LINE_MAX=56`, 1 byte reservat per `\0`).
* Parser simple separat per espais (sense cometes/escape), amb màxim de 3 tokens totals (`XSH_ARGV_MAX=3`).
* Si la comanda no existeix: `unknown command`.
* Si la sintaxi és invàlida: mostra la línia `usage` de la comanda.

Comandes disponibles:

* `help`: Mostra: `commands: help cfg tasks start stop weight heap stack stats`
* `cfg`: Mostra la configuració compilada (`max_tasks`, `task_heap`, paràmetres `zbus`, etc.).
* `tasks [task_id]`: Sense arguments: compta tasks actives i mostra `task <id> name=<nom>`. Amb `task_id`: mostra detall `state`, `tty`, `w` (weight), `b` (budget), `sp`, `name`.
* `start <task_name> [weight]`: Arrenca una task del registre (`task_registry`), actualment `b`, `c` i `rchk`. El paràmetre `weight` opcional a rang `1..3`.
* `stop <task_name>`: Sol·licita parada d'una task en execució.
* `weight <task_id> <1..3>`: Canvia el pes de planificació d'una task.
* `heap [task_id]`: Mostra estat de heap per task: `free`, `free_blocks`, `used_blocks`.
* `stack [task_id]`: Mostra mètriques de stack: `size`, `peak`, `free_peak`, `current`.
* `stats`: Mostra 3 blocs:
    * `zbus`: `tx_drop`, `rx_overflow`, `attach_fail`
    * `zlink`: `ok`, `crc`, `dup`, `type`, `len`
    * `ipc`: `q_used`, `q_cap`

### Sortida de comandes: l'emissor de línies

Les comandes que emeten una línia formatada (`tasks`, `stack`, `stats`, ...) la construeixen en un buffer petit i fix (`xsh_cmd_emitter_t`, basat en el mateix helper `sprint_t` que fan servir `zbus.c` i `rchk`) i l'escriuen com un únic frame atòmic quan hi cap dins `XSH_CMD_LINE_CAP` bytes, amb un fallback a escriure cada camp directament (encara correcte, però no atòmic) si no hi cap.

Aquest buffer és una **única instància estàtica compartida** (`g_xsh_cmd_emitter`), no una variable local per crida. `xsh` només executa una comanda cada vegada (sense reentrada, sense tasques concurrents tocant-lo), així que només pot existir una "línia en construcció" alhora — fent estructuralment impossible que dos marcs de crida propietaris d'un emissor estiguin vius a la pila alhora. Una versió anterior amb variable local per crida no tenia aquesta garantia: `tasks` sense filtre mantenia el seu propi buffer viu a `cmd_tasks` mentre iterava i cridava una funció separada que en necessitava un altre, i en maquinari real això desbordava la pila de 320 bytes de la tasca cap a la de la següent (confirmat amb `stack`, mostrant `peak=320 free_peak=0`), corrompent-la i penjant el sistema al següent tick del rellotge. Si `xsh` mai esdevé reentrant/concurrent, aquesta assumpció d'instància única es trenca i cada funció `xsh_cmd_emit_*` necessitarà rebre una instància explícita altre cop.

Les constants de final de línia escrites literalment (el banner, les línies de `cfg`) sempre acaben en `"\r\n"`, no en un `"\n"` solt: confiar que el terminal del client tradueixi un `\n` solt (p. ex. via `OPOST`) és fràgil — un client de terminal en mode raw que desactivi aquesta traducció mostraria cada línia d'aquest tipus desplaçada una columna més cap a la dreta que l'anterior.

Després del boot, les tasks `xsh`, `b` i `c` es creen des del manifest del target via `BOOT_AUTOSTART` (actualment `xsh:2 b:1 c:3` als targets inclosos). La task `rchk` es pot arrencar sota demanda amb:

```tcl
zlink_dev::shell_cmd "start rchk safe"
```

Des de la consola Tcl d'openMSX, es pot injectar una comanda de shell via:

```tcl
zlink_dev::shell_cmd help
zlink_dev::shell_cmd "tasks"
zlink_dev::shell_cmd "stack 0"
zlink_dev::shell_cmd "help" 0 auto -decode 1
```

Equivalent en brut (sense helper):

```tcl
zlink_dev::queue_text 0 "help\r"
```

Per desactivar la decodificació textual:

```tcl
zlink_dev::shell_cmd "help" 0 auto -decode 0
```

## IPC
L'objectiu de l'IPC és oferir un mecanisme mínim, previsible i de baix cost per coordinar tasques i intercanviar dades dins del sistema, així com evitar esperes actives i mantenint el sistema reactiu sota càrrega.

El mòdul IPC es construeix amb dues primitives:

- `ipc_semaphore_t`: sincronització bloquejant entre tasks (`wait/signal`).
  - Serveix per protegir recursos compartits o esperar esdeveniments.
  - Si el recurs no està disponible, la task no gira en bucle: queda bloquejada fins que algú faci `signal`.
- `ipc_queue_t`: FIFO de mida fixa per a missatgeria entre productor i consumidor.
  - Internament usa dos semàfors:
    - `items`: nombre d'elements disponibles per rebre.
    - `slots`: espai lliure disponible per enviar.
  - Això imposa control de flux: no es pot enviar si la cua és plena ni rebre si és buida.

Funcionament operatiu (model productor/consumidor):

1. `send`: el productor espera `slots`, escriu a la cua i incrementa `items`.
2. `recv`: el consumidor espera `items`, llegeix de la cua i incrementa `slots`.
3. Quan no es pot progressar (cua plena o buida), la task queda en espera del nucli i es reprèn quan toca.

Integració amb el scheduler:

- Els bloquejos d'IPC es reflecteixen a l'estat de task (`wait_sem`, `wait_q_send`, `wait_q_recv`).
- Això permet que el planificador executi altres tasks preparades mentre una task espera IPC.
- El resultat és menor latència global i millor aprofitament de CPU que amb polling.

En resum, aquest IPC és la base de missatgeria interna del sistema: prou simple per ser robusta en Z80 i prou expressiva per construir canals entre tasks sense acoblament fort.

# Zbridge
Segons els requeriments establerts en aquest projecte, és necessari desenvolupar una interfície de maquinari entre el _host_ i el sistema MSX. L'objectiu és que aquesta interfície detecti l'arribada d'una operació IO de Z80 sobre un número de port arbitrari. També ha de fixar els valors dels senyals de control interns. En particular, farem servir un mòdul basat en l'integrat [FT245](https://ftdichip.com/wp-content/uploads/2020/08/DS_FT245R.pdf). Aquest maquinari ha de permetre modificar certs paràmetres com ara el número de port i les lògiques dels senyals de control, sense modificar el maquinari.

Zbridge és una targeta connectada al port d'expansió o _cartridge_ que tots els sistemes MSX tenen. Aquest port conté tots els senyals necessaris per enviar i rebre les trames `zlink` entre el MSX i el _host_. El detall del circuit serà incorporat en un futur.

La solució que hem triat implementa una màquina d'estats finits (FSM), també amb la flaix 39SF010A. La idea és que aquesta FSM detecti l'arribada d'una operació IO de Z80 sobre un número de port programable. Aquesta entrada situa la FSM en un estat tal que genera les sortides de control per activar la lectura o escriptura d'un _byte_ des del FIFO del FT245. Aquesta solució permet configurar tots els paràmetres de sortida en funció dels d'entrada amb xips barats. En principi també seria possible implementar-ho amb solucions basades en dispositius lògics programables (PLD), com ara FPGA o CPLD. Tanmateix, la programació de dispositius PLD requereix estar familiaritzat amb els seus llenguatges de programació.

La programació d'aquesta FSM exigeix desenvolupar un generador de codi per obtenir les imatges que gravarem a la flaix. El directori `zbridge` conté aquest generador de copy (`zbrc.py`). La imatge es genera a partir dels paràmetres de l'arxiu de _target_ (o del port indicat directament). Les lògiques de bits i polaritat no són configurables però si poden modificar-se canviant adequadament `zbrc.py`. No tenim per objectiu crear un compilador de FSM d'ús general. Per tant, prioritzem la simplicitat i el mínim temps de desenvolupament.

Cada target té el seu propi número de port a `IO_DEFAULT_PORT` dins `targets/<nom>.mk`, així que l'ús recomanat és llegir-lo directament del manifest (evita haver de repetir el número de port a mà i queda sincronitzat si mai canvia):

```bash
python3 zbridge/zbrc.py --target vg-8010 -o zbridge/build/zbridge_fsm.bin
```

O indicant el port manualment:

```bash
python3 zbridge/zbrc.py --port 0x38 -o zbridge/build/zbridge_fsm.bin
```

Si no es passa ni `--target` ni `--port`, s'utilitza el valor de _fallback_ intern del compilador (`0x38`), que només coincideix amb el port de `vg-8010` per casualitat i no s'ha d'assumir vàlid per a altres targets:

```bash
python3 zbridge/zbrc.py
```

També es poden generar formats addicionals, útils per depurar o simular la FSM abans de gravar-la. En particular, podem simular el seu comportament amb l'eina [Logisim Evolution](https://github.com/logisim-evolution/logisim-evolution). Per generar una imatge acceptable per aquesta eina executem:

```bash
python3 zbridge/zbrc.py --target vg-8010 --out-hex zbridge/build/zbridge_fsm.hex --out-logisim zbridge/build/zbridge_fsm.img
```

Opcions disponibles:

* `-o, --out-bin <path>`: fitxer de sortida binari, 128 KiB (per defecte `zbridge_fsm.bin`). Conté la imatge real de 64 KiB duplicada a la meitat superior, ja que `A16` va lligat a 0V a la placa; el duplicat és necessari perquè programadors d'EEPROM com `minipro` l'acceptin sense avisos de mida incorrecta.
* `--out-hex <path>` (opcional): també escriu la imatge en format Intel HEX.
* `--out-logisim <path>` (opcional): també escriu una imatge Logisim-evolution (`v2.0 raw`) de només 64 KiB, sense duplicar, per carregar-la al component ROM amb "Load Image...".
* `--port <n>`: port d'E/S de MSX a decodificar directament (accepta notació `0x..`). Per defecte `0x38` si no es passa ni `--port` ni `--target`.
* `--target <nom>`: llegeix `IO_DEFAULT_PORT` directament de `targets/<nom>.mk` en lloc de `--port`. Mútuament exclusiu amb `--port`.

# Compilació
El projecte utilitza SDCC (`sdcc`) i l'assembler `sdasz80`. SDCC és una suite de compilador de C estàndard (ANSI C89, ISO C99, ISO C11, ISO C23), retargetable i optimitzadora, orientada als microprocessadors Intel basats en MCS51 (8031, 8032, 8051, 8052, etc.), variants DS80C390 de Maxim (abans Dallas), microcontroladors Freescale basats en HC08 (abans Motorola) (hc08, s08), MCUs Zilog basats en Z80 (Z80, Z80N, Z180, SM83, Rabbit 2000, 2000A, 3000A, SM83, TLCS-90, eZ80, R800), Padauk (pdk14, pdk15), STM8 de STMicroelectronics, MOS 6502 i WDC 65C02.

Els _assemblers_ ASxxxx són una sèrie d’_assemblers_ de microprocessadors escrits en C. Aquesta col·lecció conté _assemblers_ creuats per a les sèries 1802, S2650, SC/MP, 4040(4004), MPS430, 6100, 61860, 6500, 6800(6802/6808), 6801(6803/HD6303), 6804, 6805, 68HC(S)08, 6809, 68HC11, 68HC(S)12, 68HC16, 68CF 68K, 740, 78K/0, 78K/0S, 8008, 8008S, 8048(8041/8022/8021), 8051, 8085(8080), AT89LP, 8X300(8X305), COP4, COP8, DS8XCXXX, AVR, EZ8, EZ80, F2MC8L/FX, F8/3870, GameBoy(Z80), H8/3xx, Cypress PSoC(M8C), PDP11, PIC, Rabbit 2000/3000, RS08, ST6, ST7, ST8, ST9, SX, TLCS90, Z8, Z80(HD64180, ZXN, 8080, 8085) i Z280.

Podem trobar els manifests del maquinari o plataforma de destí (_targets_) a `targets/*.mk`. Aquests arxius defineixen tant el layout de compilació com els paràmetres de _boot_. Per compilar el target per defecte (`ztick`):

```bash
make bootstrap
```
També podem compilar un target en concret:

```bash
make TARGET=ztick bootstrap
make TARGET=ztick-unitcard bootstrap
make TARGET=hb-55p bootstrap
make TARGET=hb-75p bootstrap
make TARGET=vg-8010 bootstrap
```
Per eliminar el codi objecte creat en fases anteriors:

```bash
make clean
```
Podem incloure aquests paràmetres per tal de personalitzar la compilació:

* `TARGET`: nom del _target_ referit a `targets/<target>.mk`
* `IMAGE_LAYOUT`: determina el flux d'imatge ROM. Els seus possibles valors per defecte es defineixen al manifest del target, però es poden sobreescriure:
  * `flat64`: una única ROM de 64KB.
  * `flash2x64`: dues ROM de 64KB concatenades en una imatge final de 128KB.
* `GEN_COMPACT_IMAGE`: només aplica amb `flash2x64`. Els seus valors per defecte es defineixen al manifest del target:
  * `no`: no genera cap imatge addicional
  * `yes`: genera també `bin/<target>/startup_slot01.rom`, els primers 32KB de `startup.rom`. Serveix per a dos casos:
    * gravar el codi en un xip real més petit i barat (p. ex. una EEPROM `AT28C256` de 32KB), ja que tot el codi hi cap dins la primera meitat de l'espai adreçable (`ADDR_CODE=0x0040`) i la resta (`ADDR_DATA=0x8000` en amunt) és RAM física a la placa, no contingut necessari a la flaix;
    * `ztick-unitcard`, que simula el flux d'arrencada en dues etapes a openMSX, necessita aquest fitxer com a `OPENMSX_EXTRA_ROM_FILES` del seu perfil de màquina (`scripts/setup_openmsx.sh` falla si no existeix). Com que `bootloader` i `startup` contenen sempre el mateix codi, `openmsx/Z-Tick-UnitCard.xml` referencia aquest mateix fitxer (`startup_slot01.rom`) tant per a la ROM del _slot 0_ com per a la del _slot 1_; no cal cap còpia amb un altre nom.
* `GEN_MIRRORED_IMAGE`: només aplica amb `flash2x64`. Els seus valors per defecte es defineixen al manifest del target:
  * `yes` (per defecte): `<ROM_IMAGE_NAME>` malla `startup.rom` a les dues meitats d'una imatge de 128KB, tal com es descriu més avall.
  * `no`: `<ROM_IMAGE_NAME>` és una còpia plana, sense mallar, de 64KB de `startup.rom` -- per a un target l'arrencada en fred del qual ja no passa mai pel banc baix d'aquest xip (vegeu `hb-75p` més avall).
* `EEPROM_IMAGE_NAME`: només aplica amb `flash2x64` i `GEN_COMPACT_IMAGE=yes`. Quan es defineix al manifest del target, també copia la imatge compacta de 32KB a `bin/<target>/<EEPROM_IMAGE_NAME>` -- un artefacte amb nom propi del target, pensat per gravar-lo en un segon xip físicament separat (vegeu `hb-75p` més avall).

Exemple amb _override_ explícit:

```bash
make TARGET=hb-55p IMAGE_LAYOUT=flash2x64 bootstrap
make TARGET=hb-75p IMAGE_LAYOUT=flash2x64 bootstrap
make TARGET=vg-8010 IMAGE_LAYOUT=flash2x64 bootstrap
```

Existeixen dos formats o _layouts_ d'imatge. L'elecció entre una o altra dependrà de si fem servir un MSX físic amb la targeta de _bootstrapping_. 

El _layout_ `flat64` s'utilitza en targets com `ztick` i es genera en una sola imatge: `bin/<target>/<ROM_IMAGE_NAME>`. Fa `65536` bytes i el codi d'arrencada entra directament per `startup.s`.

El _layout_ `flash2x64` s'utilitza en targets físics com `hb-55p`, `hb-75p` i `vg-8010`. Es compila `startup.s` una sola vegada i es genera:

* `bin/<target>/startup.rom` (64KB): la imatge compilada.
* `bin/<target>/<ROM_IMAGE_NAME>` (128KB): la mateixa imatge concatenada amb ella mateixa (`startup.rom` + `startup.rom`), ja programable a la SST39SF010A.

No hi ha cap `bootloader.s` diferent: `startup.s` conté alhora el vector de reset (`0x0000`), que ja fa el _Memory Switching_ inicial (`PPI_PSR_PORT <- BOOT_PSR_VALUE`) i salta a `_main_boot`. Com que el _slot 0_ (actiu al _power-on_) i el _slot_ de destí després del canvi contenen exactament el mateix codi als mateixos _offsets_, l'execució continua sense sorpreses independentment de quin dels dos bancs físics estigui mapat en cada moment; no cal, doncs, un binari de _bootloader_ separat i més petit, ni generar-lo com a fitxer a part.

`hb-75p` n'és una excepció parcial. El seu port d'expansió té un conjunt de _buffers_ del bus de dades entre aquest i el bus del Z80, no habilitats just després del reset, així que una ROM situada a la flaix pròpia de la placa d'expansió no pot proporcionar el primer _fetch_ d'instrucció del _slot 0_ en aquell moment -- els _buffers_ bloquegen aquest camí de dades exactament llavors. L'arrencada en fred passa ara per una EEPROM genuïnament separada i sense _buffers_ (32KB, p. ex. `27C256`/`28C256`) situada físicament al socket original de la ROM de la màquina, generada a partir de la mateixa imatge compacta de 32KB descrita més amunt (`GEN_COMPACT_IMAGE=yes`, copiada a `EEPROM_IMAGE_NAME`) i fent servir exactament el mateix truc de "bytes idèntics a banda i banda del canvi" -- només ha canviat quin xip físic proporciona el _slot 0_. Com que el banc baix d'aquell xip ara és permanentment inabastable en aquest target, `hb-75p` també defineix `GEN_MIRRORED_IMAGE=no`: `<ROM_IMAGE_NAME>` esdevé una còpia plana de 64KB de `startup.rom`, amb només el contingut de "startup, RTOS" que encara s'assoleix després del canvi.

# Execució amb openMSX
OpenMSX és un emulador lliure i de codi obert per a ordinadors MSX, MSX2, MSX2+, MSX turboR i maquinari relacionat. El seu lema al repositori és “the MSX emulator that aims for perfection”, és a dir, un emulador orientat a alta fidelitat i precisió.

Per a les proves completes de `zlink` (RX/TX), es recomana usar `openMSX 21` _upstream_ atès que aquesta versió inclou l'extensió `ProgrammableDevice`. Es tracta d'un dispositiu MSX virtual programable que pots connectar a una llista de ports d’I/O. Serveix per fer de “pont” entre el Z80 emulat i l’entorn host d’openMSX mitjançant callbacks Tcl. El manual oficial el descriu com un dispositiu virtual connectable “on the fly” a ports I/O d’usuari i útil per crear comunicació bidireccional entre el MSX virtual i el sistema host.

Podem fer servir binaris pre-compilats sense instal·lar [openMSX](https://github.com/openMSX/openMSX/releases) al sistema ni substituir-ne la versió:

```bash
mkdir -p ~/opt
cd ~/opt
curl -L -o openmsx-21.0-linux-x86_64-bin.zip \
  https://github.com/openMSX/openMSX/releases/download/RELEASE_21_0/openmsx-21.0-linux-x86_64-bin.zip
unzip -q openmsx-21.0-linux-x86_64-bin.zip -d openmsx-21.0
```

Després de compilar, es pot arrencar openMSX amb el script del projecte. Si vols usar la versió del sistema:

```bash
./scripts/setup_openmsx.sh ztick
./scripts/setup_openmsx.sh ztick-unitcard
./scripts/setup_openmsx.sh hb-55p
./scripts/setup_openmsx.sh hb-75p
./scripts/setup_openmsx.sh vg-8010
```

`ztick-unitcard` simula un flux d'arrencada en dues etapes per una targeta ROM+RAM al _primary slot 1_:

* el bootloader s'executa des del slot 0 pàgines 0-1
* l'_startup_ s'executa des del slot 1 pàgines 0-1
* la RAM de tasques/dades és al slot 1 pàgines 2-3

Si volem forçar la versió 21 local:

```bash
export OPENMSX_BIN="$HOME/opt/openmsx-21.0/bin/openmsx"
./scripts/setup_openmsx.sh --target ztick
```

Si vols aturar l'execució al punt d'entrada de xsh (`_main_xsh`), activa el breakpoint explícitament:

```bash
./scripts/setup_openmsx.sh ztick --bp-xsh
```

Si vols aturar l'execució al bootloader (`0x0000`), fes servir:

```bash
./scripts/setup_openmsx.sh ztick --bp-bootloader
```

Si vols observar les escriptures I/O del port configurat al target, activa:

```bash
./scripts/setup_openmsx.sh ztick --watch-io
```

Per defecte, `setup_openmsx.sh` no executa cap _self-check_. Si vols les comprovacions de diagnòstic (`get_task_list`, `get_stack_wm` i `shell_cmd help`) després d'instal·lar `zlink`, afegeix:

```bash
./scripts/setup_openmsx.sh ztick --self-check
```
