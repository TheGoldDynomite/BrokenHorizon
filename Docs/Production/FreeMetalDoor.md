# Free Metal Door - local acquired content

Source: Metal Door by Guadalupe Munoz on Fab.
https://www.fab.com/listings/3a42f5a7-65ef-44d0-8532-15dd4ee276e4

Acquired/downloaded by the user on 2026-09-06. Current budget: free assets only.
Source file: bunkerdoor.fbx, 84,880 bytes.
SHA-256: F771B7A6A509B4668089D8B19073385ED35B5E7D521333B60F74C7A70942FF80

The delivery contains seven mesh parts and one plain lambert material. It has
no supplied or embedded textures; the preview's rust finish is not included.
The integration uses the existing project military material. All mesh parts
form one moving leaf with handles/latches; none is a stationary jamb.

Local import folder: /Game/BrokenHorizon/Environment/FreeMetalDoor.
Keep the acquired files and derived Unreal assets out of the public repository.
The download metadata confirms the listing/acquisition but does not specify the
license variant. Public source redistribution has not been authorized. Fab's
standard-license summary permits project use and private collaborator sharing,
while prohibiting standalone redistribution: https://www.fab.com/eula
This note records provenance; it is not a replacement for the acquisition license.

The tracked map retains its public-safe project-authored door. Apply the local
integration script after importing the acquired asset to use the free door in
First Light. Keep that local map change out of public commits while its acquired
mesh is excluded, so a fresh public checkout retains resolvable references.

## Local integration - 2026-09-06

The downloaded mesh is imported and installed locally in First Light. It is
uniformly scaled to 200 cm tall (112.28 cm wide), with its pivot aligned to the
existing door root. A stationary panel using the existing engine cube fills
the remaining opening. The native keycard, persistence and swing behavior stay
on the original actor. One simple box collider was added to the imported mesh.

Reproduction after acquiring the asset:
1. Run Content/Python/import_free_metal_door.py with --source pointing to the
   local bunkerdoor.fbx. This refuses overwrite of an existing import folder.
2. In a fresh full UnrealEditor process with -nullrhi, run
   Content/Python/integrate_free_first_light_door.py --apply through
   -ExecutePythonScript. The script backs up the map and mesh before mutation.
3. Run the integration script without --apply in another fresh full Editor
   process for saved-state inspection. Test the keycard and passage in PIE.

Evidence: Saved/Logs/BH-FreeDoor-Import.log imported successfully;
Saved/FreeFirstLightDoor/20260906-145659-e5676ec3/report.json verified scoped
apply/save/reload (125 existing actors preserved except door presentation,
with one side panel added). Fresh inspection passed at
Saved/FreeFirstLightDoor/20260906-145759-b3b1e16e/report.json, and
Saved/Logs/BH-FreeDoor-FreshInspect.log records First Light route-contract PASS.
No C++ changes were needed. Physical-input passage through the narrower opening
and multiplayer still require PIE testing; these checks do not establish that.

Final validation: Tools/Validate.ps1 -RequireTests -SkipReview passed Editor
build and 145 tests (136 success, 9 success with warnings, zero failures/not-run/
in-process), report Saved/Logs/Codex/AutomationReport-20260906-075918/index.json.
Saved/Reports/DoorPresentation-Free.png was visually inspected. Runtime startup
loaded First Light and exited normally without logged errors in
Saved/Logs/BH-FreeDoor-RuntimeSmoke.log. Review by bh_free_door_reviewer found no
supported blocker; bh_blueprint_guide inspected the source FBX and
bh_cpp_implementer wrote the scoped scripts. No cook/package was produced.
