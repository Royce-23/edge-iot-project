"""Persist validated device messages; shared by MQTT and authenticated HTTP."""

import time

from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from . import models
from .schemas import FeaturePayload, StatusPayload


def record_dict(record: models.FeatureRecord) -> dict:
    return {name: getattr(record, name) for name in (
        "id", "device_id", "boot_id", "sequence", "timestamp", "received_at",
        "uptime_ms", "sample_rate_hz", "sample_count", "rms", "peak_to_peak",
        "crest_factor", "dominant_frequency", "band_energy", "anomaly_score",
        "health_state", "temperature_c",
    )}


def event_dict(event: models.HealthEvent) -> dict:
    return {name: getattr(event, name) for name in (
        "id", "event_id", "device_id", "timestamp", "received_at", "type",
        "message", "acknowledged", "acknowledged_at",
    )}


def save_feature(db: Session, payload: FeaturePayload) -> tuple[models.FeatureRecord, models.HealthEvent | None, bool]:
    existing = db.query(models.FeatureRecord).filter_by(
        device_id=payload.device_id, boot_id=payload.boot_id,
        sequence=payload.sequence,
    ).first()
    if existing:
        return existing, None, True

    if db.get(models.Device, payload.device_id) is None:
        db.add(models.Device(device_id=payload.device_id, name=payload.device_id))
        db.flush()

    data = payload.model_dump(exclude={"schema_version"})
    record = models.FeatureRecord(**data, received_at=str(time.time()))
    db.add(record)
    event = None
    if payload.health_state in ("WARNING", "FAULT"):
        event = models.HealthEvent(
            event_id=f"{payload.device_id}_{payload.boot_id}_{payload.sequence}",
            device_id=payload.device_id,
            timestamp=payload.timestamp,
            received_at=record.received_at,
            type=payload.health_state,
            message=f"Thiết bị {payload.device_id}: {payload.health_state}",
            acknowledged=False,
        )
        db.add(event)
    try:
        db.commit()
    except IntegrityError:
        db.rollback()
        existing = db.query(models.FeatureRecord).filter_by(
            device_id=payload.device_id, boot_id=payload.boot_id,
            sequence=payload.sequence,
        ).first()
        if existing:
            return existing, None, True
        raise
    db.refresh(record)
    if event:
        db.refresh(event)
    return record, event, False


def save_status(db: Session, payload: StatusPayload) -> models.DeviceStatus:
    if db.get(models.Device, payload.device_id) is None:
        db.add(models.Device(device_id=payload.device_id, name=payload.device_id))
        db.flush()
    status = db.get(models.DeviceStatus, payload.device_id)
    if status is None:
        status = models.DeviceStatus(device_id=payload.device_id)
        db.add(status)
    status.online = payload.online
    status.last_seen = int(time.time())
    db.commit()
    db.refresh(status)
    return status
