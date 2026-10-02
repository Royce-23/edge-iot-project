"""Wire payloads accepted from the ESP32 and the MQTT broker."""

from typing import Literal

from pydantic import BaseModel, ConfigDict, Field


class FeaturePayload(BaseModel):
    model_config = ConfigDict(extra="forbid")
    schema_version: Literal[1]
    device_id: str = Field(pattern=r"^[A-Za-z0-9_-]{1,64}$")
    boot_id: str = Field(pattern=r"^[A-Za-z0-9_-]{1,64}$")
    sequence: int = Field(ge=0)
    timestamp: int | None = Field(default=None, ge=0)
    uptime_ms: int = Field(ge=0)
    sample_rate_hz: float = Field(gt=0, allow_inf_nan=False)
    sample_count: int = Field(ge=2)
    rms: float = Field(ge=0, allow_inf_nan=False)
    peak_to_peak: float = Field(ge=0, allow_inf_nan=False)
    crest_factor: float = Field(ge=0, allow_inf_nan=False)
    dominant_frequency: float = Field(ge=0, allow_inf_nan=False)
    band_energy: float = Field(ge=0, allow_inf_nan=False)
    anomaly_score: float | None = Field(default=None, ge=0, allow_inf_nan=False)
    health_state: Literal["OFF", "NORMAL", "WARNING", "FAULT"]
    temperature_c: float | None = Field(default=None, allow_inf_nan=False)


class StatusPayload(BaseModel):
    model_config = ConfigDict(extra="forbid")
    device_id: str = Field(pattern=r"^[A-Za-z0-9_-]{1,64}$")
    online: bool
    timestamp: int | None = Field(default=None, ge=0)
