"""Configuracion de SQLAlchemy: engine, sesion y base declarativa."""
from __future__ import annotations

from collections.abc import Generator

from sqlalchemy import create_engine, inspect, text
from sqlalchemy.orm import DeclarativeBase, Session, sessionmaker

from . import config

# check_same_thread solo aplica a SQLite; permite uso desde el server.
_is_sqlite = config.DATABASE_URL.startswith("sqlite")
_connect_args = {"check_same_thread": False} if _is_sqlite else {}

# pool_pre_ping evita conexiones muertas en Postgres administrado (Neon/Supabase,
# que cierran conexiones ociosas); pool_recycle las renueva periodicamente.
_engine_kwargs = {} if _is_sqlite else {"pool_pre_ping": True, "pool_recycle": 1800}

engine = create_engine(config.DATABASE_URL, connect_args=_connect_args, future=True, **_engine_kwargs)
SessionLocal = sessionmaker(bind=engine, autoflush=False, autocommit=False, future=True)


class Base(DeclarativeBase):
    """Base declarativa para todos los modelos ORM."""


def get_db() -> Generator[Session, None, None]:
    """Dependencia de FastAPI que entrega una sesion y la cierra al final."""
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


def _ensure_columns(eng=None) -> None:
    """Migracion ligera: agrega columnas nuevas a tablas ya existentes.

    create_all() crea tablas faltantes pero NO altera las existentes. Para que
    una BD ya poblada (produccion) gane columnas nuevas del modelo, aqui se
    comparan las columnas del modelo con las de la BD y se agregan las que
    falten (ALTER TABLE ADD COLUMN; soportado por SQLite y Postgres).
    """
    eng = eng or engine
    insp = inspect(eng)
    tables = set(insp.get_table_names())
    for table in Base.metadata.sorted_tables:
        if table.name not in tables:
            continue  # recien creada por create_all
        have = {c["name"] for c in insp.get_columns(table.name)}
        for col in table.columns:
            if col.name in have:
                continue
            ddl = col.type.compile(dialect=eng.dialect)
            default = ""
            arg = getattr(col.default, "arg", None) if col.default is not None else None
            if arg is not None and not callable(arg):
                default = f" DEFAULT {arg!r}" if isinstance(arg, str) else f" DEFAULT {arg}"
            try:
                with eng.begin() as conn:
                    conn.execute(text(f'ALTER TABLE {table.name} ADD COLUMN {col.name} {ddl}{default}'))
            except Exception as exc:  # noqa: BLE001 - no romper el arranque
                print(f"[migracion] no se pudo agregar {table.name}.{col.name}: {exc}")


def init_db() -> None:
    """Crea las tablas si no existen y aplica migraciones ligeras."""
    # Importacion local para registrar los modelos en el metadata.
    from . import models  # noqa: F401

    Base.metadata.create_all(bind=engine)
    _ensure_columns()
