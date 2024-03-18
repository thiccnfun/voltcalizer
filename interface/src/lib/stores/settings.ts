import type { AppSettings, EventStep } from '$lib/types';
import { writable } from 'svelte/store';

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

	correction_steps: [],
	affirmation_steps: [],
};

function createSettings() {
	const { subscribe, set, update } = writable(appSettings);

	return {
		subscribe,
		updateSettings: (settings: any) => {
			update((existing) => ({
				...existing,
				...settings,
			}));
		},
		addEventStep: (key: 'correction_steps' | 'affirmation_steps', step: EventStep) => {
			update((existing) => {
				existing[key].push(step);
				return existing;
			});
		},
		removeEventStep: (key: 'correction_steps' | 'affirmation_steps', index: number) => {
			update((existing) => {
				existing[key].splice(index, 1);
				return existing;
			});
		},
	};
}

export const settings = createSettings();
