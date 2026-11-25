import React from 'react';
import _ from 'lodash';

import {htmlToReact} from '../utils';
import CtaButtons from './CtaButtons';

export default class SectionCta extends React.Component {
    render() {
        let section = _.get(this.props, 'section', null);
        return (
            <section id={_.get(section, 'section_id', null)} className="block block-cta outer">
              <div className="inner">
                <div className="has-gradient">
                  <div className="grid grid-middle grid-center">
                    {(_.get(section, 'title', null) || _.get(section, 'subtitle', null)) && (
                    <div className="grid-item block-header">
                      {_.get(section, 'title', null) && (
                      <h2 className="block-title">{_.get(section, 'title', null)}</h2>
                      )}
                      {_.get(section, 'subtitle', null) && (
                      <p className="block-subtitle">{htmlToReact(_.get(section, 'subtitle', null))}</p>
                      )}
                    </div>
                    )}
                    {_.get(section, 'actions', null) && (
                    <div className="grid-item block-buttons">
                      <CtaButtons {...this.props} actions={_.get(section, 'actions', null)} />
                    </div>
                    )}
                  </div>
                </div>
              </div>
            </section>
        );
    }
}
