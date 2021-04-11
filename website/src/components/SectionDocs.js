import React from 'react';
import _ from 'lodash';

import {classNames, htmlToReact, pathJoin, getPage, Link, withPrefix} from '../utils';

export default class SectionDocs extends React.Component {
    render() {
        let section = _.get(this.props, 'section', null);
        return (
            <section id={_.get(section, 'section_id', null)} className={classNames('block', 'block-grid', 'outer', {'has-header': _.get(section, 'title', null) || _.get(section, 'subtitle', null)})}>
              <div className="inner">
                {(_.get(section, 'title', null) || _.get(section, 'subtitle', null)) && (
                <div className="block-header inner-sm">
                  {_.get(section, 'title', null) && (
                  <h2 className="block-title">{_.get(section, 'title', null)}</h2>
                  )}
                  {_.get(section, 'subtitle', null) && (
                  <p className="block-subtitle">{htmlToReact(_.get(section, 'subtitle', null))}</p>
                  )}
                </div>
                )}
                <div className="block-content">
                  <div className={classNames('grid', {'grid-col-2': _.get(section, 'col_number', null) === 'two', 'grid-col-3': _.get(section, 'col_number', null) === 'three'})}>
                    {_.map(_.get(this.props, 'pageContext.site.data.doc_sections.sections', null), (doc_section, doc_section_idx) => {
                        let doc_section_path = pathJoin(_.get(this.props, 'pageContext.site.data.doc_sections.root_docs_path', null), doc_section);
                        let doc_section_page = getPage(this.props.pageContext.pages, doc_section_path);
                        return (
                          <div key={doc_section_idx} className="grid-item">
                            <div className="grid-item-inside">
                              <h3 className="grid-item-title line-left"><Link to={withPrefix(_.get(doc_section_page, 'url', null))}>{_.get(doc_section_page, 'frontmatter.title', null)}</Link></h3>
                              {_.get(doc_section_page, 'frontmatter.excerpt', null) && (
                              <div className="grid-item-content">
                                <p>{htmlToReact(_.get(doc_section_page, 'frontmatter.excerpt', null))}</p>
                              </div>
                              )}
                              <div className="grid-item-buttons">
                                <Link to={withPrefix(_.get(doc_section_page, 'url', null))}>Learn More</Link>
                              </div>
                            </div>
                          </div>
                        )
                    })}
                  </div>
                </div>
              </div>
            </section>
        );
    }
}
