// Yurta mongola (ger) low-poly — prueba de pipeline Kiln -> GLB -> Godot.
//
// Marco de Kiln: +X adelante, +Y arriba, +Z derecha; todo apoya en Y=0.
// 10 segmentos radiales: con 10 lados, la cara plana (no una arista) queda
// mirando a +X, así la puerta asienta sobre un panel del muro.

const meta = { name: 'MongolYurt', category: 'prop' };

function build() {
  const root = createRoot('MongolYurt');

  // Materiales mate (metalness 0) para que el render corra por la vía CPU.
  const felt = gameMaterial(0xe9e4d6, { roughness: 0.95 });
  const feltRoof = gameMaterial(0xd8d0bd, { roughness: 0.95 });
  const rope = gameMaterial(0x5a4632, { roughness: 0.9 });
  const wood = gameMaterial(0x8a5a2b, { roughness: 0.85 });
  const doorPaint = gameMaterial(0xd2601a, { roughness: 0.7 });
  const doorFrame = gameMaterial(0x7a2e1a, { roughness: 0.8 });
  const soot = gameMaterial(0x3b3733, { roughness: 0.9 });

  const SEG = 10;
  const R = 2.2; // radio del muro
  const WALL_H = 1.6;
  const ROOF_H = 0.9;
  const APOTHEM = R * Math.cos(Math.PI / SEG); // distancia al centro de una cara plana

  // Plataforma baja de madera.
  createPart('Platform', cylinderGeo(R + 0.25, R + 0.25, 0.08, SEG), wood, {
    position: [0, 0.04, 0],
    parent: root,
  });

  // Muro de fieltro sobre el entramado (khana).
  createPart('Wall', cylinderGeo(R, R, WALL_H, SEG), felt, {
    position: [0, 0.08 + WALL_H / 2, 0],
    parent: root,
  });

  // Dos cuerdas tensoras que ciñen el fieltro.
  for (const [i, y] of [0.6, 1.3].entries()) {
    createPart(`RopeBand${i}`, cylinderGeo(R + 0.03, R + 0.03, 0.06, SEG), rope, {
      position: [0, 0.08 + y, 0],
      parent: root,
    });
  }

  // Techo cónico truncado (uni + fieltro); deja abierta la corona.
  const roofBase = 0.08 + WALL_H;
  createPart('Roof', cylinderGeo(0.45, R + 0.15, ROOF_H, SEG), feltRoof, {
    position: [0, roofBase + ROOF_H / 2, 0],
    parent: root,
  });

  // Corona de madera (toono) y tubo de la estufa.
  createPart('Crown', cylinderGeo(0.5, 0.5, 0.12, SEG), wood, {
    position: [0, roofBase + ROOF_H + 0.06, 0],
    parent: root,
  });
  createPart('StovePipe', cylinderGeo(0.07, 0.07, 0.7, 8), soot, {
    position: [0.15, roofBase + ROOF_H + 0.35, 0],
    parent: root,
  });

  // Puerta pintada de naranja con marco, asentada sobre la cara +X.
  createPart('DoorFrame', boxGeo(0.06, 1.38, 0.98), doorFrame, {
    position: [APOTHEM + 0.03, 0.08 + 0.69, 0],
    parent: root,
  });
  createPart('Door', boxGeo(0.06, 1.25, 0.8), doorPaint, {
    position: [APOTHEM + 0.07, 0.08 + 0.625, 0],
    parent: root,
  });

  return root;
}
