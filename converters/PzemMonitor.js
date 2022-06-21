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
    toZigbee: [tz.local_time],
    configure: async (device, coordinatorEndpoint, logger) => {
        const endpoint = device.getEndpoint(1);
        await bind(endpoint, coordinatorEndpoint, ['haElectricalMeasurement', 'genTime']);
        await reporting.rmsVoltage(endpoint);
        await reporting.rmsCurrent(endpoint);
        await reporting.activePower(endpoint);
        await reporting.acFrequency(endpoint);
        await reporting.powerFactor(endpoint);

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
    exposes: [e.power(),
        e.voltage(),
        e.current(),
        exposes.enum('local_time', ACCESS_STATE | ACCESS_WRITE, ['set']).withDescription('Set date and time'),
    ],
};

module.exports = device;
