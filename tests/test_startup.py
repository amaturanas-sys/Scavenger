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
