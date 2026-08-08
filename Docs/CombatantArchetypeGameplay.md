# Combatant archetypes

Ambient patrols, convoy security, operation forces, friendly support, and
defense waves now receive a deterministic mixture of combat roles. The same
Blueprint soldier class is retained so existing content and animation wiring
remain compatible.

## Roles

- **Rifleman:** preserves the authored Blueprint baseline and remains the
  balanced core of each formation.
- **Scout:** moves and repositions faster, detects players farther away, fights
  at longer range, seeks cover sooner under suppression, carries less
  ammunition, and retreats earlier when wounded.
- **Gunner:** has more health and body armor, moves slowly, holds cover longer,
  fires long rapid bursts from a 60-round magazine, and tolerates substantially
  more suppression before retreating.

Specialists display a temporary native role label for combat readability until
dedicated meshes, silhouettes, and equipment assets replace it. Friendly labels
are blue; hostile scouts are amber and hostile gunners are red.

Manual review is required for encounter difficulty, role readability at combat
distance, label occlusion, navigation/repositioning quality, damage tuning,
animation cadence, audio variation, and two-client replication.
