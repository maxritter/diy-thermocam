import React from 'react';
import _ from 'lodash';

import {toStyleObj, withPrefix, markdownify} from '../utils';
import CtaButtons from './CtaButtons';

export default class SectionHero extends React.Component {
    render() {
        let section = _.get(this.props, 'section', null);
        return (
            <section id={_.get(section, 'section_id', null)} className="block block-hero has-gradient outer">
              {_.get(section, 'image', null) && (
              <div className="bg-img" style={toStyleObj('background-image: url(\'' + withPrefix(_.get(section, 'image', null)) + '\')')}/>
              )}
              <div className="inner-sm">
                {_.get(section, 'title', null) && (
                <div className="block-header">
                  <h1 className="block-title">{_.get(section, 'title', null)}</h1>
                </div>
                )}
                {_.get(section, 'content', null) && (
                <div className="block-content">
                  {markdownify(_.get(section, 'content', null))}
                </div>
                )}
                {_.get(section, 'actions', null) && (
                <div className="block-buttons">
                  <CtaButtons {...this.props} actions={_.get(section, 'actions', null)} />
                </div>
                )}
              </div>
            </section>
        );
    }
}
