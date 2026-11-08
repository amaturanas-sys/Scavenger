# Tamaños de los animales

Regla del juego: **cada animal tiene el tamaño estándar de los ejemplares más grandes conocidos de su especie**.
Es el extremo alto del rango adulto típico, casi siempre de los machos de la población o la raza más grande. No es
el récord de un ejemplar suelto. La naturaleza salvaje de la estepa es feroz y grandiosa, pero real.

Dos medidas salen de aquí:

- **La altura a la cruz** escala los modelos (`tools/modelos/piezas.py --cruz M`). La cruz se busca en la vista de
  lado de la hoja, sobre la parte de atrás de las patas de adelante. El resto de las medidas del modelo sale de
  la imagen, y el inventario (`assets/inventario.tsv`) guarda la caja que resulta.
- **El largo de cabeza y cuerpo**, sin la cola, es la talla de la especie en la simulación (`size` en
  `src/sim/animals.c`). En los reptiles es el largo total, en las aves el del cuerpo, en la tortuga el
  caparazón y en la araña las patas extendidas. Con la talla nueva se recalculó el `hunt_max` de cada depredador
  para que siga cazando las mismas presas. El único cambio es que el coyote ya no caza la tortuga marina adulta,
  que con la laúd mide 1.8 m.

Las cifras salen de una búsqueda en la web: Wikipedia citada en buscadores, el Handbook of the Mammals of the
World, Ultimate Ungulate, Animal Diversity Web, San Diego Zoo, Guinness y registros de razas. Las marcadas "est."
son estimaciones por proporciones, sin fuente directa. Conviene revisar cualquier fila de la que dependa algo
importante.

## Los que tienen modelo

| Animal | A la cruz (modelo) | Cabeza y cuerpo (simulación) | Población o raza | Notas |
|---|---|---|---|---|
| Lobo | 0.85 m | 1.50 m | lobo del noroeste (Mackenzie) | 60–65 kg |
| Oso pardo | 1.50 m | 2.65 m | Kodiak | 600–680 kg en otoño; unos 3 m de pie |
| Jabalí | 1.10 m | 1.85 m (est.) | Cárpatos y noreste de Rusia | 220–270 kg |
| Ciervo | 1.45 m | 2.40 m | ciervo del Caspio (C. e. maral) | 250–300 kg; sin el wapití, que es otra especie |
| Puma | 0.80 m | 1.50 m | machos grandes de Canadá y Patagonia | 70–80 kg |
| Tigre | 1.10 m | 2.05 m | tigre de Bengala del Terai | 240–260 kg; 3.0–3.1 m con la cola |
| Caballo estepario | 1.58 m | 2.30 m (est.) | Akhal-Teke (el kazajo, 1.45–1.50 m) | 450–500 kg |
| Caballo de guerra | 1.75 m | — | Shire y Percherón (el destrero, 1.60–1.65 m) | 900–1100 kg; la simulación no lo tiene aparte (usa la del caballo) |
| Dromedario | 2.00 m | — | machos grandes; la joroba, 2.2 m o más | 600 kg; la simulación no lo tiene (tiene el bactriano) |
| Buey | 1.88 m | 3.00 m (est.) | bueyes de Chianina | 1300–1600 kg |
| Coyote | 0.62 m | 1.07 m | coyote del noreste | 23–25 kg |
| Perro pastor (mastín) | 0.78 m (est.) | — | pastor de Asia central (estándar FCI: los machos, desde 0.70 m) | 50 kg o más |
| Perro asilvestrado | 0.65 m (est.) | 1.00 m (est.) | perros asilvestrados grandes, de tipo guardián | 30–40 kg |

Con id en el inventario, todavía sin especie en la simulación:

| Animal (id) | A la cruz | Población o raza |
|---|---|---|
| Oso polar (`animal.salvaje.oso_polar`) | 1.60 m | machos adultos (2.7 m de cabeza y cuerpo, 600–700 kg) |
| Cierva (`animal.salvaje.cierva`) | 1.20 m (est.) | hembra del ciervo del Caspio, escalada del macho |
| Vaca (`animal.ganado.vaca`) | 1.62 m | vacas de Chianina (900–1000 kg) |
| Caballo estepario, variantes 2 y 3 | 1.58 m | como el caballo estepario |

Las personas no siguen esta regla: el cuerpo común mide 1.70 m, el robusto 1.85 m y la mujer estándar 1.60 m.

## La simulación (`size`, cabeza y cuerpo)

| Especie | Antes | Ahora | Base |
|---|---|---|---|
| Caballo estepario | 2.2 | 2.3 | Akhal-Teke (est.) |
| Mula | 2.0 | 2.4 | mulas de tiro, 1.65–1.70 m a la cruz (est.) |
| Burro | 1.6 | 2.0 | mamut americano y Poitou, 1.50–1.55 m a la cruz (est.) |
| Buey | 2.5 | 3.0 | Chianina (est.) |
| Camello bactriano | 3.0 | 3.3 | machos grandes (3.0–3.5 m) |
| Elefante | 5.0 | 6.0 | elefante asiático macho, 3.0–3.2 m a la cruz |
| Lobo | 1.4 | 1.5 | lobo del noroeste |
| Perro asilvestrado | 1.1 | 1.0 | perros grandes de tipo guardián (est.) |
| Tigre | 2.6 | 2.05 | Bengala; antes contaba parte de la cola |
| Puma | 1.9 | 1.5 | machos grandes; ídem |
| Halcón | 0.5 | 0.56 | sacre, hembras grandes (1.20–1.26 m de envergadura) |
| Cuervo | 0.5 | 0.7 | cuervos del Himalaya y Groenlandia |
| Oso pardo | 2.2 | 2.65 | Kodiak |
| Hiena | 1.3 | 1.55 | hiena manchada de Zambia |
| Coyote | 1.1 | 1.07 | coyote del noreste |
| Jabalí | 1.4 | 1.85 | Cárpatos y Rusia (est.) |
| Antílope saiga | 1.6 | 1.43 | machos adultos |
| Reno salvaje | 2.0 | 2.1 | machos grandes de bosque |
| Gacela | 1.2 | 1.12 | gacela persa, machos |
| Ciervo | 1.8 | 2.4 | ciervo del Caspio |
| Liebre | 0.5 | 0.7 | liebre europea |
| Íbice | 1.4 | 1.62 | íbice siberiano, machos |
| Cabra | 1.1 | 1.3 | machos bóer (est.) |
| Becerro | 1.4 | 1.8 | ternero de un año (est.) |
| Cocodrilo | 3.5 | 5.0 | marino y del Nilo, machos adultos (largo total) |
| Tortuga marina | 1.0 | 1.8 | laúd (caparazón) |
| Víbora | 1.0 | 1.5 | víbora levantina (largo total) |
| Escorpión | 0.2 | 0.19 | Heterometrus swammerdami |
| Araña | 0.15 | 0.28 | Goliat comepájaros (patas extendidas) |
| Águila real | 0.9 | 1.0 | hembras de Asia central (2.2–2.3 m de envergadura) |

Lo que depende de la talla en la simulación crece con ella: el cuerpo de cápsulas, lo que vadea (0.4 de la talla,
hasta 1.2 m), la masa (30 x talla³, para romper el hielo) y el alcance contra presas grandes. Los `hunt_max`
nuevos conservan las mismas presas de cada depredador; las pruebas del núcleo pasan sin cambios.
