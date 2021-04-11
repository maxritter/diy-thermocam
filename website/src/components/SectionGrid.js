import React from 'react';
import _ from 'lodash';

import {classNames, htmlToReact, withPrefix, Link, markdownify} from '../utils';
import CtaButtons from './CtaButtons';

export default class SectionGrid extends React.Component {
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
                {_.get(section, 'grid_items', null) && (
                <div className="block-content">
                  <div className={classNames('grid', {'grid-col-2': _.get(section, 'col_number', null) === 'two', 'grid-col-3': _.get(section, 'col_number', null) === 'three'})}>
                    {_.map(_.get(section, 'grid_items', null), (item, item_idx) => (
                    <div key={item_idx} className="grid-item">
                      <div className="grid-item-inside">
                        {_.get(item, 'image', null) && (
                        <div className="grid-item-image">
                          <img src={withPrefix(_.get(item, 'image', null))} alt={_.get(item, 'image_alt', null)} />
                        </div>
                        )}
                        {_.get(item, 'title', null) && (
                        <h3 className="grid-item-title line-left">
                          {_.get(item, 'title_url', null) ? (
                          <Link to={withPrefix(_.get(item, 'title_url', null))}>{_.get(item, 'title', null)}</Link>
                          ) : 
                          _.get(item, 'title', null)
                          }
                        </h3>
                        )}
                        {_.get(item, 'content', null) && (
                        <div className="grid-item-content">
                          {markdownify(_.get(item, 'content', null))}
                        </div>
                        )}
                        {_.get(item, 'actions', null) && (
                        <div className="grid-item-buttons">
                          <CtaButtons {...this.props} actions={_.get(item, 'actions', null)} />
                        </div>
                        )}
                      </div>
                    </div>
                    ))}
                  </div>
                </div>
                )}
              </div>
            </section>
        );
    }
}
