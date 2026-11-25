import React from 'react';
import _ from 'lodash';

import {classNames, Link, withPrefix} from '../utils';

export default class DocsSubmenu extends React.Component {
    render() {
        let child_pages = _.get(this.props, 'child_pages', null);
        let page = _.get(this.props, 'page', null);
        return (
            <ul className="docs-submenu">
              {_.map(child_pages, (child_page, child_page_idx) => (
                <li key={child_page_idx} className={classNames('docs-menu-item', {'current': _.get(page, 'url', null) === _.get(child_page, 'url', null)})}>
                  <Link to={withPrefix(_.get(child_page, 'url', null))}>{_.get(child_page, 'frontmatter.title', null)}</Link>
                </li>
              ))}
            </ul>
        );
    }
}
