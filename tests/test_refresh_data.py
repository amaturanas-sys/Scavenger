"""Pruebas del orquestador resiliente de datos reales (sin red)."""
from backend.refresh_data import DataRefreshReport, run_refresh


class _Res:
    def __init__(self, s):
        self._s = s

    def summary(self):
        return self._s


def test_run_refresh_continues_after_a_retailer_fails():
    calls = []

    def fake_refresh(db, rid, **kw):
        calls.append(rid)
        if rid == "jumbo":
            raise RuntimeError("403 bloqueado")
        return _Res(f"{rid} ok")

    rep = run_refresh(None, ["jumbo", "lider"], enrich=False,
                      refresh_one=fake_refresh, log=lambda *_: None)
    assert calls == ["jumbo", "lider"]  # no aborto tras el fallo de jumbo
    assert rep.ok_retailers == 1 and rep.failed_retailers == 1
    assert rep.retailers["lider"] == "lider ok"
    assert "ERROR" in rep.retailers["jumbo"]
    assert rep.all_failed is False


def test_run_refresh_tolerates_systemexit_from_retailer():
    # refresh_retailer lanza SystemExit ante host bloqueado; no debe propagarse.
    def boom(db, rid, **kw):
        raise SystemExit("host bloqueado")

    rep = run_refresh(None, ["jumbo"], enrich=False,
                      refresh_one=boom, log=lambda *_: None)
    assert rep.all_failed is True


def test_run_refresh_skips_enrich_without_credentials():
    class _Prov:
        configured = False

    rep = run_refresh(None, [], enrich=True, enrich_provider=_Prov(),
                      refresh_one=lambda *a, **k: _Res("x"), log=lambda *_: None)
    assert "omitido" in rep.enrich


def test_run_refresh_runs_enrich_when_configured():
    class _Prov:
        configured = True

    seen = {}

    def fake_enrich(db, provider=None, only_missing=True, limit=None, log=None):
        seen["called"] = True
        seen["only_missing"] = only_missing
        return _Res("3 enriquecidos")

    rep = run_refresh(None, [], enrich=True, enrich_provider=_Prov(),
                      enrich_fn=fake_enrich, refresh_one=lambda *a, **k: _Res("x"),
                      log=lambda *_: None)
    assert seen.get("called") and seen["only_missing"] is True
    assert rep.enrich == "3 enriquecidos"


def test_report_summary_lists_each_retailer():
    rep = DataRefreshReport()
    rep.retailers = {"jumbo": "ok", "lider": "ERROR: x"}
    rep.ok_retailers, rep.failed_retailers = 1, 1
    s = rep.summary()
    assert "jumbo" in s and "lider" in s and "con error: 1" in s
