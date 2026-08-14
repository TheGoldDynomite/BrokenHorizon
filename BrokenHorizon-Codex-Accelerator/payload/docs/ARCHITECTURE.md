# Broken Horizon — Architecture

## Evidence key

- **Verified:** confirmed in current source or runtime.
- **Provisional:** derived from prior project history; inspect before relying on it.
- **Planned:** not implemented unless current source proves otherwise.

## Provisional system map

```text
ABHCharacter [provisional]
├── first-person camera and movement
├── Enhanced Input forwarding
├── interaction trace and prompt integration
└── UBHObjectiveComponent ownership or access

IBHInteractable / UBHInteractable [provisional]
├── ABHDoor
├── keycard or pickup actors
└── future interactable actors

Objective UI [provisional]
├── UBHObjectiveWidget
└── UBHObjectiveNotificationWidget

Weapon foundation [planned until verified]
├── focused player/AI reusable weapon component
├── weapon actor or weapon instance
├── ammo and reload state
└── Blueprint presentation events
```

## Ownership principles

- Durable gameplay rules and state live in C++.
- Blueprint assigns assets, animation, audio, VFX, widget layout, and designer tuning.
- `ABHCharacter` forwards player intent but should not become the owner of every gameplay system.
- Components are preferred for reusable character capabilities when lifecycle and ownership fit.
- Interfaces and delegates reduce hard dependencies when they make data flow clearer.
- Multiplayer is not assumed. Replication requires a separate explicit design.

## Verified class inventory

Populate this table during repository bootstrap.

| System | Class/file | Owner | Inputs | Outputs/events | Blueprint assets | Status |
|---|---|---|---|---|---|---|
| Character | Unknown | Unknown | Unknown | Unknown | Unknown | Provisional |
| Interaction | Unknown | Unknown | Unknown | Unknown | Unknown | Provisional |
| Door/keycard | Unknown | Unknown | Unknown | Unknown | Unknown | Provisional |
| Objectives | Unknown | Unknown | Unknown | Unknown | Unknown | Provisional |
| Weapons | Unknown | Unknown | Unknown | Unknown | Unknown | Planned/unknown |

## Data-flow questions to resolve

- Where inventory/keycards are owned.
- Which class creates and owns UI widgets.
- Which input mapping context and actions are active.
- Whether objective state is character-owned, world-owned, or subsystem-owned.
- Whether any replication or save-system code already exists.
- Whether tests live in the primary module, a test module, or a plugin.
