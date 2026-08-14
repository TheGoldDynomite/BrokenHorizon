# Broken Horizon — Test Matrix

## Commands

```powershell
.\Tools\RunTests.ps1
.\Tools\Validate.ps1
.\Tools\Validate.ps1 -RequireTests
```

`RunTests.ps1` returns a distinct status when no matching tests exist. `Validate.ps1` allows that during initial bootstrap; `-RequireTests` turns it into a failing gate after tests are established.

## Initial coverage targets

Every test below is **planned until the real API is inspected**.

| Area | Candidate behavior | Level | Status | Evidence/test name |
|---|---|---|---|---|
| Interaction | Non-interactable actor is never dispatched through `Execute_*` | Automation/unit | Planned | Unknown |
| Door | Unlocked door changes state correctly | Automation/unit | Planned | Unknown |
| Door | Locked door rejects missing/wrong keycard | Automation/unit | Planned | Unknown |
| Door | Correct keycard permits expected transition | Automation/unit | Planned | Unknown |
| Objectives | Valid objective state transition | Automation/unit | Planned | Unknown |
| Objectives | Invalid/duplicate transition is handled predictably | Automation/unit | Planned | Unknown |
| Weapons | Firing decrements magazine exactly once | Automation/unit | Future | Unknown |
| Weapons | Reload transfers bounded ammo and preserves totals | Automation/unit | Future | Unknown |
| Startup | Editor target starts without fatal initialization error | Smoke/Gauntlet or manual | Future | Unknown |
| Blueprint | Required Blueprint assets compile and load | Content validation | Future | Unknown |

## Test quality rules

- Deterministic and independent of execution order.
- No dependence on final art assets unless the test is explicitly a content test.
- No uncontrolled sleeps or timing assumptions.
- Restore global, disk, and object state.
- Prefer state and invariant checks before visual checks.
- Record manual PIE coverage when automation is not yet practical.
