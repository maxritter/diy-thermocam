const { withPrefix: gatsbyWithPrefix } = require("gatsby");
const _ = require('lodash');

export default function withPrefix(url) {
    if (!url) {
        return url;
    }

    if (_.startsWith(url, '#') || _.startsWith(url, 'http://') || _.startsWith(url, 'https://')) {
        return url;
    }
    return gatsbyWithPrefix(url);
}
