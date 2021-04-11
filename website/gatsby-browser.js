/**
 * Implement Gatsby's Browser APIs in this file.
 *
 * See: https://www.gatsbyjs.org/docs/browser-apis/
 */

// onPreRouteUpdate() and onRouteUpdate() are called before onInitialClientRender,
// use initialized flag to ensure that window.onGatsbyPreRouteUpdate() and
// window.onGatsbyRouteUpdate() will not be called before
// window.onGatsbyInitialClientRender() has run
let initialized = false;

exports.onInitialClientRender = () => {
    initialized = true;
    if ('onGatsbyInitialClientRender' in window && typeof window.onGatsbyInitialClientRender === 'function') {
        window.onGatsbyInitialClientRender();
    }
    if ('onGatsbyRouteUpdate' in window && typeof window.onGatsbyRouteUpdate === 'function') {
        window.onGatsbyRouteUpdate();
    }
};

exports.onRouteUpdate = () => {
    if (initialized && 'onGatsbyRouteUpdate' in window && typeof window.onGatsbyRouteUpdate === 'function') {
        window.onGatsbyRouteUpdate();
    }
};

exports.onPreRouteUpdate = () => {
    if (initialized && 'onGatsbyPreRouteUpdate' in window && typeof window.onGatsbyPreRouteUpdate === 'function') {
        window.onGatsbyPreRouteUpdate();
    }
};
