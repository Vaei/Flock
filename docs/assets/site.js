/* Copyright (c) Jared Taylor. All Rights Reserved */

/* The only file that knows which repo this is. Copy assets/docs.css + assets/docs.js
   into another plugin, replace this, and it has a documentation site. */

window.DOCS = {
	title: 'Flock',
	repo: 'https://github.com/Vaei/Flock',
	icon: 'assets/icon.png',
	imgDir: 'img/',
	footer: 'Flock is MIT licensed. &middot; <a href="shots.html">Art checklist</a>',

	sections: [
		{
			name: 'Start',
			pages: [
				{ file: 'index.html', label: 'Home', blurb: 'what this is' },
				{ file: 'install.html', label: 'Install', blurb: 'clone it, and get birds in a level' },
				{ file: 'bake.html', label: 'Bake a bird', blurb: 'skeletal mesh to instanced flock' }
			]
		},
		{
			name: 'Build a flock',
			pages: [
				{ file: 'placing.html', label: 'Placing a flock', blurb: 'the order that avoids doing it twice' },
				{ file: 'behaviour.html', label: 'Tuning behaviour', blurb: 'symptom to knob' },
				{ file: 'ambient.html', label: 'Ambient', blurb: 'birds as atmosphere' },
				{ file: 'reactive.html', label: 'Reactive', blurb: 'birds the player reads' }
			]
		},
		{
			name: 'Reference',
			pages: [
				{ file: 'species.html', label: 'Species', blurb: 'the asset, its clips, its breaks' },
				{ file: 'perception.html', label: 'Being noticed', blurb: 'what alarms birds, and scaring them' },
				{ file: 'flight.html', label: 'Flying', blurb: 'takeoff, wheeling, coming down' },
				{ file: 'idle.html', label: 'Idling', blurb: 'what a settled bird does' },
				{ file: 'perches.html', label: 'Perches and blockers', blurb: 'where birds land, and will not go' },
				{ file: 'blending.html', label: 'Blending', blurb: 'clips cut; two things soften it' },
				{ file: 'audio.html', label: 'Sound and VFX', blurb: 'a bed per flock, one-shots per bird' },
				{ file: 'performance.html', label: 'Cost', blurb: 'LOD, counters, what to measure' },
				{ file: 'api.html', label: 'In your own code', blurb: 'scares, events, scripting the bake' }
			]
		},
		{
			name: 'Meta',
			pages: [
				{ file: 'example.html', label: 'The Crow', blurb: 'one bird, start to finish' },
				{ file: 'troubleshooting.html', label: 'If it is wrong', blurb: 'symptom to cause' },
				{ file: 'changelog.html', label: 'Changelog' },
				{ file: 'shots.html', label: 'Art checklist' }
			]
		}
	],

	/* Figure slots. Declared once here, placed on a page by id alone.
	   A file that is not in img/ renders as a one-line placeholder instead of a gap,
	   so a page is the same length before and after the art exists. */
	shots: {
		'index.hero': { page: 'index.html', cap: 'A flock on a rooftop, one bird mid-takeoff', file: 'index-hero.png' },
		'index.tour': { page: 'index.html', cap: 'Idling, noticing, scattering, coming back down', vid: 'index-tour.mp4', poster: 'index-tour-poster.png' },
		'index.stat': { page: 'index.html', cap: 'stat flock over a hundred birds', file: 'index-stat.png' },

		'install.menu': { page: 'install.html', cap: 'The Flock toolbar menu, open', file: 'install-menu.png' },
		'install.volume': { page: 'install.html', cap: 'A flock volume in the level, box visible, details panel beside it', file: 'install-volume.png' },
		'install.first': { page: 'install.html', cap: 'Five birds on the ground, first play', file: 'install-first.png' },

		'bake.matsettings': { page: 'bake.html', cap: 'The material details panel with the three required settings ticked', file: 'bake-matsettings.png' },
		'bake.graph': { page: 'bake.html', cap: 'MakeMaterialAttributes into MF_FlockBoneAnimation into the output', file: 'bake-graph.png' },
		'bake.window': { page: 'bake.html', cap: 'The bake window, filled in', file: 'bake-window.png' },
		'bake.assets': { page: 'bake.html', cap: 'What Prepare Asset Set produced', file: 'bake-assets.png' },
		'bake.data': { page: 'bake.html', cap: 'The data asset, showing NumFrames, NumBones and the Animations array', file: 'bake-data.png' },
		'bake.clips': { page: 'bake.html', cap: 'The species Clips map, expanded', file: 'bake-clips.png' },
		'bake.preview': { page: 'bake.html', cap: 'A preview grid animating in the viewport, no PIE', vid: 'bake-preview.mp4', poster: 'bake-preview-poster.png' },

		'placing.circuit': { page: 'placing.html', cap: 'The volume selected, drawing the circuit airborne birds wheel around', file: 'placing-circuit.png' },
		'placing.ground': { page: 'placing.html', cap: 'Snap To Ground off leaves birds on the plane through the box centre', file: 'placing-ground-off.png', compare: 'placing-ground-on.png', compareLabels: ['Off', 'On'] },
		'placing.perches': { page: 'placing.html', cap: 'Perch slots drawn along a roof ridge and a fence rail', file: 'placing-perches.png' },
		'placing.blocker': { page: 'placing.html', cap: 'A blocking box over a roof, and the flock curving around it', file: 'placing-blocker.png' },
		'placing.density': { page: 'placing.html', cap: 'The same volume at 8, 20 and 60 birds', file: 'placing-density.png' },

		'behaviour.falloff': { page: 'behaviour.html', cap: 'Proximity Exponent 1 breaks the flock from across the square; 3 lets you walk most of the way in', file: 'behaviour-falloff-1.png', compare: 'behaviour-falloff-3.png', compareLabels: ['Exponent 1', 'Exponent 3'] },
		'behaviour.split': { page: 'behaviour.html', cap: 'Orbit Preference 0.5: half resettle, half wheel overhead', vid: 'behaviour-split.mp4', poster: 'behaviour-split-poster.png' },
		'behaviour.cascade': { page: 'behaviour.html', cap: 'Contagion turning one bird leaving into a flock leaving', vid: 'behaviour-cascade.mp4', poster: 'behaviour-cascade-poster.png' },
		'behaviour.jitter': { page: 'behaviour.html', cap: 'Threshold Jitter 0 launches every bird on one frame; 0.25 ragged', file: 'behaviour-jitter.png' },

		'ambient.scene': { page: 'ambient.html', cap: 'A settled flock nobody is looking at, with two birds wheeling above it', vid: 'ambient-scene.mp4', poster: 'ambient-scene-poster.png' },
		'ambient.roofline': { page: 'ambient.html', cap: 'Birds along a roofline, spaced by their perch slots', file: 'ambient-roofline.png' },

		'reactive.scare': { page: 'reactive.html', cap: 'A Scare Flock notify on a swing, and the flock going', vid: 'reactive-scare.mp4', poster: 'reactive-scare-poster.png' },
		'reactive.tell': { page: 'reactive.html', cap: 'Birds breaking over a wall, seen by a player who cannot see what caused it', file: 'reactive-tell.png' },

		'species.asset': { page: 'species.html', cap: 'A species asset, whole', file: 'species-asset.png' },
		'species.breaks': { page: 'species.html', cap: 'The Rest Breaks array, collapsed, showing the names', file: 'species-breaks.png' },
		'species.mirror': { page: 'species.html', cap: 'A mirrored break: one entry, two animations', file: 'species-mirror.png' },

		'perception.sources': { page: 'perception.html', cap: 'flock.Debug.Perception 2, threat sources drawn with their radii', file: 'perception-sources.png' },
		'perception.states': { page: 'perception.html', cap: 'flock.Debug.Perception 1, bird state and alert level per bird', file: 'perception-states.png' },
		'perception.perk': { page: 'perception.html', cap: 'Perked but not fleeing: heads up, tracking, still on the wall', file: 'perception-perk.png' },

		'flight.takeoff': { page: 'flight.html', cap: 'A launch, from the ground to the circuit', vid: 'flight-takeoff.mp4', poster: 'flight-takeoff-poster.png' },
		'flight.bank': { page: 'flight.html', cap: 'Bank Scale 0 against 35 on the same turn', file: 'flight-bank-0.png', compare: 'flight-bank-35.png', compareLabels: ['Bank Scale 0', 'Bank Scale 35'] },
		'flight.landing': { page: 'flight.html', cap: 'The approach: line up above the slot, drop, flare, settle', vid: 'flight-landing.mp4', poster: 'flight-landing-poster.png' },

		'idle.breaks': { page: 'idle.html', cap: 'A preen, a head cock and a caw, on three birds', vid: 'idle-breaks.mp4', poster: 'idle-breaks-poster.png' },
		'idle.walk': { page: 'idle.html', cap: 'A bird dawdling within its walk radius of where it spawned', file: 'idle-walk.png' },
		'idle.restless': { page: 'idle.html', cap: 'A voluntary move: one bird crossing to a perch, nothing wrong', vid: 'idle-restless.mp4', poster: 'idle-restless-poster.png' },

		'perches.sources': { page: 'perches.html', cap: 'The four slot sources: box, sockets, spline, explicit', file: 'perches-sources.png' },
		'perches.draw': { page: 'perches.html', cap: 'A selected perch component: disc, facing arrow, slot index', file: 'perches-draw.png' },
		'perches.slots': { page: 'perches.html', cap: 'flock.Debug.Slots 1 in play: green free, yellow reserved, red occupied', file: 'perches-slots.png' },
		'perches.margin': { page: 'perches.html', cap: 'flock.Debug.Perception 3 drawing a blocker and its avoid margin', file: 'perches-margin.png' },

		'blending.posematch': { page: 'blending.html', cap: 'Leaving Fly for Glide, with the pose match table and with flock.PoseMatch 0', vid: 'blending-posematch.mp4', poster: 'blending-posematch-poster.png' },
		'blending.interp': { page: 'blending.html', cap: 'A slow wingbeat at 30 fps playback, whole frames against interpolated', vid: 'blending-interp.mp4', poster: 'blending-interp-poster.png' },

		'audio.bed': { page: 'audio.html', cap: 'The bed MetaSound, showing the four inputs and two triggers', file: 'audio-bed.png' },
		'audio.break': { page: 'audio.html', cap: 'A caw break, expanded: sounds, trigger and delay', file: 'audio-break.png' },

		'performance.stat': { page: 'performance.html', cap: 'stat flock, cycles and counts together', file: 'performance-stat.png' },
		'performance.tiers': { page: 'performance.html', cap: 'Near, Mid, Far and Culled counts as the camera pulls back', file: 'performance-tiers.png' },
		'performance.insights': { page: 'performance.html', cap: 'The Flock group in Unreal Insights', file: 'performance-insights.png' },

		'example.content': { page: 'example.html', cap: "The crow's content folder, before the bake", file: 'example-content.png' },
		'example.anims': { page: 'example.html', cap: 'All 21 sequences', vid: 'example-anims.mp4', poster: 'example-anims-poster.png' },
		'example.material': { page: 'example.html', cap: "M_Flock's graph and its details panel", file: 'example-material.png' },
		'example.window': { page: 'example.html', cap: 'The bake window as the crow was baked', file: 'example-window.png' },
		'example.folder': { page: 'example.html', cap: 'The Flock folder after the bake', file: 'example-folder.png' },
		'example.data': { page: 'example.html', cap: 'DA_Crow_BoneAnimation: NumBones 18, and the Animations array', file: 'example-data.png' },
		'example.clips': { page: 'example.html', cap: "The species' Clips map, expanded", file: 'example-clips.png' },
		'example.breaks': { page: 'example.html', cap: 'The eight rest break entries', file: 'example-breaks.png' },
		'example.caw': { page: 'example.html', cap: 'The Caw entry, expanded, showing its sound and delay', file: 'example-caw.png' },
		'example.bed': { page: 'example.html', cap: 'MSS_FlockBed_Crow and the species audio slots', file: 'example-bed.png' },
		'example.volume': { page: 'example.html', cap: 'The volume in the viewport, box visible, details beside it', file: 'example-volume.png' },
		'example.perch': { page: 'example.html', cap: 'A perch component on a fence, slots drawn', file: 'example-perch.png' },
		'example.result': { page: 'example.html', cap: 'The crows idling, then scattering as the player walks in', vid: 'example-result.mp4', poster: 'example-result-poster.png' }
	}
};
