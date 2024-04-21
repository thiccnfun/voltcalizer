export enum EventType {
    COLLAR_BEEP,
    COLLAR_VIBRATION,
    COLLAR_SHOCK,
}

export enum RangeType {
    FIXED,
    RANDOM,
    PROGRESSIVE,
    REDEEMABLE,
    GRADED,
}

export enum AlertType {
    NONE,
    COLLAR_BEEP,
    COLLAR_VIBRATION,
}

export type AppSettings = {
    idle_period_min_ms: number;
    idle_period_max_ms: number;
    action_period_min_ms: number;
    action_period_max_ms: number;
    decibel_threshold_min: number;
    decibel_threshold_max: number;
    mic_sensitivity: 26 | 27 | 28 | 29;
    alert_type: AlertType;

    collar_min_shock: number;
    collar_max_shock: number;
    collar_min_vibe: number;
    collar_max_vibe: number;

    correction_steps: EventStep[];
    affirmation_steps: EventStep[];
};

export type EventStep = {
    type: EventType;
    start_delay: number;
    end_delay: number;
    time_range_type: RangeType;
    time_range: [number, number] | [number];
    strength_range_type: RangeType;
    strength_range: [number, number] | [number];
};