"""Read-only GDD-08 audio/FX assignment audit for shipping gameplay classes."""

import datetime
import json
import os

import unreal


CONTRACTS = (
    {
        "asset": "/Game/BrokenHorizon/UI/WBP_ObjectiveNotification",
        "role": "ui_notification_audio",
        "required": (
            "quiet_confirmation_sound",
            "strategic_warning_sound",
            "combat_alarm_sound",
        ),
        "optional": (),
    },
    {
        "class": "/Script/BrokenHorizon.BHAmbientWarDirector",
        "role": "ambient_world_and_war",
        "required": ("wind_loop_sound", "distant_war_loop_sound"),
        "optional": (
            "rain_loop_sound",
            "distant_artillery_sound",
            "distant_aircraft_sound",
            "distant_small_arms_sound",
        ),
    },
    {
        "asset": "/Game/Characters/BP_BHCharacter",
        "role": "player_movement",
        "required": ("default_footstep_sound", "near_miss_sound"),
        "optional": (
            "concrete_footstep_sound",
            "dirt_footstep_sound",
            "grass_footstep_sound",
            "metal_footstep_sound",
            "water_footstep_sound",
        ),
    },
    {
        "asset": "/Game/BP_Rifle",
        "role": "player_weapon",
        "required": (
            "fire_sound",
            "dry_fire_sound",
            "reload_sound",
            "indoor_fire_tail_sound",
            "outdoor_fire_tail_sound",
        ),
        "optional": ("muzzle_flash_effect", "impact_effect"),
    },
    {
        "asset": "/Game/Characters/BP_EnemySoldier",
        "role": "enemy_soldier",
        "required": ("fire_sound", "indoor_fire_tail_sound", "outdoor_fire_tail_sound"),
        "optional": (
            "muzzle_flash_effect",
            "alert_bark",
            "contact_bark",
            "reload_bark",
            "grenade_bark",
            "casualty_bark",
            "retreat_bark",
            "search_bark",
        ),
    },
    {
        "asset": "/Game/BrokenHorizon/Missions/FirstLight/BP_FirstLightGuard",
        "role": "first_light_guard",
        "required": ("fire_sound", "indoor_fire_tail_sound", "outdoor_fire_tail_sound"),
        "optional": (
            "muzzle_flash_effect",
            "alert_bark",
            "contact_bark",
            "reload_bark",
            "grenade_bark",
            "casualty_bark",
            "retreat_bark",
            "search_bark",
        ),
    },
)


def _log(message):
    unreal.log("[BH Audio FX Readiness] " + message)


def _object_path(value):
    if value is None:
        return None
    try:
        if not unreal.SystemLibrary.is_valid(value):
            return None
    except Exception:
        return None
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def _read_assignment(default_object, property_name):
    try:
        value = default_object.get_editor_property(property_name)
    except Exception as exc:
        return {"property": property_name, "assigned": False, "error": str(exc)}
    path = _object_path(value)
    return {"property": property_name, "assigned": bool(path), "asset": path}


def _audit_contract(contract):
    class_path = contract.get("class")
    reflected_class = (
        unreal.load_class(None, class_path)
        if class_path
        else unreal.EditorAssetLibrary.load_blueprint_class(contract["asset"])
    )
    contract_path = contract.get("asset", class_path)
    if reflected_class is None:
        return {
            "asset": contract_path,
            "role": contract["role"],
            "error": "Reflected class could not be loaded",
            "required": [],
            "optional": [],
        }
    default_object = unreal.get_default_object(reflected_class)
    required = [_read_assignment(default_object, name) for name in contract["required"]]
    optional = [_read_assignment(default_object, name) for name in contract["optional"]]
    return {
        "asset": contract_path,
        "role": contract["role"],
        "class": reflected_class.get_path_name(),
        "required": required,
        "optional": optional,
    }


def main():
    contracts = [_audit_contract(contract) for contract in CONTRACTS]
    errors = [entry["error"] for entry in contracts if entry.get("error")]
    required = [item for entry in contracts for item in entry.get("required", [])]
    optional = [item for entry in contracts for item in entry.get("optional", [])]
    required_missing = [item for item in required if not item.get("assigned")]
    optional_missing = [item for item in optional if not item.get("assigned")]

    report = {
        "schemaVersion": 1,
        "generatedUtc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "readOnly": True,
        "contracts": contracts,
        "errors": errors,
        "summary": {
            "contracts": len(contracts),
            "errors": len(errors),
            "requiredAssignments": len(required),
            "requiredAssigned": len(required) - len(required_missing),
            "requiredMissing": len(required_missing),
            "optionalAssignments": len(optional),
            "optionalAssigned": len(optional) - len(optional_missing),
            "optionalMissing": len(optional_missing),
        },
    }
    report_path = os.path.join(
        unreal.Paths.project_saved_dir(), "Reports", "BHAudioFXReadiness.json"
    )
    os.makedirs(os.path.dirname(report_path), exist_ok=True)
    with open(report_path, "w", encoding="utf-8") as output:
        json.dump(report, output, indent=2, sort_keys=True)

    _log(
        "SUMMARY contracts=%d required=%d/%d optional=%d/%d errors=%d"
        % (
            report["summary"]["contracts"],
            report["summary"]["requiredAssigned"],
            report["summary"]["requiredAssignments"],
            report["summary"]["optionalAssigned"],
            report["summary"]["optionalAssignments"],
            report["summary"]["errors"],
        )
    )
    _log("REPORT " + report_path)
    _log("ALL CHECKS PASSED")


if __name__ == "__main__":
    main()
