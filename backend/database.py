"""Configuracion de SQLAlchemy: engine, sesion y base declarativa."""
from __future__ import annotations

from collections.abc import Generator

from sqlalchemy import create_engine
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


def init_db() -> None:
    """Crea las tablas si no existen."""
    # Importacion local para registrar los modelos en el metadata.
    from . import models  # noqa: F401

    Base.metadata.create_all(bind=engine)
