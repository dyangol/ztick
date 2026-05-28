# Notes de disseny
El sistema MSX està basat en un maquinari que respon a una arquitectura estàndard. És relativament senzill i està pensat per arrencar software de consum, bàsicament jocs.
A meitat de camí entre un experiment excèntric i un desafiament personal, volem dur a terme un projecte que faci que un sistema MSX faci:

* L'establiment d'un canal de comunicació amb sistemes externs, que permet la transmissió de dades entre el port d'expansió MSX i un port USB
* L'execució de processos executats per multiplexació per temps basats en el senyal d'interrupció nadiu de MSX (VDP) cada 20 ms (50 Hz): un sistema operatiu de temps real (RTOS)

Dividim el projecte en dues fases de desenvolupament que poden dur-se a terme de manera independent. D'una banda, el programari que permetrà les característiques plantejades i de l'altra, el desenvolupament de la interfície de maquinari (i el seu programari associat) que haurà de permetre la pretesa comunicació.

En última instància, el projecte pretén establir una plataforma per avaluar problemes de maquinari del propi MSX, executar, monitoritzar i transferir dades amb sistemes externs moderns.

## Arquitectura del MSX
En primer lloc, cal conèixer els principis bàsics de funcionament d'un sistema MSX. Una mirada superficial apreciarà els aspectes més evidents, com ara la simplicitat de funcionament. Tanmateix, la seva aparent simplicitat imposa fortes restriccions a l'hora d'assolir els objectius previstos.

Aquests sistemes estaven pensats per executar programes de manera directa i de forma ràpida. Després d'un _power on_, el MSX executa una fase de _startup_ de codi ubicat en una ROM. Aquest processador sempre llegeix la primera instrucció a l'adreça `0x0000`. És a dir, el primer codi que llegeix és a les primeres adreces. Aquest conté rutines que només s'executen en el moment del _startup_ i també d'altres que poden executar-se en una fase posterior per altres programes. Les tasques que es duen a terme durant aquesta fase inicial són bàsicament la inicialització del maquinari i tests de memòria RAM.

Els sistemes MSX van aparèixer el 1983 i estaven basats en un processador Zilog Z80. Aquest processador té un bus de dades de 8 bits i un d'adreces de 16 bits. En conseqüència, pot accedir a 65536 adreces de memòria. Els sistemes MSX venien equipats amb entre 16KB i 64KB de memòria RAM, però podien ser ampliats en recursos i per tant, accedir a més de 64KB de memòria. Els enginyers que van crear el MSX varen incorporar una funció que permet exposar rangs d'adreces de memòria _visibles_ pel processador a maquinari extern. El que van fer és crear els conceptes d'_slots_ i _pages_. La idea és presentar al Z80 un **espai d'adreces** com interfície única d'intercanvi d'informació (instruccions o dades) amb l'exterior. No hem de confondre aquest espai d'adreces amb una memòria física concreta, sinó més aviat com una capa d'abstracció. Aquest espai està dividit en 4 subespais anomenats _pages_, que consten d'un interval d'adreces consecutives de 16KB cadascun:

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

És a dir, el HB-55P tenia 16KB de RAM nadiua. El MSX HB-75P en canvi, tenia aquesta configuració de _slot 0_:

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


## Disseny del Sistema Operatiu
Una de les primeres decisions que incorpora el projecte és la substitució de la memòria ROM original per una de nova que incorpori les rutines bàsiques de _boot_. Per tal de facilitar els cicles de desenvolupament sobre hardware original, dissenyem una nova placa d'expansió que incorpori una memòria flash i una de RAM, atès que les memòries EPROM o EEPROM són força més cares que les flash i permeten emmagatzemar menys dades. Aquesta placa anirà instal·lada un _slot_ de _cartridge_. Però si volem que aquesta flash proporcioni al Z80, el codi de _startup_ haurem d'extraure la ROM original i interceptar el senyal de sel·lecció de ROM, atès que el _slot 0_ és el que està activat quan en el MSX experimenta un _power on_. El codi que conté la pròpia flash pot dur a terme un _memory switching_ per fer accessibles pàgines de RAM sobre la mateixa placa. És a dir, seria possible executar tots el programari necessari sense necessitar la RAM integrada.

Aquesta nova targeta està basada en la memòria flash multi propòsit SST39SF010A de tipus CMOS. Té una capacitat de 1048576 bits amb bus d'accés de dades de 8 bits. L'espai adreçable és de 17 bits, que permet reservar dos espais adjacents de 65536 bits (64KB). Concretament:

* De `0x00000` a `0x0FFFF` : `bootloader` seleccionat quan el senyal `ROM_OE` és generat
* De `0x10000` a `0x1FFFF` : `startup`, RTOS i processos d'usuari

# Protocol `zlink`
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

# `zbus`
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
- Response MSX->host (`DATA`, `tty=15`): payload
  - `83` (`RSP_TASK_LIST`)
  - `status` (`00=OK`, `02=BAD_LEN`)
  - `count`
  - `count` entrades de 11 bytes:
    - `task_id`
    - `task_tty`
    - `task_name_len`
    - `task_name[8]`
- A `openmsx/zlink.tcl`, usa:
  - `zlink_dev::get_task_list` / `zlink::get_task_list`
  - `zlink_dev::get_task_list_json` / `zlink::get_task_list_json`
  - Format JSON:
    - `type` = `"kernel_task_list"`
    - `status`
    - `count`
    - `tasks`: llista d'objectes amb `id`, `tty`, `name_len`, `name`
    - en error: `len`, `payload_hex`

## Watermark de stack

Permet mesurar ús de pila (actual i pic) per dimensionar stacks i prevenir `stack overflow`.

- Request host->MSX (`DATA`, `tty=15`):
  - payload `07` (`GET_STACK_WM`, task actual), o
  - payload `07 <task_id>` (`GET_STACK_WM` d'un task concret)
- Response MSX->host (`DATA`, `tty=15`): payload
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

# IPC
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
make TARGET=hb-55p bootstrap
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

Exemple amb _override_ explícit:

```bash
make TARGET=hb-55p IMAGE_LAYOUT=flash2x64 bootstrap
```

Existeixen dos formats o _layouts_ d'imatge. L'elecció entr una o altra dependrà de si fem servir un MSX físic amb la targeta de _bootstrapping_. 

El _layout_ `flat64` s'utilitza en targets com `ztick` i es genera en una sola imatge: `bin/<target>/<ROM_IMAGE_NAME>`. Fa `65536` bytes i el codi d'arrencada entra directament per `startup.s`.

El _layout_ `flash2x64` s'utilitza en targets físics com `hb-55p`. Es generen dos imatges primaries i una composta a partir de les anteriors:

* `bin/<target>/bootloader.rom` (64KB)
* `bin/<target>/startup.rom` (64KB)
* `bin/<target>/<ROM_IMAGE_NAME>` (concatenació, 128KB)

La imatge concatenada ja pot ser programada a SST39SF010A.

## Execució amb openMSX
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
./scripts/setup_openmsx.sh hb-55p
```

Si volem forçar la versió 21 local:

```bash
export OPENMSX_BIN="$HOME/opt/openmsx-21.0/bin/openmsx"
./scripts/setup_openmsx.sh --target ztick
```

Si vols aturar l'execució al punt d'entrada de shell (`_main_shell`), activa el breakpoint explícitament:

```bash
./scripts/setup_openmsx.sh ztick --bp-main-shell
```

Per defecte, `setup_openmsx.sh` llança un petit _self-check_ de diagnòstic (`get_task_list`, `get_stack_wm` i `shell_cmd help`) just després d'instal·lar `zlink`. Si prefereixes arrencada neta:

```bash
./scripts/setup_openmsx.sh ztick --no-self-check
```

## Shell mínima (task `xsh`)
La shell (`xsh`) s'executa al task registrat com `xsh` sobre el seu `tty` (`zbus`). El seu punt d'entrada és `_main_shell`. En arrencar, mostra el prompt:

```text
Z-Tick shell
ztick>
```

Comandes disponibles:

* `help`
* `cfg`
* `tasks`
* `start b|c [weight]`
* `stop b|c`
* `weight <task_id> <1..3>`
* `heap [task_id]`
* `stack [task_id]`
* `stats`

Després del boot només s'inicia la shell (`xsh`). Les tasques `b` i `c` es poden arrencar sota demanda amb:

```tcl
zlink_dev::shell_cmd "start b"
zlink_dev::shell_cmd "start c"
zlink_dev::shell_cmd "start b 3"
zlink_dev::shell_cmd "weight 1 2"
zlink_dev::shell_cmd "stop b"
zlink_dev::shell_cmd "stop c"
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
