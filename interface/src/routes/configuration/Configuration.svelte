<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	// import { tweened } from 'svelte/motion';
	import { user } from '$lib/stores/user';
	import { page } from '$app/stores';
	import { notifications } from '$lib/components/toasts/notifications';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import AdjustmentsHor from '~icons/tabler/adjustments-horizontal';
	import Clock from '~icons/tabler/clock';
	import Microphone from '~icons/tabler/microphone';
	import MoodSadDizzy from '~icons/tabler/mood-sad-dizzy';
	import MoodSmileDizzy from '~icons/tabler/mood-smile-dizzy';
	import DeviceWatchBolt from '~icons/tabler/device-watch-bolt';
	// import { cubicOut } from 'svelte/easing';
    import RangeSlider from 'svelte-range-slider-pips';
	import TestCollarModal from './TestCollarModal.svelte';
	import EventSeries from './EventSeries.svelte';
	import type { AppSettings } from '$lib/types';
	import { settings } from '$lib/stores/settings';

	// type AppSettings = {
	// 	led_on: boolean;
	// };


	// let $settings: AppSettings = {
	// 	idle_period_min_ms: 5000,
	// 	idle_period_max_ms: 30000,
	// 	action_period_min_ms: 3000,
	// 	action_period_max_ms: 5000,
	// 	decibel_threshold_min: 80,
	// 	decibel_threshold_max: 95,
	// 	mic_sensitivity: 27,

	// 	collar_min_shock: 5,
	// 	collar_max_shock: 75,
	// 	collar_min_vibe: 5,
	// 	collar_max_vibe: 100,

	// 	correction_steps: [],
	// 	affirmation_steps: [],
	// };
	// let ecdMax = Infinity;
	// const ecdProgress = tweened(0, {
	// 	duration: 400,
	// 	easing: cubicOut
	// });
	let testCollarModalOpen = false;
	let shockRanges = [$settings.collar_min_shock, $settings.collar_max_shock];
	let vibeRanges = [$settings.collar_min_vibe, $settings.collar_max_vibe];
	let idleRanges = [$settings.idle_period_min_ms / 1000, $settings.idle_period_max_ms / 1000];
	let actionRanges = [$settings.action_period_min_ms / 1000, $settings.action_period_max_ms / 1000];
	let loudnessRanges = [$settings.decibel_threshold_min, $settings.decibel_threshold_max];
	let micSensitivity = [$settings.mic_sensitivity];

	// async function getAppSettings() {
	// 	try {
	// 		const response = await fetch('/rest/$settings', {
	// 			method: 'GET',
	// 			headers: {
	// 				Authorization: $page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
	// 				'Content-Type': 'application/json'
	// 			}
	// 		});
	// 		const light = await response.json();
	// 		lightOn = light.led_on;
	// 	} catch (error) {
	// 		console.error('Error:', error);
	// 	}
	// 	return;
	// }

	const ws_token = $page.data.features.security ? '?access_token=' + $user.bearer_token : '';

	const appSettingsSocket = new WebSocket('ws://' + $page.url.host + '/ws/appSettings' + ws_token);

	// window.appSettingsSocket = appSettingsSocket;

	appSettingsSocket.onopen = (event) => {
		appSettingsSocket.send('Hello');
	};

	appSettingsSocket.addEventListener('close', (event) => {
		const closeCode = event.code;
		const closeReason = event.reason;
		console.log('WebSocket closed with code:', closeCode);
		console.log('Close reason:', closeReason);
		notifications.error('Websocket disconnected', 5000);
	});

	appSettingsSocket.onmessage = (event) => {
		const message = JSON.parse(event.data) as any;
		if (message.type != 'id') {
			// $settings = message;

			settings.updateSettings(message);

			shockRanges = [$settings.collar_min_shock, $settings.collar_max_shock];
			vibeRanges = [$settings.collar_min_vibe, $settings.collar_max_vibe];
			idleRanges = [$settings.idle_period_min_ms / 1000, $settings.idle_period_max_ms / 1000];
			actionRanges = [$settings.action_period_min_ms / 1000, $settings.action_period_max_ms / 1000];
			loudnessRanges = [$settings.decibel_threshold_min, $settings.decibel_threshold_max];

			// if (micState.ecd > ecdMax || ecdMax === Infinity) {
			// 	ecdMax = micState.ecd;
			// 	ecdProgress.set(0);
			// } else {
			// 	ecdProgress.set(1 - micState.ecd / ecdMax);
			// }
		}
	};

	onDestroy(() => appSettingsSocket.close());

	async function emitSettings (settings: AppSettings) {
		appSettingsSocket.send(JSON.stringify({ 
			...$settings,
			...settings
		}));
	}

	let formField: any;

	function handleSubmit(event: Event) {

		const fd = new FormData(formField)
		
		const {
			mic_sensitivity_26 = null,
			mic_sensitivity_27 = null,
			mic_sensitivity_28 = null,
			mic_sensitivity_29 = null,
			...formObject
		} = Object.fromEntries(fd.entries());

		const newSettings: AppSettings = {
			mic_sensitivity: !!mic_sensitivity_26 ? 26 : !!mic_sensitivity_27 ? 27 : !!mic_sensitivity_28 ? 28 : 29,
			
			decibel_threshold_min: loudnessRanges[0],
			decibel_threshold_max: loudnessRanges[1],
			action_period_min_ms: actionRanges[0] * 1000,
			action_period_max_ms: actionRanges[1] * 1000,
			idle_period_min_ms: idleRanges[0] * 1000,
			idle_period_max_ms: idleRanges[1] * 1000,
			collar_min_shock: shockRanges[0],
			collar_max_shock: shockRanges[1],
			collar_min_vibe: vibeRanges[0],
			collar_max_vibe: vibeRanges[1],

			correction_steps: $settings.correction_steps,
			affirmation_steps: $settings.affirmation_steps,
		};

		settings.updateSettings(newSettings);

		console.log(newSettings);

		emitSettings(newSettings);
	}

	function sensitivityFormatter(value: number) {
		return value < 28 ? 'Less' : 'More';
	}

	onMount(() => {
		// getAppSettings();
	});

</script>

<SettingsCard collapsible={false}>
	<AdjustmentsHor slot="icon" class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
	<span slot="title">Configuration</span>
	<div class="w-full overflow-x-auto">
	<form
		class="form-control w-full"
		on:submit|preventDefault={handleSubmit}
		novalidate
		bind:this={formField}
	>


		<div class="join join-vertical w-full">
			<div class="collapse collapse-arrow join-item border border-base-300">
			  <input type="radio" name="conf-accordion" checked={true} /> 
			  <div class="collapse-title text-xl font-medium flex">
				<Clock class="lex-shrink-0 mr-2 mt-2 h-4 w-4" />
				Routine Settings
			  </div>
			  <div class="collapse-content"> 
                <div class="form-control">
                    <label for="idle_duration" class="label">
                        <span class="label-text">Idle Duration (seconds)</span>
                    </label>
					<RangeSlider 
						range 
						pushy 
						pips 
						float 
						min={1} 
						max={300} 
						first=label 
						last=label 
						bind:values={idleRanges} 
					/>
                </div>
                <div class="form-control">
                    <label for="action_period" class="label">
                        <span class="label-text">Action Period (seconds)</span>
                    </label>
					<RangeSlider 
						range 
						pushy 
						pips 
						float 
						min={2} 
						max={30} 
						first=label 
						last=label 
						bind:values={actionRanges} 
					/>
                </div>
			  </div>
			</div>

			<div class="collapse collapse-arrow join-item border border-base-300">
			  <input type="radio" name="conf-accordion" checked={true} /> 
			  <div class="collapse-title text-xl font-medium flex">
				<MoodSmileDizzy class="lex-shrink-0 mr-2 mt-2 h-4 w-4" />
				Affirmation Settings
			  </div>
			  <div class="collapse-content"> 
                <EventSeries dataKey="affirmation_steps" />
			  </div>
			</div>

			<div class="collapse collapse-arrow join-item border border-base-300">
			  <input type="radio" name="conf-accordion" checked={true} /> 
			  <div class="collapse-title text-xl font-medium flex">
				<MoodSadDizzy class="lex-shrink-0 mr-2 mt-2 h-4 w-4" />
				Correction Settings
			  </div>
			  <div class="collapse-content"> 
                <EventSeries dataKey="correction_steps" />
			  </div>
			</div>




			<div class="collapse collapse-arrow join-item border border-base-300">
			  <input type="radio" name="conf-accordion" /> 
			  <div class="collapse-title text-xl font-medium flex">
				<Microphone class="lex-shrink-0 mr-2 mt-2 h-4 w-4" />
				Input Settings
			  </div>
			  <div class="collapse-content"> 
                <div class="form-control">
                    <label for="loudness" class="label">
                        <span class="label-text">Loudness Threshold (dB)</span>
                    </label>
					<RangeSlider 
						range 
						pushy 
						pips 
						float 
						min={30} 
						max={125} 
						first=label 
						last=label 
						bind:values={loudnessRanges} 
					/>
                </div>
                <div class="form-control">
                    <label class="label">
                        <span class="label-text">Microphone Sensitivity</span>
                    </label>
					<RangeSlider 
						pips 
						first=label
						last=label 
						min={26} 
						max={29} 
						bind:values={micSensitivity} 
						formatter={sensitivityFormatter}
					/>
                    <!-- <div class="rating input-group">
                        <span>Less</span>
                        <input type="radio" name="mic_sensitivity_26" class="mask mask-hexagon" checked={$settings.mic_sensitivity === 26} />
                        <input type="radio" name="mic_sensitivity_27" class="mask mask-hexagon" checked={$settings.mic_sensitivity === 27} />
                        <input type="radio" name="mic_sensitivity_28" class="mask mask-hexagon" checked={$settings.mic_sensitivity === 28} />
                        <input type="radio" name="mic_sensitivity_29" class="mask mask-hexagon" checked={$settings.mic_sensitivity === 29} />
                        <span>More</span>
                    </div> -->
                </div>
                <div class="form-control">
                    <label class="label">
                        <span class="label-text">Piezo Enabled</span>
                        <input type="checkbox" class="toggle toggle-info" checked />
                    </label>
                </div>
			  </div>
			</div>
			<div class="collapse collapse-arrow join-item border border-base-300">
			  <input type="radio" name="conf-accordion" /> 
			  <div class="collapse-title text-xl font-medium flex">
				<DeviceWatchBolt class="lex-shrink-0 mr-2 mt-2 h-4 w-4" />
				Collar Settings
			  </div>
			  <div class="collapse-content"> 
                <div class="form-control">
                    <label for="remote-id" class="label">
                        <span class="label-text">Remote ID</span>
                        <input name="collar_id" placeholder="13,37" class="input input-bordered max-w-4" />
                    </label>
                </div>
                <div class="form-control">
                    <label class="label">
                        <span class="label-text">Shock Strength Range</span>
                    </label>
                    <RangeSlider range pushy pips float first=label last=label bind:values={shockRanges} />
                </div>
                <div class="form-control">
                    <label class="label">
                        <span class="label-text">Vibration Strength Range</span>
                    </label>
                    <RangeSlider range pushy pips float first=label last=label bind:values={vibeRanges} />
                </div>
			  </div>
			</div>
		</div>

		<div class="mt-6 flex">
			<button class="btn btn-warning mr-auto" on:click={() => testCollarModalOpen = true}>
				Test Collar
			</button>
			<button class="btn btn-primary" type="submit">
				Apply Settings
			</button>
		</div>
	</form>
		
	</div>
</SettingsCard>

{#if testCollarModalOpen}
	<TestCollarModal close={() => testCollarModalOpen = false} />
{/if}
