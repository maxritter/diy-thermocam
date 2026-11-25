const path = require('path');
const yaml = require('js-yaml');
const fse = require('fs-extra');
const chokidar = require('chokidar');
const _ = require('lodash');


const metadataFileName = 'site-metadata.json';

const parsers = {
    yaml: (data) => yaml.safeLoad(data, {schema: yaml.JSON_SCHEMA}),
    json: (data) => JSON.parse(data)
};

const supportedExtensions = {
    'yaml': parsers.yaml,
    'yml': parsers.yaml,
    'json': parsers.json
};

exports.sourceNodes = (props, pluginOptions = {}) => {
    const createContentDigest = props.createContentDigest;
    const { createNode } = props.actions;
    const reporter = props.reporter;

    if (!_.get(pluginOptions, 'path')) {
        pluginOptions.path = 'src/data';
    }

    if (!path.isAbsolute(pluginOptions.path)) {
        pluginOptions.path = path.resolve(process.cwd(), pluginOptions.path)
    }

    reporter.info(`[gatsby-source-data] setup file watcher and create site data`);

    const dataPath = pluginOptions.path;
    const createSiteDataFromFilesPartial = _.partial(createSiteDataFromFiles, { dataPath, createNode, createContentDigest, reporter });
    const watcher = chokidar.watch([dataPath, metadataFileName], {
        cwd: '.',
        ignoreInitial: true
    });
    watcher.on('add', createSiteDataFromFilesPartial);
    watcher.on('change', createSiteDataFromFilesPartial);
    watcher.on('unlink', createSiteDataFromFilesPartial);

    return createSiteDataFromFiles({ dataPath, createNode, createContentDigest, reporter }, null);
};

async function createSiteDataFromFiles({ dataPath, createNode, createContentDigest, reporter }, changedFile) {
    reporter.info(`[gatsby-source-data] create site data from files, updated path: ${changedFile}`);
    let dataFiles = [];

    const dataPathExists = await fse.pathExists(dataPath);
    if (dataPathExists) {
        dataFiles = await readDirRecursively(dataPath);
    }

    const metadataPath = path.resolve(metadataFileName);
    const metadataExists = await fse.pathExists(metadataPath);
    if (metadataExists) {
        dataFiles.push(metadataFileName);
    }

    const sortedDataFiles = dataFiles.slice().sort();
    const data = await convertDataFilesToJSON(sortedDataFiles, dataPath, reporter);

    createNode({
        id: 'SiteData',
        parent: null,
        children: [],
        data: data,
        internal: {
            type: 'SiteData',
            contentDigest: createContentDigest(JSON.stringify(data)),
            description: `Site data from ${path.relative(process.cwd(), dataPath)}`
        }
    });
}

async function readDirRecursively(dir, options) {
    const rootDir = _.get(options, 'rootDir', dir);
    const files = await fse.readdir(dir);
    const promises = _.map(files, async file => {
        const filePath = path.join(dir, file);
        const stats = await fse.stat(filePath);
        if (stats.isDirectory()) {
            return readDirRecursively(filePath, {rootDir});
        } else if (stats.isFile()) {
            return path.relative(rootDir, filePath);
        } else {
            return null;
        }
    });
    const recFiles = await Promise.all(promises);
    return _.chain(recFiles).compact().flatten().value();
}

function convertDataFilesToJSON(dataFiles, dataDirPath, reporter) {
    let promises = _.map(dataFiles, filePath => {
        const pathObject = path.parse(filePath);
        const absFilePath = pathObject.base === metadataFileName ? metadataFileName : path.join(dataDirPath, filePath);
        const relPath = pathObject.base === metadataFileName ? metadataFileName : filePath;
        const relDir = pathObject.base === metadataFileName ? '' : pathObject.dir;
        const ext = pathObject.ext.substring(1);
        if (!_.has(supportedExtensions, ext)) {
            return null;
        }
        return fse.readFile(absFilePath).then(data => {
            const propPath = _.compact(relDir.split(path.sep).concat(pathObject.name));
            const res = {};
            try {
                const parsedData = supportedExtensions[ext](data);
                _.set(res, propPath, parsedData);
            } catch (err) {
                reporter.warn(`[gatsby-source-data] could not parse file: ${relPath}`);
            }
            return res;
        });
    });
    return Promise.all(promises).then(results => {
        return _.reduce(results, (data, res) => _.merge(data, res), {})
    });
}
