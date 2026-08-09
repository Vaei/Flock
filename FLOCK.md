# Flock Reference

Setup, troubleshooting basics and profiling are in [`README.md`](README.md). A worked setup is in
[`EXAMPLE.md`](EXAMPLE.md). This is everything else.

| System | |
|---|---|
| [Species](#species) | one kind of bird: its baked mesh, its clips, its tuning |
| [Baking](#baking) | making that mesh and its animation textures, and re-making them |
| [Preview](#preview) | playing a baked clip in the viewport, without PIE |
| [Flocks](#flocks) | the volume, where birds are placed, and what state they start in |
| [Perches](#perches) | slots on any actor, reserved so two birds never claim one branch |
| [Being noticed](#being-noticed) | what alarms birds, and how hard |
| [Scaring them](#scaring-them) | from gameplay code, or from an animation |
| [Flying](#flying) | takeoff, wheeling, contagion, coming back down |
| [Idling](#idling) | rest breaks, dawdling, glances |
| [Moving unprompted](#moving-unprompted) | shuffling between perches with nothing wrong |
| [LOD](#lod) | four tiers, and what each one stops doing |
| [Networking](#networking) | nothing runs on a dedicated server |
| [Sound and VFX](#sound-and-vfx) | a bed per flock, one-shots per bird, bursts |
| [In your own code](#in-your-own-code) | reacting to a clip, driving playback yourself, instrumenting |

---

## Species

### Clips

| Clip | |
|---|---|
| **Idle** | required. Everything falls back to it |
| **Turn Left**, **Turn Right** | tracking a threat, and idle glances |
| **Take Off**, **Fly**, **Land** | required for a bird to ever leave the ground. Without **Fly** the flock is permanently earthbound, which is a valid way to ship one |
| **Take Off Loop**, **Land Loop** | optional. Held once the one-shot beside them has finished but the bird has not yet finished climbing, or has not yet touched down. Unmapped, that gap is filled by **Fly** |
| **Glide** | optional. Replaces **Fly** while descending faster than **Glide Descent Rate**, and fills most of a descent when **Land Loop** is unmapped |
| **Bank Left**, **Bank Right** | optional. Replace **Fly** while turning harder than **Bank Clip Yaw Rate** |
| **Walk** | a slow dawdle. Without it birds stand still between idles |
| Rest breaks | authored separately, below |

A launch lasts **Takeoff Time** and a descent lasts however long the bird needs to reach its slot, and
neither is the length of a clip. So a one-shot that ends early hands over rather than holding its last
frame: to its loop clip if there is one, and to **Fly** if there is not. A **Take Off** clip that is
*longer* than the launch plays out in full - a one-shot is never cut mid-pose, because VAT cannot blend.

**Land is a flare, not a touchdown.** It starts when the bird commits to coming down, which is above and
usually away from where it lands, so most of the descent is the bridging clip. Touchdown snaps to **Idle**.

Each mapping:

| | |
|---|---|
| **Animation Index** | index into the data asset's `Animations` - the *enabled* clips, in bake order |
| **Loop** | on for Idle, Walk, Fly and the turns; off for TakeOff, Land and rest breaks |
| **Random Start Phase** | on for **Idle** only |
| **Play Rate** | multiplies the bird's own rate for this clip alone. Fixes one clip authored at the wrong speed, or slows a walk to match the ground speed it is driven at, without re-baking |
| **Sounds** | played at the bird when the clip starts, one picked at random |
| **Audio Trigger** | fired on the flock's bed when the clip starts, so a MetaSound decides what the moment sounds like |
| **Sound Delay** | how far into the clip both land |

### Rest breaks

A list, **Rest Breaks**, not clip slots. Each entry has a **Name** (which the list shows), an **Animation
Index**, a **Weight**, a **Play Rate**, its own **Sounds** / **Audio Trigger** / **Sound Delay**, and
optional mirroring.

| | |
|---|---|
| **Weight** | likelihood relative to the other breaks |
| **Mirrored**, **Mirror Animation Index**, **Mirror Chance** | for a break authored as two clips: a wing stretch left and right, a head cock either way |

A mirrored pair is **one entry**, so it spends one Weight across both sides and **Mirror Chance** picks
which plays. Two entries would make it twice as likely as a single-clip break, which is backwards - paired
breaks tend to be the big conspicuous ones you wanted rare. Both sides share the entry's audio.

There is a fixed pool of playback slots behind the list. Anything past what it holds is dropped with a
warning.

> [!TIP]
> Set **Sound Delay** on a caw. The sound belongs where the beak opens, not on frame 0.

### Per bird

| | |
|---|---|
| **Scale Min/Max**, **Play Rate Jitter** | variation, so a flock is not one bird repeated |
| **Mesh Yaw Offset** | correction for art that does not face +X. Applied everywhere facing comes from a direction, so a bird authored facing +Y flies forwards rather than sideways |
| **Bounds Radius** | roughly the bird's radius in world units. Feeds the screen-size estimate that drives LOD |

---

## Baking

**The recipe lives on the species, not the window.** Assigning a **Species** loads its recipe; a successful
Prepare or Bake writes it back and points the species at what was produced. Moving to another bird costs
nothing.

| Button | |
|---|---|
| **Load** | pull the species' recipe into the window, discarding what is there |
| **Save To Species** | push the window's recipe onto the species without baking |
| **Prepare Asset Set** | create the assets, then save the recipe |
| **Bake** | bake, then save the recipe |

Turn off **Write Back To Species** to use the window as a scratchpad. An empty recipe is never loaded over
a populated window, so pointing at a fresh species starts from what you already have.

### What Prepare Asset Set makes

| Asset | Name |
|---|---|
| Static mesh | `SM_<Name>_VAT` |
| Textures | `TX_<Name>_BonePosition`, `_BoneRotation`, `_BoneWeight` |
| Data asset | `DA_<Name>_BoneAnimation` |

It also sets `UVChannel = 1`, `NumDriverTriangles = 1`, moves the lightmap to UV2, and sets
`bAutoPlay = false`.

> [!WARNING]
> Set `bAutoPlay` on the **data asset**, never on the material instance. The bake pushes it onto the
> `AutoPlay` switch, so a manual override is silently undone.

### Reading the results

| Field | Expect |
|---|---|
| `NumFrames` | the sum of every enabled clip's frame count |
| `NumBones` | your skeleton's raw bone count, virtual bones excluded |
| `BoneRowsPerFrame` | `1`, unless the skeleton is wider than `MaxWidth` |
| `Animations` | one entry per enabled clip, contiguous `StartFrame`/`EndFrame` |

Texture sizes follow: position and rotation are `NumBones` × `NumFrames + 1` (the extra row is the ref
pose), the weight texture is the welded vertex count wide by 2 rows.

`Animations` is index-aligned to the **enabled** clips only. A disabled clip is dropped and shifts every
later index, which is why the mapping is stored per clip rather than assumed to match your source list.

### Bone vs Vertex

**Bone** stores per-bone position and rotation plus a static weight texture. Small textures, and the
position/rotation textures are shareable between meshes on the same skeleton. Discards bone scale.

**Vertex** stores per-vertex position and normal. Textures grow with vertex count. Keeps scale. Use
`MF_VertexAnimation` instead.

### Writing outside /Game

The default material instance slots point at the AnimToTexture plugin's Mannequin instances, which live in
engine content with no source control. `bAllowWritingOutsideProject` (off, under Advanced) makes the bake
skip anything outside `/Game`. Only tick it to deliberately re-bake the plugin's own assets.

### From script

The window is a front end for these. For batching several birds, or baking in CI.

```python
import unreal

BIRD = "/Game/Path/To/YourBird"
CLIPS = ["Idle", "TurnLeft", "TakeOff", "Fly", "Land"]

unreal.FlockBakeLibrary.configure_flock_bake(
    f"{BIRD}/SKM_YourBird.SKM_YourBird",   # source skeletal mesh
    f"{BIRD}/VAT",                          # output path
    "YourBird",                             # asset name
    [f"{BIRD}/{c}.{c}" for c in CLIPS],     # clips, in index order
    30.0)                                   # sample rate
unreal.FlockBakeLibrary.set_flock_bake_material_instances(
    [f"{BIRD}/VAT/MI_YourBird_VAT.MI_YourBird_VAT"])

unreal.FlockBakeLibrary.prepare_flock_assets()
unreal.FlockBakeLibrary.bake_flock_textures()
```

Both return a bool and log failures to `LogFlockEditor`.

> [!CAUTION]
> Headless runs need `-AllowCommandletRendering`, or the mesh conversion returns null and logs nothing.

```
UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="bake.py" ^
    -unattended -nosplash -nosound -AllowCommandletRendering
```

---

## Preview

**Flock → Spawn Preview in Current Level**, or place a **Flock VAT Preview** actor and set **Anim Data**.
It plays in the viewport, no PIE.

| | |
|---|---|
| **Animation Index** | which clip, indexing the data asset's `Animations` |
| **Loop**, **Play Rate** | |
| **Random Phase** | stagger instances, so a grid does not move in unison |
| **Hold Frame** | freeze on one frame instead of playing. Negative plays |
| **Hold Frame Is Absolute** | address the raw baked frame, ignoring the clip range. The tool for telling a bad bake from a bad frame mapping |
| **Count X/Y**, **Spacing** | grid size, for eyeballing instancing cost |

Dragging the **`Frame`** scalar on the material instance is the cheaper check: if the preview mesh deforms,
the textures, the material and the bake are all right.

Check both feature levels before trusting a bake. The vertex shader budget is tighter on ES3.1 than SM5.

---

## Flocks

A **Flock Volume**'s box is where birds are scattered, and the bounds the disturbance broadphase tests
against.

| | |
|---|---|
| **Species**, **Spawn Count** | |
| **Spawn On Begin Play** | off leaves it to `SpawnFlock()` |
| **Orbit Preference** | chance a spooked bird wheels overhead rather than heading straight back down. Negative takes the species' value |
| **Snap To Ground** | trace down through the box for each bird, once at spawn. Off puts them on the plane through the actor, which is the box's *centre* |
| **Ground Trace Channel**, **Ground Offset** | |
| **Min Spawn Spacing** | nearest another bird may spawn. Scattered randomly, never stacked |
| **Min Ground Normal Z** | steepest surface a bird will accept. `1` is flat, `0` accepts a wall |
| **Headroom Radius** | clear space needed above the feet, so none is placed inside geometry. Zero skips the check |

Set **Default Species** in Project Settings → Game → Flock and a volume works with nothing assigned. For
real use, subclass the volume as a Blueprint per species.

**Select it and it draws** the circuit airborne birds wheel around - **Cruise Radius** at **Cruise Ceiling**,
taken from whichever species it will actually use - plus the plane birds fall back to, and a line each for
bird count and any missing species. The circuit is the useful one: it is invisible otherwise, and the usual
surprise is that it reaches through a roof or a cliff the volume itself sits clear of.

### Where a flock starts

Left alone every bird would spawn standing on the ground, and the level's first seconds would be one burst
of activity as they sorted themselves out. Instead the flock is fast forwarded on its first tick:

| | |
|---|---|
| **Initial Perched Fraction** (0.4) | teleported straight onto free perches |
| **Initial Airborne Fraction** (0.15) | placed part way round a circuit, already flying |
| the rest | stay on the ground |

All with their timers scattered. A bird that finds no free perch stays put.

This happens a tick after spawning rather than during it: perch components register their slots from their
own `BeginPlay`, and nothing orders that against the volume's.

---

## Perches

Add a **Flock Perch Component** to any actor - a fence Blueprint, a rooftop, a tree - and birds will land
on it. Slots resolve in the editor and cook into the owning asset; nothing traces at runtime.

| **Source** | |
|---|---|
| **Box** | a grid over the box, optionally traced down onto whatever is under it |
| **Sockets** | every socket on the picked mesh whose name starts with **Socket Prefix** |
| **Spline** | sampled along the picked spline at **Spacing**, optionally facing along the tangent |
| **Explicit** | typed in by hand |

| | |
|---|---|
| **Position Jitter** | scatter within each cell, so a box does not read as a grid |
| **Is Ground** | ground rather than perch. Only changes which birds consider the slot |
| **Baked Slots** | the resolved slots, in component space. Rebuilt by the button or on edit |
| **Auto Register** | |

**Rebuild Slots** on the component, or **Flock → Bake All Perches in Level** for every placed one. A perch
inside a Blueprint is not written back to the asset that way - rebuild it in the Blueprint.

**Select the actor and the slots draw**: a disc where the bird stands, an arrow for the way it will face,
green for a perch and tan for ground, with the slot index beside it. A Box source also draws the box the grid
comes from, so a component with no slots yet still shows where they would land. Nothing baked draws
*No slots - press Rebuild Slots* instead of nothing at all.

Slots go Free → Reserved → Occupied, resolved in a single game-thread pass so two birds cannot claim one.
A slot whose reserving bird no longer exists is freed in the same pass. A bird that finds nothing for
**Max Orbit Time** lands on open ground instead.

---

## Being noticed

Every `APawn` alarms birds already, with no per-character setup. Four ways in, deduped as component >
interface > class list:

| | |
|---|---|
| **Flock Disturbance Component** | **Threat Weight** and **Max Radius**, and `SetEnabled` for a source that comes and goes. Always outranks the class list |
| `IFlockDisturbanceInterface` | for a type your own module owns. `IsFlockThreatActive` covers going unnoticed while crouched or hidden |
| **Auto Register Disturbance Classes** | Project Settings → Game → Flock. Defaults to `APawn`, at **Auto Source Threat Weight** and **Auto Source Radius** |
| `FlockIgnore` tag | opts an actor out |

Alarm builds per bird from its flock's four strongest threats, and decays when they leave.

| **Perception** | |
|---|---|
| **Proximity Exponent** | **how sharply alarm falls off with distance - the knob for *where* birds break.** At 1 something at the edge of its radius is already mildly alarming, so a heavy source builds enough alarm to flee from far away and no radius change fixes it. At 3 the halfway point contributes an eighth as much |
| **Safe Radius** | inside which proximity is already full |
| **Max Notice Radius** | this species' attention span, capping whatever radius a source claims |
| **Panic Radius** | close enough to flee outright, which covers a sprinting or teleporting source |
| **Base Weight**, **Closing Weight**, **Speed Weight** | how much merely being nearby, *approaching*, and moving fast each matter |
| **Closing Speed Ref**, **Speed Ref** | the speeds those two are measured against |
| **Alert Gain**, **Alert Decay**, **Max Threat Rate** | how fast alarm builds, fades, and its ceiling |
| **Perk Threshold** | alarm before a bird looks up |
| **Perk Release Ratio** | how far it must fall before relaxing. Below 1, so birds do not flicker |
| **Flee Threshold** | alarm before it launches |
| **Threshold Jitter** | spread across birds, so a flock reacts raggedly rather than as one animal |
| **Turn Rate Degrees**, **Turn Deadband Degrees** | facing speed, and the error below which turning gives way to idle |

Per-bird cost is bounded by those four slots, not by how many sources exist in the world, and a flock with
none is skipped whole.

---

## Scaring them

### From code

```cpp
#include "System/FlockStatics.h"

UFlockStatics::ScareFlock(this, GetActorLocation(), /*Radius*/ 2000.f);
```

Nothing to register, nothing to clean up. It is a **source with a lifetime**, not a command: for
**Duration** it sits in the same list a walking player does and is felt through the same falloff, so birds
in the middle break and scatter while the ones at the edge only look round, and none of them do it on the
exact frame of the bang.

| | |
|---|---|
| **Radius** | how far it carries. **Not** capped by **Max Notice Radius** |
| **Weight** | how alarming. Comparable to a source's threat weight, where a walking character is `1`, so the default `8` is emphatic |
| **Duration** | how long it keeps alarming. Alarm accumulates over this - shorten it for a flinch, lengthen it for a rout |
| **Falloff** | `1` linear, the default rather than the species' curve, because a scare should carry to its edge. Raise it to pull the reaction in tight |

`ScareFlockAtActor` is the same centred on an actor, read once at the call. Both are `BlueprintCallable`.

### From an animation

Add a **Scare Flock** notify to any montage - a swing, a landing, a shout. Same path, same four
properties, plus **Socket Name** to centre it on a socket rather than the actor.

This is the preferred way in: *where* in a swing the noise happens is a property of the animation, and only
whoever authored it knows where that falls. It no-ops in preview and editor worlds.

---

## Flying

Takeoff is committed: the launch direction is chosen once, from **Takeoff Up Bias** blended with
away-from-threat, then eased to **Takeoff Speed** over **Takeoff Time** rather than snapping.

Airborne birds steer toward one attractor per flock that sweeps around it at **Attractor Sweep Degrees**,
at **Cruise Radius** and **Cruise Ceiling**, at **Cruise Speed** and **Turn Rate Degrees**, with
**Cohesion Weight** against per-bird **Jitter Amplitude** / **Jitter Frequency** and **Bank Scale** roll
from the turn. That one moving point is what makes a flock wheel: **no bird ever looks at another bird**, so
the cost is flat in flock size.

While cruising, the clip follows what the bird is doing: **Bank Left**/**Right** past **Bank Clip Yaw
Rate**, else **Glide** past **Glide Descent Rate**, else **Fly**. A clip that has started holds until well
under the threshold that started it, so nothing chatters at the boundary. Both alternatives are optional and
**Fly** covers whatever is unmapped, so a species with neither never changes clip in the air. Bank Clip Yaw
Rate is capped by **Turn Rate Degrees** - set at or above it, a bank never plays.

Two things stop it reading mechanically:

- **Contagion.** One bird taking off alarms its flockmates by **Contagion Strength** for **Contagion
  Window**, so a flock erupts together instead of one bird at a time. Two floats in a shared fragment, not
  a neighbour query.
- **Orbit Preference.** The chance a spooked bird wheels overhead rather than heading straight back down.
  At `0.5` a flock visibly splits: some resettle, some circle for **Orbit Time Min**/**Max**.

Coming down: a bird heading straight back looks for a slot at least **Safe Relocate Distance** from what
spooked it and on the far side of itself, within **Land Search Radius**; failing that it orbits instead. It
descends at **Land Speed** from **Land Approach Height** and settles within **Land Arrive Distance**.
**Min Flight Time** stops a launch from being aborted immediately.

The **Land** clip starts when it commits to coming down, not when it arrives, so most of the descent is
**Land Loop** or **Fly** - see [Clips](#clips).

**Landed Cooldown** then keeps it from bouncing straight back up: alert decays **Refractory Decay Scale**
faster and the flee bar is raised by **Refractory Flee Bonus** while it lasts.

---

## Idling

Three things fill the time between takeoffs, under **Idle** on the species.

- **Rest breaks.** Every **Rest Interval Min**/**Max** a settled bird plays one of its rest breaks once and
  returns to idle, picked by **Weight**. The preen, the head cock, the caw. Nothing happens if the species
  mapped none. **Allow Rest Breaks** turns it off.
- **Dawdling.** Every **Walk Interval Min**/**Max** a bird on **open ground** wanders somewhere between
  **Walk Min Distance** and **Walk Radius** of the spot it spawned on, at **Walk Speed**, turning at **Walk
  Turn Rate Degrees** to face where it is going and slowing into the turn rather than crabbing sideways.
  Aiming from the spawn spot rather than from where it currently stands is what keeps a bird that dawdles
  all day from drifting out of its flock. Needs a **Walk** clip; **Allow Walking** turns it off.
- **Glances.** Every **Glance Interval Min**/**Max**, a turn of up to **Glance Yaw Degrees** using the turn
  clips it already has, so it needs no extra animation. Under Perception. **Allow Glances** turns it off.

A bird holding a perch slot never walks - it would be stepping off a branch and leaving a reserved perch
stood empty.

All three yield instantly to a real alert, and none can interrupt a clip mid-pose. VAT has no blending, so
a clip cut short is a visible snap.

> [!WARNING]
> Walking never traces. A bird holds the height it was placed at, so keep **Walk Radius** short on uneven
> ground or birds will walk into a slope.

---

## Moving unprompted

Birds also move with nothing disturbing them.

On a per-bird interval between **Restless Interval Min** and **Max** (35-110s) a settled bird takes off and
picks a perch or its original ground spot, weighted by **Perch Preference** - so it sometimes chooses the
same kind of place twice rather than ping-ponging. **Restless Orbit Chance** decides how many take a lap
overhead first instead of hopping straight across.

**Ambient Airborne Fraction** (0.15) is a standing target for how much of the flock is in the air with
nothing wrong. Below it, restless countdowns run **Ambient Airborne Urgency** (8×) faster and every move is
an orbit; once met, everything drops back to the slow interval. Without this a lap overhead is far too rare
to catch - on ten birds the interval alone gives one short orbit every twenty seconds or so. Zero switches
it off.

A voluntary move carries no contagion, so one bird shuffling never panics the flock, and it ignores the
threat-avoidance filter because it is not fleeing. **Allow Restless Moves** turns it off.

> [!TIP]
> These intervals are deliberately long. Shorten them and a flock reads as agitated rather than settled -
> the shuffling is meant to be something you notice out of the corner of your eye, not a rhythm.

---

## LOD

Tier comes from camera distance and an estimated screen size, re-evaluated at **LOD Rate Hz**. Promotion
happens at the distance, demotion at **LOD Hysteresis** past it, with a minimum **LOD Dwell Time** in a
tier, so nothing flickers across a boundary.

| Tier | Runs |
|---|---|
| **Near** | everything, every frame |
| **Mid** | everything, one frame in **Mid Frame Divisor** (2) |
| **Far** | one frame in **Far Frame Divisor** (6) |
| **Culled** | LOD re-evaluation only. Render writes zero scale |

Skipped time is folded into a scaled delta, so a Far bird still moves at the right speed. The phase comes
from each bird's seed, so work is **spread across frames** rather than every Far bird spiking on the same
one. Tiers are *tags*, so a Culled bird's chunks are never visited at all rather than visited and skipped.

Idle behaviours run at every tier except Culled, at that tier's stride. Far is included on purpose: a bird
part way through a dawdle has to finish it wherever it is.

**Separation** between airborne flockmates is on by default (**Enable Separation**, with **Separation
Radius** and **Separation Strength**). **Separation Max Tier** decides how far out it runs - that tier and
every closer one, defaulting to **Near**.

It is chunk-local rather than grid-based: a flock's shared-fragment value already partitions its own
chunks, so the birds in a chunk are the ones close enough to matter and no spatial structure is rebuilt each
frame. It is the one part of the system that is not flat in flock size.

---

## Networking

**Nothing runs on a dedicated server.** Birds are cosmetic and simulate independently on each client;
nothing is replicated, and two clients will not see the same bird in the same place.

Three separate gates, so no single mistake can put them on a server:

- Every processor declares `Standalone | Client`, and Mass reports `Server` for a dedicated server, so the
  pipeline is built with **zero** processors and the subsystem's tick returns immediately.
- `UFlockSubsystem` is not created there at all, so there is no audio pool, no actor-spawned handler and no
  event queue to fill. `ScareFlock` and the notify find no subsystem and do nothing.
- `AFlockVolume` spawns nothing: no entities, no render actor, no instanced meshes.

A **listen server** is a client too, so it gets birds like any other.

---

## Sound and VFX

All optional. Leave a slot empty and nothing is created.

| On the species | |
|---|---|
| **Take Off VFX**, **Land VFX** | any Niagara system, spawned per event from Niagara's own component pool. Nothing special is asked of the asset |
| **Flock Bed** | a continuous sound for the whole flock, one component whatever its size |
| **Take Off One Shots**, **Land One Shots** | spatialised to the individual bird, one picked at random, from a small pool |

The bed is fed these every frame, so a MetaSound can own its own mixing:

| Parameter | |
|---|---|
| `Distance` | listener to flock centre. The MetaSound owns the falloff curve - it is handed a distance, not a volume |
| `BirdCount` | live birds in the flock |
| `Alert` | mean alert, 0..1 |
| `AirborneRatio` | fraction currently flying, 0..1 |

plus `TakeOff` and `Land` **triggers** per event, so cascades are audible as cascades.

**Rest breaks carry their own audio**, set on the break rather than here, because which sound goes with a
caw is a property of the caw. **Sounds is a list, picked at random per play**: one caw asset repeated across
a flock is what gives an ambience away, and separate assets do not contend for one asset's concurrency
budget the way repeats of a single one do.

**One Shot Pool Size** (Project Settings → Game → Flock, default 8) caps overlapping one-shots. A cascade
past it drops the surplus rather than queueing - a late caw is worse than a missing one. **Enable One Shot
Audio** turns them off entirely, leaving the bed and its triggers.

> [!TIP]
> If caws still cut each other off, check **Sound Concurrency** on the asset. A group with Max Count 1 and
> Stop Oldest steals the voice however many pool slots are free.

Nothing here is spawned from a processor. A Mass processor is not guaranteed to be on the game thread, and
spawning an actor, touching a component or broadcasting a delegate from a worker crashes; so events go into
a locked queue drained once per frame on the game thread.

---

## In your own code

### Reacting to a clip

```cpp
UFlockSubsystem::Get(this)->OnClipStarted.AddDynamic(this, &AMyActor::HandleFlockClip);
```

`FlockIndex`, `Clip`, `Position`. `BlueprintAssignable`, fired on the game thread the moment the clip
starts, for every rest break whether or not it has a sound attached. Unlike the audio it is **never
delayed** - a clip starting is a clip starting.

### Driving playback yourself

`NumCustomDataFloats = 2` on the ISM: `[0] Frame`, `[1] PrevFrame`.

```cpp
const FAnimToTextureAnimInfo& Range = AnimData->Animations[ClipIndex];
const float NumClipFrames = Range.EndFrame - Range.StartFrame + 1;
const float Elapsed = (Now - ClipStartTime) * PlayRate * AnimData->SampleRate;
const float Local = bLoop ? FMath::Fmod(Elapsed, NumClipFrames)
                          : FMath::Min(Elapsed, NumClipFrames - 1.f);   // one-shots hold on the last frame
const float Frame = Range.StartFrame + Local;
```

> [!CAUTION]
> Do **not** offset `Frame` to account for the reference pose. `Fmod` already keeps `Local` inside
> `[0, NumClipFrames)`, so adding one makes the last step of every loop sample the *next* clip's first
> frame - one wrong frame per cycle, which reads as a flicker. Verify with **Hold Frame Is Absolute** on the
> preview actor: the last frame of a clip must still look like that clip.

```cpp
Instances->SetCustomData(0, Count - 1, CustomData, /*bMarkRenderStateDirty*/ false);
```

> [!WARNING]
> Leave `bMarkRenderStateDirty` **false** when updating every frame. `SetCustomData` already calls
> `CustomDataChanged` per instance, which is the incremental GPU update; marking dirty on top destroys and
> recreates the scene proxy every frame - wasteful, and visible as flicker.

`Frame` is a float but the textures are `TF_Nearest`, so values snap. There is no inter-frame blending, by
design.

`AFlockVATPreview` (`Flock/Public/Debug/FlockVATPreview.h`) is the whole thing working in ~60 lines.

### Instrumenting

`FLOCK_SCOPE(Name)` from `FlockStats.h` emits both the `stat flock` cycle counter and the Insights scope, so
the two cannot drift apart.

```cpp
void UFlockThreatProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    FLOCK_SCOPE(Threat);
    ...
}
```

Bakes are traced too (`FFlockBake::PrepareAssets`, `::Bake`, `::AnimationToTexture`).

---

## Troubleshooting

Watch the Output Log on **`LogAnimToTextureEditor`** during a bake. The plugin's own message log listing is
never written to.

| Symptom | Cause |
|---|---|
| No birds appear | The species has no valid **Idle** mapping, or its mesh failed to load. Both log to `LogFlock` |
| Birds appear but never animate | `stat flock` shows `Anim` at zero, so the processors are not running - see [README](README.md#performance-and-profiling) |
| Frozen on the bind pose | `NumCustomDataFloats` disagrees with `AutoPlay`. 4 floats for AutoPlay, 2 for Frame |
| Clip plays the wrong animation | An `Animations` index taken from the source list rather than the enabled clips |
| Frozen mid-air on one pose | A **Take Off** or **Land** one-shot that has run out, with no **Fly** mapped to hand over to. Map **Fly**, or the matching loop clip |
| Birds all move identically | **Random Start Phase** is off on the Idle mapping |
| Birds fly or walk sideways | **Mesh Yaw Offset** does not match art that faces something other than +X |
| Birds standing in the air | Nothing under the volume to trace, or the ground is steeper than **Min Ground Normal Z**. `LogFlock` warns |
| Birds float or sink | **Snap To Ground** off puts them on the plane through the volume's centre, not its floor |
| One wrong frame per loop | `Frame` offset past the clip's end, sampling the next clip's first frame. `Frame` maps directly onto the baked index |
| Playback is noise | sRGB, mips or compression changed on a baked texture. The bake sets these; leave them |
| Animation runs past its end | `SampleRate` is not the source clips' frame rate. It is a time step, not a frame count |
| Wrong UVs sampled | `UVChannel` of 4+. Only 0-3 work |
| Birds pop at screen edges | Missing bounds extensions on the static mesh |
| Jitter or stair-stepping in slow motion | 8-bit precision quantises position to 256 steps across the whole animation bounds. Use 16-bit |
| *"Too many Bones"* | Over 256 bones at 8-bit precision. Use 16-bit, which has no bone limit beyond texture width |
| Stray vertices spike | More than 4 influences per vertex. Only 4 reach the texture, and the reduction can overflow |
| Squash and stretch lost | Bone mode discards bone scale. Use Vertex mode |
| Root offset ignored | `RootTransform` is Vertex mode only |
| *"Already used by LightMap"* | Lightmap index equals the data asset's `UVChannel`. Move the lightmap to 2 |
| Editor crashes mid-bake | Null `BoneWeightTexture`. It hits a bare `check()` |
| Bake succeeds, textures unchanged | Null position or rotation texture. The write result is ignored |
| Mesh conversion returns null | Headless without `-AllowCommandletRendering`, or `LODIndex` left at `-1` |
| Changes revert on restart | The bake only marks packages dirty. Leave `bSaveAfterBake` on |
| Instance material gets no parameters | It is not on the static mesh **asset's** slot, or it is a dynamic instance. Only `UMaterialInstanceConstant` on the asset is reached |
| Perch rebuild does nothing | The perch is inside a Blueprint. **Bake All Perches in Level** only rebuilds placed instances |
| Crash in `DebugSetProcessor` on tick | A `FMassRuntimePipeline` held without `UPROPERTY`. Its `Processors` array is reflected, but only if the struct instance is itself reachable |

Morph targets are not baked in either mode, and there is no root motion extraction - move the instance
transform yourself.
