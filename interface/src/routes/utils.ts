const handleFormatterSeconds = (value: number) => `${value} second${value !== 1 ? 's' : ''}`;

const handleFormatterPercentage = (value: number) => `${Math.floor(value * 100)}%`;

export {
    handleFormatterSeconds,
    handleFormatterPercentage,
};