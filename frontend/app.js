// SCAVENGER · frontend (vanilla JS)
const API = "";
let currentUserId = null;
let lastPlanPayload = null; // ultima minuta generada (para guardar)

// ---------- helpers ----------
const $ = (sel) => document.querySelector(sel);
const $$ = (sel) => document.querySelectorAll(sel);
const clp = (n) => "$" + Math.round(n || 0).toLocaleString("es-CL");
const num = (n, d = 0) => (n ?? 0).toLocaleString("es-CL", { maximumFractionDigits: d });

async function api(path, opts = {}) {
  const res = await fetch(API + path, {
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

$("#generateBtn").addEventListener("click", async () => {
  if (!currentUserId) return alert("Primero guarda un perfil.");
  $("#genStatus").textContent = "Optimizando combinaciones más económicas...";
  $("#planResult").innerHTML = "";
  try {
    const body = {
      user_id: currentUserId,
      scope: $("#scope").value,
      satiety_emphasis: +$("#satiety").value,
      use_budget: $("#useBudget").checked,
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
      <td class="num">${num(i.grams)} g</td>
      <td class="num">${num(i.kcal)}</td>
      <td class="num">${num(i.protein_g, 1)}</td>
      <td class="num">${clp(i.cost_clp)}</td>
    </tr>`
    )
    .join("");
  return `<div class="meal">
    <h3>${meal.meal} <span>${num(meal.subtotal.kcal)} kcal · ${clp(meal.subtotal.cost_clp)}</span></h3>
    <table><thead><tr><th>Alimento</th><th class="num">Cantidad</th><th class="num">kcal</th><th class="num">Prot (g)</th><th class="num">Costo</th></tr></thead>
    <tbody>${rows || '<tr><td colspan="5" class="muted">Sin items</td></tr>'}</tbody></table>
  </div>`;
}

function renderDaily(data) {
  lastPlanPayload = data;
  let html = "";
  if (data.warnings && data.warnings.length)
    html += `<div class="warnbox">⚠ ${data.warnings.join(" ")}</div>`;
  html += totalsBar(data.totals, data.requirements, data.budget_clp, data.over_budget);
  html += data.meals.map(mealTable).join("");
  html += `<button id="savePlanBtn" class="primary">Guardar esta minuta</button>`;
  $("#planResult").innerHTML = html;
  $("#savePlanBtn").addEventListener("click", () => savePlan("diario", data));
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
  html += `<button id="savePlanBtn" class="primary">Guardar esta minuta semanal</button>`;
  $("#planResult").innerHTML = html;
  $("#savePlanBtn").addEventListener("click", () => savePlan("semanal", data));
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
async function loadPlans() {
  if (!currentUserId) return;
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
        <button class="btn-sm" data-act="fb" data-id="${p.id}">Registrar saciedad</button>
        <button class="btn-sm" data-act="del" data-id="${p.id}">Eliminar</button>
      </div>
    </div>
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
    .map(
      (f) => `<tr>
      <td>${f.name} <span class="muted">${f.brand}</span></td>
      <td>${f.category}</td>
      <td class="num">${num(f.kcal)}</td>
      <td class="num">${num(f.protein_g, 1)}</td>
      <td class="num">${clp(f.price_per_100g)}</td>
      <td class="num">${num(f.satiety_index)}</td>
    </tr>`
    )
    .join("");
  $("#foodsTable").innerHTML = `<table><thead><tr><th>Alimento</th><th>Categoría</th><th class="num">kcal/100g</th><th class="num">Prot/100g</th><th class="num">Precio/100g</th><th class="num">Sac.</th></tr></thead><tbody>${rows}</tbody></table>`;
}

// ---------- init ----------
loadUsers();
