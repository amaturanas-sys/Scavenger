// SCAVENGER · frontend móvil (vanilla JS)
// Landing + carrusel (Home, Ruleta, Perfil) + vistas secundarias en overlay.
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

async function api(path, opts = {}) {
  const res = await fetch(apiBase() + path, { headers: { "Content-Type": "application/json" }, ...opts });
  if (!res.ok) throw new Error(`${res.status}: ${await res.text()}`);
  return res.status === 204 ? null : res.json();
}

let currentUserId = null;
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
    $("#welcomeSub").textContent = "Arma comidas, planifica tu semana y compra al mejor precio.";
  } else {
    $("#welcome").textContent = "Bienvenido a SCAVENGER 🦝";
    $("#welcomeSub").textContent = "Crea tu perfil para recibir recomendaciones a tu medida.";
  }
}
$$(".hub-btn").forEach((b) => b.addEventListener("click", () => {
  const a = b.dataset.act;
  if (a === "ruleta") showScreen(1);
  else if (a === "perfil") showScreen(2);
  else if (a === "calendario") openCalendar();
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
  currentUserId = null;
  $("#userForm").reset();
  $("#userStatus").textContent = "Nuevo perfil — completa tus datos y guarda.";
  showScreen(2);
  setTimeout(() => $("#userForm").name.focus(), 200);
}
$("#userSelect").addEventListener("change", async (e) => {
  currentUserId = parseInt(e.target.value);
  fillForm(await api(`/api/users/${currentUserId}`));
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
    diet_tags: f.diet_tags.value ? [f.diet_tags.value] : [],
    excluded_foods: [], preferred_retailers: selectedRetailers(),
  };
  const u = currentUserId
    ? await api(`/api/users/${currentUserId}`, { method: "PATCH", body: JSON.stringify(body) })
    : await api("/api/users", { method: "POST", body: JSON.stringify(body) });
  currentUserId = u.id;
  $("#userStatus").textContent = "✓ Guardado (#" + u.id + ")";
  await loadUsers();
  $("#userSelect").value = currentUserId;
  loadRequirements();
});

async function loadRequirements() {
  if (!currentUserId) return;
  const r = await api(`/api/users/${currentUserId}/requirements`);
  const u = await api(`/api/users/${currentUserId}`);
  $("#tKcal").textContent = num(r.kcal) + " kcal";
  $("#tBasal").textContent = num(r.bmr) + " kcal";
  $("#tNutri").textContent = `P ${num(r.protein_g)} · C ${num(r.carb_g)} · G ${num(r.fat_g)}`;
  $("#tEco").textContent = clp((u.daily_budget_clp || 0) * 30);
}

// ===================== CALENDARIO (overlay) =====================
let calRef = null, plansByDate = {};
const MONTHS = ["enero","febrero","marzo","abril","mayo","junio","julio","agosto","septiembre","octubre","noviembre","diciembre"];
const DOW = ["L","M","M","J","V","S","D"];
const dateKey = (d) => d.slice(0, 10);

function openCalendar() {
  openOverlay("Calendario", `
    <div class="cal-head"><button class="iconbtn" id="calPrev">&#10094;</button><h2 id="calTitle">—</h2><button class="iconbtn" id="calNext">&#10095;</button></div>
    <div class="cal-grid" id="calGrid"></div>
    <div class="cal-day" id="calDay"><p class="hint">Toca un día para ver sus minutas.</p></div>`);
  if (!calRef) { const n = new Date(); calRef = new Date(n.getFullYear(), n.getMonth(), 1); }
  $("#calPrev").addEventListener("click", () => { calRef.setMonth(calRef.getMonth() - 1); renderCalendar(); });
  $("#calNext").addEventListener("click", () => { calRef.setMonth(calRef.getMonth() + 1); renderCalendar(); });
  loadCalendarData().then(renderCalendar);
}
async function loadCalendarData() {
  plansByDate = {};
  if (!currentUserId) return;
  const plans = await api(`/api/plans?user_id=${currentUserId}`);
  plans.forEach((p) => {
    const k = p.created_at ? dateKey(p.created_at) : null;
    if (k) (plansByDate[k] = plansByDate[k] || []).push(p);
  });
}
function renderCalendar() {
  if (!$("#calGrid")) return;
  const y = calRef.getFullYear(), m = calRef.getMonth();
  $("#calTitle").textContent = `${MONTHS[m]} ${y}`;
  const startDow = (new Date(y, m, 1).getDay() + 6) % 7;
  const days = new Date(y, m + 1, 0).getDate();
  const todayK = dateKey(new Date().toISOString());
  let html = DOW.map((d) => `<div class="cal-dow">${d}</div>`).join("");
  for (let i = 0; i < startDow; i++) html += `<div class="cal-cell empty"></div>`;
  for (let d = 1; d <= days; d++) {
    const k = `${y}-${String(m + 1).padStart(2, "0")}-${String(d).padStart(2, "0")}`;
    html += `<div class="cal-cell ${plansByDate[k] ? "has" : ""} ${k === todayK ? "today" : ""}" data-k="${k}">${d}</div>`;
  }
  $("#calGrid").innerHTML = html;
  $$("#calGrid .cal-cell[data-k]").forEach((c) => c.addEventListener("click", () => selectDay(c.dataset.k)));
}
function selectDay(k) {
  $$("#calGrid .cal-cell").forEach((c) => c.classList.toggle("sel", c.dataset.k === k));
  const plans = plansByDate[k] || [];
  if (!plans.length) { $("#calDay").innerHTML = `<p class="hint">Sin minutas el ${k}. Arma una en la Ruleta 🎰.</p>`; return; }
  $("#calDay").innerHTML = `<h3 style="font-size:15px">Minutas · ${k}</h3>` + plans.map((p) =>
    `<div class="daycard" data-plan="${p.id}"><div class="top"><strong>${p.title}</strong>
       <span class="muted">${num(p.total_kcal)} kcal · ${clp(p.total_cost_clp)}</span></div></div>`).join("");
  $$("#calDay .daycard").forEach((c) => c.addEventListener("click", () => openPlanDetail(+c.dataset.plan)));
}
async function openPlanDetail(id) {
  const p = await api(`/api/plans/${id}`);
  let html = `<button class="btn-sm" id="backCal">&#10094; Volver al calendario</button>
    <div class="totbar"><div class="pill">${p.scope}</div><div class="pill">${num(p.total_kcal)} kcal</div><div class="pill">${clp(p.total_cost_clp)}</div></div>`;
  (p.payload.meals || []).forEach((m) => {
    html += `<div class="meal"><h3 style="text-transform:capitalize">${m.meal}
      <span>${num((m.subtotal || {}).kcal)} kcal · ${clp((m.subtotal || {}).cost_clp)}</span></h3>
      <div class="muted">${(m.items || []).map((i) => `${i.name} (${num(i.grams)}g)`).join(" · ")}</div></div>`;
  });
  html += `<button class="btn-sm" id="delPlan">🗑️ Eliminar</button>`;
  openOverlay(p.title, html);
  $("#backCal").addEventListener("click", openCalendar);
  $("#delPlan").addEventListener("click", async () => {
    if (confirm("¿Eliminar esta minuta?")) { await api(`/api/plans/${id}`, { method: "DELETE" }); openCalendar(); }
  });
}

// ===================== RULETA =====================
let builderData = null, reelIdx = {}, reelLocked = {}, reelRemoved = {}, reelSort = {}, dayMeals = [];

async function loadReels() {
  if (!currentUserId) { $("#reels").innerHTML = '<p class="hint">Crea un perfil primero.</p>'; return; }
  $("#builderStatus").textContent = "Cargando…";
  try {
    builderData = await api("/api/builder/slots", {
      method: "POST", body: JSON.stringify({ user_id: currentUserId, meal: $("#builderMeal").value }),
    });
  } catch (e) { $("#builderStatus").textContent = "Error: " + e.message; return; }
  reelIdx = {}; reelLocked = {}; reelRemoved = {}; reelSort = {};
  builderData.slots.forEach((s) => { reelIdx[s.role] = 0; reelLocked[s.role] = false; reelRemoved[s.role] = false; reelSort[s.role] = "price"; });
  $("#builderStatus").textContent = "";
  renderReels();
}
function sortedCandidates(slot) {
  const arr = [...slot.candidates];
  if (reelSort[slot.role] === "kcal") arr.sort((a, b) => a.kcal - b.kcal);     // densidad calórica (kcal/porción) asc
  else arr.sort((a, b) => a.cost_clp - b.cost_clp);                            // precio ($/porción) asc
  return arr;
}
function reelSelection() {
  if (!builderData) return [];
  return builderData.slots
    .filter((s) => !reelRemoved[s.role] && s.candidates.length)
    .map((s) => { const arr = sortedCandidates(s); return arr[reelIdx[s.role] % arr.length]; });
}
function setSort(role, crit) {
  if (reelSort[role] === crit) return;
  const slot = builderData.slots.find((s) => s.role === role);
  const cur = sortedCandidates(slot)[reelIdx[role] % slot.candidates.length]; // recordar selección
  reelSort[role] = crit;
  const i = sortedCandidates(slot).findIndex((c) => c.food_id === cur.food_id);
  reelIdx[role] = i >= 0 ? i : 0;
  renderReels();
}
function renderReels() {
  if (!builderData) return;
  $("#reels").innerHTML = builderData.slots.map((s) => {
    const n = s.candidates.length;
    const removed = reelRemoved[s.role];
    const arr = sortedCandidates(s);
    const c = n ? arr[reelIdx[s.role] % n] : null;
    const win = removed
      ? `<div class="window removed">Grupo quitado<br><small>toca ＋ para incluir</small></div>`
      : (c
        ? `<div class="window">
             <div class="nm">${c.name}</div>
             ${c.brand ? `<div class="brand">${c.brand}</div>` : ""}
             <div class="gr">${num(c.grams)} g</div>
             <div class="mc">${num(c.kcal)} kcal · P${num(c.protein_g, 0)} C${num(c.carb_g, 0)} G${num(c.fat_g, 0)}</div>
             <span class="shop">${c.retailer}</span>
             ${c.platos_por_envase ? `<div class="platos">≈ ${c.platos_por_envase} platos/envase</div>` : ""}
             <div class="cost">${clp(c.cost_clp)}</div>
           </div>`
        : `<div class="window empty">— sin opciones —</div>`);
    const dis = n && !removed ? "" : "disabled";
    return `<div class="reel ${reelLocked[s.role] ? "locked" : ""} ${removed ? "removed" : ""}">
      <div class="role">${s.label}
        <button class="lockbtn" data-lock="${s.role}" ${dis}>${reelLocked[s.role] ? "🔒" : "🔓"}</button>
        <button class="rmbtn" data-rm="${s.role}" title="Quitar/incluir grupo">${removed ? "＋" : "✕"}</button>
      </div>
      <div class="sortrow">
        <button class="sortbtn ${reelSort[s.role] === "price" ? "on" : ""}" data-sort="price" data-role="${s.role}" ${dis}>$/porción</button>
        <button class="sortbtn ${reelSort[s.role] === "kcal" ? "on" : ""}" data-sort="kcal" data-role="${s.role}" ${dis}>kcal/porción</button>
      </div>
      <button class="tri up" data-dir="-1" data-role="${s.role}" ${dis}></button>
      ${win}
      <button class="tri down" data-dir="1" data-role="${s.role}" ${dis}></button>
      <span class="counter">${removed ? "—" : (n ? (reelIdx[s.role] % n) + 1 : 0) + "/" + n}</span>
    </div>`;
  }).join("");
  $$("#reels .tri").forEach((b) => b.addEventListener("click", () => {
    const role = b.dataset.role, n = builderData.slots.find((s) => s.role === role).candidates.length;
    if (!n) return;
    reelIdx[role] = (reelIdx[role] + +b.dataset.dir + n) % n;
    renderReels();
  }));
  $$("#reels .lockbtn").forEach((b) => b.addEventListener("click", () => { reelLocked[b.dataset.lock] = !reelLocked[b.dataset.lock]; renderReels(); }));
  $$("#reels .rmbtn").forEach((b) => b.addEventListener("click", () => { reelRemoved[b.dataset.rm] = !reelRemoved[b.dataset.rm]; renderReels(); }));
  $$("#reels .sortbtn").forEach((b) => b.addEventListener("click", () => setSort(b.dataset.role, b.dataset.sort)));
  renderControl();
}
function renderControl() {
  const sel = reelSelection();
  const t = { kcal: 0, protein_g: 0, carb_g: 0, fat_g: 0, cost_clp: 0 };
  sel.forEach((c) => { for (const k in t) t[k] += c[k] || 0; });
  renderDonut(t, builderData.target);
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
function renderDonut(t, target) {
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
  $("#donut").innerHTML =
    `<circle cx="21" cy="21" r="15.915" fill="none" stroke="#2a2a2d" stroke-width="6"/>${arcs}
     <text x="21" y="20.5" text-anchor="middle" font-size="6">${kcalPct}%</text>
     <text x="21" y="26" text-anchor="middle" font-size="3" fill="#b9b9c2">Kcal/d</text>`;
}
$("#builderMeal").addEventListener("change", loadReels);
$("#spinBtn").addEventListener("click", () => {
  if (!builderData) return;
  builderData.slots.forEach((s) => {
    if (s.candidates.length && !reelLocked[s.role] && !reelRemoved[s.role]) reelIdx[s.role] = Math.floor(Math.random() * s.candidates.length);
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
    <button id="saveDayBtn" class="primary">💾 Guardar minuta del día</button>
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

// ===================== MENÚ + OVERLAY =====================
function openDrawer() { $("#drawer").classList.add("open"); $("#drawerBackdrop").classList.add("open"); }
function closeDrawer() { $("#drawer").classList.remove("open"); $("#drawerBackdrop").classList.remove("open"); }
$("#menuBtn").addEventListener("click", openDrawer);
$("#drawerBackdrop").addEventListener("click", closeDrawer);
function openOverlay(title, html) { $("#overlayTitle").textContent = title; $("#overlayBody").innerHTML = html; $("#overlay").classList.add("open"); }
function closeOverlay() { $("#overlay").classList.remove("open"); }
$("#overlayClose").addEventListener("click", closeOverlay);
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
  } catch (err) { $("#genStatus").textContent = "Error: " + err.message; }
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
  html += `<button id="savePlanBtn" class="primary">Guardar</button> <button id="shopBtn" class="btn-sm">🛒 Lista de compras</button>`;
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
  html += `<button id="savePlanBtn" class="primary">Guardar</button> <button id="shopBtn" class="btn-sm">🛒 Lista</button>`;
  $("#planResult").innerHTML = html;
  $("#savePlanBtn").addEventListener("click", () => savePlan("semanal", data));
  $("#shopBtn").addEventListener("click", () => showShoppingForPayload(data));
}
async function savePlan(scope, payload) {
  const title = prompt("Nombre de la minuta:", scope === "semanal" ? "Semana" : "Día " + new Date().toLocaleDateString("es-CL"));
  if (title === null) return;
  const toSave = scope === "semanal" ? { ...payload, totals: { cost_clp: payload.weekly_cost_clp, kcal: payload.requirements.kcal, satiety: 0 } } : payload;
  await api("/api/plans", { method: "POST", body: JSON.stringify({ user_id: currentUserId, title, scope, payload: toSave }) });
  $("#genStatus").textContent = "✓ Guardada (mírala en el Calendario).";
}
function shoppingHtml(d) {
  if (!d.retailers || !d.retailers.length) return '<p class="muted">Sin productos.</p>';
  let html = `<div class="totbar"><div class="pill">Envases: <strong>${clp(d.total_packages_clp)}</strong></div>
    <div class="pill">Neto: <strong>${clp(d.total_consumed_clp)}</strong></div><div class="pill">${d.retailer_count} cadena(s)</div></div>`;
  for (const r of d.retailers) {
    const rows = r.items.map((i) => `<tr><td>${i.name}</td><td class="num">${num(i.needed_g)} g</td>
      <td class="num">${i.packages != null ? `${i.packages}×${num(i.package_g)}g` : "—"}</td>
      <td class="num">${i.packages_cost_clp != null ? clp(i.packages_cost_clp) : "—"}</td></tr>`).join("");
    html += `<div class="meal"><h3>🛒 ${r.retailer} <span>${clp(r.subtotal_packages_clp)}</span></h3>
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
    <div><button class="btn-sm" data-act="shop" data-id="${p.id}">🛒</button>
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
