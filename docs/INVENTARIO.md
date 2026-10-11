# Inventario de assets

> Generado por `tools/assets/inventario.py doc` a partir de [`assets/inventario.tsv`](../assets/inventario.tsv).
> No editar a mano: edita el TSV y regenera.

**400 objetos** · 1 con modelo · 399 pendientes.

Cada objeto se reemplaza dejando su archivo en la ruta que marca su id; el juego lo carga solo y, mientras falte, dibuja un marcador con sus medidas. Convenciones de importación: [assets/models/README.md](../assets/models/README.md).

## Avance por categoría

| Categoría | Total | Pendiente | Kiln | Importado | Refinado |
|---|---|---|---|---|---|
| [Mapa: terreno, vegetación, agua e hitos](#mapa) | 30 | 30 | 0 | 0 | 0 |
| [Estructuras](#estructura) | 42 | 41 | 1 | 0 | 0 |
| [Vehículos](#vehiculo) | 11 | 11 | 0 | 0 | 0 |
| [Animales](#animal) | 46 | 46 | 0 | 0 | 0 |
| [Armas](#arma) | 30 | 30 | 0 | 0 | 0 |
| [Proyectiles](#proyectil) | 6 | 6 | 0 | 0 | 0 |
| [Escudos](#escudo) | 6 | 6 | 0 | 0 | 0 |
| [Tótems](#totem) | 10 | 10 | 0 | 0 | 0 |
| [Armaduras (por piezas)](#armadura) | 48 | 48 | 0 | 0 | 0 |
| [Vestimenta (personalización)](#vestimenta) | 29 | 29 | 0 | 0 | 0 |
| [Accesorios: amuletos, tatuajes, arreos y mensajes](#accesorio) | 33 | 33 | 0 | 0 | 0 |
| [Maquinaria de asedio](#asedio) | 5 | 5 | 0 | 0 | 0 |
| [Utilería y consumibles](#utileria) | 65 | 65 | 0 | 0 | 0 |
| [Personajes y NPCs](#personaje) | 39 | 39 | 0 | 0 | 0 |

## Etiquetas

Formato `clave:valor`, separadas por comas. Una clave puede repetirse (p. ej., dos biomas).

| Clave | Valores |
|---|---|
| `bioma` | `bosque`, `ciudad`, `cualquiera`, `desierto`, `estepa`, `fiordo`, `glaciar` |
| `faccion` | `cualquiera`, `culto`, `imperio`, `neutral`, `nomada`, `salvaje` |
| `acto` | `1`, `2`, `3`, `4` |
| `uso` | libre |
| `talla` | `comun`, `gigante`, `grande`, `pequena` |
| `material` | libre |
| `rig` | `ave`, `cuadrupedo`, `humanoide`, `ninguno` |
| `especial` | `gran_guerrero`, `hito`, `legado`, `narrativo`, `rastro_padre` |
| `formato` | `modelo`, `textura` |
| `estilo` | libre |
| `requiere` | libre |
| `motivo` | libre |
| `manos` | `dos`, `escudo`, `una` |

<a id="mapa"></a>

## Mapa: terreno, vegetación, agua e hitos

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `mapa.suelo.estepa` | Suelo: Estepa | 4x0x4 | 0 | pendiente | `bioma:estepa` `formato:textura` | Textura de suelo para los chunks del terreno (paleta del bioma). |
| `mapa.suelo.desierto` | Suelo: Desierto | 4x0x4 | 0 | pendiente | `bioma:desierto` `formato:textura` | Textura de suelo para los chunks del terreno (paleta del bioma). |
| `mapa.suelo.bosque` | Suelo: Bosque de coníferas | 4x0x4 | 0 | pendiente | `bioma:bosque` `formato:textura` | Textura de suelo para los chunks del terreno (paleta del bioma). |
| `mapa.suelo.fiordo` | Suelo: Costa de fiordos | 4x0x4 | 0 | pendiente | `bioma:fiordo` `formato:textura` | Textura de suelo para los chunks del terreno (paleta del bioma). |
| `mapa.suelo.glaciar` | Suelo: Altiplano glaciar | 4x0x4 | 0 | pendiente | `bioma:glaciar` `formato:textura` | Textura de suelo para los chunks del terreno (paleta del bioma). |
| `mapa.roca.pequena` | Roca pequeña | 0.6x0.4x0.6 | 40 | pendiente | `bioma:cualquiera` `uso:decorado` `material:piedra` |  |
| `mapa.roca.mediana` | Roca mediana | 1.5x1x1.5 | 80 | pendiente | `bioma:cualquiera` `uso:decorado` `material:piedra` |  |
| `mapa.roca.grande` | Peñasco (escalable) | 4x3x4 | 160 | pendiente | `bioma:cualquiera` `uso:trepable` `material:piedra` |  |
| `mapa.relieve.acantilado` | Acantilado modular | 8x6x2 | 300 | pendiente | `bioma:fiordo` `bioma:glaciar` `uso:trepable` `material:piedra` |  |
| `mapa.relieve.duna` | Duna | 20x4x10 | 120 | pendiente | `bioma:desierto` `uso:decorado` |  |
| `mapa.relieve.grieta_glaciar` | Grieta de glaciar | 6x3x2 | 150 | pendiente | `bioma:glaciar` `uso:peligro` |  |
| `mapa.agua.oasis` | Oasis | 12x1x12 | 250 | pendiente | `bioma:desierto` `uso:recolectable` | Agua potable: clave en el desierto (acto III). |
| `mapa.agua.rio` | Tramo de río modular | 8x0.5x8 | 120 | pendiente | `bioma:cualquiera` `uso:navegable` |  |
| `mapa.agua.tempano` | Témpano de hielo | 3x1x3 | 60 | pendiente | `bioma:fiordo` `uso:decorado` |  |
| `mapa.vegetacion.hierba_alta` | Hierba alta (escondite) | 2x1x2 | 40 | pendiente | `bioma:estepa` `uso:escondite` | El jugador puede esconderse dentro. |
| `mapa.vegetacion.mata_hierbas` | Mata de hierbas curativas | 0.8x0.6x0.8 | 80 | pendiente | `bioma:estepa` `uso:recolectable` | F para recoger hierbas curativas. |
| `mapa.vegetacion.arbusto` | Arbusto estepario | 1x0.8x1 | 40 | pendiente | `bioma:estepa` `uso:decorado` |  |
| `mapa.vegetacion.pino` | Pino | 3x10x3 | 120 | pendiente | `bioma:bosque` `uso:trepable` |  |
| `mapa.vegetacion.alerce` | Alerce | 3x12x3 | 120 | pendiente | `bioma:bosque` `uso:trepable` |  |
| `mapa.vegetacion.abedul` | Abedul | 2.5x8x2.5 | 100 | pendiente | `bioma:estepa` `bioma:bosque` `uso:decorado` |  |
| `mapa.vegetacion.saxaul` | Saxaúl (arbusto del desierto) | 2x2x2 | 60 | pendiente | `bioma:desierto` `uso:recolectable` | Leña en el desierto. |
| `mapa.vegetacion.palmera` | Palmera datilera | 3x7x3 | 100 | pendiente | `bioma:desierto` `uso:recolectable` |  |
| `mapa.vegetacion.liquen` | Liquen y musgo de tundra | 1x0.1x1 | 20 | pendiente | `bioma:glaciar` `bioma:fiordo` `uso:decorado` |  |
| `mapa.hito.ovoo` | Ovoo (túmulo de piedras chamánico) | 3x2x3 | 120 | pendiente | `bioma:estepa` `uso:interactuable` `material:piedra` `especial:hito` | Punto de referencia; ofrendas. |
| `mapa.hito.kurgan` | Kurgán (túmulo funerario) | 14x3x14 | 200 | pendiente | `bioma:estepa` `especial:hito` `especial:rastro_padre` |  |
| `mapa.hito.tumba_helada` | Tumba helada del padre | 8x4x8 | 800 | pendiente | `bioma:glaciar` `acto:3` `especial:narrativo` `especial:legado` | Kurgán congelado al estilo de Pazyryk; prueba final del acto III. |
| `mapa.hito.campamento_abandonado` | Campamento abandonado del padre | 10x3x10 | 400 | pendiente | `acto:3` `especial:rastro_padre` |  |
| `mapa.hito.piedra_ciervo` | Piedra-ciervo | 0.6x3x0.4 | 150 | pendiente | `bioma:estepa` `material:piedra` `especial:rastro_padre` | Estela con ciervos de estilo animal. |
| `mapa.hito.balbal` | Balbal (estatua de piedra) | 0.6x1.8x0.5 | 150 | pendiente | `bioma:estepa` `material:piedra` |  |
| `mapa.agua.coral` | Arrecife de coral | 1.5x0.8x1.5 | 120 | pendiente | `bioma:cualquiera` `uso:decorado` `uso:recolectable` | Bajo el agua de los lagos: coral rojo y perlas. |

<a id="estructura"></a>

## Estructuras

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `estructura.vivienda.yurta_comun` | Yurta común | 4.5x2.6x4.5 | 300 | kiln | `faccion:nomada` `uso:habitable` `material:fieltro` | Ya existe (Kiln). Puerta hacia +X. |
| `estructura.vivienda.yurta_jefe` | Yurta del jefe | 7x3.4x7 | 600 | pendiente | `faccion:nomada` `uso:habitable` `material:fieltro` | Más grande, con estandarte y ornamento. |
| `estructura.vivienda.tienda_ligera` | Tienda ligera de viaje | 2.5x1.5x3 | 120 | pendiente | `faccion:nomada` `uso:habitable` `material:cuero` |  |
| `estructura.vivienda.choza_fieltro` | Choza de fieltro | 3x2x3 | 150 | pendiente | `faccion:nomada` `uso:habitable` `material:fieltro` |  |
| `estructura.vivienda.casa_adobe` | Casa de adobe | 6x3.5x5 | 250 | pendiente | `bioma:desierto` `faccion:neutral` `uso:habitable` `material:barro` |  |
| `estructura.vivienda.cabana_troncos` | Cabaña de troncos | 6x4x5 | 300 | pendiente | `bioma:bosque` `faccion:neutral` `uso:habitable` `material:madera` |  |
| `estructura.vivienda.casa_larga` | Casa larga de los fiordos | 6x5x18 | 500 | pendiente | `bioma:fiordo` `faccion:neutral` `uso:habitable` `material:madera` |  |
| `estructura.vivienda.refugio_nieve` | Refugio excavado en la nieve | 4x2x4 | 150 | pendiente | `bioma:glaciar` `uso:habitable` |  |
| `estructura.vivienda.casa_campesina` | Casa campesina | 6x4x7 | 300 | pendiente | `bioma:ciudad` `faccion:imperio` `uso:habitable` `material:madera` |  |
| `estructura.vivienda.casa_urbana` | Casa urbana de dos plantas | 8x7x8 | 500 | pendiente | `bioma:ciudad` `faccion:imperio` `uso:habitable` `material:piedra` |  |
| `estructura.vivienda.palacio` | Palacio imperial | 40x18x30 | 3000 | pendiente | `bioma:ciudad` `faccion:imperio` `acto:2` `especial:narrativo` |  |
| `estructura.campamento.fogata` | Fogata | 1.2x0.5x1.2 | 60 | pendiente | `faccion:nomada` `uso:interactuable` | Hoy es un primitivo procedural. |
| `estructura.campamento.corral` | Corral de cuerdas y estacas | 10x1.2x10 | 150 | pendiente | `faccion:nomada` `uso:decorado` `material:madera` |  |
| `estructura.campamento.secadero` | Secadero de carne | 3x2x1 | 80 | pendiente | `faccion:nomada` `uso:interactuable` `material:madera` |  |
| `estructura.campamento.forja` | Forja portátil | 2x1.5x2 | 300 | pendiente | `faccion:nomada` `uso:interactuable` `material:hierro` | Función: herrero. |
| `estructura.campamento.enfermeria` | Yurta enfermería | 4x2.6x4 | 350 | pendiente | `faccion:nomada` `uso:interactuable` `material:fieltro` | Función: curandero. |
| `estructura.campamento.jaula_prisioneros` | Jaula de prisioneros | 3x2x3 | 150 | pendiente | `faccion:cualquiera` `uso:interactuable` `material:madera` |  |
| `estructura.campamento.poste_ejecucion` | Poste de castigo | 0.5x2.5x0.5 | 50 | pendiente | `faccion:cualquiera` `uso:interactuable` `material:madera` |  |
| `estructura.campamento.empalizada` | Empalizada modular | 4x2.5x0.4 | 80 | pendiente | `faccion:nomada` `uso:destructible` `material:madera` |  |
| `estructura.campamento.atalaya` | Atalaya de madera | 3x6x3 | 200 | pendiente | `faccion:nomada` `uso:trepable` `material:madera` |  |
| `estructura.imperio.muralla` | Muralla modular | 8x8x3 | 200 | pendiente | `bioma:ciudad` `faccion:imperio` `uso:trepable` `material:piedra` |  |
| `estructura.imperio.puerta` | Puerta de la ciudad | 10x10x5 | 600 | pendiente | `bioma:ciudad` `faccion:imperio` `uso:destructible` `material:madera` |  |
| `estructura.imperio.torre` | Torre de vigilancia | 5x14x5 | 400 | pendiente | `bioma:ciudad` `faccion:imperio` `uso:trepable` `material:piedra` |  |
| `estructura.imperio.cuartel` | Cuartel | 16x6x10 | 600 | pendiente | `bioma:ciudad` `faccion:imperio` |  |
| `estructura.imperio.granero` | Granero | 10x7x8 | 400 | pendiente | `bioma:ciudad` `faccion:imperio` `uso:interactuable` | Las cosechas marchitas (acto II). |
| `estructura.imperio.molino` | Molino | 6x10x6 | 500 | pendiente | `bioma:ciudad` `faccion:imperio` |  |
| `estructura.imperio.fundicion` | Fundición / taller industrial | 18x10x14 | 1200 | pendiente | `bioma:ciudad` `faccion:imperio` `especial:narrativo` | Capacidad industrial del imperio. |
| `estructura.imperio.cadalso` | Cadalso de ejecución pública | 6x3x6 | 250 | pendiente | `bioma:ciudad` `faccion:imperio` `acto:2` `especial:narrativo` | Escena de la huida del acto II. |
| `estructura.culto.altar` | Altar del culto | 12x6x12 | 1500 | pendiente | `bioma:ciudad` `faccion:culto` `acto:4` `especial:narrativo` `uso:destructible` | Objetivo del asalto final. |
| `estructura.culto.templo` | Templo del culto | 24x14x30 | 2500 | pendiente | `bioma:ciudad` `faccion:culto` `acto:4` |  |
| `estructura.culto.jaula_sacrificio` | Jaula de cautivos para el sacrificio | 3x2.5x3 | 200 | pendiente | `faccion:culto` `uso:interactuable` `material:hierro` | Rescates del acto III. |
| `estructura.ruina.muro` | Muro en ruinas | 6x3x1 | 150 | pendiente | `bioma:cualquiera` `uso:decorado` `material:piedra` |  |
| `estructura.ruina.yurta_quemada` | Yurta quemada | 4.5x1.5x4.5 | 250 | pendiente | `faccion:nomada` `acto:1` `uso:decorado` | Restos de la masacre. |
| `estructura.ruina.escombros` | Escombros | 3x0.8x3 | 80 | pendiente | `bioma:cualquiera` `uso:decorado` | Lo que deja la lluvia torrencial de una estructura sin mantenimiento. |
| `estructura.ruina.cenizas` | Cenizas de una estructura | 3x0.5x3 | 80 | pendiente | `bioma:cualquiera` `uso:decorado` | Lo que deja el fuego. |
| `estructura.campamento.refugio` | Refugio de ramas y pieles | 3x2x3 | 150 | pendiente | `faccion:nomada` `uso:habitable` `uso:construible` | Construcción en grupo. |
| `estructura.campamento.hoguera` | Hoguera grande | 2.5x1.5x2.5 | 120 | pendiente | `faccion:nomada` `uso:interactuable` `uso:construible` | Construcción en grupo: calor y luz para todo el campamento. |
| `estructura.campamento.horno_cocina` | Horno de cocina de barro | 1.5x1.4x1.5 | 150 | pendiente | `faccion:nomada` `uso:interactuable` `uso:construible` `material:barro` | Construcción en grupo; el cocinero acelera la obra. |
| `estructura.campamento.horno_bronce` | Horno de fundición de bronce | 2x2.5x2 | 300 | pendiente | `uso:interactuable` `uso:construible` `material:barro` | Armas y armaduras de bronce. Requiere un herrero. |
| `estructura.campamento.horno_acero` | Horno de fundición de acero | 2.5x3.5x2.5 | 400 | pendiente | `uso:interactuable` `uso:construible` `material:piedra` | Armas y armaduras de acero. Requiere un herrero. |
| `estructura.defensa.trinchera` | Trinchera | 1.2x0.8x4 | 80 | pendiente | `faccion:cualquiera` `uso:cobertura` | Se cava con pala (acción individual). |
| `estructura.defensa.muro_piedra` | Muro de piedra seca | 4x2x0.6 | 120 | pendiente | `faccion:nomada` `uso:destructible` `uso:construible` `material:piedra` | Construcción en grupo. |

<a id="vehiculo"></a>

## Vehículos

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `vehiculo.agua.bote_remos` | Bote de remos | 1.5x0.8x4 | 250 | pendiente | `bioma:fiordo` `uso:conducible` `material:madera` |  |
| `vehiculo.agua.canoa_pieles` | Canoa de pieles | 0.9x0.6x5 | 200 | pendiente | `bioma:fiordo` `bioma:bosque` `uso:conducible` `material:cuero` |  |
| `vehiculo.agua.balsa` | Balsa | 2.5x0.5x3 | 100 | pendiente | `bioma:cualquiera` `uso:conducible` `material:madera` |  |
| `vehiculo.agua.velero` | Velero | 3x8x9 | 600 | pendiente | `bioma:fiordo` `uso:conducible` `material:madera` |  |
| `vehiculo.agua.barco_carga` | Barco de carga | 6x14x20 | 1500 | pendiente | `bioma:fiordo` `uso:conducible` `faccion:neutral` |  |
| `vehiculo.agua.barco_guerra` | Barco de guerra con cañones | 8x18x28 | 2500 | pendiente | `bioma:fiordo` `uso:conducible` `faccion:imperio` |  |
| `vehiculo.tierra.carreta_bueyes` | Yunta de bueyes con carreta | 2.5x2.5x6 | 600 | pendiente | `faccion:nomada` `uso:conducible` `material:madera` | Mueve el campamento. Los bueyes: animal.ganado.buey. |
| `vehiculo.tierra.kibitka` | Carro con yurta montada (kibitka) | 4x4x6 | 800 | pendiente | `faccion:nomada` `uso:conducible` |  |
| `vehiculo.tierra.carreta_mercancias` | Carreta de mercancías | 2x2x4 | 400 | pendiente | `faccion:neutral` `uso:conducible` `material:madera` |  |
| `vehiculo.tierra.carruaje` | Carruaje imperial | 2.2x2.8x5 | 900 | pendiente | `faccion:imperio` `uso:conducible` |  |
| `vehiculo.tierra.trineo` | Trineo | 1.2x0.8x3 | 200 | pendiente | `bioma:glaciar` `uso:conducible` `material:madera` |  |

<a id="animal"></a>

## Animales

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `animal.montura.caballo_estepario` | Caballo estepario | 0.6x1.5x2.2 | 900 | pendiente | `faccion:nomada` `uso:montable` `rig:cuadrupedo` |  |
| `animal.montura.caballo_guerra` | Caballo de guerra | 0.7x1.7x2.4 | 1000 | pendiente | `uso:montable` `rig:cuadrupedo` | Admite barda (armadura de caballo). |
| `animal.montura.camello_bactriano` | Camello bactriano | 0.8x2.2x3 | 1000 | pendiente | `bioma:desierto` `uso:montable` `rig:cuadrupedo` |  |
| `animal.montura.dromedario` | Dromedario | 0.8x2.2x3 | 1000 | pendiente | `bioma:desierto` `uso:montable` `rig:cuadrupedo` |  |
| `animal.montura.elefante_guerra` | Elefante de guerra | 2x3.2x5 | 1500 | pendiente | `uso:montable` `rig:cuadrupedo` `talla:gigante` | Fuerza y asedio. |
| `animal.montura.reno` | Reno de tiro | 0.6x1.3x2 | 800 | pendiente | `bioma:fiordo` `bioma:glaciar` `uso:montable` `rig:cuadrupedo` |  |
| `animal.ganado.oveja` | Oveja | 0.4x0.8x1.1 | 400 | pendiente | `faccion:nomada` `rig:cuadrupedo` |  |
| `animal.ganado.cabra` | Cabra | 0.4x0.8x1.1 | 400 | pendiente | `faccion:nomada` `rig:cuadrupedo` |  |
| `animal.ganado.yak` | Yak | 0.9x1.7x2.5 | 800 | pendiente | `bioma:glaciar` `rig:cuadrupedo` |  |
| `animal.ganado.buey` | Buey | 0.8x1.6x2.5 | 700 | pendiente | `rig:cuadrupedo` |  |
| `animal.ganado.perro_pastor` | Perro pastor (mastín) | 0.35x0.8x1.1 | 500 | pendiente | `faccion:nomada` `rig:cuadrupedo` |  |
| `animal.salvaje.lobo` | Lobo | 0.3x0.8x1.4 | 500 | pendiente | `faccion:salvaje` `rig:cuadrupedo` |  |
| `animal.salvaje.tigre` | Tigre | 0.5x1x2.6 | 700 | pendiente | `faccion:salvaje` `bioma:bosque` `rig:cuadrupedo` |  |
| `animal.salvaje.oso` | Oso pardo | 0.9x1.3x2.2 | 700 | pendiente | `faccion:salvaje` `bioma:bosque` `rig:cuadrupedo` |  |
| `animal.salvaje.ciervo` | Ciervo | 0.5x1.9x1.8 | 500 | pendiente | `faccion:salvaje` `bioma:bosque` `rig:cuadrupedo` |  |
| `animal.salvaje.ibice` | Íbice | 0.4x1.2x1.4 | 500 | pendiente | `faccion:salvaje` `bioma:glaciar` `rig:cuadrupedo` |  |
| `animal.salvaje.zorro` | Zorro | 0.25x0.45x0.9 | 300 | pendiente | `faccion:salvaje` `rig:cuadrupedo` |  |
| `animal.salvaje.leopardo_nieves` | Leopardo de las nieves | 0.4x0.7x2 | 600 | pendiente | `faccion:salvaje` `bioma:glaciar` `rig:cuadrupedo` |  |
| `animal.ave.aguila` | Águila (cetrería) | 1.8x0.9x0.9 | 300 | pendiente | `rig:ave` `uso:equipable` | Puede acompañar al jugador. |
| `animal.ave.halcon` | Halcón (cetrería) | 1x0.5x0.5 | 250 | pendiente | `rig:ave` `uso:equipable` |  |
| `animal.ave.buitre` | Buitre | 2.4x1x1 | 300 | pendiente | `faccion:salvaje` `rig:ave` |  |
| `animal.montura.mula` | Mula | 0.6x1.5x2 | 800 | pendiente | `uso:montable` `rig:cuadrupedo` | Montura de carga: resiste el desierto. |
| `animal.montura.burro` | Burro | 0.5x1.2x1.6 | 700 | pendiente | `bioma:desierto` `uso:montable` `rig:cuadrupedo` |  |
| `animal.ganado.becerro` | Becerro | 0.4x1x1.4 | 500 | pendiente | `faccion:nomada` `rig:cuadrupedo` | Se pastorea; carne y piel al sacrificarlo. |
| `animal.salvaje.perro_salvaje` | Perro asilvestrado | 0.3x0.7x1.1 | 500 | pendiente | `faccion:salvaje` `rig:cuadrupedo` | Caza en jauría; domable (pelea, lazo y comida). |
| `animal.salvaje.puma` | Puma | 0.4x0.8x1.9 | 600 | pendiente | `faccion:salvaje` `rig:cuadrupedo` | Cazador solitario: acecha. |
| `animal.salvaje.hiena` | Hiena | 0.35x0.85x1.3 | 500 | pendiente | `faccion:salvaje` `bioma:desierto` `rig:cuadrupedo` | Siempre hostil; en clan. |
| `animal.salvaje.coyote` | Coyote | 0.3x0.6x1.1 | 400 | pendiente | `faccion:salvaje` `rig:cuadrupedo` | Siempre hostil; en pareja o grupo. |
| `animal.salvaje.jabali` | Jabalí | 0.45x0.9x1.4 | 500 | pendiente | `faccion:salvaje` `rig:cuadrupedo` | Siempre hostil; embiste. |
| `animal.salvaje.antilope` | Antílope saiga | 0.35x0.8x1.3 | 500 | pendiente | `faccion:salvaje` `bioma:estepa` `rig:cuadrupedo` | Presa: manadas. |
| `animal.salvaje.reno` | Reno salvaje | 0.6x1.3x2 | 600 | pendiente | `faccion:salvaje` `bioma:glaciar` `rig:cuadrupedo` | Presa: manadas del frío. |
| `animal.salvaje.gacela` | Gacela | 0.3x0.9x1.2 | 500 | pendiente | `faccion:salvaje` `bioma:desierto` `rig:cuadrupedo` | Presa: la más rápida. |
| `animal.salvaje.liebre` | Liebre | 0.15x0.3x0.5 | 200 | pendiente | `faccion:salvaje` `rig:cuadrupedo` | Presa solitaria; caza de halcón. |
| `animal.ave.cuervo` | Cuervo | 0.9x0.4x0.5 | 200 | pendiente | `faccion:salvaje` `rig:ave` `uso:equipable` | Domado, explora: revela el mapa. |
| `animal.acuatico.cocodrilo` | Cocodrilo | 0.6x0.5x3.5 | 700 | pendiente | `faccion:salvaje` `rig:cuadrupedo` | Embosca desde el agua; no se aleja de su orilla. |
| `animal.acuatico.tortuga_marina` | Tortuga marina | 0.9x0.4x1 | 400 | pendiente | `faccion:salvaje` `rig:cuadrupedo` | Presa lenta en tierra; huye al agua. |
| `animal.acuatico.peces` | Pez (banco) | 0.08x0.1x0.35 | 60 | pendiente | `faccion:salvaje` `rig:ninguno` | Bancos en los lagos; se pescan con lanza o flecha. |
| `animal.salvaje.vibora` | Víbora | 0.06x0.06x1 | 150 | pendiente | `faccion:salvaje` `rig:ninguno` | Muerde si te acercas: veneno. |
| `animal.salvaje.escorpion` | Escorpión | 0.1x0.05x0.2 | 100 | pendiente | `faccion:salvaje` `bioma:desierto` `rig:ninguno` | Pica si te acercas: veneno. |
| `animal.salvaje.arana` | Araña | 0.12x0.05x0.15 | 100 | pendiente | `faccion:salvaje` `rig:ninguno` | Muerde si te acercas: veneno leve. |
| `animal.insecto.abejas` | Abeja (enjambre) | 0.02x0.02x0.02 | 20 | pendiente | `faccion:salvaje` `rig:ninguno` | Defienden la colmena; el humo las calma. |
| `animal.insecto.colmena` | Colmena silvestre | 0.4x0.5x0.4 | 80 | pendiente | `faccion:salvaje` `uso:interactuable` | Con la antorcha (humo) se toma la miel. |
| `animal.insecto.avispas` | Avispa (enjambre) | 0.02x0.02x0.03 | 20 | pendiente | `faccion:salvaje` `rig:ninguno` | Atacan a quien se acerca al nido. |
| `animal.insecto.avispero` | Avispero | 0.3x0.4x0.3 | 80 | pendiente | `faccion:salvaje` |  |
| `animal.insecto.mosquitos` | Mosquito (nube) | 0.01x0.01x0.01 | 10 | pendiente | `faccion:salvaje` `rig:ninguno` | Junto al agua, con calor, al atardecer y de noche. |
| `animal.insecto.moscas` | Mosca (nube) | 0.01x0.01x0.01 | 10 | pendiente | `faccion:salvaje` `rig:ninguno` | Sobre los cadáveres. |

<a id="arma"></a>

## Armas

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `arma.corta.cuchillo` | Cuchillo de hueso y hierro | 0.05x0.3x0.03 | 60 | pendiente | `uso:equipable` `manos:una` |  |
| `arma.corta.daga` | Daga | 0.05x0.4x0.03 | 80 | pendiente | `uso:equipable` `manos:una` |  |
| `arma.corta.sable` | Sable curvo | 0.08x0.9x0.03 | 120 | pendiente | `uso:equipable` `faccion:nomada` `manos:una` |  |
| `arma.corta.hacha_mano` | Hacha de mano | 0.15x0.6x0.04 | 100 | pendiente | `uso:equipable` `manos:una` |  |
| `arma.corta.maza` | Maza | 0.1x0.7x0.1 | 100 | pendiente | `uso:equipable` `manos:una` |  |
| `arma.larga.lanza` | Lanza | 0.1x2.4x0.1 | 80 | pendiente | `uso:equipable` `manos:una` |  |
| `arma.larga.guja` | Guja | 0.2x2.2x0.05 | 120 | pendiente | `uso:equipable` `manos:dos` |  |
| `arma.larga.pica` | Pica imperial | 0.1x4.5x0.1 | 80 | pendiente | `uso:equipable` `faccion:imperio` `manos:dos` |  |
| `arma.larga.alabarda` | Alabarda | 0.3x2.2x0.05 | 140 | pendiente | `uso:equipable` `faccion:imperio` `manos:dos` |  |
| `arma.larga.espada_larga` | Espada larga | 0.2x1.2x0.03 | 120 | pendiente | `uso:equipable` `manos:una` |  |
| `arma.distancia.arco_compuesto` | Arco compuesto | 0.1x1.2x0.05 | 150 | pendiente | `uso:equipable` `requiere:proyectil` `faccion:nomada` `manos:dos` |  |
| `arma.distancia.arco_largo` | Arco largo | 0.08x1.8x0.05 | 120 | pendiente | `uso:equipable` `requiere:proyectil` `manos:dos` |  |
| `arma.distancia.ballesta` | Ballesta | 0.7x0.25x0.9 | 250 | pendiente | `uso:equipable` `requiere:proyectil` `manos:dos` |  |
| `arma.distancia.mosquete` | Mosquete de mecha | 0.1x0.25x1.5 | 300 | pendiente | `uso:equipable` `requiere:proyectil` `faccion:imperio` `manos:dos` |  |
| `arma.distancia.canon` | Cañón de campaña | 1.5x1.2x3 | 800 | pendiente | `uso:equipable` `requiere:proyectil` `faccion:imperio` |  |
| `arma.distancia.honda` | Honda | 0.05x0.6x0.05 | 40 | pendiente | `uso:equipable` `requiere:proyectil` `manos:una` |  |
| `arma.distancia.lazo` | Lazo | 0.5x0.1x0.5 | 80 | pendiente | `uso:equipable` `faccion:nomada` `manos:una` |  |
| `arma.distancia.boleadoras` | Boleadoras | 0.4x0.1x0.4 | 60 | pendiente | `uso:equipable` `manos:una` |  |
| `arma.especial.guja_hoja_ancha` | Guja de hoja ancha | 0.25x2.2x0.05 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:dos` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.sable_damasquinado` | Sable de acero damasquinado | 0.08x0.95x0.03 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:una` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.arco_cuerno` | Arco compuesto de cuerno | 0.1x1.2x0.05 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:dos` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.maza_tigre` | Maza de bronce con cabeza de tigre | 0.15x0.8x0.15 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:una` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.lazo_plomadas` | Lazo con plomadas | 0.5x0.1x0.5 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:una` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.hacha_ceremonial` | Hacha ceremonial | 0.3x0.9x0.04 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:una` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.lanza_doble_punta` | Lanza de caballería de doble punta | 0.1x2.8x0.1 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:dos` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.ballesta_repeticion` | Ballesta de repetición | 0.6x0.35x0.9 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:dos` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.espada_padre` | Espada ceremonial del padre | 0.12x1x0.04 | 500 | pendiente | `uso:equipable` `acto:3` `especial:legado` `especial:narrativo` `material:oro` `manos:una` | Hallada en la tumba helada; se empuña en el asalto final. |
| `arma.corta.sable_bronce` | Sable de bronce | 0.08x0.9x0.03 | 120 | pendiente | `uso:equipable` `manos:una` `material:bronce` | Se forja en el horno de bronce. |
| `arma.corta.sable_acero` | Sable de acero | 0.08x0.95x0.03 | 120 | pendiente | `uso:equipable` `manos:una` `material:hierro` | Se forja en el horno de acero. |
| `arma.larga.lanza_bronce` | Lanza de punta de bronce | 0.1x2.4x0.1 | 80 | pendiente | `uso:equipable` `manos:una` `material:bronce` | Se forja en el horno de bronce. |

<a id="proyectil"></a>

## Proyectiles

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `proyectil.flecha.comun` | Flecha | 0.02x0.8x0.02 | 20 | pendiente | `uso:municion` |  |
| `proyectil.flecha.silbadora` | Flecha silbadora (señales) | 0.03x0.8x0.03 | 30 | pendiente | `uso:municion` `faccion:nomada` |  |
| `proyectil.virote.comun` | Virote de ballesta | 0.02x0.4x0.02 | 20 | pendiente | `uso:municion` |  |
| `proyectil.bala.mosquete` | Bala de mosquete | 0.02x0.02x0.02 | 8 | pendiente | `uso:municion` |  |
| `proyectil.bala.canon` | Bala de cañón | 0.15x0.15x0.15 | 20 | pendiente | `uso:municion` |  |
| `proyectil.piedra.honda` | Piedra de honda | 0.05x0.05x0.05 | 8 | pendiente | `uso:municion` |  |

<a id="escudo"></a>

## Escudos

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `escudo.mano.mimbre` | Escudo redondo de mimbre | 0.6x0.6x0.08 | 80 | pendiente | `uso:equipable` `faccion:nomada` `material:madera` `manos:escudo` |  |
| `escudo.mano.cuero` | Escudo de cuero endurecido | 0.55x0.55x0.06 | 80 | pendiente | `uso:equipable` `faccion:nomada` `material:cuero` `manos:escudo` |  |
| `escudo.mano.umbo` | Escudo de madera con umbo | 0.8x0.8x0.1 | 100 | pendiente | `uso:equipable` `faccion:neutral` `material:madera` `manos:escudo` |  |
| `escudo.grande.paves` | Pavés imperial | 0.7x1.5x0.15 | 120 | pendiente | `uso:equipable` `faccion:imperio` `material:madera` `manos:escudo` |  |
| `escudo.mano.lamina` | Escudo de láminas de hierro | 0.6x0.8x0.08 | 120 | pendiente | `uso:equipable` `faccion:imperio` `material:hierro` `manos:escudo` |  |
| `escudo.mano.culto` | Escudo ceremonial del culto | 0.7x0.9x0.08 | 200 | pendiente | `uso:equipable` `faccion:culto` `material:bronce` `manos:escudo` |  |

<a id="totem"></a>

## Tótems

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `totem.clan.poste_clan` | Poste tótem del clan | 0.6x4x0.6 | 200 | pendiente | `faccion:nomada` `material:madera` |  |
| `totem.clan.lobo` | Tótem del lobo | 0.6x3x0.6 | 250 | pendiente | `faccion:nomada` `material:madera` | Bonificación de manada (pendiente de diseño). |
| `totem.clan.ciervo` | Tótem del ciervo | 0.8x3x0.6 | 250 | pendiente | `faccion:nomada` `material:madera` |  |
| `totem.clan.aguila` | Tótem del águila | 1.2x3.5x0.6 | 250 | pendiente | `faccion:nomada` `material:madera` |  |
| `totem.clan.tigre` | Tótem del tigre | 0.8x3x0.8 | 250 | pendiente | `faccion:nomada` `material:madera` |  |
| `totem.clan.sulde` | Sulde (estandarte de crines) | 0.4x3x0.4 | 150 | pendiente | `faccion:nomada` `uso:equipable` `material:crin` | Espíritu del clan; cola de caballo. |
| `totem.clan.estandarte_tamga` | Estandarte con la tamga del padre | 1x3.5x0.1 | 150 | pendiente | `faccion:nomada` `especial:legado` | Signo de clan del padre. |
| `totem.culto.idolo_culto` | Ídolo del culto | 1x3x1 | 400 | pendiente | `faccion:culto` `acto:4` `material:bronce` | Símbolo del culto: pendiente de definir por el autor. |
| `totem.culto.craneos_culto` | Pila de cráneos del culto | 1x1.5x1 | 200 | pendiente | `faccion:culto` `uso:decorado` `material:hueso` |  |
| `totem.proteccion.guardian` | Tótem de protección | 0.8x3.5x0.8 | 250 | pendiente | `faccion:nomada` `uso:interactuable` `uso:construible` `material:madera` | Construcción en grupo; protege el campamento (efecto pendiente de diseño). |

<a id="armadura"></a>

## Armaduras (por piezas)

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `armadura.casco.laminar_cuero` | Casco laminar de cuero | 0.3x0.35x0.3 | 150 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.cuello.laminar_cuero` | Gorjal / cofia laminar de cuero | 0.35x0.2x0.35 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.torso.laminar_cuero` | Coraza / peto laminar de cuero | 0.5x0.6x0.35 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.hombreras.laminar_cuero` | Hombreras (par) laminar de cuero | 0.6x0.2x0.3 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.brazales.laminar_cuero` | Brazales (par) laminar de cuero | 0.15x0.3x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.guantes.laminar_cuero` | Guanteletes (par) laminar de cuero | 0.12x0.2x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.faldar.laminar_cuero` | Faldar laminar de cuero | 0.5x0.4x0.4 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.grebas.laminar_cuero` | Grebas (par) laminar de cuero | 0.15x0.45x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.botas.laminar_cuero` | Botas (par) laminar de cuero | 0.15x0.35x0.3 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.casco.fieltro` | Casco de fieltro acolchado | 0.3x0.35x0.3 | 150 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` |  |
| `armadura.cuello.fieltro` | Gorjal / cofia de fieltro acolchado | 0.35x0.2x0.35 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` |  |
| `armadura.torso.fieltro` | Coraza / peto de fieltro acolchado | 0.5x0.6x0.35 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` |  |
| `armadura.hombreras.fieltro` | Hombreras (par) de fieltro acolchado | 0.6x0.2x0.3 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` |  |
| `armadura.brazales.fieltro` | Brazales (par) de fieltro acolchado | 0.15x0.3x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` |  |
| `armadura.faldar.fieltro` | Faldar de fieltro acolchado | 0.5x0.4x0.4 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` |  |
| `armadura.botas.fieltro` | Botas (par) de fieltro acolchado | 0.15x0.35x0.3 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` |  |
| `armadura.casco.escamas_hierro` | Casco de escamas de hierro | 0.3x0.35x0.3 | 150 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.cuello.escamas_hierro` | Gorjal / cofia de escamas de hierro | 0.35x0.2x0.35 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.torso.escamas_hierro` | Coraza / peto de escamas de hierro | 0.5x0.6x0.35 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.hombreras.escamas_hierro` | Hombreras (par) de escamas de hierro | 0.6x0.2x0.3 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.brazales.escamas_hierro` | Brazales (par) de escamas de hierro | 0.15x0.3x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.guantes.escamas_hierro` | Guanteletes (par) de escamas de hierro | 0.12x0.2x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.faldar.escamas_hierro` | Faldar de escamas de hierro | 0.5x0.4x0.4 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.grebas.escamas_hierro` | Grebas (par) de escamas de hierro | 0.15x0.45x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.botas.escamas_hierro` | Botas (par) de escamas de hierro | 0.15x0.35x0.3 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.casco.malla` | Casco de malla | 0.3x0.35x0.3 | 150 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.cuello.malla` | Gorjal / cofia de malla | 0.35x0.2x0.35 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.torso.malla` | Coraza / peto de malla | 0.5x0.6x0.35 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.hombreras.malla` | Hombreras (par) de malla | 0.6x0.2x0.3 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.brazales.malla` | Brazales (par) de malla | 0.15x0.3x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.guantes.malla` | Guanteletes (par) de malla | 0.12x0.2x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.faldar.malla` | Faldar de malla | 0.5x0.4x0.4 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.grebas.malla` | Grebas (par) de malla | 0.15x0.45x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.botas.malla` | Botas (par) de malla | 0.15x0.35x0.3 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.casco.culto` | Casco ritual del culto | 0.3x0.35x0.3 | 150 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.cuello.culto` | Gorjal / cofia ritual del culto | 0.35x0.2x0.35 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.torso.culto` | Coraza / peto ritual del culto | 0.5x0.6x0.35 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.hombreras.culto` | Hombreras (par) ritual del culto | 0.6x0.2x0.3 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.brazales.culto` | Brazales (par) ritual del culto | 0.15x0.3x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.guantes.culto` | Guanteletes (par) ritual del culto | 0.12x0.2x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.faldar.culto` | Faldar ritual del culto | 0.5x0.4x0.4 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.grebas.culto` | Grebas (par) ritual del culto | 0.15x0.45x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.botas.culto` | Botas (par) ritual del culto | 0.15x0.35x0.3 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.casco.mascara_culto` | Máscara ritual del culto | 0.25x0.3x0.2 | 200 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.casco.oro_padre` | Casco de oro del padre | 0.3x0.4x0.3 | 300 | pendiente | `uso:equipable` `rig:humanoide` `especial:legado` `material:oro` `acto:1` | Lo lleva el padre en el prólogo. |
| `armadura.montura.barda_cuero` | Barda de cuero para caballo | 0.8x1.2x2 | 400 | pendiente | `uso:equipable` `rig:cuadrupedo` `faccion:nomada` `material:cuero` |  |
| `armadura.montura.barda_hierro` | Barda de hierro para caballo | 0.8x1.2x2 | 500 | pendiente | `uso:equipable` `rig:cuadrupedo` `faccion:imperio` `material:hierro` |  |
| `armadura.casco.bronce` | Casco de bronce | 0.3x0.35x0.3 | 150 | pendiente | `uso:equipable` `rig:humanoide` `material:bronce` | Se forja en el horno de bronce. |

<a id="vestimenta"></a>

## Vestimenta (personalización)

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `vestimenta.torso.deel` | Deel (caftán cruzado) | 0.6x1.2x0.4 | 300 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.torso.deel_invierno` | Deel de invierno con piel | 0.7x1.3x0.5 | 350 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.cabeza.gorro_piel` | Gorro de piel | 0.3x0.25x0.3 | 100 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.cabeza.gorro_punta` | Gorro de fieltro de punta | 0.3x0.4x0.3 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.piernas.pantalon` | Pantalón de montar | 0.4x0.9x0.3 | 150 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.pies.botas_fieltro` | Botas de fieltro | 0.15x0.4x0.3 | 100 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.espalda.capa` | Capa de lana | 0.7x1.2x0.4 | 200 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.espalda.capa_piel` | Capa de piel de lobo | 0.8x1.2x0.5 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.cintura.cinturon` | Cinturón con placa de estilo animal | 0.4x0.08x0.3 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.cuello.bufanda` | Bufanda / velo contra la arena | 0.3x0.3x0.3 | 60 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.torso.tunica_campesina` | Túnica campesina | 0.6x1.1x0.4 | 200 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` | Personalización del personaje (Fase 3). |
| `vestimenta.torso.uniforme_imperial` | Uniforme imperial | 0.6x1.4x0.4 | 300 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` | Personalización del personaje (Fase 3). |
| `vestimenta.torso.tunica_culto` | Túnica del culto | 0.7x1.6x0.5 | 300 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` | Personalización del personaje (Fase 3). |
| `vestimenta.torso.harapos` | Harapos de prisionero | 0.6x1.1x0.4 | 150 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.cabeza.sombrero` | Sombrero de ala ancha | 0.45x0.15x0.45 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` | Da sombra: contra el sol y el calor. |
| `vestimenta.cabeza.gorro_lobo` | Gorro de cabeza de lobo | 0.3x0.3x0.35 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | Abriga y escarmienta: el enemigo se lo piensa. |
| `vestimenta.cuello.panuelo_desierto` | Pañuelo del desierto | 0.3x0.2x0.3 | 60 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:seda` | Cubre cara y cuello del sol y la arena. |
| `vestimenta.cuello.bufanda_piel` | Bufanda de piel de zorro | 0.3x0.2x0.3 | 60 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | De piel de coyote. |
| `vestimenta.torso.tunica_seda` | Túnica de seda blanca | 0.6x1.2x0.35 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:seda` | Fresca y blanca: refleja el sol. |
| `vestimenta.torso.tunica_lana` | Túnica de lana | 0.6x1.2x0.4 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:lana` |  |
| `vestimenta.espalda.abrigo_oso` | Abrigo de piel de oso | 0.9x1.3x0.6 | 300 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | El más abrigado; pesa. Escarmienta mucho. |
| `vestimenta.espalda.abrigo_tigre` | Abrigo de piel de tigre | 0.9x1.3x0.5 | 300 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | Escarmienta más que ninguno. |
| `vestimenta.espalda.capa_puma` | Capa de piel de puma | 0.8x1.2x0.5 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | Escarmienta. |
| `vestimenta.espalda.capa_hiena` | Capa de piel de hiena | 0.8x1.2x0.5 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | Escarmienta un poco. |
| `vestimenta.espalda.abrigo_reno` | Abrigo de piel de reno | 0.9x1.3x0.6 | 300 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | Muy abrigado. |
| `vestimenta.espalda.abrigo_cabra` | Abrigo de lana de cabra | 0.9x1.3x0.6 | 300 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:lana` |  |
| `vestimenta.espalda.manto_blanco` | Manto blanco del desierto | 0.8x1.3x0.4 | 220 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:seda` | Sombra para todo el cuerpo. |
| `vestimenta.pies.botas_piel` | Botas de piel de reno | 0.15x0.4x0.3 | 100 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | Abrigan y no se mojan. |
| `vestimenta.pies.sandalias` | Sandalias de cuero | 0.12x0.08x0.28 | 60 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | Frescas. |

<a id="accesorio"></a>

## Accesorios: amuletos, tatuajes, arreos y mensajes

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `accesorio.amuleto.lobo` | Amuleto del lobo | 0.08x0.06x0.01 | 80 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Placa de estilo animal; buffs en src/sim/loadout.c. |
| `accesorio.amuleto.ciervo` | Amuleto del ciervo | 0.08x0.06x0.01 | 80 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Placa de estilo animal; buffs en src/sim/loadout.c. |
| `accesorio.amuleto.aguila_ibice` | Amuleto del águila y el íbice | 0.08x0.06x0.01 | 80 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Placa de estilo animal; buffs en src/sim/loadout.c. |
| `accesorio.amuleto.tigre_dragon` | Amuleto del tigre y el dragón | 0.08x0.06x0.01 | 80 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Placa de estilo animal; buffs en src/sim/loadout.c. |
| `accesorio.amuleto.oso` | Amuleto del oso | 0.08x0.06x0.01 | 80 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Placa de estilo animal; buffs en src/sim/loadout.c. |
| `accesorio.amuleto.caballo` | Amuleto del caballo | 0.08x0.06x0.01 | 80 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Placa de estilo animal; buffs en src/sim/loadout.c. |
| `accesorio.tatuaje.lobo` | Tatuaje del lobo | 0.25x0.25x0 | 0 | pendiente | `uso:equipable` `formato:textura` `estilo:animal` | Calcomanía (PNG con alfa), no modelo. Permanente. |
| `accesorio.tatuaje.ciervo` | Tatuaje del ciervo | 0.25x0.25x0 | 0 | pendiente | `uso:equipable` `formato:textura` `estilo:animal` | Calcomanía (PNG con alfa), no modelo. Permanente. |
| `accesorio.tatuaje.grifo` | Tatuaje del grifo | 0.25x0.25x0 | 0 | pendiente | `uso:equipable` `formato:textura` `estilo:animal` | Calcomanía (PNG con alfa), no modelo. Permanente. |
| `accesorio.tatuaje.tamga` | Tatuaje de la tamga | 0.25x0.25x0 | 0 | pendiente | `uso:equipable` `formato:textura` `estilo:animal` | Calcomanía (PNG con alfa), no modelo. Permanente. |
| `accesorio.tatuaje.olas` | Tatuaje de olas | 0.25x0.25x0 | 0 | pendiente | `uso:equipable` `formato:textura` `estilo:animal` | Calcomanía (PNG con alfa), no modelo. Permanente. |
| `accesorio.arreo.silla_montar` | Silla de montar | 0.5x0.3x0.6 | 200 | pendiente | `uso:equipable` `faccion:nomada` `material:cuero` |  |
| `accesorio.arreo.brida` | Brida y riendas | 0.3x0.4x0.6 | 100 | pendiente | `uso:equipable` `material:cuero` |  |
| `accesorio.arreo.estribos` | Estribos (par) | 0.15x0.2x0.1 | 60 | pendiente | `uso:equipable` `material:hierro` |  |
| `accesorio.arreo.alforjas` | Alforjas | 0.6x0.4x0.3 | 120 | pendiente | `uso:equipable` `material:cuero` |  |
| `accesorio.arreo.placa_arreo_padre` | Placa de arreo de estilo animal del padre | 0.12x0.08x0.01 | 120 | pendiente | `uso:equipable` `especial:rastro_padre` `material:oro` `estilo:animal` | Rastros del padre (acto III). |
| `accesorio.mensaje.piedra_runica` | Piedra con mensaje cifrado | 0.5x0.8x0.2 | 120 | pendiente | `acto:3` `especial:rastro_padre` `material:piedra` | Mensajes en las antiguas lenguas de la estepa. |
| `accesorio.mensaje.tablilla` | Tablilla de madera cifrada | 0.2x0.3x0.02 | 60 | pendiente | `acto:3` `especial:rastro_padre` `material:madera` |  |
| `accesorio.anillo.bronce` | Anillo de bronce | 0.025x0.025x0.008 | 40 | pendiente | `uso:equipable` `material:bronce` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.anillo.plata` | Anillo de plata | 0.025x0.025x0.008 | 40 | pendiente | `uso:equipable` `material:plata` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.anillo.oro` | Anillo de oro | 0.025x0.025x0.008 | 40 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.brazalete.bronce` | Brazalete de bronce | 0.08x0.03x0.08 | 60 | pendiente | `uso:equipable` `material:bronce` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.brazalete.plata` | Brazalete de plata | 0.08x0.03x0.08 | 60 | pendiente | `uso:equipable` `material:plata` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.brazalete.oro` | Brazalete de oro | 0.08x0.03x0.08 | 60 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.collar.bronce` | Collar de bronce | 0.2x0.25x0.02 | 80 | pendiente | `uso:equipable` `material:bronce` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.collar.plata` | Collar de plata | 0.2x0.25x0.02 | 80 | pendiente | `uso:equipable` `material:plata` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.collar.oro` | Collar de oro | 0.2x0.25x0.02 | 80 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.aretes.bronce` | Aretes de bronce | 0.03x0.05x0.01 | 40 | pendiente | `uso:equipable` `material:bronce` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.aretes.plata` | Aretes de plata | 0.03x0.05x0.01 | 40 | pendiente | `uso:equipable` `material:plata` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.aretes.oro` | Aretes de oro | 0.03x0.05x0.01 | 40 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.hebilla.bronce` | Hebilla de cinturón de bronce | 0.08x0.06x0.015 | 60 | pendiente | `uso:equipable` `material:bronce` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.hebilla.plata` | Hebilla de cinturón de plata | 0.08x0.06x0.015 | 60 | pendiente | `uso:equipable` `material:plata` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.hebilla.oro` | Hebilla de cinturón de oro | 0.08x0.06x0.015 | 60 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |

<a id="asedio"></a>

## Maquinaria de asedio

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `asedio.maquina.ariete` | Ariete cubierto | 3x3x6 | 500 | pendiente | `faccion:imperio` `acto:1` `uso:conducible` `uso:destructible` | La maquinaria que aplasta al padre en el prólogo. |
| `asedio.maquina.torre_asedio` | Torre de asedio | 5x12x5 | 1000 | pendiente | `faccion:imperio` `acto:1` `uso:conducible` `uso:destructible` | La maquinaria que aplasta al padre en el prólogo. |
| `asedio.maquina.trebuchet` | Trebuchet | 5x10x8 | 900 | pendiente | `faccion:imperio` `acto:1` `uso:conducible` `uso:destructible` | La maquinaria que aplasta al padre en el prólogo. |
| `asedio.maquina.catapulta` | Catapulta | 3x3x4 | 600 | pendiente | `faccion:imperio` `acto:1` `uso:conducible` `uso:destructible` | La maquinaria que aplasta al padre en el prólogo. |
| `asedio.maquina.mantelete` | Mantelete | 2.5x2x0.3 | 100 | pendiente | `faccion:imperio` `acto:1` `uso:conducible` `uso:destructible` | La maquinaria que aplasta al padre en el prólogo. |

<a id="utileria"></a>

## Utilería y consumibles

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `utileria.objeto.caldero` | Caldero | 0.6x0.5x0.6 | 80 | pendiente | `faccion:nomada` `material:hierro` |  |
| `utileria.objeto.odre` | Odre de cuero | 0.3x0.4x0.2 | 60 | pendiente | `uso:recolectable` `material:cuero` |  |
| `utileria.objeto.cofre` | Cofre de botín | 0.8x0.5x0.5 | 100 | pendiente | `uso:interactuable` `material:madera` |  |
| `utileria.objeto.barril_polvora` | Barril de pólvora | 0.6x0.8x0.6 | 80 | pendiente | `faccion:imperio` `uso:destructible` |  |
| `utileria.objeto.saco_grano` | Saco de grano | 0.5x0.6x0.4 | 60 | pendiente | `uso:recolectable` |  |
| `utileria.objeto.antorcha` | Antorcha | 0.1x0.7x0.1 | 40 | pendiente | `uso:equipable` `manos:una` |  |
| `utileria.objeto.cadenas` | Grilletes y cadenas | 0.4x0.1x0.4 | 80 | pendiente | `material:hierro` |  |
| `utileria.objeto.yunque` | Yunque | 0.6x0.5x0.3 | 80 | pendiente | `material:hierro` |  |
| `utileria.objeto.telar` | Telar de fieltro | 1.5x1.2x1 | 150 | pendiente | `faccion:nomada` `material:madera` |  |
| `utileria.objeto.tamboril_chaman` | Tambor chamánico | 0.5x0.1x0.5 | 100 | pendiente | `faccion:nomada` `material:cuero` |  |
| `utileria.consumible.carne_seca` | Carne seca | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` |  |
| `utileria.consumible.airag` | Odre de airag (leche de yegua) | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` |  |
| `utileria.consumible.queso_seco` | Queso seco | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` |  |
| `utileria.consumible.hierbas` | Hierbas curativas | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` |  |
| `utileria.consumible.agua` | Agua cruda | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` |  |
| `utileria.consumible.pan` | Pan del imperio | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` |  |
| `utileria.consumible.carne_fresca` | Carne fresca | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` | De la caza o el sacrificio; se come antes que la seca. |
| `utileria.consumible.leche` | Leche | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` | Del ordeño diario del ganado. |
| `utileria.consumible.miel` | Miel silvestre | 0.15x0.15x0.15 | 40 | pendiente | `uso:recolectable` `uso:consumible` | De las colmenas, con humo. |
| `utileria.consumible.unguento` | Ungüento de hierbas y miel | 0.1x0.1x0.1 | 30 | pendiente | `uso:recolectable` `uso:consumible` | Cura más que las hierbas: corta el veneno y cierra las heridas. |
| `utileria.herramienta.pala` | Pala | 0.25x1.2x0.05 | 60 | pendiente | `uso:equipable` `manos:dos` `material:hierro` | Para cavar trincheras. |
| `utileria.herramienta.gancho_trepa` | Trepa (gancho con cuerda) | 0.3x0.4x0.3 | 80 | pendiente | `uso:equipable` `manos:una` `material:hierro` | Se lanza para escalar muros en los asedios. |
| `utileria.objeto.lena` | Haz de leña | 0.5x0.3x0.8 | 40 | pendiente | `uso:recolectable` `material:madera` | Material de fogatas, hogueras y hornos. |
| `utileria.material.troncos` | Troncos | 0.4x0.4x2 | 40 | pendiente | `uso:recolectable` `uso:material` `material:madera` | Material del acopio de la tribu. |
| `utileria.material.piedra` | Piedras | 0.5x0.4x0.5 | 30 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Material del acopio de la tribu. |
| `utileria.material.barro` | Barro (adobe) | 0.4x0.3x0.4 | 30 | pendiente | `uso:recolectable` `uso:material` `material:barro` | Material del acopio de la tribu. |
| `utileria.material.pieles` | Pieles curtidas | 0.6x0.2x0.8 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Material del acopio de la tribu. |
| `utileria.material.cuerda` | Rollo de cuerda | 0.3x0.15x0.3 | 40 | pendiente | `uso:recolectable` `uso:material` | Material del acopio de la tribu. |
| `utileria.material.carbon` | Carbón vegetal | 0.4x0.3x0.4 | 30 | pendiente | `uso:recolectable` `uso:material` | Material del acopio de la tribu. |
| `utileria.material.cobre` | Mineral de cobre | 0.25x0.2x0.25 | 30 | pendiente | `uso:recolectable` `uso:material` `material:cobre` | Material del acopio de la tribu. |
| `utileria.material.estano` | Mineral de estaño | 0.25x0.2x0.25 | 30 | pendiente | `uso:recolectable` `uso:material` `material:estaño` | Material del acopio de la tribu. |
| `utileria.material.hierro` | Mineral de hierro | 0.25x0.2x0.25 | 30 | pendiente | `uso:recolectable` `uso:material` `material:hierro` | Material del acopio de la tribu. |
| `utileria.material.plumas` | Plumas | 0.2x0.05x0.3 | 20 | pendiente | `uso:recolectable` `uso:material` | De las aves cazadas: para emplumar flechas. |
| `utileria.material.hueso` | Huesos | 0.3x0.1x0.3 | 30 | pendiente | `uso:recolectable` `uso:material` `material:hueso` | Del despiece: puntas de virote. |
| `utileria.material.tendones` | Tendones | 0.2x0.05x0.3 | 20 | pendiente | `uso:recolectable` `uso:material` | Del despiece: cuerda. |
| `utileria.material.grasa` | Grasa animal | 0.2x0.1x0.2 | 20 | pendiente | `uso:recolectable` `uso:material` | De la caza (más en otoño): sebo para lámparas, ungüentos y para engrasar el cuero. |
| `utileria.material.pedernal` | Pedernal | 0.15x0.1x0.15 | 30 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Se recoge en la estepa: puntas de flecha. |
| `utileria.gema.turquesa` | Turquesa | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.gema.cornalina` | Cornalina | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.gema.lapislazuli` | Lapislázuli | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.gema.ambar` | Ámbar | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.gema.jade` | Jade | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.gema.granate` | Granate | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.gema.perla` | Perla | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.gema.coral` | Coral rojo | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.material.bronce` | Lingote de bronce | 0.2x0.05x0.08 | 20 | pendiente | `uso:material` `material:bronce` | Metal de joyas y forja. |
| `utileria.material.plata` | Plata | 0.15x0.05x0.08 | 20 | pendiente | `uso:recolectable` `uso:material` `material:plata` | Metal de joyas. |
| `utileria.material.oro` | Oro | 0.12x0.04x0.07 | 20 | pendiente | `uso:recolectable` `uso:material` `material:oro` | Metal de joyas. |
| `utileria.consumible.forraje` | Forraje | 0.4x0.3x0.4 | 30 | pendiente | `uso:recolectable` `uso:consumible` `material:fibra` | Hierba seca para los herbívoros de la tribu cuando no hay pasto (se corta de la hierba alta; los pastores lo juntan). |
| `utileria.mochila.pequena` | Mochila pequeña | 0.35x0.4x0.2 | 80 | pendiente | `uso:equipable` `material:cuero` | 15 kg, 12 huecos; no frena. |
| `utileria.mochila.mediana` | Mochila mediana | 0.4x0.55x0.25 | 100 | pendiente | `uso:equipable` `material:cuero` | 25 kg, 18 huecos; frena un poco. |
| `utileria.mochila.grande` | Mochila grande | 0.5x0.75x0.3 | 120 | pendiente | `uso:equipable` `material:cuero` | 40 kg, 28 huecos; frena (se puede dejar en el suelo para pelear). |
| `utileria.piel.lobo` | Piel de lobo | 0.6x0.1x1.0 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Del despiece. Capas y gorros que escarmientan. |
| `utileria.piel.oso` | Piel de oso | 0.9x0.15x1.4 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Del despiece. |
| `utileria.piel.tigre` | Piel de tigre | 0.8x0.1x1.3 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Del despiece. |
| `utileria.piel.puma` | Piel de puma | 0.7x0.1x1.1 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Del despiece. |
| `utileria.piel.hiena` | Piel de hiena | 0.6x0.1x1.0 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Del despiece. |
| `utileria.piel.coyote` | Piel de coyote | 0.5x0.1x0.8 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Del despiece. |
| `utileria.piel.reno` | Piel de reno | 0.8x0.15x1.2 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Del despiece: abrigos y botas. |
| `utileria.piel.cabra` | Lana de cabra | 0.5x0.2x0.5 | 40 | pendiente | `uso:recolectable` `uso:material` `material:lana` | De las cabras (despiece y esquila de los pastores): fieltro, lana. |
| `utileria.material.seda` | Seda | 0.4x0.1x0.3 | 30 | pendiente | `uso:recolectable` `uso:material` `material:seda` | De las caravanas del imperio (botín). |
| `utileria.consumible.agua_hervida` | Agua hervida | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` | Hervir el agua (Tab, junto a un fuego) echa a los espíritus malditos. |
| `utileria.consumible.agua_vino` | Agua con vino | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` | El vino purifica el agua: segura y apenas achispa. |
| `utileria.consumible.cerveza` | Cerveza | 0.2x0.25x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` | Segura de beber; en exceso, torpe y fatigado. |
| `utileria.consumible.vino` | Vino | 0.12x0.3x0.12 | 40 | pendiente | `uso:recolectable` `uso:consumible` | De las caravanas del imperio. Seguro; emborracha pronto. |

<a id="personaje"></a>

## Personajes y NPCs

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `personaje.base.cuerpo_nino` | Cuerpo base: niño | 0.35x1.2x0.25 | 800 | pendiente | `rig:humanoide` `talla:pequena` | Malla base modular: ropa y armadura encajan encima. |
| `personaje.base.cuerpo_comun` | Cuerpo base: adulto común | 0.5x1.7x0.3 | 1200 | pendiente | `rig:humanoide` `talla:comun` |  |
| `personaje.base.cuerpo_robusto` | Cuerpo base: adulto robusto | 0.6x1.85x0.35 | 1300 | pendiente | `rig:humanoide` `talla:grande` |  |
| `personaje.base.cuerpo_gigante` | Cuerpo base: gigante (gran guerrero) | 0.75x2.3x0.45 | 1500 | pendiente | `rig:humanoide` `talla:gigante` `especial:gran_guerrero` | Talla x1.2–1.45 del común. |
| `personaje.base.cuerpo_anciano` | Cuerpo base: anciano | 0.5x1.6x0.3 | 1200 | pendiente | `rig:humanoide` `talla:comun` |  |
| `personaje.narrativo.protagonista` | Protagonista ({NOMBRE}) | 0.5x1.75x0.3 | 2000 | pendiente | `rig:humanoide` `acto:2` `especial:narrativo` `uso:personalizable` | Personalizable: nombre, aspecto, ropa. |
| `personaje.narrativo.padre` | El padre (jefe de la confederación) | 0.5x1.75x0.3 | 2000 | pendiente | `rig:humanoide` `acto:1` `especial:narrativo` `faccion:nomada` | Jugable en el prólogo, a caballo. |
| `personaje.narrativo.madre` | La madre | 0.5x1.75x0.3 | 2000 | pendiente | `rig:humanoide` `acto:1` `especial:narrativo` |  |
| `personaje.narrativo.medio_hermano` | El medio-hermano | 0.5x1.75x0.3 | 2000 | pendiente | `rig:humanoide` `acto:2` `especial:narrativo` `faccion:culto` | Antagonista. |
| `personaje.narrativo.rey` | El rey (anciano) | 0.5x1.75x0.3 | 2000 | pendiente | `rig:humanoide` `acto:2` `especial:narrativo` `faccion:imperio` |  |
| `personaje.npc.pastor` | Pastor | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:nomada` `motivo:civil` |  |
| `personaje.npc.jinete_arquero` | Jinete arquero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:nomada` `motivo:combate` |  |
| `personaje.npc.guerrero_nomada` | Guerrero nómada | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:nomada` `motivo:combate` |  |
| `personaje.npc.chaman` | Chamán | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:nomada` `motivo:espiritual` |  |
| `personaje.npc.curandero` | Curandero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:nomada` `motivo:servicio` |  |
| `personaje.npc.herrero` | Herrero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:nomada` `motivo:servicio` |  |
| `personaje.npc.explorador` | Explorador | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:nomada` `motivo:servicio` |  |
| `personaje.npc.campesino` | Campesino | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `bioma:ciudad` `motivo:civil` |  |
| `personaje.npc.obrero` | Obrero de la fundición | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `bioma:ciudad` `motivo:civil` |  |
| `personaje.npc.burocrata` | Burócrata | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `bioma:ciudad` `motivo:civil` |  |
| `personaje.npc.lancero` | Soldado lancero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `motivo:combate` |  |
| `personaje.npc.ballestero` | Soldado ballestero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `motivo:combate` |  |
| `personaje.npc.mosquetero` | Mosquetero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `motivo:combate` |  |
| `personaje.npc.artillero` | Artillero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `motivo:combate` |  |
| `personaje.npc.oficial` | Oficial imperial | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `motivo:combate` |  |
| `personaje.npc.verdugo` | Verdugo | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `motivo:castigo` |  |
| `personaje.npc.sacerdote_culto` | Sacerdote del culto | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:culto` `motivo:espiritual` |  |
| `personaje.npc.fanatico` | Fanático del culto | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:culto` `motivo:combate` |  |
| `personaje.npc.captor_culto` | Captor del culto | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:culto` `motivo:captura` `acto:3` |  |
| `personaje.npc.prisionero` | Prisionero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:cualquiera` `motivo:rescate` |  |
| `personaje.npc.refugiado` | Refugiado | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:neutral` `motivo:rescate` |  |
| `personaje.npc.comerciante` | Comerciante de caravana | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:neutral` `bioma:desierto` `motivo:comercio` |  |
| `personaje.npc.nomada_desierto` | Nómada del desierto | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:neutral` `bioma:desierto` `motivo:civil` |  |
| `personaje.npc.cazador_bosque` | Cazador del bosque | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:neutral` `bioma:bosque` `motivo:civil` |  |
| `personaje.npc.pescador_fiordo` | Pescador de los fiordos | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:neutral` `bioma:fiordo` `motivo:civil` |  |
| `personaje.npc.bandido` | Bandido | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:salvaje` `motivo:combate` |  |
| `personaje.npc.mercenario` | Mercenario | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:neutral` `motivo:combate` |  |
| `personaje.npc.nino` | Niño | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:cualquiera` `talla:pequena` `motivo:civil` |  |
| `personaje.npc.anciano` | Anciano | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:cualquiera` `motivo:civil` |  |

