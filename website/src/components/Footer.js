import React from 'react';
import _ from 'lodash';

import {htmlToReact} from '../utils';
import ActionLink from './ActionLink';

export default class Footer extends React.Component {
    render() {
        return (
            <footer id="colophon" className="site-footer outer">
              <div className="inner">
                <div className="site-footer-inside">
                  <p className="site-info">
                    {_.get(this.props, 'pageContext.site.siteMetadata.footer.content', null) && (
                    <span className="copyright">{htmlToReact(_.get(this.props, 'pageContext.site.siteMetadata.footer.content', null))}</span>
                    )}
                    {_.map(_.get(this.props, 'pageContext.site.siteMetadata.footer.links', null), (action, action_idx) => (
                      <ActionLink key={action_idx} {...this.props} action={action} />
                    ))}
                  </p>
                  {_.get(this.props, 'pageContext.site.siteMetadata.footer.has_social', null) && (
                  <div className="social-links">
                    {_.map(_.get(this.props, 'pageContext.site.siteMetadata.footer.social_links', null), (action, action_idx) => (
                      <ActionLink key={action_idx} {...this.props} action={action} />
                    ))}
                  </div>
                  )}
                </div>
              </div>
            </footer>
        );
    }
}
