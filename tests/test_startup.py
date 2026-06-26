"""Pruebas del arranque resiliente (reintentos de inicializacion de BD)."""
from backend.seed import run_with_retries


def test_run_with_retries_succeeds_after_failures():
    sleeps = []
    state = {"n": 0}

    def flaky():
        state["n"] += 1
        if state["n"] < 3:
            raise RuntimeError("BD no lista")
        return "ok"

    ok, res = run_with_retries(flaky, attempts=5, delay=0.01,
                               sleeper=sleeps.append, log=lambda *_: None)
    assert ok is True and res == "ok"
    assert state["n"] == 3
    assert len(sleeps) == 2  # durmio entre los 2 fallos previos al exito


def test_run_with_retries_gives_up_after_attempts():
    sleeps = []

    def always_fail():
        raise RuntimeError("sin BD")

    ok, err = run_with_retries(always_fail, attempts=4, delay=0.01,
                               sleeper=sleeps.append, log=lambda *_: None)
    assert ok is False
    assert isinstance(err, RuntimeError)
    assert len(sleeps) == 3  # no duerme tras el ultimo intento


def test_run_with_retries_first_try_no_sleep():
    sleeps = []
    ok, res = run_with_retries(lambda: 42, attempts=3, delay=1.0,
                               sleeper=sleeps.append, log=lambda *_: None)
    assert ok and res == 42 and sleeps == []


def test_init_and_seed_respects_seed_demo(monkeypatch):
    import backend.seed as s

    class _FakeSession:
        def close(self):
            pass

    calls = []
    monkeypatch.setattr(s, "init_db", lambda: None)
    monkeypatch.setattr(s, "SessionLocal", lambda: _FakeSession())
    monkeypatch.setattr(s, "seed_foods", lambda db, p, refresh=False: calls.append("foods"))
    monkeypatch.setattr(s, "seed_demo_user", lambda db: calls.append("demo"))

    s.init_and_seed("local", attempts=1, seed_demo=False, log=lambda *_: None)
    assert calls == ["foods"]  # sin usuario demo

    calls.clear()
    s.init_and_seed("local", attempts=1, seed_demo=True, log=lambda *_: None)
    assert calls == ["foods", "demo"]


def test_init_and_seed_propagates_refresh_flag(monkeypatch):
    import backend.seed as s

    class _FakeSession:
        def close(self):
            pass

    seen = {}
    monkeypatch.setattr(s, "init_db", lambda: None)
    monkeypatch.setattr(s, "SessionLocal", lambda: _FakeSession())
    monkeypatch.setattr(s, "seed_demo_user", lambda db: None)
    monkeypatch.setattr(s, "seed_foods",
                        lambda db, p, refresh=False: seen.__setitem__("refresh", refresh))

    s.init_and_seed("local", attempts=1, seed_demo=False, refresh=True, log=lambda *_: None)
    assert seen["refresh"] is True  # propaga refresh a la BD ya poblada

    s.init_and_seed("local", attempts=1, seed_demo=False, log=lambda *_: None)
    assert seen["refresh"] is False  # por defecto no refresca (respeta precios)
