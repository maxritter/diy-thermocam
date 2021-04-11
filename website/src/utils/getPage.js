import _ from 'lodash';

/**
 * Get the page at the provided `urlPath`.
 *
 * @param {Array} pages Array of page objects. All pages must have 'url' field.
 * @param {string} urlPath The url path to find the page by
 * @return {Object}
 */
export default function getPage(pages, urlPath) {
    urlPath = _.trim(urlPath, '/');
    urlPath = urlPath.replace(/^src\/pages\//, '');

    return _.find(pages, page => {
        const pageUrlPath = _.trim(_.get(page, 'url'), '/');
        return urlPath === pageUrlPath;
    });
}
