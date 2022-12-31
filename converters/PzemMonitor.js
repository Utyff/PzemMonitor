const {
    fromZigbeeConverters,
    toZigbeeConverters,
    exposes
} = require('zigbee-herdsman-converters');

const reporting = require('zigbee-herdsman-converters/lib/reporting');
const e = exposes.presets;
const ea = exposes.access;

const device = {
    zigbeeModel: ['DIYRuZ_PzemMonitor'],
    model: 'DIYRuZ_PzemMonitor',
    vendor: 'DIYRuZ',
    description: '[Energy Monitor](https://github.com/Utyff/PzemMonitor/)',
    supports: 'time',
    icon: 'https://raw.githubusercontent.com/Utyff/PzemMonitor/image/images/PZEM-004-module.jpg',
    fromZigbee: [fromZigbeeConverters.electrical_measurement],
    toZigbee: [toZigbeeConverters.acvoltage,
        toZigbeeConverters.accurrent,
        toZigbeeConverters.electrical_measurement_power,
        toZigbeeConverters.frequency,
        toZigbeeConverters.powerfactor,
    ],
    configure: async (device, coordinatorEndpoint, logger) => {
        const endpoint1 = device.getEndpoint(1);
        const endpoint2 = device.getEndpoint(2);
        const endpoint3 = device.getEndpoint(3);
        await reporting.bind(endpoint1, coordinatorEndpoint, ['haElectricalMeasurement']);
        await reporting.bind(endpoint2, coordinatorEndpoint, ['haElectricalMeasurement']);
        await reporting.bind(endpoint3, coordinatorEndpoint, ['haElectricalMeasurement']);

        await reporting.rmsVoltage(endpoint1, {min: 0, max: 3600, change: 0});
        await reporting.rmsCurrent(endpoint1, {min: 0, max: 3600, change: 0});
        await reporting.activePower(endpoint1, {min: 0, max: 3600, change: 0});
        await reporting.acFrequency(endpoint1, {min: 0, max: 3600, change: 0});
        await reporting.powerFactor(endpoint1, {min: 0, max: 3600, change: 0});

        await reporting.rmsVoltage(endpoint2, {min: 0, max: 3600, change: 0});
        await reporting.rmsCurrent(endpoint2, {min: 0, max: 3600, change: 0});
        await reporting.activePower(endpoint2, {min: 0, max: 3600, change: 0});
        await reporting.acFrequency(endpoint2, {min: 0, max: 3600, change: 0});
        await reporting.powerFactor(endpoint2, {min: 0, max: 3600, change: 0});

        await reporting.rmsVoltage(endpoint3, {min: 0, max: 3600, change: 0});
        await reporting.rmsCurrent(endpoint3, {min: 0, max: 3600, change: 0});
        await reporting.activePower(endpoint3, {min: 0, max: 3600, change: 0});
        await reporting.acFrequency(endpoint3, {min: 0, max: 3600, change: 0});
        await reporting.powerFactor(endpoint3, {min: 0, max: 3600, change: 0});
    },
    exposes: [e.voltage().withEndpoint('e1'),
        e.current().withEndpoint('e1'),
        e.power().withEndpoint('e1'),
        e.energy().withEndpoint('e1'),
        e.power_factor().withEndpoint('e1'),
        e.ac_frequency().withEndpoint('e1'),
        e.ac_frequency().withEndpoint('e2'),
        e.ac_frequency().withEndpoint('e3'),
    ],
    meta: {multiEndpoint: true},
    endpoint: (device) => {
        return {e1: 1, e2: 2, e3: 3};
    },
};

module.exports = device;
