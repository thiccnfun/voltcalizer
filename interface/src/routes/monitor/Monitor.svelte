<script lang="ts">
	import { onMount, onDestroy } from 'svelte';
	import { tweened } from 'svelte/motion';
	import { user } from '$lib/stores/user';
	import { page } from '$app/stores';
	import { notifications } from '$lib/components/toasts/notifications';
	import SettingsCard from '$lib/components/SettingsCard.svelte';
	import HeatRateMonitor from '~icons/tabler/heart-rate-monitor';
	// import Info from '~icons/tabler/info-circle';
	// import Save from '~icons/tabler/device-floppy';
	// import Reload from '~icons/tabler/reload';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { Chart, registerables } from 'chart.js';
	// import Metrics from '~icons/tabler/report-analytics';
	import { daisyColor } from '$lib/DaisyUiHelper';
	// import { analytics } from '$lib/stores/analytics';

	// type LightState = {
	// 	led_on: boolean;
	// };
	type MicState = {
		dbt: number; // decibel threshold
		dbv: number; // decibel value
		ecd: number; // event count down
		en: boolean; // enabled evaluation
		dpr: number; // decibel pass rate
	};

	let micState: MicState = { dbt: 80, dbv: 0, ecd: Infinity, dpr: 0, en: false };
	let ecdMax = Infinity;
	const ecdProgress = tweened(0, {
		duration: 400,
		easing: cubicOut
	});

	const easing = (t: number) => t;

	// async function getLightstate() {
	// 	try {
	// 		const response = await fetch('/rest/micState', {
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

	const micStateSocket = new WebSocket('ws://' + $page.url.host + '/ws/micState' + ws_token);

	micStateSocket.onopen = (event) => {
		micStateSocket.send('Hello');
	};

	micStateSocket.addEventListener('close', (event) => {
		const closeCode = event.code;
		const closeReason = event.reason;
		console.log('WebSocket closed with code:', closeCode);
		console.log('Close reason:', closeReason);
		notifications.error('Websocket disconnected', 5000);
	});

	micStateSocket.onmessage = (event) => {
		const message = JSON.parse(event.data);
		if (message.type != 'id') {
			if (message.ecd > ecdMax || ecdMax === Infinity) {
				ecdMax = message.ecd;
			} 
			if (message.ecd <= 0) {
				ecdMax = Infinity;
				ecdProgress.set(0);
			} else if (message.ecd > micState.ecd) {
				ecdProgress.set(1, { duration: message.ecd, easing });
			}
			micState = message;
		}
	};

	onDestroy(() => micStateSocket.close());


	function toggleEvaluator () {
		micStateSocket.send(JSON.stringify({ en: !micState.en }));
	}

	// chart

	Chart.register(...registerables);

	let heapChartElement: HTMLCanvasElement;
	let heapChart: Chart;
	let stateHistory = [];




	onMount(() => {
		// getLightstate();

		heapChart = new Chart(heapChartElement, {
			type: 'line',
			data: {
				labels: [],
				datasets: [
					{
						label: 'Decibels',
						borderColor: daisyColor('--p'),
						backgroundColor: daisyColor('--p', 50),
						borderWidth: 2,
						fill: false,
						cubicInterpolationMode: 'monotone',
      					tension: -0.4,
						data: stateHistory,
						yAxisID: 'y',
						pointStyle: false,
						parsing: {
							yAxisKey: 'dbv',
							xAxisKey: 'dbv',
						}
					},
					{
						label: 'Target dB',
						borderColor: daisyColor('--a', 50),
						backgroundColor: daisyColor('--a', 20),
						borderWidth: 2,
						fill: true,
						// cubicInterpolationMode: 'monotone',
	  					// tension: -0.4,
						data: stateHistory,
						yAxisID: 'y',
						pointStyle: false,
						parsing: {
							yAxisKey: 'dbt',
							xAxisKey: 'dbv',
						}
					}
				]
			},
			options: {
				maintainAspectRatio: false,
				responsive: true,
				spanGaps: false,
				plugins: {
					legend: {
						display: true
					},
					tooltip: {
						mode: 'index',
						intersect: false
					}
				},
				elements: {
					point: {
						radius: 1
					}
				},
				scales: {
					x: {
						grid: {
							color: daisyColor('--bc', 10)
						},
						ticks: {
							color: daisyColor('--bc')
						},
						display: false
					},
					y: {
						// type: 'linear',
						title: {
							display: false,
							text: 'Decibels',
							color: daisyColor('--bc'),
							font: {
								size: 16,
								weight: 'bold'
							}
						},
						position: 'left',
						// min: 40,
						// max: 90,
						suggestedMin: 50,
        				suggestedMax: 90,
						grid: { color: daisyColor('--bc', 10) },
						ticks: {
							color: daisyColor('--bc')
						},
						border: { color: daisyColor('--bc', 10) }
					}
				}
			}
		});

		loop();
	});

	function loop () {
		updateData();
		requestAnimationFrame(loop);
	};

	function updateData() {
		stateHistory.push({
			...micState,
			dbt: micState.dbt === 0 ? NaN : micState.dbt
		});

		
		heapChart.data.labels.push(heapChart.data.labels.length);

		stateHistory = stateHistory.slice(Math.max(stateHistory.length - 500, 0));		// heapChart.data.datasets[0].data = heapChart.data.datasets[0].data.slice(
		// 	Math.max(heapChart.data.datasets[0].data.length - 500, 0)
		// );
		// heapChart.data.datasets[1].data = heapChart.data.datasets[1].data.slice(
		// 	Math.max(heapChart.data.datasets[1].data.length - 500, 0)
		// );
		heapChart.data.labels = heapChart.data.labels.slice(Math.max(heapChart.data.labels.length - 500, 0));
		heapChart.data.datasets[0].data = stateHistory;
		heapChart.data.datasets[1].data = micState.en ? stateHistory : [];

		
		// if (stateHistory[stateHistory.length - 1].dpr > 0) {
		// 	console.log('-------------', stateHistory[stateHistory.length - 1]);
		// 	heapChart.data.datasets[1].backgroundColor = `rgba(0,255,0,0.5)`
		// }


		// console.log('dbData:', heapChart.data.datasets[0].data);

		// heapChart.data.labels = $analytics.uptime;
		// heapChart.data.datasets[0].data = $analytics.free_heap;
		// heapChart.data.datasets[1].data = $analytics.max_alloc_heap;
		heapChart.update('none');
	}

	// function convertSeconds(seconds: number) {
	// 	// Calculate the number of seconds, minutes, hours, and days
	// 	let minutes = Math.floor(seconds / 60);
	// 	let hours = Math.floor(minutes / 60);
	// 	let days = Math.floor(hours / 24);

	// 	// Calculate the remaining hours, minutes, and seconds
	// 	hours = hours % 24;
	// 	minutes = minutes % 60;
	// 	seconds = seconds % 60;

	// 	// Create the formatted string
	// 	let result = '';
	// 	if (days > 0) {
	// 		result += days + ' day' + (days > 1 ? 's' : '') + ' ';
	// 	}
	// 	if (hours > 0) {
	// 		result += hours + ' hour' + (hours > 1 ? 's' : '') + ' ';
	// 	}
	// 	if (minutes > 0) {
	// 		result += minutes + ' minute' + (minutes > 1 ? 's' : '') + ' ';
	// 	}
	// 	result += seconds + ' second' + (seconds > 1 ? 's' : '');

	// 	return result;
	// }





	// async function postLightstate() {
	// 	try {
	// 		const response = await fetch('/rest/lightState', {
	// 			method: 'POST',
	// 			headers: {
	// 				Authorization: $page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
	// 				'Content-Type': 'application/json'
	// 			},
	// 			body: JSON.stringify({ led_on: lightOn })
	// 		});
	// 		if (response.status == 200) {
	// 			notifications.success('Light state updated.', 3000);
	// 			const light = await response.json();
	// 			lightOn = light.led_on;
	// 		} else {
	// 			notifications.error('User not authorized.', 3000);
	// 		}
	// 	} catch (error) {
	// 		console.error('Error:', error);
	// 	}
	// }
</script>

<SettingsCard collapsible={false}>
	<HeatRateMonitor slot="icon" class="lex-shrink-0 mr-2 h-6 w-6 self-end" />
	<span slot="title">Monitor</span>
	<div class="w-full overflow-x-auto">
		<div
			class="flex w-full flex-col space-y-1 h-60"
			transition:slide|local={{ duration: 300, easing: cubicOut }}
		>
			<canvas bind:this={heapChartElement} />
		</div>
		<progress 
			class="
				progress w-100 
				{
					(ecdMax !== Infinity && (micState.ecd / ecdMax < 0.33)) ? 
					'progress-error blink' : 
					(ecdMax !== Infinity && (micState.ecd / ecdMax < 0.66)) ?
					'progress-warning' 
					: ''
				}" 
			value={$ecdProgress} 
		/>
		<div class="mt-6 flex">
			{#if micState.en}
				<button class="btn btn-primary" type="submit" on:click={toggleEvaluator}>
					Stop Evaluating
				</button>
			{:else}
				<button class="btn btn-primary" type="submit" on:click={toggleEvaluator}>
					Start Evaluating
				</button>
			{/if}

		</div>
	</div>
</SettingsCard>
