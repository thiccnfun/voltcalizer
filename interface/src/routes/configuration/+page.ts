import type { PageLoad } from './$types';

export const load = (async ({ fetch }) => {
	return {
		title: 'Voltcalizer - Configuration'
	};
}) satisfies PageLoad;
