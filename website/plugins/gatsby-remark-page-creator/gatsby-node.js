const path = require("path");
const fs = require("fs");
const {createFilePath} = require("gatsby-source-filesystem");
const _ = require("lodash");


function findFileNode({node, getNode}) {
    let fileNode = node;
    let ids = [fileNode.id];

    while (fileNode && fileNode.internal.type !== `File` && fileNode.parent) {
        fileNode = getNode(fileNode.parent);

        if (!fileNode) {
            break;
        }

        if (_.includes(ids, fileNode.id)) {
            console.log(`found cyclic reference between nodes`);
            break;
        }

        ids.push(fileNode.id);
    }

    if (!fileNode || fileNode.internal.type !== `File`) {
        console.log('did not find ancestor File node');
        return null;
    }

    return fileNode
}

exports.onCreateNode = ({node, getNode, actions}, options) => {

    const {createNodeField} = actions;

    if (node.internal.type === "MarkdownRemark") {
        let fileNode = findFileNode({node, getNode});
        if (!fileNode) {
            throw new Error('could not find parent File node for MarkdownRemark node: ' + node);
        }

        let url;
        if (node.frontmatter.url) {
            url = node.frontmatter.url;
        } else if (_.get(options, 'uglyUrls', false)) {
            url = path.join(fileNode.relativeDirectory, fileNode.name + '.html');
        } else {
            url = createFilePath({node, getNode});
        }

        createNodeField({node, name: "url", value: url});
        createNodeField({node, name: "absolutePath", value: fileNode.absolutePath});
        createNodeField({node, name: "relativePath", value: fileNode.relativePath});
        createNodeField({node, name: "absoluteDir", value: fileNode.dir});
        createNodeField({node, name: "relativeDir", value: fileNode.relativeDirectory});
        createNodeField({node, name: "base", value: fileNode.base});
        createNodeField({node, name: "ext", value: fileNode.ext});
        createNodeField({node, name: "name", value: fileNode.name});
    }
};

exports.createPages = ({graphql, getNode, actions, getNodesByType}) => {
    const {createPage, deletePage} = actions;

    // Use GraphQL to bring only the "id" and "html" (added by gatsby-transformer-remark)
    // properties of the MarkdownRemark nodes. Don't bring additional fields
    // such as "relativePath". Otherwise, Gatsby's GraphQL resolvers might infer
    // types these fields as File and change their structure. For example, the
    // "html" attribute exists only on a GraphQL node, but does not exist on the
    // underlying node.
    return graphql(`
    {
      allMarkdownRemark {
        edges {
          node {
            id
            html
          }
        }
      }
    }
    `).then(result => {
        if (result.errors) {
            return Promise.reject(result.errors);
        }

        const nodes = result.data.allMarkdownRemark.edges.map(({node}) => node);
        const siteNode = getNode('Site');
        const siteDataNode = getNode('SiteData');
        const sitePageNodes = getNodesByType('SitePage');
        const sitePageNodesByPath = _.keyBy(sitePageNodes, 'path');
        const siteData = _.get(siteDataNode, 'data', {});

        const pages = nodes.map(graphQLNode => {
            // Use the node id to get the underlying node. It is not exactly the
            // same node returned by GraphQL, because GraphQL resolvers might
            // transform node fields.
            const node = getNode(graphQLNode.id);
            return {
                url: node.fields.url,
                relativePath: node.fields.relativePath,
                relativeDir: node.fields.relativeDir,
                base: node.fields.base,
                name: node.fields.name,
                frontmatter: node.frontmatter,
                html: graphQLNode.html
            };
        });

        nodes.forEach(graphQLNode => {
            const node = getNode(graphQLNode.id);
            const url = node.fields.url;
            
            const template = node.frontmatter.template;
            if (!template) {
                console.error(`Error: undefined template for ${url}`);
                return;
            }

            const component = path.resolve(`./src/templates/${template}.js`);
            if (!fs.existsSync(component)) {
                console.error(`Error: component "src/templates/${template}.js" missing for ${url}`);
                return;
            }

            const existingPageNode = _.get(sitePageNodesByPath, url);

            const page = {
                path: url,
                component: component,
                context: {
                    url: url,
                    relativePath: node.fields.relativePath,
                    relativeDir: node.fields.relativeDir,
                    base: node.fields.base,
                    name: node.fields.name,
                    frontmatter: node.frontmatter,
                    html: graphQLNode.html,
                    pages: pages,
                    site: {
                        siteMetadata: _.get(siteData, 'site-metadata', {}),
                        pathPrefix: siteNode.pathPrefix,
                        data: _.omit(siteData, 'site-metadata')
                    }
                }
            };

            if (existingPageNode && !_.get(page, 'context.menus')) {
                page.context.menus = _.get(existingPageNode, 'context.menus');
            }

            createPage(page);
        });
    });
};
