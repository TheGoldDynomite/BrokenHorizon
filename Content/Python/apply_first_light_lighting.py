"""Apply a non-destructive cold-dawn lighting baseline to First Light.

Run from Tools > Execute Python Script. The script refuses to alter the map
when generated or custom lighting already exists. To replace a previous
generated baseline, deliberately set REPLACE_GENERATED_LIGHTING to True.
Only actors carrying the generated lighting tag (or the two legacy generated
lights carrying the original graybox tag) are eligible for removal.
"""

import unreal


MAP_PATH = "/Game/BrokenHorizon/Maps/L_FirstLight_Graybox"
LIGHTING_TAG = "BH_Auto_FirstLight_Lighting"
LEGACY_GRAYBOX_TAG = "BH_Auto_FirstLight"
LIGHTING_FOLDER = "FirstLight/Lighting/Generated Baseline"

REPLACE_GENERATED_LIGHTING = False
ALLOW_ALONGSIDE_CUSTOM_LIGHTING = False
ENABLE_VOLUMETRIC_FOG = False

LEGACY_GENERATED_LABELS = {
    "FL_DirectionalLight",
    "FL_SkyLight",
}

WARM_LIGHT_LOCATIONS = (
    (4050.0, -300.0, 250.0),
    (4550.0, 300.0, 250.0),
    (5900.0, -780.0, 260.0),
    (7100.0, 780.0, 260.0),
)


def _log(message):
    unreal.log("[First Light Lighting] " + message)


def _warning(message):
    unreal.log_warning("[First Light Lighting] " + message)


def _set(target, property_name, value):
    """Set an editor property when supported, without aborting the baseline."""
    try:
        target.set_editor_property(property_name, value)
        return True
    except Exception as error:
        target_name = (
            target.get_name()
            if hasattr(target, "get_name")
            else type(target).__name__
        )
        _warning(
            "%s: could not set %s (%s)"
            % (target_name, property_name, error)
        )
        return False


def _tags(actor):
    return {str(tag) for tag in actor.get_editor_property("tags")}


def _is_generated_lighting(actor):
    tags = _tags(actor)
    if LIGHTING_TAG in tags:
        return True
    return (
        LEGACY_GRAYBOX_TAG in tags
        and actor.get_actor_label() in LEGACY_GENERATED_LABELS
    )


def _is_lighting_actor(actor):
    lighting_types = (
        unreal.DirectionalLight,
        unreal.SkyLight,
        unreal.SkyAtmosphere,
        unreal.ExponentialHeightFog,
        unreal.PostProcessVolume,
        unreal.PointLight,
        unreal.SpotLight,
        unreal.RectLight,
    )
    return isinstance(actor, lighting_types)


def _all_level_actors():
    subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    return list(subsystem.get_all_level_actors())


def _tag_and_label(actor, label):
    tags = list(actor.get_editor_property("tags"))
    if LIGHTING_TAG not in {str(tag) for tag in tags}:
        tags.append(unreal.Name(LIGHTING_TAG))
        actor.set_editor_property("tags", tags)
    actor.set_actor_label(label)
    actor.set_folder_path(unreal.Name(LIGHTING_FOLDER))
    return actor


def _spawn(actor_class, location, label, rotation=None):
    subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    actor = subsystem.spawn_actor_from_class(
        actor_class,
        unreal.Vector(*location),
        rotation if rotation else unreal.Rotator(0.0, 0.0, 0.0),
    )
    if not actor:
        raise RuntimeError("Could not spawn " + label)
    return _tag_and_label(actor, label)


def _component(actor, component_class):
    component = actor.get_component_by_class(component_class)
    if not component:
        raise RuntimeError(
            "%s has no %s"
            % (actor.get_actor_label(), component_class.__name__)
        )
    return component


def _remove_generated_lighting(actors):
    subsystem = unreal.get_editor_subsystem(
        unreal.EditorActorSubsystem
    )
    removed = 0
    for actor in actors:
        if _is_generated_lighting(actor):
            subsystem.destroy_actor(actor)
            removed += 1
    _log("Removed %d previously generated lighting actors." % removed)


def _configure_directional_light():
    actor = _spawn(
        unreal.DirectionalLight,
        (2500.0, 0.0, 1800.0),
        "FL_Lighting_ColdDawnSun",
        unreal.Rotator(-16.0, -32.0, 0.0),
    )
    component = _component(actor, unreal.DirectionalLightComponent)
    _set(component, "intensity", 3.0)
    _set(component, "light_color", unreal.Color(184, 205, 255, 255))
    _set(component, "use_temperature", True)
    _set(component, "temperature", 9000.0)
    _set(component, "atmosphere_sun_light", True)
    _set(component, "cast_shadows", True)
    return actor


def _configure_sky_light():
    actor = _spawn(
        unreal.SkyLight,
        (2500.0, 0.0, 900.0),
        "FL_Lighting_OvercastSky",
    )
    component = _component(actor, unreal.SkyLightComponent)
    _set(component, "intensity", 0.7)
    _set(component, "real_time_capture", True)
    _set(
        component,
        "lower_hemisphere_color",
        unreal.Color(22, 28, 36, 255),
    )
    return actor


def _configure_atmosphere():
    return _spawn(
        unreal.SkyAtmosphere,
        (2500.0, 0.0, 0.0),
        "FL_Lighting_SkyAtmosphere",
    )


def _configure_fog():
    actor = _spawn(
        unreal.ExponentialHeightFog,
        (4200.0, 0.0, 0.0),
        "FL_Lighting_RestrainedFog",
    )
    component = _component(
        actor, unreal.ExponentialHeightFogComponent
    )
    _set(component, "fog_density", 0.012)
    _set(component, "fog_height_falloff", 0.22)
    _set(
        component,
        "fog_inscattering_color",
        unreal.Color(110, 132, 155, 255),
    )
    _set(component, "start_distance", 250.0)
    _set(component, "volumetric_fog", ENABLE_VOLUMETRIC_FOG)
    return actor


def _configure_post_process():
    actor = _spawn(
        unreal.PostProcessVolume,
        (4200.0, 0.0, 300.0),
        "FL_Lighting_ColdDawnGrade",
    )
    _set(actor, "unbound", True)

    settings = actor.get_editor_property("settings")
    _set(settings, "override_auto_exposure_bias", True)
    _set(settings, "auto_exposure_bias", -0.35)
    _set(settings, "override_color_saturation", True)
    _set(
        settings,
        "color_saturation",
        unreal.Vector4(0.86, 0.90, 0.96, 1.0),
    )
    _set(settings, "override_color_contrast", True)
    _set(
        settings,
        "color_contrast",
        unreal.Vector4(1.04, 1.03, 1.02, 1.0),
    )
    _set(settings, "override_bloom_intensity", True)
    _set(settings, "bloom_intensity", 0.15)
    _set(actor, "settings", settings)
    return actor


def _configure_warm_security_lights():
    created = []
    for index, location in enumerate(WARM_LIGHT_LOCATIONS, 1):
        actor = _spawn(
            unreal.PointLight,
            location,
            "FL_Lighting_Security_%02d" % index,
        )
        component = _component(actor, unreal.PointLightComponent)
        _set(component, "intensity", 1600.0)
        _set(
            component,
            "light_color",
            unreal.Color(255, 112, 42, 255),
        )
        _set(component, "attenuation_radius", 650.0)
        _set(component, "cast_shadows", False)
        created.append(actor)
    return created


def apply_first_light_lighting():
    """Apply the baseline only after passing non-destructive safety checks."""
    if not unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        raise RuntimeError("First Light map does not exist: " + MAP_PATH)

    level_editor = unreal.get_editor_subsystem(
        unreal.LevelEditorSubsystem
    )
    if not level_editor.load_level(MAP_PATH):
        raise RuntimeError("Could not load First Light map: " + MAP_PATH)
    actors = _all_level_actors()
    generated = [
        actor for actor in actors if _is_generated_lighting(actor)
    ]
    custom = [
        actor
        for actor in actors
        if _is_lighting_actor(actor)
        and not _is_generated_lighting(actor)
    ]

    if generated and not REPLACE_GENERATED_LIGHTING:
        _warning(
            "Generated lighting already exists. Nothing was changed. "
            "Set REPLACE_GENERATED_LIGHTING = True to replace only "
            "tagged generated lighting."
        )
        return False

    if custom and not ALLOW_ALONGSIDE_CUSTOM_LIGHTING:
        labels = ", ".join(
            sorted(actor.get_actor_label() for actor in custom)
        )
        _warning(
            "Custom/untagged lighting exists (%s). Nothing was changed. "
            "Review it first; then deliberately set "
            "ALLOW_ALONGSIDE_CUSTOM_LIGHTING = True if coexistence is wanted."
            % labels
        )
        return False

    if generated:
        _remove_generated_lighting(generated)

    _configure_directional_light()
    _configure_sky_light()
    _configure_atmosphere()
    _configure_fog()
    _configure_post_process()
    warm_lights = _configure_warm_security_lights()

    level_editor.save_current_level()
    _log(
        "Saved cold-dawn baseline: sun, sky, atmosphere, fog, "
        "post process, and %d warm security lights."
        % len(warm_lights)
    )
    _log(
        "Graybox geometry and materials were not changed. Review exposure, "
        "route readability, and scalability in-editor."
    )
    return True


if __name__ == "__main__":
    apply_first_light_lighting()
