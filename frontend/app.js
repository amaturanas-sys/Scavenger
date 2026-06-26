// SCAVENGER · frontend (vanilla JS)
// Base del backend: en la web servida por FastAPI es el mismo origen ("");
// en la APK (file://) el usuario configura la URL del servidor (localStorage).
function apiBase() {
  return (localStorage.getItem("scavenger_api_base") || window.SCAVENGER_API_BASE || "").replace(/\/+$/, "");
}
function setApiBase() {
  const cur = apiBase();
  const v = prompt("URL del servidor SCAVENGER\n(ej: http://192.168.1.10:8000)", cur);
  if (v === null) return;
  localStorage.setItem("scavenger_api_base", v.trim());
  location.reload();
}
let currentUserId = null;
let lastPlanPayload = null; // ultima minuta generada (para guardar)
let retailersCache = []; // cadenas disponibles

// ---------- helpers ----------
const $ = (sel) => document.querySelector(sel);
const $$ = (sel) => document.querySelectorAll(sel);
const clp = (n) => "$" + Math.round(n || 0).toLocaleString("es-CL");
const num = (n, d = 0) => (n ?? 0).toLocaleString("es-CL", { maximumFractionDigits: d });

async function api(path, opts = {}) {
  const res = await fetch(apiBase() + path, {
    headers: { "Content-Type": "application/json" },
    ...opts,
  });
  if (!res.ok) {
    const txt = await res.text();
    throw new Error(`${res.status}: ${txt}`);
  }
  return res.status === 204 ? null : res.json();
}

// ---------- tabs ----------
$$(".tab").forEach((t) =>
  t.addEventListener("click", () => {
    $$(".tab").forEach((x) => x.classList.remove("active"));
    $$(".panel").forEach((x) => x.classList.remove("active"));
    t.classList.add("active");
    $("#" + t.dataset.tab).classList.add("active");
    if (t.dataset.tab === "requerimientos") loadRequirements();
    if (t.dataset.tab === "armar") loadReels();
    if (t.dataset.tab === "minutas") loadPlans();
    if (t.dataset.tab === "alimentos") loadFoods();
  })
);

// ---------- usuarios ----------
async function loadUsers() {
  const users = await api("/api/users");
  const sel = $("#userSelect");
  sel.innerHTML = "";
  users.forEach((u) => {
    const o = document.createElement("option");
    o.value = u.id;
    o.textContent = `${u.name || "Usuario"} #${u.id}`;
    sel.appendChild(o);
  });
  if (users.length) {
    currentUserId = users[users.length - 1].id;
    sel.value = currentUserId;
    fillForm(users[users.length - 1]);
  }
}

function fillForm(u) {
  const f = $("#userForm");
  ["name", "sex", "age", "weight_kg", "height_cm", "activity_level", "goal", "daily_budget_clp"].forEach((k) => {
    if (f[k]) f[k].value = u[k];
  });
  f.diet_tags.value = (u.diet_tags && u.diet_tags[0]) || "";
  const pref = new Set(u.preferred_retailers || []);
  $$("#retailerChecks input").forEach((chk) => (chk.checked = pref.has(chk.value)));
  // Sincroniza el monto del selector de presupuesto con el del perfil.
  if (u.daily_budget_clp != null) $("#budgetAmount").value = u.daily_budget_clp;
}

// Habilita/deshabilita el monto según el modo de presupuesto.
function syncBudgetControls() {
  const none = $("#budgetMode").value === "none";
  $("#budgetAmount").disabled = none;
  $("#budgetAmount").style.opacity = none ? 0.5 : 1;
}

async function loadRetailers() {
  retailersCache = await api("/api/foods/retailers");
  $("#retailerChecks").innerHTML = retailersCache
    .map((r) => `<label><input type="checkbox" value="${r.retailer_id}" /> ${r.retailer}</label>`)
    .join("");
}

function selectedRetailers() {
  return [...$$("#retailerChecks input:checked")].map((c) => c.value);
}

$("#userSelect").addEventListener("change", async (e) => {
  currentUserId = parseInt(e.target.value);
  const u = await api(`/api/users/${currentUserId}`);
  fillForm(u);
});

$("#userForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  const f = e.target;
  const diet = f.diet_tags.value ? [f.diet_tags.value] : [];
  const body = {
    name: f.name.value,
    sex: f.sex.value,
    age: +f.age.value,
    weight_kg: +f.weight_kg.value,
    height_cm: +f.height_cm.value,
    activity_level: f.activity_level.value,
    goal: f.goal.value,
    daily_budget_clp: +f.daily_budget_clp.value,
    diet_tags: diet,
    excluded_foods: [],
    preferred_retailers: selectedRetailers(),
  };
  // Si hay usuario seleccionado, actualiza; si no, crea.
  let u;
  if (currentUserId) {
    u = await api(`/api/users/${currentUserId}`, { method: "PATCH", body: JSON.stringify(body) });
  } else {
    u = await api("/api/users", { method: "POST", body: JSON.stringify(body) });
  }
  currentUserId = u.id;
  $("#userStatus").textContent = "✓ Perfil guardado (#" + u.id + ")";
  await loadUsers();
  $("#userSelect").value = currentUserId;
});

// Boton "nuevo usuario" implicito: limpiar seleccion
$("#userForm").addEventListener("reset", () => (currentUserId = null));

// ---------- requerimientos ----------
async function loadRequirements() {
  if (!currentUserId) return;
  const r = await api(`/api/users/${currentUserId}/requirements`);
  const cards = [
    ["Calorías", num(r.kcal) + " kcal", "Objetivo diario"],
    ["Proteína", num(r.protein_g) + " g", "Meta"],
    ["Carbohidratos", num(r.carb_g) + " g", "Meta"],
    ["Grasa", num(r.fat_g) + " g", "Meta"],
    ["Fibra", num(r.fiber_g) + " g", "Mínimo"],
    ["TMB", num(r.bmr) + " kcal", "Metabolismo basal"],
    ["GET", num(r.tdee) + " kcal", "Gasto total"],
    ["Hierro", num(r.micros.iron_mg, 1) + " mg", "Mínimo"],
    ["Calcio", num(r.micros.calcium_mg) + " mg", "Mínimo"],
    ["Vit. C", num(r.micros.vitamin_c_mg) + " mg", "Mínimo"],
  ];
  $("#reqCards").innerHTML = cards
    .map((c) => `<div class="card"><div class="lbl">${c[0]}</div><div class="big">${c[1]}</div><div class="muted">${c[2]}</div></div>`)
    .join("");
}

// ---------- generar minuta ----------
$("#satiety").addEventListener("input", (e) => ($("#satVal").textContent = (+e.target.value).toFixed(1)));
$("#budgetMode").addEventListener("change", syncBudgetControls);
syncBudgetControls();

$("#generateBtn").addEventListener("click", async () => {
  if (!currentUserId) return alert("Primero guarda un perfil.");
  $("#genStatus").textContent = "Optimizando combinaciones más económicas...";
  $("#planResult").innerHTML = "";
  $("#shoppingResult").innerHTML = "";
  try {
    const mode = $("#budgetMode").value;
    const body = {
      user_id: currentUserId,
      scope: $("#scope").value,
      satiety_emphasis: +$("#satiety").value,
      budget_mode: mode,
      budget_clp: mode === "none" ? null : +$("#budgetAmount").value,
    };
    const res = await api("/api/plans/generate", { method: "POST", body: JSON.stringify(body) });
    $("#genStatus").textContent = "";
    if (res.scope === "semanal") renderWeekly(res.data);
    else renderDaily(res.data);
  } catch (err) {
    $("#genStatus").textContent = "Error: " + err.message;
  }
});

function totalsBar(t, req, budget, overBudget) {
  const macroPill = (label, val, target, unit) => {
    const pct = target ? Math.round((val / target) * 100) : 0;
    return `<div class="pill">${label}: <strong>${num(val)}${unit}</strong> <span class="muted">/ ${num(target)}${unit} (${pct}%)</span></div>`;
  };
  let pills = `<div class="pill ${overBudget ? "danger" : ""}">Costo: <strong>${clp(t.cost_clp)}</strong>${budget ? ` <span class="muted">/ ${clp(budget)}</span>` : ""}</div>`;
  pills += macroPill("Energía", t.kcal, req?.kcal, " kcal");
  pills += macroPill("Proteína", t.protein_g, req?.protein_g, " g");
  pills += macroPill("Carbs", t.carb_g, req?.carb_g, " g");
  pills += macroPill("Grasa", t.fat_g, req?.fat_g, " g");
  pills += `<div class="pill">Saciedad: <strong>${num(t.satiety)}</strong></div>`;
  return `<div class="totbar">${pills}</div>`;
}

function mealTable(meal) {
  const rows = meal.items
    .map(
      (i) => `<tr>
      <td>${i.name} <span class="muted">${i.brand || ""}</span></td>
      <td><span class="shop">${i.retailer || "—"}</span></td>
      <td class="num">${num(i.grams)} g</td>
      <td class="num">${num(i.kcal)}</td>
      <td class="num">${num(i.protein_g, 1)}</td>
      <td class="num">${clp(i.cost_clp)}</td>
    </tr>`
    )
    .join("");
  return `<div class="meal">
    <h3>${meal.meal} <span>${num(meal.subtotal.kcal)} kcal · ${clp(meal.subtotal.cost_clp)}</span></h3>
    <table><thead><tr><th>Alimento</th><th>Comprar en</th><th class="num">Cantidad</th><th class="num">kcal</th><th class="num">Prot (g)</th><th class="num">Costo</th></tr></thead>
    <tbody>${rows || '<tr><td colspan="6" class="muted">Sin items</td></tr>'}</tbody></table>
  </div>`;
}

function renderDaily(data) {
  lastPlanPayload = data;
  let html = "";
  if (data.warnings && data.warnings.length)
    html += `<div class="warnbox">⚠ ${data.warnings.join(" ")}</div>`;
  const showBudget = data.budget_mode && data.budget_mode !== "none";
  html += totalsBar(data.totals, data.requirements, showBudget ? data.budget_clp : null, data.over_budget);
  if (data.budget_mode === "target")
    html += `<p class="muted">Modo «aprovechar el presupuesto»: maximiza saciedad sin pasar de ${clp(data.budget_clp)}.</p>`;
  html += data.meals.map(mealTable).join("");
  html += `<button id="savePlanBtn" class="primary">Guardar esta minuta</button>
    <button id="shopBtn" class="btn-sm">🛒 Lista de compras consolidada</button>`;
  $("#planResult").innerHTML = html;
  $("#savePlanBtn").addEventListener("click", () => savePlan("diario", data));
  $("#shopBtn").addEventListener("click", () => showShoppingForPayload(data));
}

function renderWeekly(data) {
  lastPlanPayload = data;
  let html = totalsBar(
    { cost_clp: data.avg_daily_cost_clp, kcal: data.requirements.kcal, protein_g: data.requirements.protein_g, carb_g: data.requirements.carb_g, fat_g: data.requirements.fat_g, satiety: 0 },
    data.requirements
  );
  html = `<div class="pill">Costo semanal: <strong>${clp(data.weekly_cost_clp)}</strong></div><div class="pill">Promedio diario: <strong>${clp(data.avg_daily_cost_clp)}</strong></div>` + html;
  html = `<div class="totbar">${html}</div>`;
  data.days.forEach((d) => {
    const t = d.plan.totals;
    html += `<div class="week-day"><h3 style="text-transform:capitalize">${d.day} · ${num(t.kcal)} kcal · ${clp(t.cost_clp)}</h3>`;
    html += d.plan.meals
      .map((m) => `<div class="muted">• <strong style="text-transform:capitalize">${m.meal}</strong>: ${m.items.map((i) => i.name).join(", ") || "—"}</div>`)
      .join("");
    html += `</div>`;
  });
  html += `<button id="savePlanBtn" class="primary">Guardar esta minuta semanal</button>
    <button id="shopBtn" class="btn-sm">🛒 Lista de compras consolidada</button>`;
  $("#planResult").innerHTML = html;
  $("#savePlanBtn").addEventListener("click", () => savePlan("semanal", data));
  $("#shopBtn").addEventListener("click", () => showShoppingForPayload(data));
}

// ---------- lista de compras ----------
function shoppingHtml(d) {
  if (!d.retailers || !d.retailers.length) return '<p class="muted">Sin productos para listar.</p>';
  let html = `<div class="totbar">
    <div class="pill">Total comprando envases: <strong>${clp(d.total_packages_clp)}</strong></div>
    <div class="pill">Consumo neto: <strong>${clp(d.total_consumed_clp)}</strong></div>
    <div class="pill">${d.retailer_count} cadena(s)</div>
  </div>`;
  for (const r of d.retailers) {
    const rows = r.items
      .map((i) => `<tr>
        <td>${i.name} <span class="muted">${i.brand || ""}</span></td>
        <td class="num">${num(i.needed_g)} g</td>
        <td class="num">${i.packages != null ? `${i.packages} × ${num(i.package_g)} g` : "—"}</td>
        <td class="num">${i.packages_cost_clp != null ? clp(i.packages_cost_clp) : "—"}</td>
      </tr>`)
      .join("");
    html += `<div class="meal">
      <h3>🛒 ${r.retailer} <span>${r.item_count} productos · ${clp(r.subtotal_packages_clp)}</span></h3>
      <table><thead><tr><th>Producto</th><th class="num">Necesario</th><th class="num">Comprar</th><th class="num">Costo</th></tr></thead>
      <tbody>${rows}</tbody></table>
    </div>`;
  }
  return html;
}

async function showShoppingForPayload(payload) {
  $("#shoppingResult").innerHTML = '<p class="muted">Consolidando lista de compras…</p>';
  const data = await api("/api/plans/shopping-list", { method: "POST", body: JSON.stringify({ payload }) });
  $("#shoppingResult").innerHTML = `<h3 style="margin-top:18px">Lista de compras por cadena</h3>` + shoppingHtml(data);
  $("#shoppingResult").scrollIntoView({ behavior: "smooth", block: "start" });
}

async function showShoppingForSaved(planId, containerSel) {
  const el = $(containerSel);
  el.innerHTML = '<p class="muted">Consolidando…</p>';
  const data = await api(`/api/plans/${planId}/shopping-list`);
  el.innerHTML = shoppingHtml(data);
}

async function savePlan(scope, payload) {
  const title = prompt("Nombre de la minuta:", scope === "semanal" ? "Semana" : "Día " + new Date().toLocaleDateString("es-CL"));
  if (title === null) return;
  // Para semanal guardamos totales agregados aproximados.
  const toSave = scope === "semanal" ? { ...payload, totals: { cost_clp: payload.weekly_cost_clp, kcal: payload.requirements.kcal, satiety: 0 } } : payload;
  await api("/api/plans", { method: "POST", body: JSON.stringify({ user_id: currentUserId, title, scope, payload: toSave }) });
  $("#genStatus").textContent = "✓ Minuta guardada.";
}

// ---------- minutas guardadas ----------
async function loadSatietyHistory() {
  if (!currentUserId) return;
  const h = await api(`/api/users/${currentUserId}/satiety-history`);
  if (!h.count) {
    $("#satietyHistory").innerHTML =
      '<p class="muted">Aún no hay historial de saciedad. Registra la saciedad de tus minutas para ir afinando las sugerencias.</p>';
    return;
  }
  const bars = h.entries
    .map((e) => {
      const pct = (e.satiety_score / 5) * 100;
      const date = e.created_at ? new Date(e.created_at).toLocaleDateString("es-CL", { day: "2-digit", month: "2-digit" }) : "";
      return `<div class="histbar" title="${e.title}: saciedad ${e.satiety_score}/5 · costo ${clp(e.total_cost_clp)}">
        <div class="histfill" style="height:${pct}%"></div>
        <div class="histlbl">${e.satiety_score}</div>
        <div class="histdate">${date}</div>
      </div>`;
    })
    .join("");
  $("#satietyHistory").innerHTML = `<div class="card" style="margin-bottom:16px">
    <div class="lbl">Historial de saciedad</div>
    <div class="totbar" style="margin:8px 0">
      <div class="pill">Promedio saciedad: <strong>${h.avg_satiety}/5</strong></div>
      <div class="pill">Satisfacción de costo: <strong>${h.avg_cost_score}/5</strong></div>
      <div class="pill">${h.count} registro(s)</div>
    </div>
    <div class="histchart">${bars}</div>
  </div>`;
}

async function loadPlans() {
  if (!currentUserId) return;
  loadSatietyHistory();
  const plans = await api(`/api/plans?user_id=${currentUserId}`);
  if (!plans.length) {
    $("#plansList").innerHTML = '<p class="muted">Aún no tienes minutas guardadas.</p>';
    return;
  }
  $("#plansList").innerHTML = plans.map(planCard).join("");
  plans.forEach((p) => wirePlanCard(p));
}

function planCard(p) {
  return `<div class="plan-card" id="plan-${p.id}">
    <div class="row">
      <div>
        <strong>${p.title}</strong> <span class="muted">· ${p.scope} · ${clp(p.total_cost_clp)} · ${num(p.total_kcal)} kcal</span>
        ${p.satiety_score ? `<span class="muted"> · saciedad reportada: ${p.satiety_score}/5</span>` : ""}
      </div>
      <div>
        <button class="btn-sm" data-act="shop" data-id="${p.id}">🛒 Lista de compras</button>
        <button class="btn-sm" data-act="fb" data-id="${p.id}">Registrar saciedad</button>
        <button class="btn-sm" data-act="del" data-id="${p.id}">Eliminar</button>
      </div>
    </div>
    <div id="shop-${p.id}"></div>
    <div class="feedback-box" id="fb-${p.id}">
      <p class="muted">¿Qué tan saciado quedaste? (1 = mucha hambre, 5 = muy lleno)</p>
      <div class="stars" data-id="${p.id}">${[1, 2, 3, 4, 5].map((n) => `<span data-v="${n}">☆</span>`).join("")}</div>
      <label style="margin-top:8px">Satisfacción con el costo (1-5)
        <input type="number" min="1" max="5" value="3" id="cost-${p.id}" style="width:70px" />
      </label>
      <label>Notas <input type="text" id="notes-${p.id}" placeholder="opcional" /></label>
      <button class="primary" data-act="sendfb" data-id="${p.id}" style="margin-top:8px">Enviar y aprender</button>
      <span class="status" id="fbstatus-${p.id}"></span>
    </div>
  </div>`;
}

function wirePlanCard(p) {
  const card = $(`#plan-${p.id}`);
  let satiety = 3;
  card.querySelectorAll('[data-act="fb"]').forEach((b) =>
    b.addEventListener("click", () => $(`#fb-${p.id}`).classList.toggle("open"))
  );
  card.querySelectorAll('[data-act="shop"]').forEach((b) =>
    b.addEventListener("click", () => showShoppingForSaved(p.id, `#shop-${p.id}`))
  );
  card.querySelectorAll('[data-act="del"]').forEach((b) =>
    b.addEventListener("click", async () => {
      if (confirm("¿Eliminar esta minuta?")) {
        await api(`/api/plans/${p.id}`, { method: "DELETE" });
        loadPlans();
      }
    })
  );
  const starWrap = card.querySelector(".stars");
  const paint = () =>
    starWrap.querySelectorAll("span").forEach((s) => (s.textContent = +s.dataset.v <= satiety ? "★" : "☆"));
  starWrap.querySelectorAll("span").forEach((s) =>
    s.addEventListener("click", () => {
      satiety = +s.dataset.v;
      paint();
    })
  );
  paint();
  card.querySelector('[data-act="sendfb"]').addEventListener("click", async () => {
    const body = {
      satiety_score: satiety,
      cost_score: +$(`#cost-${p.id}`).value,
      notes: $(`#notes-${p.id}`).value,
      food_ratings: {},
    };
    const res = await api(`/api/plans/${p.id}/feedback`, { method: "POST", body: JSON.stringify(body) });
    $(`#fbstatus-${p.id}`).textContent = `✓ Aprendido · ${Object.keys(res.updated_preferences).length} preferencias ajustadas`;
  });
}

// ---------- catalogo de alimentos ----------
let foodsCache = [];
async function loadFoods() {
  if (!foodsCache.length) foodsCache = await api("/api/foods");
  renderFoods(foodsCache);
}
$("#foodSearch").addEventListener("input", (e) => {
  const q = e.target.value.toLowerCase();
  renderFoods(foodsCache.filter((f) => (f.name + f.brand + f.category).toLowerCase().includes(q)));
});
function renderFoods(foods) {
  const rows = foods
    .map((f) => {
      const cmp = (f.prices || [])
        .map((p) => `${p.retailer}: ${clp(p.price_per_100g)}`)
        .join(" · ");
      const range =
        f.price_max_per_100g > f.price_per_100g
          ? `<span class="muted"> – ${clp(f.price_max_per_100g)}</span>`
          : "";
      return `<tr title="${cmp}">
      <td>${f.name} <span class="muted">${f.brand}</span></td>
      <td>${f.category}</td>
      <td class="num">${num(f.kcal)}</td>
      <td class="num">${num(f.protein_g, 1)}</td>
      <td class="num"><strong>${clp(f.price_per_100g)}</strong>${range}<br><span class="shop">${f.retailer}</span></td>
      <td class="num">${num(f.satiety_index)}</td>
    </tr>`;
    })
    .join("");
  $("#foodsTable").innerHTML = `<table><thead><tr><th>Alimento</th><th>Categoría</th><th class="num">kcal/100g</th><th class="num">Prot/100g</th><th class="num">Precio/100g (más barato)</th><th class="num">Sac.</th></tr></thead><tbody>${rows}</tbody></table>`;
}

// ---------- constructor de comidas (tragamonedas) ----------
let builderData = null; // respuesta de /api/builder/slots
let reelIdx = {};       // role -> índice actual
let reelLocked = {};    // role -> bloqueado (no gira)
let dayMeals = [];      // comidas agregadas al día

async function loadReels() {
  if (!currentUserId) {
    $("#reels").innerHTML = '<p class="muted">Primero guarda un perfil.</p>';
    return;
  }
  $("#builderStatus").textContent = "Cargando carretes…";
  builderData = await api("/api/builder/slots", {
    method: "POST",
    body: JSON.stringify({ user_id: currentUserId, meal: $("#builderMeal").value }),
  });
  reelIdx = {};
  reelLocked = {};
  builderData.slots.forEach((s) => {
    reelIdx[s.role] = 0;
    reelLocked[s.role] = false;
  });
  $("#builderStatus").textContent = "";
  renderReels();
}

function reelSelection() {
  if (!builderData) return [];
  return builderData.slots
    .filter((s) => s.candidates.length)
    .map((s) => s.candidates[reelIdx[s.role] % s.candidates.length]);
}

function renderReels() {
  if (!builderData) return;
  const reels = builderData.slots
    .map((s) => {
      const n = s.candidates.length;
      const c = n ? s.candidates[reelIdx[s.role] % n] : null;
      const item = c
        ? `<div class="item"><div class="nm">${c.name}</div>
             <div class="meta">${num(c.grams)} g · ${num(c.kcal)} kcal · ${clp(c.cost_clp)}</div>
             <div class="meta">P ${num(c.protein_g, 1)} · C ${num(c.carb_g, 1)} · G ${num(c.fat_g, 1)}</div>
             <div class="meta"><span class="shop">${c.retailer}</span></div></div>`
        : `<div class="item">— sin opciones —</div>`;
      const locked = reelLocked[s.role];
      return `<div class="reel ${n ? "" : "empty"} ${locked ? "locked" : ""}">
        <div class="role">${s.label}
          <button class="lockbtn" data-lock="${s.role}" title="Fijar este carrete al girar" ${n ? "" : "disabled"}>${locked ? "🔒" : "🔓"}</button>
        </div>
        <div class="nav">
          <button data-dir="-1" data-role="${s.role}" ${n ? "" : "disabled"}>◀</button>
          <span class="counter">${n ? (reelIdx[s.role] % n) + 1 : 0}/${n}</span>
          <button data-dir="1" data-role="${s.role}" ${n ? "" : "disabled"}>▶</button>
        </div>${item}</div>`;
    })
    .join("");
  $("#reels").innerHTML = `<div class="reels">${reels}</div>`;
  $$("#reels .nav button").forEach((b) =>
    b.addEventListener("click", () => {
      const role = b.dataset.role;
      const n = builderData.slots.find((s) => s.role === role).candidates.length;
      if (!n) return;
      reelIdx[role] = (reelIdx[role] + +b.dataset.dir + n) % n;
      renderReels();
    })
  );
  $$("#reels .lockbtn").forEach((b) =>
    b.addEventListener("click", () => {
      reelLocked[b.dataset.lock] = !reelLocked[b.dataset.lock];
      renderReels();
    })
  );
  renderMealTotals();
}

function renderMealTotals() {
  if (!builderData) return;
  const t = { kcal: 0, protein_g: 0, carb_g: 0, fat_g: 0, cost_clp: 0 };
  reelSelection().forEach((c) => {
    for (const k in t) t[k] += c[k] || 0;
  });
  const tg = builderData.target;
  const fitClass = (r) => (r >= 0.8 && r <= 1.2 ? "ok" : "off");
  const pill = (label, val, target, unit) =>
    `<div class="pill fitpill ${fitClass(target ? val / target : 0)}">${label}: <strong>${num(val)}${unit}</strong> <span class="muted">/ ${num(target)}${unit}</span></div>`;
  $("#mealTotals").innerHTML = `<div class="totbar">
    <div class="pill">Costo: <strong>${clp(t.cost_clp)}</strong></div>
    ${pill("Energía", t.kcal, tg.kcal, " kcal")}
    ${pill("Proteína", t.protein_g, tg.protein_g, " g")}
    ${pill("Carbs", t.carb_g, tg.carb_g, " g")}
    ${pill("Grasa", t.fat_g, tg.fat_g, " g")}
  </div>`;
}

$("#builderMeal").addEventListener("change", loadReels);
$("#spinBtn").addEventListener("click", () => {
  if (!builderData) return;
  builderData.slots.forEach((s) => {
    // Los carretes bloqueados conservan su elemento; solo giran los libres.
    if (s.candidates.length && !reelLocked[s.role])
      reelIdx[s.role] = Math.floor(Math.random() * s.candidates.length);
  });
  renderReels();
});
$("#addMealBtn").addEventListener("click", () => {
  const items = reelSelection();
  if (!items.length) return;
  dayMeals.push({ meal: builderData.meal, items });
  renderDayBuild();
});

function mealSubtotal(items) {
  const s = { kcal: 0, protein_g: 0, carb_g: 0, fat_g: 0, cost_clp: 0, satiety: 0 };
  items.forEach((i) => {
    s.kcal += i.kcal; s.protein_g += i.protein_g; s.carb_g += i.carb_g;
    s.fat_g += i.fat_g; s.cost_clp += i.cost_clp; s.satiety += i.satiety_contrib;
  });
  for (const k in s) s[k] = Math.round(s[k] * 10) / 10;
  return s;
}

function renderDayBuild() {
  if (!dayMeals.length) {
    $("#dayBuild").innerHTML = "";
    return;
  }
  let total = { kcal: 0, cost_clp: 0 };
  let html = "<h3>Minuta del día en construcción</h3>";
  dayMeals.forEach((m, idx) => {
    const st = mealSubtotal(m.items);
    total.kcal += st.kcal;
    total.cost_clp += st.cost_clp;
    html += `<div class="daycard">
      <div style="display:flex;justify-content:space-between;gap:10px">
        <strong style="text-transform:capitalize">${m.meal}</strong>
        <span class="muted">${num(st.kcal)} kcal · ${clp(st.cost_clp)}
          <button class="btn-sm" data-rm="${idx}">✕</button></span>
      </div>
      <div class="muted">${m.items.map((i) => `${i.name} (${num(i.grams)}g)`).join(" · ")}</div>
    </div>`;
  });
  html += `<div class="totbar"><div class="pill">Día: <strong>${num(total.kcal)} kcal</strong></div>
    <div class="pill">Total: <strong>${clp(total.cost_clp)}</strong></div></div>
    <button id="saveDayBtn" class="primary">💾 Guardar minuta del día</button>
    <span id="saveDayStatus" class="status"></span>`;
  $("#dayBuild").innerHTML = html;
  $$("#dayBuild [data-rm]").forEach((b) =>
    b.addEventListener("click", () => {
      dayMeals.splice(+b.dataset.rm, 1);
      renderDayBuild();
    })
  );
  $("#saveDayBtn").addEventListener("click", saveBuiltDay);
}

async function saveBuiltDay() {
  if (!currentUserId || !dayMeals.length) return;
  const meals = dayMeals.map((m) => ({ meal: m.meal, items: m.items, subtotal: mealSubtotal(m.items) }));
  const totals = { kcal: 0, protein_g: 0, carb_g: 0, fat_g: 0, cost_clp: 0, satiety: 0 };
  meals.forEach((m) => {
    ["kcal", "protein_g", "carb_g", "fat_g", "cost_clp"].forEach((k) => (totals[k] += m.subtotal[k]));
    totals.satiety += m.subtotal.satiety;
  });
  for (const k in totals) totals[k] = Math.round(totals[k] * 10) / 10;
  const title = prompt("Nombre de la minuta:", "Armada " + new Date().toLocaleDateString("es-CL"));
  if (title === null) return;
  await api("/api/plans", {
    method: "POST",
    body: JSON.stringify({ user_id: currentUserId, title, scope: "diario", payload: { meals, totals } }),
  });
  $("#saveDayStatus").textContent = "✓ Minuta guardada.";
  dayMeals = [];
  renderDayBuild();
}

// ---------- init ----------
const serverBtn = document.getElementById("serverBtn");
if (serverBtn) {
  // El selector de servidor solo tiene sentido en la APK (origen file://).
  // En la web servida por el backend se usa el mismo origen, así que se oculta.
  if (location.protocol === "file:") {
    serverBtn.addEventListener("click", setApiBase);
    serverBtn.title = "Servidor: " + (apiBase() || "este sitio");
  } else {
    serverBtn.style.display = "none";
  }
}

function startApp() {
  loadRetailers().then(loadUsers).catch((e) => {
    const msg = "No se pudo conectar con el servidor SCAVENGER" + (apiBase() ? " (" + apiBase() + ")" : "") + ".";
    const status = document.getElementById("genStatus");
    if (status) status.textContent = msg;
    console.error(msg, e);
  });
}

// En la APK (file://) se requiere configurar la URL del backend antes de cargar.
if (location.protocol === "file:" && !apiBase()) {
  alert("Bienvenido a SCAVENGER.\n\nConfigura la URL del servidor (tu backend corriendo en el PC/servidor) para comenzar.");
  setApiBase(); // recarga al guardar
} else {
  startApp();
}
