const {
    fromZigbeeConverters,
    toZigbeeConverters,
    reporting,
    exposes
} = require('zigbee-herdsman-converters');

const e = exposes.presets;
const ea = exposes.access;


const device = {
    zigbeeModel: ['DIYRuZ_PzemMonitor'],
    model: 'DIYRuZ_PzemMonitor',
    vendor: 'DIYRuZ',
    description: '[Energy Monitor](https://github.com/Utyff/PzemMonitor/)',
    icon: 'https://raw.githubusercontent.com/Utyff/PzemMonitor/image/images/PZEM-004-module.jpg',
    fromZigbee: [fromZigbeeConverters.electrical_measurement],
    toZigbee: [],
    exposes: [e.power(), e.voltage(), e.current()],
    configure: async (device, coordinatorEndpoint, logger) => {
        const endpoint = device.getEndpoint(1);
        await reporting.bind(endpoint, coordinatorEndpoint, ['haElectricalMeasurement']);
        await reporting.rmsVoltage(endpoint) / 10;
        await reporting.rmsCurrent(endpoint);
        await reporting.activePower(endpoint);
    }
};

module.exports = device;
