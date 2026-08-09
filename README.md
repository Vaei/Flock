# Flock <img align="right" width=128, height=128 src="https://github.com/Vaei/Flock/blob/main/Resources/Icon128.png">

> [!IMPORTANT]
> Bird flocks that idle, notice you, and scatter
> <br>Mass (ECS) simulation, instanced static meshes, baked
vertex animation
> <br>No actors per bird, no skeletal meshes, no anim blueprints, no runtime traces.

UE5.8+

---

> [!CAUTION]
> <br>Flock has not officially released. Expect terrible bugs, and updates to occur without versioning or changelog reflecting them. Also any documentation is incomplete, no images or videos are available yet either. **Come back soon!**

<!-- TODO(image): hero shot - a flock on a rooftop, one bird mid-takeoff -->

<!-- TODO(video): hero shot -->

---

## How to Use

> [!TIP]
> View the setup used for the Crow in the examples here: [`EXAMPLE.md`](./EXAMPLE.md).

> [!NOTE]
> Full documentation: [`FLOCK.md`](./FLOCK.md).

### 1. Material

Assign your bird a material you're willing to modify, then on it set:


| Setting | Value | Why |
|---|---|---|
| **Use Material Attributes** | ✔ | Mandatory. The bone animation function takes attributes in and returns both World Position Offset and Normal on one wire; without this there is no pin to connect. |
| **Num Customized UVs** | `1` | |
| **Used with Instanced Static Meshes** | ✔ | Required to render on an ISM. |
| **Used with Skeletal Meshes** | ✔ | Only if you also want the material on the source skeletal mesh. |
| Blend Mode | Opaque | There is no depth prepass in this project (`r.EarlyZPass=0`), so Masked is materially worse. |
| Shading Model | Default Lit | |
| Tangent Space Normal | ✔ (default) | The function's normal path assumes it. |

Insert **`MF_FlockBoneAnimation`** between `MakeMaterialAttributes` and the `MaterialAttributes` output.

It is a drop-in replacement for the AnimToTexture plugin's `MF_BoneAnimation` - same inputs, same
parameters, filled by the same bake - with one addition: an **Interpolate** static switch for blending
between adjacent frames. Left off it costs nothing whatsoever, compiling to the same vertex shader
instruction count as the engine function, so there is no reason to use the engine one instead.

> [!IMPORTANT]
> Interpolation needs **both halves**, and each is useless alone. Tick **Interpolate** on the material
> instance, *and* tick **Interpolate Frames** on the species. With only the material the shader blends two
> identical frames: full cost, no change on screen. With only the species nothing reads the frame it sends.
> <br>
> <br>Leave both off unless the animation reads as steppy. It does nothing for clip changes - that is
> [pose matching](./FLOCK.md#pose-matching).

**On cost:** it is all vertex shader, about eight extra texture fetches per vertex, and on the crow it takes
956 instructions to 1440. Vertex cost does not shrink with distance, so it is paid on every bird drawn with
that material, in the base pass and again in every shadow depth pass. Dropping the data asset to **two**
bone influences instead of four costs 992 with interpolation against the 956 you already pay without it, so
that trade is usually the answer rather than going without. Full table in
[Blending](./FLOCK.md#frame-interpolation).

<img width="1068" height="271" alt="image" src="https://github.com/user-attachments/assets/04ae2f91-960a-41b0-a1f1-cf1976062e7e" />

### 2. Species

Content Browser → **Miscellaneous → Data Asset → Flock Species Data**, named `DA_Species_<Name>`.

> [!NOTE]
> Leave it empty, data will be generated in the bake step

### 3. Bake

**Flock → Bake Animation Textures…**

1. Set **Species** to the asset you just made.
2. Set **Source Skeletal Mesh**, **Output Path**, **Asset Name**, your **Clips**, and **Sample Rate**.
3. **Prepare Asset Set** - creates the mesh, textures and data asset.
4. Put your material instance in **Bone Material Instances**, and on the new static mesh's material slot.
5. Leave **Build Pose Match Table** on.
6. **Bake**.

<!-- TODO(image): the bake window filled in -->

> [!WARNING]
> Sample Rate must be your animations' actual frame rate

> [!IMPORTANT]
> **Build Pose Match Table is what stops your clip changes snapping.** Nothing blends one baked pose into
> another, so a bird changing clip cuts from one texture row to another. The table records, for every baked
> frame, which frame of each animation is the closest pose to it, and a bird entering a looping clip opens
> it there instead of at its first frame. It costs a second pass over your clips at bake time, about 40 KB
> on the species, and nothing at runtime.
> <br>
> <br>It is measured against one bake's frame layout, so **a re-bake makes the old one stale** and it is
> then ignored rather than used wrongly. The bake rebuilds it; if you ever re-bake another way, use
> **Flock → Build Pose Match Table**, which rebuilds it without touching a texture.

### 4. Map the clips

On the species, set **Mesh** to `SM_<Name>_VAT` and **Anim Data** to `DA_<Name>_BoneAnimation`, then fill
**Clips**. **Idle** is the only one required. Read the indices off `Animations` on the data asset, not off
the order you typed the clips in.

> [!CAUTION]
> Indices follow the **enabled** sequences in data-asset order, and adding animations reorders that list
> (a multi-select drop sorts alphabetically). **Re-check every mapping after any re-bake.** The failure is
> silent - birds animate, with the wrong clips.

Turn **Random Start Phase** on for **Idle**.

### 5. Fly

**Flock → Spawn Flock Volume in Current Level**, set **Species** and **Spawn Count**, press Play. Walk at
them.

<!-- TODO(image): birds scattered across a volume, one alert -->

---

## If it's wrong

In order of likelihood.

| Symptom | Cause |
|---|---|
| No birds at all | No valid **Idle** mapping, or the mesh failed to load. Both log to `LogFlock` |
| Birds but no animation | `stat flock` shows `Anim` at zero - the processors aren't running, see below |
| Frozen on the bind pose | `NumCustomDataFloats` disagrees with `AutoPlay`. Set `bAutoPlay` on the **data asset**, never on the material instance - the bake overwrites it |
| Clip changes snap | No pose match table, or it went stale on a re-bake and is being ignored. Right-click the species → **Validate Data** says which, then **Flock → Build Pose Match Table** |
| Animation still steppy with Interpolate on | Only one of the two halves is set. The material instance needs **Interpolate** and the species needs **Interpolate Frames** |
| Wrong clip plays | An index taken from your source list rather than the enabled sequences |
| Every bird moves identically | **Random Start Phase** off on Idle |
| Birds standing in the air | Nothing under the volume to trace against, or the ground is too steep for **Min Ground Normal Z**. `LogFlock` warns |
| Birds fly up through a roof | Birds have no collision at all. Put a **Flock Blocking Volume** there - see [Blocking volumes](./FLOCK.md#blocking-volumes) |
| Playback is noise | sRGB, mips or compression changed on a baked texture. The bake sets these; leave them |
| Birds pop at screen edges | Bounds extensions missing on the static mesh |
| Bake fails, *"Already used by LightMap"* | Lightmap index equals the data asset's `UVChannel` |
| Prepare fails, *"has sequences this recipe does not list"* | An animation was added to the data asset directly. The recipe owns that list, so add it to **Anim Sequences** too - preparing would otherwise drop it and move every later index |

Watch **`LogAnimToTextureEditor`** during a bake; the plugin's own message log listing is never written to.

**Debug draws**, all non-shipping:

```
flock.Debug.Perception 1    bird state, alert level, and the clip name and frame it is playing
flock.Debug.Perception 2    also threat sources
flock.Debug.Perception 3    also flock bounds and the attractor
flock.Debug.Slots 1         perch slots: green free, yellow reserved, red occupied
flock.Debug.Slots 2         also which bird holds each one
flock.PoseMatch 0           clips open on their first frame, to A/B the pose match table
```

`Perception 1` is the one to reach for when a clip is wrong: it names the clip on the bird, so a bad index
shows up without opening the data asset.

Full symptom table in [`FLOCK.md`](./FLOCK.md#troubleshooting).

---

## Then tune it

Everything is on the species asset except the per-flock mix, which is on the volume. The knobs worth
reaching for first:

| | |
|---|---|
| **Proximity Exponent** (Perception) | how sharply alarm falls off with distance. **The knob for *where* birds break** - no radius change substitutes for it |
| **Perk Threshold**, **Flee Threshold** | when a bird looks up, and when it launches |
| **Threshold Jitter** | spread across birds, so a flock reacts raggedly rather than as one animal |
| **Orbit Preference** (volume) | chance a spooked bird wheels overhead rather than resettling. `0.5` visibly splits a flock |
| **Rest Interval Min/Max**, **Walk Interval Min/Max** | how busy a settled bird looks |
| **Restless Interval Min/Max**, **Ambient Airborne Fraction** | how much unprompted movement there is |
| **Weight** per rest break | raise on a preen, drop on a full body shake, and one is common while the other stays a treat |

The intervals are deliberately long. Shorten them and a flock reads as agitated rather than settled.

---

## Performance and profiling

`stat flock` in the console, or the **Flock** group in Unreal Insights. Both come from one macro, so they
can't drift apart.

| Group | Counters |
|---|---|
| Subsystem tick | `Tick`, `RefreshSources`, `Broadphase`, `SlotRequests`, `DrainEvents`, `RenderFlush` |
| Processors | `LOD`, `Threat`, `Decision`, `Idle`, `Takeoff`, `Flight`, `Landing`, `Anim`, `Render` |
| Counts | `Flocks`, `Birds`, `Sources`, `Instances Written`, and birds per tier (`Near`/`Mid`/`Far`/`Culled`) |

Read the counts alongside the cycles - a cycle figure means nothing without knowing how many birds
produced it.

**`Anim` and `Render` reading non-zero is the proof a flock is live.** Nothing in engine-core ticks Mass:
the only runtime class hosting a processing phase manager ships in the MassGameplay plugin, which this
project doesn't enable, so Flock runs its own pipeline from the subsystem tick. Those two at zero while
birds are placed means the processors aren't executing - which looks identical to a query matching
nothing.

### Actual costs

Roughly in order:

1. **Rendering.** One ISM primitive per flock, two calls per component per frame. Bird count barely moves
   the CPU; **triangles and overdraw** move the GPU. There's no depth prepass (`r.EarlyZPass=0`), so keep
   the mesh lean and the material **Opaque**.
2. **Separation.** O(n²) within a chunk, and the only thing here that isn't flat in flock size.
   **Separation Max Tier** decides how far out it runs (default **Near**) - the further out, the more it
   costs and the less anyone can see it.
3. **Everything else is flat or tag-gated.** Steering never queries neighbours. Threat cost is bounded at
   four sources per flock regardless of how many exist in the world, and a calm flock's chunks are skipped
   whole. Culled birds aren't visited.

### Measuring

`flock.Separation 0` / `1` forces separation off, or on at every tier; `-1` (default) leaves it to the
species. It's a cheat and compiles out of Shipping - the species setting is what ships.

Everything else is a setting rather than a cvar, under **Project Settings → Game → Flock**:

| | |
|---|---|
| **Near / Mid / Far Distance** | push **Near Distance** past the whole level to price the Near tier alone, then bring it in to see the tiers pay off |
| **Mid / Far Frame Divisor** | `1` for both prices the LOD striding, by removing it |
| **Max Birds Total**, **Max Instances Per Component** | the caps |
| **Enable Flock** | off prices the whole system out, without touching the level |

Bird count itself comes off the volume's **Spawn Count** - raise it on one volume rather than scaling
everything, so the counters stay attributable.

Bakes are traced too (`FFlockBake::PrepareAssets`, `::Bake`, `::AnimationToTexture`) - useful when a dense
mesh takes a while and you want to know which step.

## Changelog

### 1.0.0
* Initial Release
