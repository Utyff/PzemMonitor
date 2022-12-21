const {
    fromZigbeeConverters,
    toZigbeeConverters,
    reporting,
    exposes
} = require('zigbee-herdsman-converters');

const ACCESS_STATE = 0b001, ACCESS_WRITE = 0b010, ACCESS_READ = 0b100;

const OneJanuary2000 = new Date('January 01, 2000 00:00:00 UTC+00:00').getTime();

const e = exposes.presets;
const ea = exposes.access;

const fz = {
    local_time: {
        cluster: 'genTime',
        type: ['attributeReport', 'readResponse'],
        convert: (model, msg, publish, options, meta) => {
            return {local_time: msg.data.localTime};
        },
    },
};

const tz = {
    local_time: {
        // set
        key: ['local_time'],
        convertSet: async (entity, key, value, meta) => {
            const firstEndpoint = meta.device.getEndpoint(1);
            const time = Math.round((((new Date()).getTime() - OneJanuary2000) / 1000) + (((new Date()).getTimezoneOffset() * -1) * 60));
            // Time-master + synchronised
            await firstEndpoint.write('genTime', {time: time});
            return {state: {local_time: time}};
//            await firstEndpoint.write('genTime', {time: value});
//            return {state: {local_time: value}};
        },
        convertGet: async (entity, key, meta) => {
            const firstEndpoint = meta.device.getEndpoint(1);
            await firstEndpoint.read('genTime', ['time']);
        },
    },
};

const bind = async (endpoint, target, clusters) => {
    for (const cluster of clusters) {
        await endpoint.bind(cluster, target);
    }
};

const device = {
    zigbeeModel: ['DIYRuZ_PzemMonitor'],
    model: 'DIYRuZ_PzemMonitor',
    vendor: 'DIYRuZ',
    description: '[Energy Monitor](https://github.com/Utyff/PzemMonitor/)',
    supports: 'time',
    icon: 'https://raw.githubusercontent.com/Utyff/PzemMonitor/image/images/PZEM-004-module.jpg',
    fromZigbee: [fromZigbeeConverters.electrical_measurement, fz.local_time],
    toZigbee: [toZigbeeConverters.acvoltage,
        toZigbeeConverters.accurrent,
        toZigbeeConverters.electrical_measurement_power,
        toZigbeeConverters.frequency,
        toZigbeeConverters.powerfactor,
        tz.local_time],
    configure: async (device, coordinatorEndpoint, logger) => {
        const endpoint = device.getEndpoint(1);
        await bind(endpoint, coordinatorEndpoint, ['haElectricalMeasurement', 'genTime']);

        const rmsVoltageBindPayload = [{
            attribute: 'rmsVoltage',
            minimumReportInterval: 0,
            maximumReportInterval: 3600,
            reportableChange: 0,
        }];
        await endpoint.configureReporting('haElectricalMeasurement', rmsVoltageBindPayload);
        // await reporting.rmsVoltage(endpoint);

        const rmsCurrentBindPayload = [{
            attribute: 'rmsCurrent',
            minimumReportInterval: 0,
            maximumReportInterval: 3600,
            reportableChange: 0,
        }];
        await endpoint.configureReporting('haElectricalMeasurement', rmsCurrentBindPayload);
        // await reporting.rmsCurrent(endpoint);

        const activePowerBindPayload = [{
            attribute: 'activePower',
            minimumReportInterval: 0,
            maximumReportInterval: 3600,
            reportableChange: 0,
        }];
        await endpoint.configureReporting('haElectricalMeasurement', activePowerBindPayload);
        // await reporting.activePower(endpoint);

        const acFrequencyBindPayload = [{
            attribute: 'acFrequency',
            minimumReportInterval: 0,
            maximumReportInterval: 3600,
            reportableChange: 0,
        }];
        await endpoint.configureReporting('haElectricalMeasurement', acFrequencyBindPayload);
        // await reporting.acFrequency(endpoint);

        const powerFactorBindPayload = [{
            attribute: 'powerFactor',
            minimumReportInterval: 0,
            maximumReportInterval: 3600,
            reportableChange: 0,
        }];
        await endpoint.configureReporting('haElectricalMeasurement', powerFactorBindPayload);
        // await reporting.powerFactor(endpoint);

        const timeBindPayload = [{
            attribute: 'localTime',
            minimumReportInterval: 0,
            maximumReportInterval: 3600,
            reportableChange: 0,
        }];
        await endpoint.configureReporting('genTime', timeBindPayload);

        const time = Math.round((((new Date()).getTime() - OneJanuary2000) / 1000) + (((new Date()).getTimezoneOffset() * -1) * 60));
        // Time-master + synchronised
        firstEndpoint.write('genTime', {time: time});
    },
    exposes: [e.voltage(),
        e.current(),
        e.power(),
        e.energy(),
        e.ac_frequency(),
        e.power_factor(),
        exposes.enum('local_time', ACCESS_STATE | ACCESS_WRITE, ['set']).withDescription('Set date and time'),
    ],
};

module.exports = device;
