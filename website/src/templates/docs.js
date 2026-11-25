import React from 'react';
import _ from 'lodash';
import {graphql} from 'gatsby';

import {Layout} from '../components/index';
import DocsMenu from '../components/DocsMenu';
import {htmlToReact, getPages, Link, withPrefix} from '../utils';

// this minimal GraphQL query ensures that when 'gatsby develop' is running,
// any changes to content files are reflected in browser
export const query = graphql`
  query($url: String) {
    sitePage(path: {eq: $url}) {
      id
    }
  }
`;

export default class Docs extends React.Component {
    render() {
        let root_docs_path = _.trim(_.get(this.props, 'pageContext.site.data.doc_sections.root_docs_path', null), '/');
        let current_page_url = _.trim(_.get(this.props, 'pageContext.url', null), '/');
        return (
            <Layout {...this.props}>
            <div className="inner outer">
              <div className="docs-content">
                <DocsMenu {...this.props} page={this.props.pageContext} site={this.props.pageContext.site} />
                <article className="post type-docs">
                  <div className="post-inside">
                    <header className="post-header">
                      <h1 className="post-title line-left">{_.get(this.props, 'pageContext.frontmatter.title', null)}</h1>
                    </header>
                    <div className="post-content">
                      {htmlToReact(_.get(this.props, 'pageContext.html', null))}
                      {(root_docs_path !== current_page_url) && ((() => {
                          let child_pages = _.orderBy(getPages(this.props.pageContext.pages, current_page_url), 'frontmatter.weight');
                          let child_count = _.size(child_pages);
                          let has_children = (child_count > 0) ? (true) : false;
                          return (<React.Fragment>
                            {has_children && (
                              <ul id="docs-section-items" className="docs-section-items">
                                {_.map(child_pages, (child_page, child_page_idx) => (
                                <li key={child_page_idx} className="docs-section-item"><Link to={withPrefix(_.get(child_page, 'url', null))} className="docs-item-link">{_.get(child_page, 'frontmatter.title', null)}<span className="icon-angle-right" aria-hidden="true" /></Link></li>
                                ))}
                              </ul>
                            )}
                          </React.Fragment>);
                      })())}
                    </div>
                  </div>
                </article>
                <nav id="page-nav" className="page-nav">
                  <div id="page-nav-inside" className="page-nav-inside sticky">
                    <h2 className="page-nav-title">Jump to Section</h2>
                    <div id="page-nav-link-container" />
                  </div>
                </nav>
              </div>
            </div>
            </Layout>
        );
    }
}
