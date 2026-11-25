import React from 'react';
import _ from 'lodash';

import {Link, withPrefix, classNames} from '../utils';
import ActionLink from './ActionLink';
import Submenu from './Submenu';

export default class Header extends React.Component {
    render() {
        return (
            <header id="masthead" className="site-header outer">
              <div className="inner">
                <div className="site-header-inside">
                  <div className="site-branding">
                    {_.get(this.props, 'pageContext.site.siteMetadata.header.logo_img', null) ? (
                    <p className="site-logo"><Link to={withPrefix(_.get(this.props, 'pageContext.site.siteMetadata.header.url', null) || '/')}><img src={withPrefix(_.get(this.props, 'pageContext.site.siteMetadata.header.logo_img', null))} alt={_.get(this.props, 'pageContext.site.siteMetadata.header.logo_img_alt', null)} /></Link></p>
                    ) : 
                    <p className="site-title"><Link to={withPrefix(_.get(this.props, 'pageContext.site.siteMetadata.header.url', null) || '/')}>{_.get(this.props, 'pageContext.site.siteMetadata.header.title', null)}</Link></p>
                    }
                  </div>
                  {_.get(this.props, 'pageContext.site.siteMetadata.header.has_nav', null) && (<React.Fragment>
                  <nav id="main-navigation" className="site-navigation" aria-label="Main Navigation">
                    <div className="site-nav-inside">
                      <button id="menu-close" className="menu-toggle"><span className="screen-reader-text">Open Menu</span><span className="icon-close" aria-hidden="true" /></button>
                      <ul className="menu">
                      {_.map(_.get(this.props, 'pageContext.site.siteMetadata.header.nav_links', null), (action, action_idx) => {
                          let page_url = _.trim(_.get(this.props, 'pageContext.url', null), '/');
                          let action_url = _.trim(_.get(action, 'url', null), '/');
                          return (
                            <li key={action_idx} className={classNames('menu-item', {'has-children': _.get(action, 'has_subnav', null) && _.get(action, 'subnav_links', null), 'current': page_url === action_url, 'menu-button': _.get(action, 'style', null) !== 'link'})}>
                              <ActionLink {...this.props} action={action} />
                              {(_.get(action, 'has_subnav', null) && _.get(action, 'subnav_links', null)) && (<React.Fragment>
                                <button className="submenu-toggle"><span className="icon-angle-right" aria-hidden="true" /><span className="screen-reader-text">Sub-menu</span></button>
                                <Submenu {...this.props} submenu={_.get(action, 'subnav_links', null)} menu_class={'submenu'} page={this.props.pageContext} />
                              </React.Fragment>)}
                            </li>
                          )
                      })}
                      </ul>
                    </div>
                  </nav>
                  <button id="menu-open" className="menu-toggle"><span className="screen-reader-text">Close Menu</span><span className="icon-menu" aria-hidden="true" /></button>
                  </React.Fragment>)}
                </div>
              </div>
            </header>
        );
    }
}
