# Flock <img align="right" width=128, height=128 src="https://github.com/Vaei/Flock/blob/main/Resources/Icon128.png">

> [!IMPORTANT]
> Bird flocks that idle, notice you, and scatter
> <br>Mass (ECS) simulation, instanced static meshes, baked vertex animation
> <br>No actors per bird, no skeletal meshes, no anim blueprints, no runtime traces

UE5.8+

---

> [!CAUTION]
> Flock has not officially released. Expect terrible bugs, and updates without versioning or a changelog reflecting them. Documentation is incomplete and there are no images or videos yet. **Come back soon!**

<!-- TODO(image): hero shot - a flock on a rooftop, one bird mid-takeoff -->

## Documentation

**[vaei.github.io/Flock](https://vaei.github.io/Flock/)**

Or open [`docs/index.html`](docs/index.html) from a clone - it is a static site with no build step and no network, so it works straight off disk.

| | |
|---|---|
| [Install](https://vaei.github.io/Flock/install.html) | clone, build, and get birds in a level |
| [Bake a bird](https://vaei.github.io/Flock/bake.html) | skeletal mesh and clips in, static mesh and textures out |
| [Placing a flock](https://vaei.github.io/Flock/placing.html) | the order that avoids doing it twice |
| [Tuning behaviour](https://vaei.github.io/Flock/behaviour.html) | symptom to knob |
| [Ambient](https://vaei.github.io/Flock/ambient.html) / [Reactive](https://vaei.github.io/Flock/reactive.html) | the two things a flock is for, with numbers |
| [Species](https://vaei.github.io/Flock/species.html), [Perception](https://vaei.github.io/Flock/perception.html), [Flying](https://vaei.github.io/Flock/flight.html) | every parameter |
| [The Crow](https://vaei.github.io/Flock/example.html) | one bird, start to finish |
| [Cost](https://vaei.github.io/Flock/performance.html) | LOD, counters, what to measure |
| [If it is wrong](https://vaei.github.io/Flock/troubleshooting.html) | symptom to cause |

## Why

Birds are the cheapest thing in a level to want and the most expensive to build. A dozen actors, each with a skeletal mesh, an anim blueprint and a behaviour tree, is a real budget spent on set dressing nobody will walk up to.

Flock spends none of it. A bird is a handful of floats in a Mass chunk; a whole flock is one instanced mesh component and two calls a frame; the animation is a texture the vertex shader reads. Nothing traces, nothing collides, nothing replicates, and none of it exists on a dedicated server.

## Features

- **A whole flock for the cost of one actor** - Mass (ECS) simulation, one instanced mesh draw per flock, animation baked into textures. No actor per bird, no skeletal meshes, no anim blueprints, no runtime traces
- **Idle behaviour** - rest breaks (preen, caw, shake), dawdling a few steps, glancing about, and moving between perches unprompted, so a settled flock is never a still one
- **They notice you** - anything can alarm them, scored on how near it is, how fast it is closing and how much it counts for. Perk up, turn to track, then go. Per-bird thresholds, so no two leave at once
- **Alarm is contagious** - one bird leaving alarms the rest, so a flock cascades instead of switching
- **A scatter splits** - some relocate far from what frightened them, some wheel overhead and come back once it is calm. The ratio is authored per flock
- **Reserved perch slots** - no two birds ever claim one branch. Slots from mesh sockets, a spline, a grid or placed by hand, on any actor, landing on the authored rotation
- **Pose-matched clip changes** - the bake works out which frame of each clip best continues the one being left, so a change of clip does not snap
- **Per-flock audio and VFX** - one MetaSound bed fed live distance, count, alert and airborne ratio with triggers for cascades, plus a pool of spatialised one-shots. Bursts through one batched Niagara component
- **Blocking volumes** - a box or sphere birds will not fly into, since they have no collision of their own
- **Four LOD tiers** - structural, not branched: the tier is a tag, so a distant flock's chunks are never visited at all
- **No replication** - birds are cosmetic, simulate independently per client, and never run on a dedicated server

### And the tooling to set it up

Everything is one menu, and the bake is automated end to end:

- Creates the static mesh, all three textures and the data asset
- Moves the lightmap off the UV channel the bake needs, the most common first-run failure
- Validates every texture before starting, and re-points every material instance afterwards
- Builds the pose match table
- Bakes every perch in the level
- Drops a preview you can scrub in the viewport without entering play

<img width="321" height="322" alt="Photoshop_2026-08-09_20-36-31" src="https://github.com/user-attachments/assets/a08c0e96-caad-4427-bf8a-3998ea5b90e3" />

## Quick start

```
cd YourProject/Plugins
git clone git@github.com:Vaei/Flock.git
```

1. Build, enable the plugin, restart.
1. Give your bird a material with **Use Material Attributes**, **Num Customized UVs 1** and **Used with Instanced Static Meshes**, and put `MF_FlockBoneAnimation` before the output.
1. Make an empty **Flock Species Data** asset.
1. **Flock → Bake Animation Textures...**, then **Prepare Asset Set**, then **Bake**.
1. Map the clips on the species. **Idle** is the only one required.
1. Drag a **Flock Volume** out of the **Flock** menu, set **Species** and **Spawn Count**, press Play. Walk at them.

Full walkthrough: [Install](https://vaei.github.io/Flock/install.html) and [Bake a bird](https://vaei.github.io/Flock/bake.html).

## What it is not

- **Birds have no collision of any kind.** No traces, no queries, nothing touching physics. A [blocking volume](https://vaei.github.io/Flock/perches.html#blockers) is the whole of their world awareness
- **Nothing is replicated.** Two clients will not see the same bird in the same place, and nothing runs on a dedicated server. If gameplay has to agree about a bird, this is the wrong tool
- **Clips cut, they do not blend.** A frame is a row of a texture. [Pose matching](https://vaei.github.io/Flock/blending.html) picks where the cut lands; nothing blends one clip into another
- **Bone mode discards bone scale**, and neither mode bakes morph targets or extracts root motion
- **Only six blocking volumes** are kept per flock, chosen nearest-first

## License

MIT. The documentation site bundles IBM Plex Sans and Mono under the SIL Open Font License 1.1.
