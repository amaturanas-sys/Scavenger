"""Pruebas del calculo de requerimientos nutricionales."""
from backend.nutrition import compute_requirements, mifflin_st_jeor


def test_mifflin_male_vs_female():
    m = mifflin_st_jeor("M", 80, 180, 30)
    f = mifflin_st_jeor("F", 80, 180, 30)
    # El hombre tiene TMB mayor (constante +5 vs -161).
    assert m > f
    assert abs(m - (10 * 80 + 6.25 * 180 - 5 * 30 + 5)) < 1e-6


def test_requirements_goal_affects_kcal():
    base = dict(sex="M", age=30, weight_kg=80, height_cm=180, activity_level="moderado")
    bajar = compute_requirements(goal="bajar", **base)
    mantener = compute_requirements(goal="mantener", **base)
    subir = compute_requirements(goal="subir", **base)
    assert bajar.kcal < mantener.kcal < subir.kcal
    # Deficit del 20% respecto al TDEE.
    assert abs(bajar.kcal - mantener.tdee * 0.8) < 1.0


def test_macros_sum_close_to_kcal():
    req = compute_requirements(sex="F", age=28, weight_kg=60, height_cm=165,
                               activity_level="ligero", goal="mantener")
    kcal_from_macros = req.protein_g * 4 + req.carb_g * 4 + req.fat_g * 9
    # Debe acercarse al objetivo calorico (tolerancia por redondeo).
    assert abs(kcal_from_macros - req.kcal) < 25
    assert req.protein_g > 0 and req.carb_g > 0 and req.fat_g > 0


def test_iron_higher_for_women():
    male = compute_requirements("M", 30, 80, 180, "moderado", "mantener")
    female = compute_requirements("F", 30, 80, 180, "moderado", "mantener")
    assert female.micros["iron_mg"] > male.micros["iron_mg"]
