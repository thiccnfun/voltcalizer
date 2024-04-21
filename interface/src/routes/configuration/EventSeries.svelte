<script lang="ts">
    import { onMount } from 'svelte';
	import { page } from '$app/stores';
    import RangeSlider from 'svelte-range-slider-pips';
    import Bolt from '~icons/tabler/bolt';
    import Volume from '~icons/tabler/volume';
    import MDVibration from '~icons/tabler/device-mobile-vibration';
	import { EventType, type EventStep, RangeType } from '$lib/types';
    import { settings } from '$lib/stores/settings';
    // export let close: () => void;
    // import { modal } from 'daisyui/dist/full.js';

    export let dataKey: "correction_steps" | "affirmation_steps";
    // export let steps: EventStep[];

    let steps = $settings[dataKey] as EventStep[];

    settings.subscribe(value => steps = value[dataKey]);

    const handleFormatterSeconds = (value: number) => `${value} second${value !== 1 ? 's' : ''}`;
    const handleFormatterPercentage = (value: number) => `${Math.floor(value * 100)}%`;

    function addStep() {
        const newStep: EventStep = {
            type: EventType.COLLAR_BEEP,
            start_delay: 0,
            end_delay: 0,
            time_range_type: RangeType.FIXED,
            time_range: [1],
            strength_range_type: RangeType.FIXED,
            strength_range: [0.1],
        };
        settings.addEventStep(dataKey, newStep);
    }

    function removeStep(index: number) {
        settings.removeEventStep(dataKey, index);
    }

    function typeLabel(type: EventType | string) {
        switch (type) {
            case EventType.COLLAR_BEEP:
                return "Collar Beep";
            case EventType.COLLAR_VIBRATION:
                return "Collar Vibration";
            case EventType.COLLAR_SHOCK:
                return "Collar Shock";
        }
    }

    function rangeTypeLabel(type: RangeType | string) {
        switch (type) {
            case RangeType.FIXED:
                return "Fixed";
            case RangeType.RANDOM:
                return "Random";
            case RangeType.PROGRESSIVE:
                return "Progressive";
            case RangeType.REDEEMABLE:
                return "Redeemable";
            case RangeType.GRADED:
                return "Graded";
        }
    }

    onMount(() => {
        // modal('#my-modal');
    });

</script>




<div class="join join-vertical w-full p-5">
    {#each steps as step, i}
    <div class="collapse collapse-arrow join-item border bg-base-100 border-base-300">
        <input type="radio" name={dataKey} checked={true} /> 
        <div class="collapse-title text-xl font-medium flex">
          <!-- <Clock class="lex-shrink-0 mr-2 mt-2 h-4 w-4" /> -->
          {i + 1} - {typeLabel(step.type)}
        </div>
        <div class="collapse-content"> 
            <div class="form-control">
                <select class="select select-bordered select-sm w-full" bind:value={step.type}>
                    {#each Object.values(EventType).filter(v => typeof v !== 'string') as type}
                        <option value={type}>{typeLabel(type)}</option>
                    {/each}
                </select>
            </div>
            {#if step.type !== EventType.COLLAR_BEEP}
            <div class="form-control mt-2">
                <label for="strength">
                    Strength
                    <select 
                        class="select select-bordered select-sm ml-3" 
                        bind:value={step.strength_range_type} 
                        on:change={() => {
                            if (step.strength_range_type === RangeType.FIXED) {
                                step.strength_range = [step.strength_range[0]];
                            } else if (step.strength_range.length === 1) {
                                step.strength_range = [step.strength_range[0], step.strength_range[0] + 10];
                            }
                        }}
                    >
                        {#each Object.values(RangeType).filter(v => typeof v !== 'string') as type}
                            <option value={type}>{rangeTypeLabel(type)}</option>
                        {/each}
                    </select>
                </label>
                <RangeSlider 
                    formatter={handleFormatterPercentage} 
                    pips 
                    float 
                    range={step.strength_range_type !== RangeType.FIXED}
                    pushy
                    min={0.01}
                    max={1}
                    step={0.01}
                    pipstep={10}
                    first=label 
                    last=label 
                    bind:values={step.strength_range}
                />
            </div>
            {/if}
            <div class="form-control mt-2">
                <label for="duration flex">
                    Duration
                    <select 
                        class="select select-bordered select-sm ml-3" 
                        bind:value={step.time_range_type}
                        on:change={() => {
                            if (step.time_range_type === RangeType.FIXED) {
                                step.time_range = [step.time_range[0]];
                            } else if (step.time_range.length === 1) {
                                step.time_range = [step.time_range[0], step.time_range[0] + 1];
                            }
                        }}
                    >
                        {#each Object.values(RangeType).filter(v => typeof v !== 'string') as type}
                            <option value={type}>{rangeTypeLabel(type)}</option>
                        {/each}
                    </select>
                </label>
                <RangeSlider 
                    handleFormatter={handleFormatterSeconds} 
                    range={step.time_range_type !== RangeType.FIXED}
                    pushy
                    min={0.5}
                    max={5}
                    pipstep={10}
                    step={0.1}
                    pips
                    float
                    all="label"
                    bind:values={step.time_range}
                />
            </div>
            <button on:click={() => removeStep(i)} class="btn w-full btn-warning btn-sm">
                Remove Event
            </button>
        </div>
    </div>
    {/each}
    <div class="flex justify-between gap-x-6 py-5">
        <button on:click|preventDefault={addStep} class="btn w-full btn-secondary btn-sm">
            Add an Event
        </button>
    </div>
</div>