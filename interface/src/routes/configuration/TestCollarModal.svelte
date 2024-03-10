<script lang="ts">
    import { onMount } from 'svelte';
    import { user } from '$lib/stores/user';
	import { page } from '$app/stores';
    import RangeSlider from 'svelte-range-slider-pips';
    import Bolt from '~icons/tabler/bolt';
    import Volume from '~icons/tabler/volume';
    import MDVibration from '~icons/tabler/device-mobile-vibration';
    export let close: () => void;
    // import { modal } from 'daisyui/dist/full.js';


    let type = 'beep';
    let values = [0.1];
    let duration = [0.5];
    let isWorking = false;

	async function testCollar() {
        if (isWorking) return;
        isWorking = true;
		try {
			const response = await fetch('/rest/testCollar', {
				method: 'POST',
				headers: {
					Authorization: $page.data.features.security ? 'Bearer ' + $user.bearer_token : 'Basic',
					'Content-Type': 'application/json'
				},
                body: JSON.stringify({
                    type,
                    duration: duration[0] * 1000,
                    value: values[0],
                })
			});
			const result = await response.json();

            isWorking = false;
		} catch (error) {
			console.error('Error:', error);
            isWorking = false;
		}
		return;
	}


    const handleFormatter = (value: number) => `${value} second${value !== 1 ? 's' : ''}`;

    onMount(() => {
        // modal('#my-modal');
    });

</script>

<div class="modal modal-open">
    <div class="modal-box">
        <h2 class="text-xl">Test Collar</h2>
        <div class="form-control mt-4">
            <select class="select select-bordered w-full" bind:value={type}>
                <option value="beep">Beep</option>
                <option value="vibration">Vibration</option>
                <option value="shock">Shock</option>
            </select>
        </div>
        <div class="form-control mt-2">
            <label for="strength">
                Strength
            </label>
            <RangeSlider 
                name="strength"
                pips 
                float 
                min={0.1}
                max={99}
                pipstep={10}
                first=label 
                last=label 
                bind:values 
                disabled={type === 'beep'} 
            />
        </div>
        <div class="form-control mt-2">
            <label for="duration">
                Duration
            </label>
            <RangeSlider 
                {handleFormatter} 
                name="duration"
                min={0.5}
                max={5}
                pipstep={10}
                step={0.1}
                pips
                float
                all="label"
                bind:values={duration}
            />
        </div>
        <div class="modal-action">
            <button on:click={testCollar} class="btn btn-warning mr-auto" disabled={isWorking}>
                {#if isWorking}
                    <span class="loading loading-spinner"></span>
                {:else if type === 'beep'}
                    <Volume class="icon" />
                {:else if type === 'vibration'}
                    <MDVibration class="icon" />
                {:else }
                    <Bolt class="icon" />
                {/if}

                Test {type.toUpperCase()}
            </button>
            <button on:click={close} class="btn">Close</button>
        </div>
    </div>
</div>