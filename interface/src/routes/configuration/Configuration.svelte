<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	// import { tweened } from 'svelte/motion';
	import { user } from '$lib/stores/user';
	import { page } from '$app/stores';
	import { notifications } from '$lib/components/toasts/notifications';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import AdjustmentsHor from '~icons/tabler/adjustments-horizontal';
	// import ClockBolt from '~icons/tabler/clock-bolt';
	// import Microphone from '~icons/tabler/microphone';
	// import { cubicOut } from 'svelte/easing';
    import RangeSlider from 'svelte-range-slider-pips';
	import TestCollarModal from './TestCollarModal.svelte';

	// type AppSettings = {
	// 	led_on: boolean;
	// };
	type AppSettings = {
		idle_period_min_ms: number;
		idle_period_max_ms: number;
		action_period_min_ms: number;
		action_period_max_ms: number;
		decibel_threshold_min: number;
		decibel_threshold_max: number;
		mic_sensitivity: 26 | 27 | 28 | 29;

		collar_min_shock: number;
		collar_max_shock: number;
		collar_min_vibe: number;
		collar_max_vibe: number;
	};

	let appSettings: AppSettings = {
		idle_period_min_ms: 5000,
		idle_period_max_ms: 30000,
		action_period_min_ms: 3000,
		action_period_max_ms: 5000,
		decibel_threshold_min: 80,
		decibel_threshold_max: 95,
		mic_sensitivity: 27,

		collar_min_shock: 5,
		collar_max_shock: 75,
		collar_min_vibe: 5,
		collar_max_vibe: 100,
	};
	// let ecdMax = Infinity;
	// const ecdProgress = tweened(0, {
	// 	duration: 400,
	// 	easing: cubicOut
	// });
	let testCollarModalOpen = false;
	let shockRanges = [appSettings.collar_min_shock, appSettings.collar_max_shock];
	let vibeRanges = [appSettings.collar_min_vibe, appSettings.collar_max_vibe];

	// async function getAppSettings() {
	// 	try {
	// 		const response = await fetch('/rest/appSettings', {
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
		const message = JSON.parse(event.data);
		if (message.type != 'id') {
			appSettings = message;

			shockRanges = [appSettings.collar_min_shock, appSettings.collar_max_shock];

			// if (micState.ecd > ecdMax || ecdMax === Infinity) {
			// 	ecdMax = micState.ecd;
			// 	ecdProgress.set(0);
			// } else {
			// 	ecdProgress.set(1 - micState.ecd / ecdMax);
			// }
		}
	};

	onDestroy(() => appSettingsSocket.close());

	async function updateSettings (settings: AppSettings) {
		appSettingsSocket.send(JSON.stringify({ 
			...appSettings,
			...settings
		}));
	}

	let formField: any;

	function handleSubmit() {
		const fd = new FormData(formField)
		
		const {
			mic_sensitivity_26 = null,
			mic_sensitivity_27 = null,
			mic_sensitivity_28 = null,
			mic_sensitivity_29 = null,
			...formObject
		} = Object.fromEntries(fd.entries());

		const newSettings: AppSettings = {
			idle_period_max_ms: Number(formObject.idle_period_max_ms) * 1000,
			idle_period_min_ms: Number(formObject.idle_period_min_ms) * 1000,
			action_period_max_ms: Number(formObject.action_period_max_ms) * 1000,
			action_period_min_ms: Number(formObject.action_period_min_ms) * 1000,
			decibel_threshold_max: Number(formObject.decibel_threshold_max),
			decibel_threshold_min: Number(formObject.decibel_threshold_min),
			mic_sensitivity: !!mic_sensitivity_26 ? 26 : !!mic_sensitivity_27 ? 27 : !!mic_sensitivity_28 ? 28 : 29,

			collar_min_shock: shockRanges[0],
			collar_max_shock: shockRanges[1],
			collar_min_vibe: vibeRanges[0],
			collar_max_vibe: vibeRanges[1],
		};

		updateSettings(newSettings);
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
			  <input type="radio" name="my-accordion-4" checked={true} /> 
			  <div class="collapse-title text-xl font-medium">
				<!-- <ClockBolt class="lex-shrink-0 mr-2 h-4 w-4 self-center" /> -->
				Routine Settings
			  </div>
			  <div class="collapse-content"> 

                <div class="form-control">
                    <label class="label">
                        <span class="label-text">Idle Duration (seconds)</span>
                    </label>
                    <label class="input-group">
                        <span>Min</span>
                        <input name="idle_period_min_ms" type="number" placeholder="5" class="input input-bordered" value={appSettings.idle_period_min_ms / 1000} />
                        <span>Max</span>
                        <input name="idle_period_max_ms" type="number" placeholder="30" class="input input-bordered" value={appSettings.idle_period_max_ms / 1000} />
                    </label>
                </div>
                <div class="form-control">
                    <label class="label">
                        <span class="label-text">Action Period (seconds)</span>
                    </label>
                    <label class="input-group">
                        <span>Min</span>
                        <input name="action_period_min_ms" type="number" placeholder="3" class="input input-bordered" value={appSettings.action_period_min_ms / 1000} />
                        <span>Max</span>
                        <input name="action_period_max_ms" type="number" placeholder="5" class="input input-bordered" value={appSettings.action_period_max_ms / 1000} />
                    </label>
                </div>



			  </div>
			</div>
			<div class="collapse collapse-arrow join-item border border-base-300">
			  <input type="radio" name="my-accordion-4" /> 
			  <div class="collapse-title text-xl font-medium">
				<!-- <Microphone class="lex-shrink-0 mr-2 h-4 w-4 self-center" /> -->
				Input Settings
			  </div>
			  <div class="collapse-content"> 
                <div class="form-control">
                    <label class="label">
                        <span class="label-text">Loudness Threshold (dB)</span>
                    </label>
                    <label class="input-group">
                        <span>Min</span>
                        <input name="decibel_threshold_min" type="number" placeholder="80" class="input input-bordered" value={appSettings.decibel_threshold_min} />
                        <span>Max</span>
                        <input name="decibel_threshold_max" type="number" placeholder="95" class="input input-bordered" value={appSettings.decibel_threshold_max} />
                    </label>
                </div>
                <div class="form-control">
                    <label class="label">
                        <span class="label-text">Microphone Sensitivity</span>
                    </label>
                    <div class="rating input-group">
                        <span>Less</span>
                        <input type="radio" name="mic_sensitivity_26" class="mask mask-hexagon" checked={appSettings.mic_sensitivity === 26} />
                        <input type="radio" name="mic_sensitivity_27" class="mask mask-hexagon" checked={appSettings.mic_sensitivity === 27} />
                        <input type="radio" name="mic_sensitivity_28" class="mask mask-hexagon" checked={appSettings.mic_sensitivity === 28} />
                        <input type="radio" name="mic_sensitivity_29" class="mask mask-hexagon" checked={appSettings.mic_sensitivity === 29} />
                        <span>More</span>
                    </div>
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
			  <input type="radio" name="my-accordion-4" /> 
			  <div class="collapse-title text-xl font-medium">
				<!-- <Microphone class="lex-shrink-0 mr-2 h-4 w-4 self-center" /> -->
				Collar Settings
			  </div>
			  <div class="collapse-content"> 
                <div class="form-control">
                    <label class="label">
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

		<div class="mt-6 place-self-end">
			<button class="btn btn-warning" on:click={() => testCollarModalOpen = true}>
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
