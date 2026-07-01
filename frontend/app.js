// SCAVENGER · frontend móvil (vanilla JS)
// Landing (con calendario) + carrusel (Home, Ruleta, Perfil) + jornadas + overlays.
function apiBase() {
  return (localStorage.getItem("scavenger_api_base") || window.SCAVENGER_API_BASE || "").replace(/\/+$/, "");
}
function setApiBase() {
  const v = prompt("URL del servidor SCAVENGER\n(ej: http://192.168.1.10:8000)", apiBase());
  if (v === null) return;
  localStorage.setItem("scavenger_api_base", v.trim());
  location.reload();
}

const $ = (s) => document.querySelector(s);
const $$ = (s) => document.querySelectorAll(s);
const clp = (n) => "$" + Math.round(n || 0).toLocaleString("es-CL");
const num = (n, d = 0) => (n ?? 0).toLocaleString("es-CL", { maximumFractionDigits: d });
// Estado con color: kind = "ok" (verde) | "err" (rojo) | "" (neutro).
function setStatus(sel, msg, kind = "") { const el = typeof sel === "string" ? $(sel) : sel; if (!el) return; el.className = "status" + (kind ? " " + kind : ""); el.textContent = msg; }

async function api(path, opts = {}) {
  const res = await fetch(apiBase() + path, { headers: { "Content-Type": "application/json" }, ...opts });
  if (!res.ok) throw new Error(`${res.status}: ${await res.text()}`);
  return res.status === 204 ? null : res.json();
}

let currentUserId = null;
let currentUserObj = null;
let reqCache = null;          // requerimientos diarios del usuario (para márgenes)
let retailersCache = [];

// ===================== CARRUSEL =====================
const SCREENS = ["home", "ruleta", "perfil"];
let screenIdx = 0;
function showScreen(i) {
  screenIdx = (i + SCREENS.length) % SCREENS.length;
  $$(".screen").forEach((s) => s.classList.toggle("active", s.dataset.screen === SCREENS[screenIdx]));
  $$("#dots span").forEach((d, k) => d.classList.toggle("on", k === screenIdx));
  if (SCREENS[screenIdx] === "home") renderLanding();
  if (SCREENS[screenIdx] === "ruleta") loadReels();
  if (SCREENS[screenIdx] === "perfil") loadRequirements();
}
$("#prevScreen").addEventListener("click", () => showScreen(screenIdx - 1));
$("#nextScreen").addEventListener("click", () => showScreen(screenIdx + 1));
$("#homeLogo").addEventListener("click", () => showScreen(0));
$$("#dots span").forEach((d) => d.addEventListener("click", () => showScreen(+d.dataset.go)));

// ===================== LANDING =====================
function renderLanding() {
  const sel = $("#userSelect");
  const name = sel && sel.selectedOptions.length ? sel.selectedOptions[0].textContent.split(" #")[0] : "";
  if (currentUserId && name) {
    $("#welcome").textContent = `Hola, ${name} 🦝`;
    $("#welcomeSub").textContent = "Arma tu jornada, planifica la semana y compra al mejor precio.";
  } else {
    $("#welcome").textContent = "Bienvenido a SCAVENGER 🦝";
    $("#welcomeSub").textContent = "Crea tu perfil para recibir recomendaciones a tu medida.";
  }
  loadCalendarData().then(renderLandingCalendar);
}
$$(".hub-btn").forEach((b) => b.addEventListener("click", () => {
  const a = b.dataset.act;
  if (a === "guiado") openGuided(null);
  else if (a === "ruleta") showScreen(1);
  else if (a === "perfil") showScreen(2);
  else if (a === "calendario") $("#landingCal").scrollIntoView({ behavior: "smooth", block: "start" });
  else if (a === "generar") openGenerar();
  else if (a === "minutas") openMinutas();
  else if (a === "catalogo") openCatalogo();
  else if (a === "nuevo") newUser();
}));

// ===================== USUARIOS / PERFIL =====================
async function loadUsers() {
  const users = await api("/api/users");
  const sel = $("#userSelect");
  sel.innerHTML = "";
  users.forEach((u) => {
    const o = document.createElement("option");
    o.value = u.id; o.textContent = `${u.name || "Usuario"} #${u.id}`;
    sel.appendChild(o);
  });
  if (users.length) {
    currentUserObj = users[users.length - 1];
    currentUserId = currentUserObj.id;
    sel.value = currentUserId;
    fillForm(currentUserObj);
  }
}
function fillForm(u) {
  const f = $("#userForm");
  ["name", "sex", "age", "weight_kg", "height_cm", "activity_level", "goal",
   "daily_budget_clp", "monthly_budget_clp", "meals_per_day", "min_protein_per_meal_g"].forEach((k) => {
    if (f[k] && u[k] != null) f[k].value = u[k];
  });
  f.diet_tags.value = (u.diet_tags && u.diet_tags[0]) || "";
  const pref = new Set(u.preferred_retailers || []);
  $$("#retailerChecks input").forEach((chk) => (chk.checked = pref.has(chk.value)));
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
function newUser() {
  currentUserId = null; currentUserObj = null; reqCache = null;
  $("#userForm").reset();
  $("#userStatus").textContent = "Nuevo perfil — completa tus datos y guarda.";
  showScreen(2);
  setTimeout(() => $("#userForm").name.focus(), 200);
}
$("#userSelect").addEventListener("change", async (e) => {
  currentUserId = parseInt(e.target.value);
  currentUserObj = await api(`/api/users/${currentUserId}`);
  reqCache = null;
  fillForm(currentUserObj);
  loadRequirements();
});
$("#newUserBtn").addEventListener("click", newUser);
$("#userForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  const f = e.target;
  const body = {
    name: f.name.value, sex: f.sex.value, age: +f.age.value,
    weight_kg: +f.weight_kg.value, height_cm: +f.height_cm.value,
    activity_level: f.activity_level.value, goal: f.goal.value,
    daily_budget_clp: +f.daily_budget_clp.value,
    monthly_budget_clp: +f.monthly_budget_clp.value,
    meals_per_day: +f.meals_per_day.value,
    min_protein_per_meal_g: +f.min_protein_per_meal_g.value,
    diet_tags: f.diet_tags.value ? [f.diet_tags.value] : [],
    excluded_foods: [], preferred_retailers: selectedRetailers(),
  };
  const u = currentUserId
    ? await api(`/api/users/${currentUserId}`, { method: "PATCH", body: JSON.stringify(body) })
    : await api("/api/users", { method: "POST", body: JSON.stringify(body) });
  currentUserId = u.id; currentUserObj = u; reqCache = null;
  setStatus("#userStatus", "✓ Guardado (#" + u.id + ")", "ok");
  await loadUsers();
  $("#userSelect").value = currentUserId;
  loadRequirements();
});

async function loadRequirements() {
  if (!currentUserId) return;
  reqCache = await api(`/api/users/${currentUserId}/requirements`);
  if (!currentUserObj) currentUserObj = await api(`/api/users/${currentUserId}`);
  const u = currentUserObj, r = reqCache;
  $("#tKcal").textContent = num(r.kcal) + " kcal";
  $("#tBasal").textContent = num(r.bmr) + " kcal";
  $("#tNutri").textContent = `P ${num(r.protein_g)} · C ${num(r.carb_g)} · G ${num(r.fat_g)}`;
  $("#tEco").textContent = clp(u.monthly_budget_clp > 0 ? u.monthly_budget_clp : (u.daily_budget_clp || 0) * 30);
}
// Requerimientos diarios cacheados (los carga si faltan).
async function ensureRequirements() {
  if (!reqCache && currentUserId) reqCache = await api(`/api/users/${currentUserId}/requirements`);
  if (!currentUserObj && currentUserId) currentUserObj = await api(`/api/users/${currentUserId}`);
  return reqCache;
}
function dailyBudget() {
  const u = currentUserObj || {};
  return u.monthly_budget_clp > 0 ? u.monthly_budget_clp / 30 : (u.daily_budget_clp || 0);
}

// ===================== CONSTRUCTOR DE COMIDAS (carretes reutilizables) =====================
// Controlador de carretes: encapsula el estado por rol (índice, bloqueo,
// quitado, orden, origen) y se renderiza en cualquier contenedor. Lo usan
// tanto la Ruleta suelta (armado rápido) como cada comida de una jornada.
let dayMeals = [];

async function fetchBuilder(meal) {
  return api("/api/builder/slots", { method: "POST", body: JSON.stringify({ user_id: currentUserId, meal }) });
}

function makeReelCtrl(data) {
  const st = { idx: {}, locked: {}, removed: {}, sort: {}, origin: {}, grams: {}, gramsFor: {} };
  // grams/gramsFor: override de cantidad por rol (y el food_id al que aplica).
  data.slots.forEach((s) => { st.idx[s.role] = 0; st.locked[s.role] = false; st.removed[s.role] = false; st.sort[s.role] = "price"; st.origin[s.role] = ""; st.grams[s.role] = 0; st.gramsFor[s.role] = ""; });

  function sorted(slot) {
    let arr = slot.candidates;
    const o = st.origin[slot.role];                                            // 1er filtro: origen
    if (o) { const f = arr.filter((c) => c.origin === o); if (f.length) arr = f; }
    arr = [...arr];
    const sv = st.sort[slot.role];
    if (sv === "kcal") arr.sort((a, b) => a.kcal - b.kcal);                     // densidad calórica asc
    else if (sv === "protein") arr.sort((a, b) => b.protein_g - a.protein_g);   // más proteína primero (paso 1)
    else if (sv === "carb") arr.sort((a, b) => b.carb_g - a.carb_g);            // más carbohidrato primero (paso 2)
    else if (sv === "micro") arr.sort((a, b) => (b.micro_score || 0) - (a.micro_score || 0)); // micronutrientes (paso 3)
    else if (sv === "pref") arr.sort((a, b) => (b.pref || 0) - (a.pref || 0) || a.cost_clp - b.cost_clp); // preferencia, luego precio (paso 4)
    else arr.sort((a, b) => a.cost_clp - b.cost_clp);                           // precio ($/porción) asc
    return arr;
  }
  function selection() {
    return data.slots.filter((s) => !st.removed[s.role] && s.candidates.length)
      .map((s) => { const arr = sorted(s); return scaledCandidate(st, s.role, arr[st.idx[s.role] % arr.length]); });
  }
  function totals() {
    const t = { kcal: 0, protein_g: 0, carb_g: 0, fat_g: 0, cost_clp: 0, satiety_contrib: 0 };
    selection().forEach((c) => { for (const k in t) t[k] += c[k] || 0; });
    return t;
  }
  function rememberSel(role, mutate) {                                          // cambiar criterio sin perder la selección
    const slot = data.slots.find((s) => s.role === role);
    const curArr = sorted(slot);
    const cur = curArr.length ? curArr[st.idx[role] % curArr.length] : null;
    mutate();
    const arr = sorted(slot);
    const i = cur ? arr.findIndex((c) => c.food_id === cur.food_id) : -1;
    st.idx[role] = i >= 0 ? i : 0;
  }
  function spin() {
    data.slots.forEach((s) => {
      if (st.locked[s.role] || st.removed[s.role]) return;
      const n = sorted(s).length;                 // respeta el filtro de origen / orden vigente
      if (n) st.idx[s.role] = Math.floor(Math.random() * n);
    });
  }
  function render(container, onChange) {
    container.innerHTML = data.slots.map((s) => reelHtml(s, st, sorted)).join("");
    const redraw = () => render(container, onChange);
    container.querySelectorAll(".tri").forEach((b) => b.addEventListener("click", () => {
      const role = b.dataset.role, n = sorted(data.slots.find((s) => s.role === role)).length;
      if (!n) return; st.idx[role] = (st.idx[role] + +b.dataset.dir + n) % n; redraw();
    }));
    container.querySelectorAll(".lockbtn").forEach((b) => b.addEventListener("click", () => { st.locked[b.dataset.lock] = !st.locked[b.dataset.lock]; redraw(); }));
    container.querySelectorAll(".rmbtn").forEach((b) => b.addEventListener("click", () => { st.removed[b.dataset.rm] = !st.removed[b.dataset.rm]; redraw(); }));
    container.querySelectorAll(".sortbtn").forEach((b) => b.addEventListener("click", () => { rememberSel(b.dataset.role, () => (st.sort[b.dataset.role] = b.dataset.sort)); redraw(); }));
    container.querySelectorAll(".originsel").forEach((sel) => sel.addEventListener("change", () => { rememberSel(sel.dataset.role, () => (st.origin[sel.dataset.role] = sel.value)); redraw(); }));
    container.querySelectorAll(".qbtn").forEach((b) => b.addEventListener("click", () => {
      const role = b.dataset.role, slot = data.slots.find((s) => s.role === role);
      const arr = sorted(slot), c = arr.length ? arr[st.idx[role] % arr.length] : null;
      if (!c) return;
      const cur = reelGrams(st, role, c);                            // pasos de 10 g, mínimo 5 g, en múltiplos de 5
      st.grams[role] = Math.max(5, Math.round((cur + (+b.dataset.q) * 10) / 5) * 5);
      st.gramsFor[role] = c.food_id;
      redraw();
    }));
    wireShowcase(container);   // vitrina móvil: agranda el carrete centrado
    if (onChange) onChange();
  }
  return { data, st, meal: data.meal, sorted, selection, totals, spin, render };
}

// Vitrina móvil: marca con .focus el carrete más cercano al centro del contenedor
// (se ve más grande). Se recalcula en cada scroll y tras cada render.
function markCenterReel(container) {
  const reels = container.querySelectorAll(".reel");
  if (!reels.length) return;
  const rect = container.getBoundingClientRect();
  if (!rect.width) return;
  const cx = rect.left + rect.width / 2;
  let best = null, bestDist = Infinity;
  reels.forEach((r) => {
    const rr = r.getBoundingClientRect();
    const d = Math.abs(rr.left + rr.width / 2 - cx);
    if (d < bestDist) { bestDist = d; best = r; }
  });
  reels.forEach((r) => r.classList.toggle("focus", r === best));
}
function wireShowcase(container) {
  if (!container._showcaseWired) {
    container._showcaseWired = true;
    container.addEventListener("scroll", () => {
      if (container._raf) return;
      container._raf = requestAnimationFrame(() => { container._raf = 0; markCenterReel(container); });
    }, { passive: true });
  }
  requestAnimationFrame(() => markCenterReel(container));
}

// Cantidad (g) elegida para el rol: override del usuario si aplica al alimento
// mostrado; si no, la porción pre-calculada del candidato.
function reelGrams(st, role, c) {
  if (!c) return 0;
  if (st.gramsFor[role] === c.food_id && st.grams[role] > 0) return st.grams[role];
  return c.grams;
}
// Candidato escalado a la cantidad elegida (macros, costo, porciones y platos/envase).
function scaledCandidate(st, role, c) {
  if (!c) return c;
  const g = reelGrams(st, role, c);
  if (!c.grams || g === c.grams) return c;
  const f = g / c.grams, r1 = (x) => Math.round((x || 0) * 10) / 10;
  return { ...c, grams: g, servings: r1((c.servings || 0) * f),
    kcal: r1(c.kcal * f), protein_g: r1(c.protein_g * f), carb_g: r1(c.carb_g * f),
    fat_g: r1(c.fat_g * f), fiber_g: r1((c.fiber_g || 0) * f), cost_clp: r1(c.cost_clp * f),
    satiety_contrib: r1((c.satiety_contrib || 0) * f),
    platos_por_envase: (c.package_g && g > 0) ? Math.floor(c.package_g / g) : (c.platos_por_envase || 0) };
}

function reelHtml(s, st, sorted) {
  const hasAny = s.candidates.length, removed = st.removed[s.role];
  const arr = sorted(s), n = arr.length, c = n ? scaledCandidate(st, s.role, arr[st.idx[s.role] % n]) : null;
  const win = removed
    ? `<div class="window removed">Grupo quitado<br><small>toca ＋ para incluir</small></div>`
    : (c
      ? `<div class="window">
           <div class="nm">${c.name}</div>
           ${c.brand ? `<div class="brand">${c.brand}</div>` : ""}
           <div class="qty">
             <button class="qbtn" data-q="-1" data-role="${s.role}" title="Menos cantidad">−</button>
             <span class="qval"><strong>${num(c.grams)} g</strong><small>${num(c.servings, 1)} porc.</small></span>
             <button class="qbtn" data-q="1" data-role="${s.role}" title="Más cantidad">＋</button>
           </div>
           <div class="mc">${num(c.kcal)} kcal · P${num(c.protein_g, 0)} C${num(c.carb_g, 0)} G${num(c.fat_g, 0)}</div>
           <span class="shop">${c.retailer}</span>
           ${c.platos_por_envase ? `<div class="platos">≈ ${c.platos_por_envase} platos/envase</div>` : ""}
           <div class="cost">${clp(c.cost_clp)}</div>
         </div>`
      : `<div class="window empty">— sin opciones —</div>`);
  const off = !hasAny || removed ? "disabled" : "";
  const origins = [...new Set(s.candidates.map((x) => x.origin))].sort();
  const originSel = origins.length > 1
    ? `<select class="originsel" data-role="${s.role}" ${off}>
         <option value="">Origen: todos</option>
         ${origins.map((o) => `<option value="${o}" ${st.origin[s.role] === o ? "selected" : ""}>${o}</option>`).join("")}
       </select>`
    : "";
  return `<div class="reel ${st.locked[s.role] ? "locked" : ""} ${removed ? "removed" : ""}">
    <div class="role">${s.label}
      <button class="lockbtn ${st.locked[s.role] ? "on" : ""}" data-lock="${s.role}" ${off}>${st.locked[s.role] ? "Fijo" : "Libre"}</button>
      <button class="rmbtn" data-rm="${s.role}" title="Quitar/incluir grupo">${removed ? "＋" : "✕"}</button>
    </div>
    ${originSel}
    <div class="sortrow">
      <button class="sortbtn ${st.sort[s.role] === "price" ? "on" : ""}" data-sort="price" data-role="${s.role}" ${off}>$/porción</button>
      <button class="sortbtn ${st.sort[s.role] === "kcal" ? "on" : ""}" data-sort="kcal" data-role="${s.role}" ${off}>kcal/porción</button>
    </div>
    <button class="tri up" data-dir="-1" data-role="${s.role}" ${off}></button>
    ${win}
    <button class="tri down" data-dir="1" data-role="${s.role}" ${off}></button>
    <span class="counter">${removed ? "—" : (n ? (st.idx[s.role] % n) + 1 : 0) + "/" + n}</span>
  </div>`;
}

// ---- Ruleta suelta (pantalla del carrusel): armado rápido ----
let activeCtrl = null;

async function loadReels() {
  if (!currentUserId) { $("#reels").innerHTML = '<p class="hint">Crea un perfil primero.</p>'; return; }
  $("#builderStatus").textContent = "Cargando…";
  try {
    activeCtrl = makeReelCtrl(await fetchBuilder($("#builderMeal").value));
  } catch (e) { $("#builderStatus").textContent = "Error: " + e.message; return; }
  $("#builderStatus").textContent = "";
  activeCtrl.render($("#reels"), renderRuletaControl);
}
function renderRuletaControl() {
  if (!activeCtrl) return;
  const sel = activeCtrl.selection(), t = activeCtrl.totals();
  renderDonut($("#donut"), t, activeCtrl.data.target);
  $("#priceBox").textContent = clp(t.cost_clp);
  const platos = sel.map((c) => c.platos_por_envase).filter((x) => x > 0);
  const minP = platos.length ? Math.min(...platos) : 0;
  const pcal = t.protein_g * 4, ccal = t.carb_g * 4, gcal = t.fat_g * 9, tot = pcal + ccal + gcal || 1;
  $("#platosInfo").innerHTML =
    `<span style="color:var(--p)">P ${Math.round(pcal / tot * 100)}%</span> ·
     <span style="color:var(--c)">C ${Math.round(ccal / tot * 100)}%</span> ·
     <span style="color:var(--g)">G ${Math.round(gcal / tot * 100)}%</span>
     ${minP ? `<br>Con una compra de cada producto preparas <strong>≈ ${minP} platos</strong>.` : ""}`;
}
function renderDonut(el, t, target) {
  const pcal = (t.protein_g || 0) * 4, ccal = (t.carb_g || 0) * 4, gcal = (t.fat_g || 0) * 9;
  const tot = pcal + ccal + gcal || 1;
  const segs = [[pcal / tot * 100, "var(--p)"], [ccal / tot * 100, "var(--c)"], [gcal / tot * 100, "var(--g)"]];
  const kcalPct = target && target.kcal ? Math.round((t.kcal || 0) / target.kcal * 100) : 0;
  let acc = 0;
  const arcs = segs.map(([p, color]) => {
    const c = `<circle cx="21" cy="21" r="15.915" fill="none" stroke="${color}" stroke-width="6"
      stroke-dasharray="${p.toFixed(2)} ${(100 - p).toFixed(2)}" stroke-dashoffset="${(25 - acc).toFixed(2)}"/>`;
    acc += p; return c;
  }).join("");
  el.innerHTML =
    `<circle cx="21" cy="21" r="15.915" fill="none" stroke="#2a2a2d" stroke-width="6"/>${arcs}
     <text x="21" y="20.5" text-anchor="middle" font-size="6">${kcalPct}%</text>
     <text x="21" y="26" text-anchor="middle" font-size="3.6" fill="#cfcfd6">Kcal/d</text>`;
}
$("#builderMeal").addEventListener("change", loadReels);
$("#spinBtn").addEventListener("click", () => { if (!activeCtrl) return; activeCtrl.spin(); activeCtrl.render($("#reels"), renderRuletaControl); });
$("#addMealBtn").addEventListener("click", () => {
  if (!activeCtrl) return;
  const items = activeCtrl.selection();
  if (!items.length) return;
  dayMeals.push({ meal: activeCtrl.meal, items });
  renderDayBuild();
});

function mealSubtotal(items) {
  const s = { kcal: 0, protein_g: 0, carb_g: 0, fat_g: 0, cost_clp: 0, satiety: 0 };
  items.forEach((i) => {
    s.kcal += i.kcal; s.protein_g += i.protein_g; s.carb_g += i.carb_g;
    s.fat_g += i.fat_g; s.cost_clp += i.cost_clp; s.satiety += i.satiety_contrib || 0;
  });
  for (const k in s) s[k] = Math.round(s[k] * 10) / 10;
  return s;
}
function renderDayBuild() {
  if (!dayMeals.length) { $("#dayBuild").innerHTML = ""; return; }
  const total = { kcal: 0, cost_clp: 0 };
  let html = '<h3 style="font-size:15px;margin-top:14px">Minuta del día</h3>';
  dayMeals.forEach((m, idx) => {
    const st = mealSubtotal(m.items);
    total.kcal += st.kcal; total.cost_clp += st.cost_clp;
    html += `<div class="daycard"><div class="top">
        <strong style="text-transform:capitalize">${m.meal}</strong>
        <span class="muted">${num(st.kcal)} kcal · ${clp(st.cost_clp)}
          <button class="btn-sm" data-rm="${idx}">✕</button></span></div>
        <div class="muted">${m.items.map((i) => `${i.name} (${num(i.grams)}g)`).join(" · ")}</div></div>`;
  });
  html += `<div class="totbar"><div class="pill">Día: <strong>${num(total.kcal)} kcal</strong></div>
    <div class="pill">Total: <strong>${clp(total.cost_clp)}</strong></div></div>
    <button id="saveDayBtn" class="primary">Guardar minuta del día</button>
    <span id="saveDayStatus" class="status"></span>`;
  $("#dayBuild").innerHTML = html;
  $$("#dayBuild [data-rm]").forEach((b) => b.addEventListener("click", () => { dayMeals.splice(+b.dataset.rm, 1); renderDayBuild(); }));
  $("#saveDayBtn").addEventListener("click", saveBuiltDay);
}
async function saveBuiltDay() {
  if (!currentUserId || !dayMeals.length) return;
  const meals = dayMeals.map((m) => ({ meal: m.meal, items: m.items, subtotal: mealSubtotal(m.items) }));
  const totals = { kcal: 0, protein_g: 0, carb_g: 0, fat_g: 0, cost_clp: 0, satiety: 0 };
  meals.forEach((m) => { ["kcal", "protein_g", "carb_g", "fat_g", "cost_clp", "satiety"].forEach((k) => (totals[k] += m.subtotal[k])); });
  for (const k in totals) totals[k] = Math.round(totals[k] * 10) / 10;
  const title = prompt("Nombre de la minuta:", "Día " + new Date().toLocaleDateString("es-CL"));
  if (title === null) return;
  await api("/api/plans", { method: "POST", body: JSON.stringify({ user_id: currentUserId, title, scope: "diario", payload: { meals, totals } }) });
  $("#saveDayStatus").textContent = "✓ Guardada (mírala en el Calendario).";
  dayMeals = []; renderDayBuild();
}

// ===================== CALENDARIO (en la landing) + JORNADAS =====================
let calRef = null, plansByDate = {}, routinesAll = [];
const MONTHS = ["enero","febrero","marzo","abril","mayo","junio","julio","agosto","septiembre","octubre","noviembre","diciembre"];
const DOW = ["L","M","M","J","V","S","D"];
const DOW_FULL = ["lunes","martes","miércoles","jueves","viernes","sábado","domingo"];
const dateKey = (d) => d.slice(0, 10);
// Orden de prioridad al armar una jornada de N comidas: primero las comidas
// principales (para que una rutina de almuerzo/cena se precargue aun con pocas
// comidas/día) y luego los snacks.
const MEAL_PRIORITY = ["desayuno", "almuerzo", "cena", "snack 1", "snack 2", "snack 3", "cena 2", "desayuno 2"];
// Lunes=0 ... domingo=6 (igual que el backend) a partir de una fecha YYYY-MM-DD.
function weekdayOf(k) { return (new Date(k + "T12:00:00").getDay() + 6) % 7; }
function fmtKey(d) { return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}`; }
const PRESET_WEEKDAYS = { "L-V": [0,1,2,3,4], "finde": [5,6], "todos": [0,1,2,3,4,5,6] };
function presetMatches(preset, wd) { return (PRESET_WEEKDAYS[preset] || PRESET_WEEKDAYS["todos"]).includes(wd); }
function planDate(p) { return (p.payload && p.payload.date) || (p.created_at ? dateKey(p.created_at) : null); }

async function loadCalendarData() {
  plansByDate = {}; routinesAll = [];
  if (!currentUserId) return;
  const [plans, routines] = await Promise.all([
    api(`/api/plans?user_id=${currentUserId}`),
    api(`/api/routines?user_id=${currentUserId}`).catch(() => []),
  ]);
  plans.forEach((p) => { const k = planDate(p); if (k) (plansByDate[k] = plansByDate[k] || []).push(p); });
  routinesAll = routines || [];
}
function renderLandingCalendar() {
  if (!$("#landingCal")) return;
  if (!calRef) { const n = new Date(); calRef = new Date(n.getFullYear(), n.getMonth(), 1); }
  $("#landingCal").innerHTML = `
    <div class="cal-head"><button class="iconbtn" id="calPrev">&#10094;</button>
      <h2 id="calTitle">—</h2><button class="iconbtn" id="calNext">&#10095;</button></div>
    <div class="cal-grid" id="calGrid"></div>
    <div class="cal-legend"><span><i class="lg-plan"></i>Con jornada</span><span><i class="lg-rout"></i>Rutina</span></div>
    <p class="hint" style="text-align:center">Toca un día para armar su jornada 🦝</p>`;
  $("#calPrev").addEventListener("click", () => { calRef.setMonth(calRef.getMonth() - 1); renderCalendarGrid(); });
  $("#calNext").addEventListener("click", () => { calRef.setMonth(calRef.getMonth() + 1); renderCalendarGrid(); });
  renderCalendarGrid();
}
function renderCalendarGrid() {
  if (!$("#calGrid")) return;
  const y = calRef.getFullYear(), m = calRef.getMonth();
  $("#calTitle").textContent = `${MONTHS[m]} ${y}`;
  const startDow = (new Date(y, m, 1).getDay() + 6) % 7;
  const days = new Date(y, m + 1, 0).getDate();
  const td = new Date();   // clave local (no UTC): evita correr "hoy" de noche en Chile
  const todayK = `${td.getFullYear()}-${String(td.getMonth() + 1).padStart(2, "0")}-${String(td.getDate()).padStart(2, "0")}`;
  let html = DOW.map((d) => `<div class="cal-dow">${d}</div>`).join("");
  for (let i = 0; i < startDow; i++) html += `<div class="cal-cell empty"></div>`;
  for (let d = 1; d <= days; d++) {
    const k = `${y}-${String(m + 1).padStart(2, "0")}-${String(d).padStart(2, "0")}`;
    const hasPlan = !!plansByDate[k];
    const hasRoutine = routinesAll.some((r) => presetMatches(r.preset, weekdayOf(k)));
    const mark = hasPlan ? "has" : (hasRoutine ? "routine" : "");
    html += `<div class="cal-cell ${mark} ${k === todayK ? "today" : ""}" data-k="${k}">${d}</div>`;
  }
  $("#calGrid").innerHTML = html;
  $$("#calGrid .cal-cell[data-k]").forEach((c) => c.addEventListener("click", () => openJornada(c.dataset.k)));
}

// ---- Jornada de un día: N comidas editables, armado por ruleta, márgenes en vivo ----
let jornada = null;   // { date, meals: [{meal, items, subtotal, routineId?}] }

function defaultMeals(n) {
  n = Math.max(1, Math.min(8, n || 4));
  return MEAL_PRIORITY.slice(0, n).map((m) => ({ meal: m, items: [], subtotal: {} }));
}
async function openJornada(k) {
  if (!currentUserId) { alert("Crea un perfil primero."); return; }
  await ensureRequirements();
  // Carga una minuta ya guardada para ese día, si existe.
  const saved = (plansByDate[k] || [])[0];
  if (saved) {
    const meals = (saved.payload.meals || []).map((m) => ({ meal: m.meal, items: m.items || [], subtotal: m.subtotal || mealSubtotal(m.items || []) }));
    jornada = { date: k, meals: meals.length ? meals : defaultMeals(currentUserObj.meals_per_day), planId: saved.id };
  } else {
    // Jornada nueva: comidas por defecto, precargando rutinas que calcen con el día.
    const wd = weekdayOf(k);
    const meals = defaultMeals(currentUserObj.meals_per_day);
    // Una sola rutina por nombre de comida (si hay varias para el mismo día,
    // gana la última) para no crear comidas duplicadas.
    const routineByMeal = {};
    routinesAll.filter((r) => presetMatches(r.preset, wd)).forEach((r) => { routineByMeal[r.meal] = r; });
    Object.values(routineByMeal).forEach((r) => {
      // Calza con un slot del mismo nombre (vacío de preferencia); si no existe,
      // anexa la comida para que la rutina siempre aparezca en su día.
      let slot = meals.find((m) => m.meal === r.meal && !m.items.length) || meals.find((m) => m.meal === r.meal);
      if (!slot) { slot = { meal: r.meal, items: [], subtotal: {} }; meals.push(slot); }
      slot.items = r.items || []; slot.subtotal = r.subtotal || mealSubtotal(slot.items); slot.fromRoutine = true;
    });
    jornada = { date: k, meals };
  }
  renderJornada();
}
function jornadaTotals() {
  const t = { kcal: 0, protein_g: 0, carb_g: 0, fat_g: 0, cost_clp: 0, satiety: 0 };
  jornada.meals.forEach((m) => { const s = m.subtotal && m.subtotal.kcal != null ? m.subtotal : mealSubtotal(m.items); ["kcal","protein_g","carb_g","fat_g","cost_clp","satiety"].forEach((k) => (t[k] += s[k] || 0)); });
  for (const k in t) t[k] = Math.round(t[k] * 10) / 10;
  return t;
}
// kind = "ceiling" (kcal, dinero: importa el margen para NO pasarse) |
//        "goal"    (proteína, macros: importa CUMPLIR el requerimiento).
// El enfoque premia alcanzar metas, no la restricción a ciegas.
function marginRow(label, used, target, unit, kind) {
  if (!target) {   // sin objetivo cargado (requerimientos no disponibles): estado neutro
    return `<div class="margin"><div class="m-top"><span>${label}</span><strong>${num(used)}${unit}</strong></div>
      <div class="m-bar"><i style="width:0%"></i></div><div class="m-left">sin objetivo</div></div>`;
  }
  const left = Math.round((target - used) * 10) / 10;
  const pct = target ? Math.min(100, Math.round(used / target * 100)) : 0;
  let cls = "", note = "";
  if (kind === "ceiling") {
    cls = used > target * 1.05 ? "over" : "";
    note = left >= 0 ? `margen ${num(left)}${unit}` : `excede ${num(-left)}${unit}`;
  } else {
    const reached = target && used >= target * 0.97;
    cls = reached ? "reached" : "";
    note = reached ? "✓ meta lograda" : `faltan ${num(Math.max(0, left))}${unit}`;
  }
  return `<div class="margin ${cls}">
    <div class="m-top"><span>${label}<span class="m-tag">${kind === "ceiling" ? "tope" : "meta"}</span></span><strong>${num(used)} / ${num(target)}${unit}</strong></div>
    <div class="m-bar"><i style="width:${pct}%"></i></div>
    <div class="m-left">${note}</div>
  </div>`;
}
function renderJornada() {
  const k = jornada.date, wd = weekdayOf(k);
  const t = jornadaTotals(), req = reqCache || {};
  const budget = dailyBudget();
  let html = `<div class="jornada">
    <div class="j-head"><h3>${DOW_FULL[wd]} · ${k}</h3>
      <div class="j-count">Comidas: <button class="stepbtn" id="mealMinus">−</button>
        <strong>${jornada.meals.length}</strong>
        <button class="stepbtn" id="mealPlus">＋</button></div></div>
    <div class="margins">
      ${marginRow("Energía", t.kcal, req.kcal || 0, " kcal", "ceiling")}
      ${marginRow("Proteína", t.protein_g, req.protein_g || 0, " g", "goal")}
      ${marginRow("Carbohidratos", t.carb_g, req.carb_g || 0, " g", "goal")}
      ${marginRow("Grasa", t.fat_g, req.fat_g || 0, " g", "goal")}
      ${budget ? marginRow("Costo", t.cost_clp, budget, " $", "ceiling") : ""}
    </div>
    <div class="j-meals">`;
  jornada.meals.forEach((m, idx) => {
    const st = m.subtotal && m.subtotal.kcal != null ? m.subtotal : mealSubtotal(m.items);
    const built = m.items.length;
    html += `<div class="jmeal ${built ? "built" : "empty"}">
      <div class="jm-head"><strong style="text-transform:capitalize">${m.meal}</strong>
        ${m.fromRoutine ? '<span class="tag">rutina</span>' : ""}
        <button class="btn-sm" data-rmmeal="${idx}" title="Quitar comida">✕</button></div>
      ${built
        ? `<div class="muted">${m.items.map((i) => `${i.name} (${num(i.grams)}g)`).join(" · ")}</div>
           <div class="jm-sub">${num(st.kcal)} kcal · P${num(st.protein_g)} · ${clp(st.cost_clp)}</div>`
        : `<div class="muted">Vacío — toca «Armar» para elegir alimentos.</div>`}
      <div class="jm-actions">
        <button class="btn-sm primary" data-build="${idx}">${built ? "Rearmar" : "Armar"}</button>
        ${built ? `<select class="presetsel" data-fix="${idx}">
            <option value="">Fijar rutina…</option>
            <option value="L-V">Lunes a viernes</option>
            <option value="finde">Fin de semana</option>
            <option value="todos">Todos los días</option>
          </select>` : ""}
      </div></div>`;
  });
  html += `</div>
    <div class="j-foot">
      <button class="primary" id="saveJornada">Guardar jornada</button>
      <button class="btn-sm" id="jGuided">Armar paso a paso</button>
      <span id="jornadaStatus" class="status"></span>
    </div></div>`;
  openOverlay("Jornada", html);
  $("#jGuided").addEventListener("click", () => openGuided(jornada.date));
  $("#mealMinus").addEventListener("click", () => { if (jornada.meals.length > 1) { jornada.meals.pop(); renderJornada(); } });
  $("#mealPlus").addEventListener("click", () => {
    const used = new Set(jornada.meals.map((m) => m.meal));
    let next = MEAL_PRIORITY.find((m) => !used.has(m));
    if (!next) { let n = jornada.meals.length + 1; while (used.has(`comida ${n}`)) n++; next = `comida ${n}`; }
    jornada.meals.push({ meal: next, items: [], subtotal: {} }); renderJornada();
  });
  $$("[data-rmmeal]").forEach((b) => b.addEventListener("click", () => { jornada.meals.splice(+b.dataset.rmmeal, 1); if (!jornada.meals.length) jornada.meals.push({ meal: "comida 1", items: [], subtotal: {} }); renderJornada(); }));
  $$("[data-build]").forEach((b) => b.addEventListener("click", () => buildJornadaMeal(+b.dataset.build)));
  $$(".presetsel[data-fix]").forEach((sel) => sel.addEventListener("change", () => { if (sel.value) fixRoutine(+sel.dataset.fix, sel.value); }));
  $("#saveJornada").addEventListener("click", saveJornada);
}
async function buildJornadaMeal(idx) {
  const m = jornada.meals[idx];
  openMealBuilder(m.meal, (items) => {
    m.items = items; m.subtotal = mealSubtotal(items); m.fromRoutine = false;
    renderJornada();
  });
}
async function fixRoutine(idx, preset) {
  const m = jornada.meals[idx];
  if (!m.items.length) return;
  await api("/api/routines", { method: "POST", body: JSON.stringify({
    user_id: currentUserId, meal: m.meal, preset, title: m.meal,
    items: m.items, subtotal: m.subtotal && m.subtotal.kcal != null ? m.subtotal : mealSubtotal(m.items),
  }) });
  m.fromRoutine = true;
  await loadCalendarData();
  renderJornada();
  setStatus("#jornadaStatus", `✓ ${m.meal} quedó como rutina (${preset}).`, "ok");
}
async function saveJornada() {
  const meals = jornada.meals.filter((m) => m.items.length)
    .map((m) => ({ meal: m.meal, items: m.items, subtotal: m.subtotal && m.subtotal.kcal != null ? m.subtotal : mealSubtotal(m.items) }));
  if (!meals.length) { $("#jornadaStatus").textContent = "Arma al menos una comida."; return; }
  const totals = jornadaTotals();
  const title = `Jornada ${jornada.date}`;
  const oldId = jornada.planId;
  try {
    // Crea primero; solo si el guardado nuevo tuvo éxito borra la jornada anterior.
    const created = await api("/api/plans", { method: "POST", body: JSON.stringify({
      user_id: currentUserId, title, scope: "diario", payload: { date: jornada.date, meals, totals } }) });
    jornada.planId = created.id;
    // Reemplazo: borra la jornada anterior. Si el borrado falla, la nueva ya
    // quedó guardada (el calendario abre la más reciente); avisamos sin romper.
    if (oldId) await api(`/api/plans/${oldId}`, { method: "DELETE" }).catch((e) => console.warn("No se pudo borrar la jornada anterior:", e));
  } catch (e) {
    setStatus("#jornadaStatus", "Error al guardar: " + e.message, "err");
    return;
  }
  setStatus("#jornadaStatus", "✓ Jornada guardada.", "ok");
  await loadCalendarData(); renderCalendarGrid();
  setTimeout(closeOverlay, 600);
}

// ---- Constructor de una comida en overlay secundario (ruleta) ----
async function openMealBuilder(meal, onConfirm) {
  openOverlay2("Armar: " + meal, '<p class="muted" id="mb2Status">Cargando carretes…</p><div id="mb2Reels" class="reels"></div>');
  let ctrl;
  try { ctrl = makeReelCtrl(await fetchBuilder(meal)); }
  catch (e) { $("#mb2Status").textContent = "Error: " + e.message; return; }
  $("#overlay2Body").innerHTML = `
    <div class="reels" id="mb2Reels"></div>
    <div class="mb2-control">
      <svg class="donut" id="mb2Donut" viewBox="0 0 42 42"></svg>
      <div class="mb2-info"><div class="pricebox" id="mb2Price">$0</div><div id="mb2Macros" class="platos"></div></div>
    </div>
    <div class="mb2-actions">
      <button class="bigbtn spin" id="mb2Spin" title="Girar">&#8634;</button>
      <button class="primary" id="mb2Confirm">✓ Confirmar comida</button>
    </div>`;
  const upd = () => {
    const t = ctrl.totals();
    renderDonut($("#mb2Donut"), t, ctrl.data.target);
    $("#mb2Price").textContent = clp(t.cost_clp);
    const pcal = t.protein_g * 4, ccal = t.carb_g * 4, gcal = t.fat_g * 9, tot = pcal + ccal + gcal || 1;
    $("#mb2Macros").innerHTML = `<span style="color:var(--p)">P ${Math.round(pcal/tot*100)}%</span> · <span style="color:var(--c)">C ${Math.round(ccal/tot*100)}%</span> · <span style="color:var(--g)">G ${Math.round(gcal/tot*100)}%</span>`;
  };
  ctrl.render($("#mb2Reels"), upd);
  $("#mb2Spin").addEventListener("click", () => { ctrl.spin(); ctrl.render($("#mb2Reels"), upd); });
  $("#mb2Confirm").addEventListener("click", () => {
    const items = ctrl.selection();
    if (!items.length) { return; }
    closeOverlay2();
    onConfirm(items);
  });
}

// ===================== ASISTENTE GUIADO (jornada paso a paso) =====================
// Estandariza el armado: el día se construye macro por macro a lo largo de todas
// las comidas. La app pone el orden mental; el usuario solo elige sobre sugerencias.
const GUIDED_STEPS = [
  { role: "proteina", label: "Proteínas", help: "La base de proteína de cada comida del día.", sort: "protein" },
  { role: "carbohidrato", label: "Carbohidratos", help: "El acompañamiento de cada comida, con la proteína ya elegida.", sort: "carb" },
  { role: "vegetal", label: "Verduras y vitaminas", help: "Plato redondo: ordenadas por fibra y micronutrientes.", sort: "micro" },
  { role: "aderezo", label: "Aderezos", help: "El toque final, según tus preferencias.", sort: "pref" },
];
let guided = null;        // { date, meals:[{meal}], stepIdx }
let guidedSlots = {};     // meal -> slots del constructor (cache)
let guidedCtrls = {};     // mealIdx -> controlador del reel del paso actual
let guidedPicks = {};     // mealIdx -> { role -> item escalado elegido }

async function openGuided(date) {
  if (!currentUserId) { alert("Crea un perfil primero."); return; }
  await ensureRequirements();
  const meals = defaultMeals(currentUserObj.meals_per_day).map((m) => ({ meal: m.meal }));
  guided = { date: date || null, meals, stepIdx: 0 };
  guidedPicks = {}; guidedSlots = {};
  meals.forEach((_, i) => (guidedPicks[i] = {}));
  openOverlay("Armar paso a paso", '<p class="muted" id="guidedLoading">Cargando alimentos…</p>');
  for (const m of meals) {
    if (!guidedSlots[m.meal]) {
      try { guidedSlots[m.meal] = await fetchBuilder(m.meal); }
      catch (e) { guidedSlots[m.meal] = { slots: [] }; }
    }
  }
  renderGuided();
}

function guidedDayTotals() {
  const t = { kcal: 0, protein_g: 0, carb_g: 0, fat_g: 0, cost_clp: 0 };
  Object.values(guidedPicks).forEach((picks) => Object.values(picks).forEach((it) => { if (it) for (const k in t) t[k] += it[k] || 0; }));
  for (const k in t) t[k] = Math.round(t[k] * 10) / 10;
  return t;
}
function guidedMarginsHtml() {
  const t = guidedDayTotals(), req = reqCache || {}, budget = dailyBudget();
  return marginRow("Energía", t.kcal, req.kcal || 0, " kcal", "ceiling")
    + marginRow("Proteína", t.protein_g, req.protein_g || 0, " g", "goal")
    + marginRow("Carbohidratos", t.carb_g, req.carb_g || 0, " g", "goal")
    + (budget ? marginRow("Costo", t.cost_clp, budget, " $", "ceiling") : "");
}
function updateGuidedMargins() {
  const box = document.querySelector(".guided .margins");
  if (box) box.innerHTML = guidedMarginsHtml();
}

function renderGuided() {
  const step = GUIDED_STEPS[guided.stepIdx];
  guidedCtrls = {};
  const html = `<div class="guided">
    <div class="g-steps">${GUIDED_STEPS.map((s, i) => `<span class="g-dot ${i === guided.stepIdx ? "on" : ""} ${i < guided.stepIdx ? "done" : ""}"></span>`).join("")}</div>
    <h3>Paso ${guided.stepIdx + 1}/4 · ${step.label}</h3>
    <p class="hint">${step.help}</p>
    <div class="margins">${guidedMarginsHtml()}</div>
    <div id="gMeals"></div>
    <div class="g-foot">
      ${guided.stepIdx > 0 ? `<button class="btn-sm" id="gBack">‹ Atrás</button>` : ""}
      <button class="primary" id="gNext">${guided.stepIdx < GUIDED_STEPS.length - 1 ? "Siguiente ›" : "Terminar y guardar"}</button>
      <span id="gStatus" class="status"></span>
    </div></div>`;
  openOverlay("Armar paso a paso", html);
  const cont = $("#gMeals");
  guided.meals.forEach((m, idx) => {
    const data = guidedSlots[m.meal] || { slots: [] };
    const slot = (data.slots || []).find((s) => s.role === step.role);
    const card = document.createElement("div");
    card.className = "g-meal";
    card.innerHTML = `<div class="g-meal-head"><strong style="text-transform:capitalize">${m.meal}</strong></div>`;
    cont.appendChild(card);
    if (!slot || !slot.candidates.length) {
      guidedPicks[idx][step.role] = null;
      card.insertAdjacentHTML("beforeend", `<div class="muted" style="padding:4px 2px">— esta comida no lleva ${step.label.toLowerCase()} —</div>`);
      return;
    }
    const host = document.createElement("div"); host.className = "reels";
    card.appendChild(host);
    const ctrl = makeReelCtrl({ meal: m.meal, target: data.target, slots: [slot] });
    ctrl.st.sort[step.role] = step.sort;   // orden sugerido del paso
    const prev = guidedPicks[idx][step.role];
    let restored = false;
    if (prev) {                            // restaura elección previa (al ir/volver)
      const arr = ctrl.sorted(slot);
      const i = arr.findIndex((c) => c.food_id === prev.food_id);
      if (i >= 0) {
        ctrl.st.idx[step.role] = i;
        if (prev._grams) { ctrl.st.grams[step.role] = prev._grams; ctrl.st.gramsFor[step.role] = prev.food_id; }
        restored = true;
      }
    }
    guidedCtrls[idx] = ctrl;
    ctrl.render(host, () => { const it = ctrl.selection()[0]; guidedPicks[idx][step.role] = it ? { ...it, _grams: it.grams } : null; updateGuidedMargins(); });
    if (prev && !restored) {
      guidedPicks[idx][step.role] = prev;  // no se pudo re-ubicar en el reel: NO perder la elección
    } else {
      const it0 = ctrl.selection()[0];     // elección inicial = la restaurada o la sugerencia del paso
      guidedPicks[idx][step.role] = it0 ? { ...it0, _grams: it0.grams } : (prev || null);
    }
  });
  updateGuidedMargins();
  if ($("#gBack")) $("#gBack").addEventListener("click", () => { guided.stepIdx--; renderGuided(); });
  $("#gNext").addEventListener("click", guidedNext);
}

async function guidedNext() {
  if (guided.stepIdx < GUIDED_STEPS.length - 1) { guided.stepIdx++; renderGuided(); return; }
  const meals = guided.meals.map((m, idx) => {
    const items = Object.values(guidedPicks[idx]).filter(Boolean).map((it) => { const { _grams, ...rest } = it; return rest; });
    return { meal: m.meal, items, subtotal: mealSubtotal(items) };
  }).filter((m) => m.items.length);
  if (!meals.length) { $("#gStatus").textContent = "Elige al menos un alimento."; return; }
  const totals = { kcal: 0, protein_g: 0, carb_g: 0, fat_g: 0, cost_clp: 0, satiety: 0 };
  meals.forEach((m) => ["kcal", "protein_g", "carb_g", "fat_g", "cost_clp", "satiety"].forEach((k) => (totals[k] += m.subtotal[k])));
  for (const k in totals) totals[k] = Math.round(totals[k] * 10) / 10;
  const date = guided.date || fmtKey(new Date());
  try {
    await api("/api/plans", { method: "POST", body: JSON.stringify({ user_id: currentUserId, title: `Jornada ${date}`, scope: "diario", payload: { date, meals, totals } }) });
  } catch (e) { setStatus("#gStatus", "Error al guardar: " + e.message, "err"); return; }
  setStatus("#gStatus", "✓ Jornada guardada.", "ok");
  await loadCalendarData(); renderCalendarGrid();
  setTimeout(closeOverlay, 700);
}

// ===================== MENÚ + OVERLAY =====================
function openDrawer() { $("#drawer").classList.add("open"); $("#drawerBackdrop").classList.add("open"); }
function closeDrawer() { $("#drawer").classList.remove("open"); $("#drawerBackdrop").classList.remove("open"); }
$("#menuBtn").addEventListener("click", openDrawer);
$("#drawerBackdrop").addEventListener("click", closeDrawer);
$("#drawerClose").addEventListener("click", closeDrawer);
function openOverlay(title, html) { $("#overlayTitle").textContent = title; $("#overlayBody").innerHTML = html; $("#overlay").classList.add("open"); }
function closeOverlay() { $("#overlay").classList.remove("open"); }
$("#overlayClose").addEventListener("click", closeOverlay);
function openOverlay2(title, html) { $("#overlay2Title").textContent = title; $("#overlay2Body").innerHTML = html; $("#overlay2").classList.add("open"); }
function closeOverlay2() { $("#overlay2").classList.remove("open"); }
$("#overlay2Close").addEventListener("click", closeOverlay2);
$$(".drawer-item[data-view]").forEach((b) => b.addEventListener("click", () => {
  closeDrawer();
  const v = b.dataset.view;
  if (v === "generar") openGenerar();
  if (v === "minutas") openMinutas();
  if (v === "catalogo") openCatalogo();
}));

// ---- Generar minuta + lista de compras (overlay) ----
function openGenerar() {
  openOverlay("Generar minuta", `
    <div class="controls">
      <label>Alcance <select id="scope"><option value="diario">Diario</option><option value="semanal">Semanal</option></select></label>
      <label>Énfasis saciedad: <strong id="satVal">0.0</strong><input id="satiety" type="range" min="0" max="1.5" step="0.1" value="0" /></label>
      <label>Presupuesto <select id="budgetMode">
        <option value="min_cost">Gastar lo mínimo</option><option value="target">Aprovechar el presupuesto</option><option value="none">Sin límite</option>
      </select></label>
      <label>Monto (CLP) <input id="budgetAmount" type="number" min="0" step="100" value="4000" /></label>
      <button id="generateBtn" class="primary">Generar</button>
    </div>
    <div id="genStatus" class="status"></div><div id="planResult"></div><div id="shoppingResult"></div>`);
  $("#satiety").addEventListener("input", (e) => ($("#satVal").textContent = (+e.target.value).toFixed(1)));
  $("#generateBtn").addEventListener("click", generatePlan);
}
async function generatePlan() {
  if (!currentUserId) return alert("Primero guarda un perfil.");
  $("#genStatus").textContent = "Optimizando…"; $("#planResult").innerHTML = ""; $("#shoppingResult").innerHTML = "";
  try {
    const mode = $("#budgetMode").value;
    const res = await api("/api/plans/generate", { method: "POST", body: JSON.stringify({
      user_id: currentUserId, scope: $("#scope").value, satiety_emphasis: +$("#satiety").value,
      budget_mode: mode, budget_clp: mode === "none" ? null : +$("#budgetAmount").value }) });
    $("#genStatus").textContent = "";
    res.scope === "semanal" ? renderWeekly(res.data) : renderDaily(res.data);
  } catch (err) { setStatus("#genStatus", "Error: " + err.message, "err"); }
}
function totalsBar(t, req) {
  const pill = (l, v, tg, u) => `<div class="pill">${l}: <strong>${num(v)}${u}</strong> <span class="muted">/ ${num(tg)}${u}</span></div>`;
  return `<div class="totbar"><div class="pill">Costo: <strong>${clp(t.cost_clp)}</strong></div>
    ${pill("Energía", t.kcal, req?.kcal, " kcal")}${pill("Prot", t.protein_g, req?.protein_g, " g")}
    ${pill("Carbs", t.carb_g, req?.carb_g, " g")}${pill("Grasa", t.fat_g, req?.fat_g, " g")}</div>`;
}
function mealBlock(m) {
  return `<div class="meal"><h3 style="text-transform:capitalize">${m.meal}
    <span>${num(m.subtotal.kcal)} kcal · ${clp(m.subtotal.cost_clp)}</span></h3>
    <div class="muted">${m.items.map((i) => `${i.name} ${num(i.grams)}g <span class="shop">${i.retailer || "—"}</span>`).join("<br>") || "—"}</div></div>`;
}
let lastPlanPayload = null;
function renderDaily(data) {
  lastPlanPayload = data;
  let html = (data.warnings || []).length ? `<div class="warnbox">⚠ ${data.warnings.join(" ")}</div>` : "";
  html += totalsBar(data.totals, data.requirements) + data.meals.map(mealBlock).join("");
  html += `<button id="savePlanBtn" class="primary">Guardar</button> <button id="shopBtn" class="btn-sm">Lista de compras</button>`;
  $("#planResult").innerHTML = html;
  $("#savePlanBtn").addEventListener("click", () => savePlan("diario", data));
  $("#shopBtn").addEventListener("click", () => showShoppingForPayload(data));
}
function renderWeekly(data) {
  lastPlanPayload = data;
  let html = `<div class="totbar"><div class="pill">Semanal: <strong>${clp(data.weekly_cost_clp)}</strong></div>
    <div class="pill">Prom/día: <strong>${clp(data.avg_daily_cost_clp)}</strong></div></div>`;
  data.days.forEach((d) => {
    html += `<div class="meal"><h3 style="text-transform:capitalize">${d.day} · ${clp(d.plan.totals.cost_clp)}</h3>
      <div class="muted">${d.plan.meals.map((m) => `<strong style="text-transform:capitalize">${m.meal}</strong>: ${m.items.map((i) => i.name).join(", ") || "—"}`).join("<br>")}</div></div>`;
  });
  html += `<button id="savePlanBtn" class="primary">Guardar</button> <button id="shopBtn" class="btn-sm">Lista</button>`;
  $("#planResult").innerHTML = html;
  $("#savePlanBtn").addEventListener("click", () => savePlan("semanal", data));
  $("#shopBtn").addEventListener("click", () => showShoppingForPayload(data));
}
async function savePlan(scope, payload) {
  if (scope === "semanal") return saveWeekly(payload);
  const title = prompt("Nombre de la minuta:", "Día " + new Date().toLocaleDateString("es-CL"));
  if (title === null) return;
  await api("/api/plans", { method: "POST", body: JSON.stringify({ user_id: currentUserId, title, scope, payload }) });
  setStatus("#genStatus", "✓ Guardada (mírala en el Calendario).", "ok");
}
// Expande una minuta semanal en 7 jornadas diarias consecutivas desde hoy, cada
// una con su fecha (payload.date), para que se vean y se abran en el calendario.
async function saveWeekly(payload) {
  const days = payload.days || [];
  if (!days.length) { $("#genStatus").textContent = "Semana vacía."; return; }
  const start = new Date(); start.setHours(12, 0, 0, 0);
  try {
    for (let i = 0; i < days.length; i++) {
      const dt = new Date(start); dt.setDate(start.getDate() + i);
      const k = fmtKey(dt);
      const dp = days[i].plan || {};
      const meals = (dp.meals || []).map((m) => ({ meal: m.meal, items: m.items, subtotal: m.subtotal }));
      await api("/api/plans", { method: "POST", body: JSON.stringify({
        user_id: currentUserId, title: `${DOW_FULL[weekdayOf(k)]} ${k}`, scope: "diario",
        payload: { date: k, meals, totals: dp.totals || {} } }) });
    }
  } catch (e) { $("#genStatus").textContent = "Error al guardar la semana: " + e.message; return; }
  const endDt = new Date(start); endDt.setDate(start.getDate() + days.length - 1);
  $("#genStatus").textContent = `✓ Semana guardada (${fmtKey(start)} → ${fmtKey(endDt)}). Cada día está en el calendario.`;
  await loadCalendarData();
}
function shoppingHtml(d) {
  if (!d.retailers || !d.retailers.length) return '<p class="muted">Sin productos.</p>';
  let html = `<div class="totbar"><div class="pill">Envases: <strong>${clp(d.total_packages_clp)}</strong></div>
    <div class="pill">Neto: <strong>${clp(d.total_consumed_clp)}</strong></div><div class="pill">${d.retailer_count} cadena(s)</div></div>`;
  for (const r of d.retailers) {
    const rows = r.items.map((i) => `<tr><td>${i.name}</td><td class="num">${num(i.needed_g)} g</td>
      <td class="num">${i.packages != null ? `${i.packages}×${num(i.package_g)}g` : "—"}</td>
      <td class="num">${i.packages_cost_clp != null ? clp(i.packages_cost_clp) : "—"}</td></tr>`).join("");
    html += `<div class="meal"><h3>${r.retailer} <span>${clp(r.subtotal_packages_clp)}</span></h3>
      <table><thead><tr><th>Producto</th><th class="num">Necesario</th><th class="num">Comprar</th><th class="num">Costo</th></tr></thead><tbody>${rows}</tbody></table></div>`;
  }
  return html;
}
async function showShoppingForPayload(payload) {
  $("#shoppingResult").innerHTML = '<p class="muted">Consolidando…</p>';
  const data = await api("/api/plans/shopping-list", { method: "POST", body: JSON.stringify({ payload }) });
  $("#shoppingResult").innerHTML = `<h3 style="margin-top:14px">Lista por cadena</h3>` + shoppingHtml(data);
}

// ---- Mis minutas y saciedad (overlay) ----
async function openMinutas() {
  if (!currentUserId) { openOverlay("Mis minutas", '<p class="hint">Crea un perfil primero.</p>'); return; }
  openOverlay("Mis minutas", '<div id="satietyHistory"></div><div id="plansList"></div>');
  loadSatietyHistory();
  const plans = await api(`/api/plans?user_id=${currentUserId}`);
  if (!plans.length) { $("#plansList").innerHTML = '<p class="muted">Aún no tienes minutas.</p>'; return; }
  $("#plansList").innerHTML = plans.map(planCard).join("");
  plans.forEach(wirePlanCard);
}
async function loadSatietyHistory() {
  if (!currentUserId) return;
  const h = await api(`/api/users/${currentUserId}/satiety-history`);
  if (!h.count) { $("#satietyHistory").innerHTML = '<p class="muted">Sin historial de saciedad aún.</p>'; return; }
  const bars = h.entries.map((e) => {
    const pct = (e.satiety_score / 5) * 100;
    const date = e.created_at ? new Date(e.created_at).toLocaleDateString("es-CL", { day: "2-digit", month: "2-digit" }) : "";
    return `<div class="histbar" title="${e.title}: ${e.satiety_score}/5"><div class="histfill" style="height:${pct}%"></div><div class="histlbl">${e.satiety_score}</div><div class="histdate">${date}</div></div>`;
  }).join("");
  $("#satietyHistory").innerHTML = `<div class="card" style="margin-bottom:14px"><div class="lbl">Saciedad</div>
    <div class="totbar"><div class="pill">Prom: <strong>${h.avg_satiety}/5</strong></div><div class="pill">Costo: <strong>${h.avg_cost_score}/5</strong></div></div>
    <div class="histchart">${bars}</div></div>`;
}
function planCard(p) {
  return `<div class="plan-card" id="plan-${p.id}"><div class="row">
    <div><strong>${p.title}</strong> <span class="muted">${p.scope} · ${clp(p.total_cost_clp)} · ${num(p.total_kcal)} kcal</span></div>
    <div><button class="btn-sm" data-act="shop" data-id="${p.id}">Lista</button>
      <button class="btn-sm" data-act="fb" data-id="${p.id}">Saciedad</button>
      <button class="btn-sm" data-act="del" data-id="${p.id}">✕</button></div></div>
    <div id="shop-${p.id}"></div>
    <div class="feedback-box" id="fb-${p.id}">
      <p class="muted">¿Qué tan saciado quedaste? (1–5)</p>
      <div class="stars" data-id="${p.id}">${[1,2,3,4,5].map((n) => `<span data-v="${n}">☆</span>`).join("")}</div>
      <label>Costo (1-5) <input type="number" min="1" max="5" value="3" id="cost-${p.id}" style="width:70px" /></label>
      <button class="primary" data-act="sendfb" data-id="${p.id}">Enviar</button>
      <span class="status" id="fbstatus-${p.id}"></span></div></div>`;
}
function wirePlanCard(p) {
  const card = $(`#plan-${p.id}`); let satiety = 3;
  card.querySelector('[data-act="fb"]').addEventListener("click", () => $(`#fb-${p.id}`).classList.toggle("open"));
  card.querySelector('[data-act="shop"]').addEventListener("click", async () => {
    const el = $(`#shop-${p.id}`); el.innerHTML = '<p class="muted">…</p>';
    el.innerHTML = shoppingHtml(await api(`/api/plans/${p.id}/shopping-list`));
  });
  card.querySelector('[data-act="del"]').addEventListener("click", async () => {
    if (confirm("¿Eliminar?")) { await api(`/api/plans/${p.id}`, { method: "DELETE" }); openMinutas(); }
  });
  const starWrap = card.querySelector(".stars");
  const paint = () => starWrap.querySelectorAll("span").forEach((s) => (s.textContent = +s.dataset.v <= satiety ? "★" : "☆"));
  starWrap.querySelectorAll("span").forEach((s) => s.addEventListener("click", () => { satiety = +s.dataset.v; paint(); }));
  paint();
  card.querySelector('[data-act="sendfb"]').addEventListener("click", async () => {
    const res = await api(`/api/plans/${p.id}/feedback`, { method: "POST", body: JSON.stringify({
      satiety_score: satiety, cost_score: +$(`#cost-${p.id}`).value, notes: "", food_ratings: {} }) });
    $(`#fbstatus-${p.id}`).textContent = `✓ ${Object.keys(res.updated_preferences).length} preferencias ajustadas`;
  });
}

// ---- Catálogo (overlay) ----
let foodsCache = [];
async function openCatalogo() {
  openOverlay("Catálogo", `<input id="foodSearch" type="text" placeholder="Buscar alimento, marca o categoría…" /><div id="foodsTable"></div>`);
  if (!foodsCache.length) foodsCache = await api("/api/foods");
  renderFoods(foodsCache);
  $("#foodSearch").addEventListener("input", (e) => {
    const q = e.target.value.toLowerCase();
    renderFoods(foodsCache.filter((f) => (f.name + f.brand + f.category).toLowerCase().includes(q)));
  });
}
function renderFoods(foods) {
  const rows = foods.map((f) => {
    const cmp = (f.prices || []).map((p) => `${p.retailer}: ${clp(p.price_per_100g)}`).join(" · ");
    return `<tr title="${cmp}"><td>${f.name} <span class="muted">${f.brand}</span></td><td>${f.category}</td>
      <td class="num">${num(f.kcal)}</td><td class="num"><strong>${clp(f.price_per_100g)}</strong><br><span class="shop">${f.retailer}</span></td></tr>`;
  }).join("");
  $("#foodsTable").innerHTML = `<table><thead><tr><th>Alimento</th><th>Cat.</th><th class="num">kcal/100g</th><th class="num">$/100g</th></tr></thead><tbody>${rows}</tbody></table>`;
}

// ===================== INIT =====================
const serverBtn = $("#serverBtn");
if (serverBtn) {
  if (location.protocol === "file:") serverBtn.addEventListener("click", setApiBase);
  else serverBtn.style.display = "none";
}
function startApp() {
  loadRetailers().then(loadUsers).then(() => showScreen(0)).catch((e) => {
    $("#welcomeSub").textContent = `No se pudo conectar al servidor${apiBase() ? " (" + apiBase() + ")" : ""}.`;
    console.error(e);
  });
}
if (location.protocol === "file:" && !apiBase()) {
  alert("Bienvenido a SCAVENGER.\n\nConfigura la URL del servidor para comenzar.");
  setApiBase();
} else { startApp(); }
